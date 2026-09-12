#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define SILVER_MAX_WINDOWS 8
#define SILVER_EVBUF_LEN   512
#define SILVER_MAX_IMAGES  64

typedef struct {
    char         path[256];
    SDL_Texture *tex;
    int          w;
    int          h;
} SilverImage;

typedef struct {
    int          used;
    SDL_Window   *win;
    SDL_Renderer *ren;
    TTF_Font     *font;
    char         evbuf[SILVER_EVBUF_LEN]; /* ostatni event jako "type|a|b|extra" */
    int          open;
    SilverImage  images[SILVER_MAX_IMAGES]; /* prosty cache tekstur per-okno */
    int          image_count;
} SilverWindow;

static SilverWindow g_windows[SILVER_MAX_WINDOWS];
static int g_sdl_ready = 0;

static int ensure_sdl(void) {
    if (g_sdl_ready) return 1;
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 0;
    if (TTF_Init() != 0) return 0;
    IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
    g_sdl_ready = 1;
    return 1;
}

/* ---- silver_window_create(title, w, h) -> handle (>=0) lub -1 ---- */
int silver_window_create(const char *title, int w, int h) {
    if (!ensure_sdl()) return -1;
    int slot = -1;
    for (int i = 0; i < SILVER_MAX_WINDOWS; i++) {
        if (!g_windows[i].used) { slot = i; break; }
    }
    if (slot < 0) return -1;

    SDL_Window *win = SDL_CreateWindow(
        title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        w, h, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!win) return -1;

    SDL_Renderer *ren = SDL_CreateRenderer(
        win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) { SDL_DestroyWindow(win); return -1; }

    g_windows[slot].used = 1;
    g_windows[slot].win  = win;
    g_windows[slot].ren  = ren;
    g_windows[slot].font = NULL;
    g_windows[slot].open = 1;
    g_windows[slot].evbuf[0] = '\0';
    g_windows[slot].image_count = 0;
    return slot;
}

/* ---- silver_load_font(handle, path, size) -> bool ---- */
int silver_load_font(int h, const char *path, int size) {
    if (h < 0 || h >= SILVER_MAX_WINDOWS || !g_windows[h].used) return 0;
    if (g_windows[h].font) TTF_CloseFont(g_windows[h].font);
    g_windows[h].font = TTF_OpenFont(path, size);
    return g_windows[h].font != NULL;
}

/* ---- silver_is_open(handle) -> bool ---- */
int silver_is_open(int h) {
    if (h < 0 || h >= SILVER_MAX_WINDOWS || !g_windows[h].used) return 0;
    return g_windows[h].open;
}

/* ---- silver_poll_event(handle) -> int event code -----------------
 * 0 = brak zdarzenia, 1 = QUIT, 2 = MOUSE_DOWN, 3 = MOUSE_UP,
 * 4 = MOUSE_MOVE, 5 = KEY_DOWN, 6 = RESIZE, 7 = TEXT_INPUT,
 * 8 = MOUSE_WHEEL, 9 = FOCUS_GAINED, 10 = FOCUS_LOST, 11 = KEY_UP
 * Szczegóły (x, y, keycode, tekst) pobiera się przez
 * silver_event_payload(handle), zwracające "x|y|extra".
 * ------------------------------------------------------------------ */
