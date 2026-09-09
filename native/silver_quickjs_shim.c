#include <quickjs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SILVER_MAX_CTX      8
#define SILVER_QUEUE_LEN    256
#define SILVER_STR_MAX      4096

typedef struct {
    char call_id[64];
    char cmd[128];
    char args_json[SILVER_STR_MAX];
} PendingInvoke;

typedef struct {
    int        used;
    JSRuntime *rt;
    JSContext *ctx;
    JSValue    global;

    /* prosta kolejka FIFO (ring buffer) wypełniana przez JS
       (__silver_native_invoke), opróżniana przez js_poll_invoke */
    PendingInvoke queue[SILVER_QUEUE_LEN];
    int head;
    int tail;
    int count;
} SilverJsCtx;

static SilverJsCtx g_ctx[SILVER_MAX_CTX];

/* ---- natywna funkcja wstrzyknięta do globalThis, wołana przez
 * src/js/silver_api.js jako `__silver_native_invoke(callId, cmd, json)` ---- */
static JSValue native_invoke(JSContext *ctx, JSValueConst this_val,
                              int argc, JSValueConst *argv) {
    /* znajdź, do którego SilverJsCtx należy ten JSContext */
    SilverJsCtx *sc = NULL;
    for (int i = 0; i < SILVER_MAX_CTX; i++) {
        if (g_ctx[i].used && g_ctx[i].ctx == ctx) { sc = &g_ctx[i]; break; }
    }
    if (!sc) return JS_UNDEFINED;
    if (argc < 3) return JS_UNDEFINED;
    if (sc->count >= SILVER_QUEUE_LEN) return JS_UNDEFINED; /* kolejka pełna — pomiń */

    const char *call_id = JS_ToCString(ctx, argv[0]);
    const char *cmd     = JS_ToCString(ctx, argv[1]);
    const char *args    = JS_ToCString(ctx, argv[2]);

    PendingInvoke *slot = &sc->queue[sc->tail];
    snprintf(slot->call_id, sizeof(slot->call_id), "%s", call_id ? call_id : "");
    snprintf(slot->cmd, sizeof(slot->cmd), "%s", cmd ? cmd : "");
    snprintf(slot->args_json, sizeof(slot->args_json), "%s", args ? args : "{}");

    sc->tail = (sc->tail + 1) % SILVER_QUEUE_LEN;
    sc->count++;

    if (call_id) JS_FreeCString(ctx, call_id);
    if (cmd)     JS_FreeCString(ctx, cmd);
    if (args)    JS_FreeCString(ctx, args);
    return JS_UNDEFINED;
}

/* ---- js_init(win_handle) -> js context handle (>=0) lub -1 ---- */
int js_init(int win_handle) {
    int slot = -1;
    for (int i = 0; i < SILVER_MAX_CTX; i++) {
        if (!g_ctx[i].used) { slot = i; break; }
    }
    if (slot < 0) return -1;

    JSRuntime *rt = JS_NewRuntime();
    if (!rt) return -1;
    JSContext *ctx = JS_NewContext(rt);
    if (!ctx) { JS_FreeRuntime(rt); return -1; }

    JSValue global = JS_GetGlobalObject(ctx);
    JSValue fn = JS_NewCFunction(ctx, native_invoke, "__silver_native_invoke", 3);
    JS_SetPropertyStr(ctx, global, "__silver_native_invoke", fn);

    g_ctx[slot].used  = 1;
    g_ctx[slot].rt    = rt;
    g_ctx[slot].ctx   = ctx;
    g_ctx[slot].global = global;
    g_ctx[slot].head = 0;
    g_ctx[slot].tail = 0;
    g_ctx[slot].count = 0;

    return slot;
}

static char *read_whole_file(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)len + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t read_n = fread(buf, 1, (size_t)len, f);
    fclose(f);
    buf[read_n] = '\0';
    if (out_len) *out_len = read_n;
    return buf;
}

/* ---- js_load_file(handle, path) -> bool ----
 * Ewaluuje plik JS w kontekście (np. src/js/silver_api.js, a potem
 * frontend/app.js — wołaj dwa razy, w tej kolejności, z H#). */
int js_load_file(int h, const char *path) {
    if (h < 0 || h >= SILVER_MAX_CTX || !g_ctx[h].used) return 0;
    size_t len = 0;
    char *src = read_whole_file(path, &len);
    if (!src) return 0;

    JSValue res = JS_Eval(g_ctx[h].ctx, src, len, path, JS_EVAL_TYPE_GLOBAL);
    int ok = !JS_IsException(res);
    if (!ok) {
        JSValue exc = JS_GetException(g_ctx[h].ctx);
        const char *msg = JS_ToCString(g_ctx[h].ctx, exc);
        if (msg) {
            fprintf(stderr, "[silver-js] %s: %s\n", path, msg);
            JS_FreeCString(g_ctx[h].ctx, msg);
        }
        JS_FreeValue(g_ctx[h].ctx, exc);
    }
    JS_FreeValue(g_ctx[h].ctx, res);
    free(src);
    return ok;
}

/* ---- js_poll_invoke(handle) -> "" | "call_id\u0001cmd\u0001json" ----
 * Zwraca wskaźnik na statyczny bufor ważny do następnego wywołania
 * (ten sam wzorzec co silver_event_payload w silver_shim.c). */
static char g_poll_buf[SILVER_STR_MAX + 256];

const char *js_poll_invoke(int h) {
    if (h < 0 || h >= SILVER_MAX_CTX || !g_ctx[h].used) return "";
    SilverJsCtx *sc = &g_ctx[h];
    if (sc->count == 0) return "";

    PendingInvoke *slot = &sc->queue[sc->head];
    snprintf(g_poll_buf, sizeof(g_poll_buf), "%s\x01%s\x01%s",
             slot->call_id, slot->cmd, slot->args_json);

    sc->head = (sc->head + 1) % SILVER_QUEUE_LEN;
    sc->count--;
    return g_poll_buf;
}

/* ---- js_resolve(handle, call_id, result_json) -> bool ----
 * Woła globalThis.__silver_resolve(call_id, result_json) w JS —
 * to rozwiązuje odpowiedni Promise po stronie frontendu. */
int js_resolve(int h, const char *call_id, const char *result_json) {
    if (h < 0 || h >= SILVER_MAX_CTX || !g_ctx[h].used) return 0;
    JSContext *ctx = g_ctx[h].ctx;

    JSValue fn = JS_GetPropertyStr(ctx, g_ctx[h].global, "__silver_resolve");
    if (!JS_IsFunction(ctx, fn)) { JS_FreeValue(ctx, fn); return 0; }

    JSValueConst args[2];
    args[0] = JS_NewString(ctx, call_id ? call_id : "");
    args[1] = JS_NewString(ctx, result_json ? result_json : "null");
    JSValue ret = JS_Call(ctx, fn, g_ctx[h].global, 2, args);

    int ok = !JS_IsException(ret);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, args[0]);
    JS_FreeValue(ctx, args[1]);
    JS_FreeValue(ctx, fn);
    return ok;
}

/* ---- js_emit(handle, event_name, payload_json) -> bool ----
 * Woła globalThis.__silver_dispatch_event(name, payload) — dociera do
 * wszystkich `silver.listen(name, cb)` zarejestrowanych w JS. */
int js_emit(int h, const char *event_name, const char *payload_json) {
    if (h < 0 || h >= SILVER_MAX_CTX || !g_ctx[h].used) return 0;
    JSContext *ctx = g_ctx[h].ctx;

    JSValue fn = JS_GetPropertyStr(ctx, g_ctx[h].global, "__silver_dispatch_event");
    if (!JS_IsFunction(ctx, fn)) { JS_FreeValue(ctx, fn); return 0; }

    JSValueConst args[2];
    args[0] = JS_NewString(ctx, event_name ? event_name : "");
    args[1] = JS_NewString(ctx, payload_json ? payload_json : "null");
    JSValue ret = JS_Call(ctx, fn, g_ctx[h].global, 2, args);

    int ok = !JS_IsException(ret);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, args[0]);
    JS_FreeValue(ctx, args[1]);
    JS_FreeValue(ctx, fn);
    return ok;
}

int js_shutdown(int h) {
    if (h < 0 || h >= SILVER_MAX_CTX || !g_ctx[h].used) return 0;
    JS_FreeValue(g_ctx[h].ctx, g_ctx[h].global);
    JS_FreeContext(g_ctx[h].ctx);
    JS_FreeRuntime(g_ctx[h].rt);
    memset(&g_ctx[h], 0, sizeof(SilverJsCtx));
    return 1;
}