int silver_poll_event(int h) {
    if (h < 0 || h >= SILVER_MAX_WINDOWS || !g_windows[h].used) return 0;
    SDL_Event e;
    if (!SDL_PollEvent(&e)) return 0;

    switch (e.type) {
        case SDL_QUIT:
            g_windows[h].open = 0;
            return 1;
        case SDL_MOUSEBUTTONDOWN:
            snprintf(g_windows[h].evbuf, SILVER_EVBUF_LEN, "%d|%d|%d",
                     e.button.x, e.button.y, e.button.button);
            return 2;
        case SDL_MOUSEBUTTONUP:
            snprintf(g_windows[h].evbuf, SILVER_EVBUF_LEN, "%d|%d|%d",
                     e.button.x, e.button.y, e.button.button);
            return 3;
        case SDL_MOUSEMOTION:
            snprintf(g_windows[h].evbuf, SILVER_EVBUF_LEN, "%d|%d|0",
                     e.motion.x, e.motion.y);
            return 4;
        case SDL_KEYDOWN:
            snprintf(g_windows[h].evbuf, SILVER_EVBUF_LEN, "0|0|%d",
                     e.key.keysym.sym);
            return 5;
        case SDL_KEYUP:
            snprintf(g_windows[h].evbuf, SILVER_EVBUF_LEN, "0|0|%d",
                     e.key.keysym.sym);
            return 11;
        case SDL_WINDOWEVENT:
            if (e.window.event == SDL_WINDOWEVENT_RESIZED) {
                snprintf(g_windows[h].evbuf, SILVER_EVBUF_LEN, "%d|%d|0",
                         e.window.data1, e.window.data2);
                return 6;
            }
            if (e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) return 9;
            if (e.window.event == SDL_WINDOWEVENT_FOCUS_LOST)   return 10;
            return 0;
        case SDL_TEXTINPUT:
            snprintf(g_windows[h].evbuf, SILVER_EVBUF_LEN, "0|0|%s",
                     e.text.text);
            return 7;
        case SDL_MOUSEWHEEL: {
            int dy = e.wheel.y;
            if (e.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) dy = -dy;
            snprintf(g_windows[h].evbuf, SILVER_EVBUF_LEN, "0|%d|0", dy * 24);
            return 8;
        }
        default:
            return 0;
    }
}

/* zwraca payload ostatniego eventu jako string "a|b|extra" */
const char *silver_event_payload(int h) {
    if (h < 0 || h >= SILVER_MAX_WINDOWS || !g_windows[h].used) return "";
    return g_windows[h].evbuf;
}

/* ---- prymitywy rysujące — wołane przez src/engine/paint.h# ---- */

int silver_clear(int h, int r, int g, int b) {
    if (h < 0 || h >= SILVER_MAX_WINDOWS || !g_windows[h].used) return 0;
    SDL_SetRenderDrawColor(g_windows[h].ren, r, g, b, 255);
    SDL_RenderClear(g_windows[h].ren);
    return 1;
}

int silver_fill_rect(int h, int x, int y, int w, int rh, int r, int g, int b, int a) {
    if (h < 0 || h >= SILVER_MAX_WINDOWS || !g_windows[h].used) return 0;
    /* v0.2: naprawiony błąd — `a` był przyjmowany od zawsze, ale bez
     * włączonego blend mode SDL2 go IGNORUJE (SDL_BLENDMODE_NONE =
     * dest = src, piksel w pełni kryjący niezależnie od alfy). Czyli
     * `opacity` na tle elementu najpewniej nigdy realnie nie działało
     * (nawet po dodaniu wsparcia w engine/style.h# i engine/paint.h# —
     * kod H# przekazywał poprawną wartość, ale native SDL i tak ją
     * odrzucał). Patrz ROADMAP.md. */
    SDL_SetRenderDrawBlendMode(g_windows[h].ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_windows[h].ren, r, g, b, a);
    SDL_Rect rect = { x, y, w, rh };
    SDL_RenderFillRect(g_windows[h].ren, &rect);
    return 1;
}

int silver_stroke_rect(int h, int x, int y, int w, int rh, int r, int g, int b, int a) {
    if (h < 0 || h >= SILVER_MAX_WINDOWS || !g_windows[h].used) return 0;
    /* v0.2: kanał alfa (wcześniej stroke_rect zawsze rysował w pełni
     * kryjąco — patrz ROADMAP.md, "Alfa dla tekstu/obramowań/obrazków"
     * była udokumentowanym ograniczeniem opacity aż do tej zmiany).
     * SDL2 wymaga włączonego blend mode, żeby alfa < 255 w ogóle coś
     * dała (domyślny SDL_BLENDMODE_NONE ją ignoruje). */
    SDL_SetRenderDrawBlendMode(g_windows[h].ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_windows[h].ren, r, g, b, a);
    SDL_Rect rect = { x, y, w, rh };
    SDL_RenderDrawRect(g_windows[h].ren, &rect);
    return 1;
}

/* ---- silver_draw_text(handle, x, y, text, r, g, b, a) -> wysokość tekstu w px ---- */
int silver_draw_text(int h, int x, int y, const char *text, int r, int g, int b, int a) {
    if (h < 0 || h >= SILVER_MAX_WINDOWS || !g_windows[h].used) return 0;
    if (!g_windows[h].font || !text || text[0] == '\0') return 0;

    /* v0.2: `a` w kolorze glifów TTF_RenderUTF8_Blended nie wystarcza
     * samo w sobie (blending glifu z tłem surface'u, nie z ekranem) —
     * potrzebny jeszcze SDL_SetTextureAlphaMod na finalnej teksturze,
     * żeby faktycznie przenikało przez to, co już narysowane pod spodem. */
    SDL_Color col = { (Uint8)r, (Uint8)g, (Uint8)b, 255 };
    SDL_Surface *surf = TTF_RenderUTF8_Blended(g_windows[h].font, text, col);
    if (!surf) return 0;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(g_windows[h].ren, surf);
    int th = surf->h;
    SDL_Rect dst = { x, y, surf->w, surf->h };
    SDL_FreeSurface(surf);
    if (tex) {
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        SDL_SetTextureAlphaMod(tex, (Uint8)a);
        SDL_RenderCopy(g_windows[h].ren, tex, NULL, &dst);
        SDL_DestroyTexture(tex);
    }
    return th;
}

/* ---- silver_measure_text(handle, text) -> szerokość w px ---- */
int silver_measure_text(int h, const char *text) {
    if (h < 0 || h >= SILVER_MAX_WINDOWS || !g_windows[h].used) return 0;
    if (!g_windows[h].font || !text) return 0;
    int w = 0, th = 0;
    TTF_SizeUTF8(g_windows[h].font, text, &w, &th);
    return w;
}

int silver_present(int h) {
    if (h < 0 || h >= SILVER_MAX_WINDOWS || !g_windows[h].used) return 0;
    SDL_RenderPresent(g_windows[h].ren);
    return 1;
}

/* ---- input tekstowy (dla <input> edytowalnych, patrz engine + app.h#) ---- */
int silver_start_text_input(int h) {
    if (h < 0 || h >= SILVER_MAX_WINDOWS || !g_windows[h].used) return 0;
    SDL_StartTextInput();
    return 1;
}

int silver_stop_text_input(int h) {
    if (h < 0 || h >= SILVER_MAX_WINDOWS || !g_windows[h].used) return 0;
    SDL_StopTextInput();
    return 1;
}

/* ---- obrazki: prosty cache tekstur per-okno kluczowany ścieżką pliku ---- */
static SilverImage *find_or_load_image(int h, const char *path) {
    SilverWindow *w = &g_windows[h];
    for (int i = 0; i < w->image_count; i++) {
        if (strncmp(w->images[i].path, path, sizeof(w->images[i].path)) == 0) {
            return &w->images[i];
        }
    }
    if (w->image_count >= SILVER_MAX_IMAGES) return NULL;

    SDL_Surface *surf = IMG_Load(path);
    if (!surf) return NULL;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(w->ren, surf);
    int iw = surf->w, ih = surf->h;
    SDL_FreeSurface(surf);
    if (!tex) return NULL;

    SilverImage *slot = &w->images[w->image_count++];
    snprintf(slot->path, sizeof(slot->path), "%s", path);
    slot->tex = tex;
    slot->w = iw;
    slot->h = ih;
    return slot;
}

/* ---- silver_image_width/height(handle, path) -> px (0 jeśli błąd) ----
 * Rozmiar naturalny obrazka — wołane z layoutu, gdy CSS nie podaje
 * jawnie width/height dla <img>. */
int silver_image_width(int h, const char *path) {
    if (h < 0 || h >= SILVER_MAX_WINDOWS || !g_windows[h].used) return 0;
    SilverImage *img = find_or_load_image(h, path);
    return img ? img->w : 0;
}

int silver_image_height(int h, const char *path) {
    if (h < 0 || h >= SILVER_MAX_WINDOWS || !g_windows[h].used) return 0;
    SilverImage *img = find_or_load_image(h, path);
    return img ? img->h : 0;
}

/* ---- silver_draw_image(handle, path, x, y, w, h, a) -> bool ----
 * `img->tex` jest cache'owana per-ścieżka (find_or_load_image) i może
 * być tą samą teksturą narysowaną wielokrotnie w tej samej klatce z
 * różną alfą (ten sam plik w dwóch miejscach o różnym opacity) —
 * bezpieczne, bo SDL2 renderuje synchronicznie: AlphaMod ustawiony
 * tuż przed RenderCopy obowiązuje tylko dla TEGO wywołania. */
int silver_draw_image(int h, const char *path, int x, int y, int w, int rh, int a) {
    if (h < 0 || h >= SILVER_MAX_WINDOWS || !g_windows[h].used) return 0;
    SilverImage *img = find_or_load_image(h, path);
    if (!img) return 0;
    SDL_SetTextureBlendMode(img->tex, SDL_BLENDMODE_BLEND);
    SDL_SetTextureAlphaMod(img->tex, (Uint8)a);
    SDL_Rect dst = { x, y, w, rh };
    SDL_RenderCopy(g_windows[h].ren, img->tex, NULL, &dst);
    return 1;
}

/* ---- silver_system_theme() -> "dark" | "light" ----
 * Best-effort: tylko Linux/GNOME (gsettings), przez popen. Na innych
 * pulpitach/platformach zwraca zawsze "light" — patrz ROADMAP.md,
 * "Motywy systemowe" wciąż wymaga realnej implementacji dla
 * macOS (NSAppearance) i Windows (rejestr AppsUseLightTheme). */
const char *silver_system_theme(void) {
    static char buf[16];
    strcpy(buf, "light");
#if defined(__linux__)
    FILE *p = popen("gsettings get org.gnome.desktop.interface color-scheme 2>/dev/null", "r");
    if (p) {
        char line[128];
        if (fgets(line, sizeof(line), p) && strstr(line, "dark")) {
            strcpy(buf, "dark");
        }
        pclose(p);
    }
#endif
    return buf;
}

int silver_set_title(int h, const char *title) {
    if (h < 0 || h >= SILVER_MAX_WINDOWS || !g_windows[h].used) return 0;
    SDL_SetWindowTitle(g_windows[h].win, title);
    return 1;
}

/* dwie osobne funkcje zamiast out-parametrów wskaźnikowych — H# FFI
 * (extern static/dynamic, oba warianty) wspiera tylko typy skalarne w
 * sygnaturach, więc unikamy `int*` jako parametru wyjściowego. */
int silver_get_width(int h) {
    if (h < 0 || h >= SILVER_MAX_WINDOWS || !g_windows[h].used) return 0;
    int w = 0, hh = 0;
    SDL_GetWindowSize(g_windows[h].win, &w, &hh);
    return w;
}

int silver_get_height(int h) {
    if (h < 0 || h >= SILVER_MAX_WINDOWS || !g_windows[h].used) return 0;
    int w = 0, hh = 0;
    SDL_GetWindowSize(g_windows[h].win, &w, &hh);
    return hh;
}

int silver_window_close(int h) {
    if (h < 0 || h >= SILVER_MAX_WINDOWS || !g_windows[h].used) return 0;
    if (g_windows[h].font) TTF_CloseFont(g_windows[h].font);
    for (int i = 0; i < g_windows[h].image_count; i++) {
        if (g_windows[h].images[i].tex) SDL_DestroyTexture(g_windows[h].images[i].tex);
    }
    SDL_DestroyRenderer(g_windows[h].ren);
    SDL_DestroyWindow(g_windows[h].win);
    memset(&g_windows[h], 0, sizeof(SilverWindow));
    return 1;
}

int silver_delay(int ms) {
    SDL_Delay((Uint32)ms);
    return 1;
}
