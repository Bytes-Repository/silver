#include <tinyfiledialogs.h>
#include <string.h>

#define SILVER_PATH_MAX 4096

static char g_result[SILVER_PATH_MAX];

/* ---- silver_dialog_open_file(title, filter_desc, filter_pattern) -> string
 * Zwraca wybraną ścieżkę, albo "" jeśli anulowano.
 * filter_pattern np. "*.png;*.jpg", filter_desc np. "Obrazy" (może być ""). */
const char *silver_dialog_open_file(const char *title, const char *filter_desc,
                                     const char *filter_pattern) {
    const char *patterns[1];
    int n = 0;
    if (filter_pattern && filter_pattern[0] != '\0') {
        patterns[0] = filter_pattern;
        n = 1;
    }
    const char *res = tinyfd_openFileDialog(
        title, "", n, n ? patterns : NULL,
        filter_desc && filter_desc[0] ? filter_desc : NULL, 0);
    strncpy(g_result, res ? res : "", SILVER_PATH_MAX - 1);
    g_result[SILVER_PATH_MAX - 1] = '\0';
    return g_result;
}

/* ---- silver_dialog_save_file(title, default_name) -> string ---- */
const char *silver_dialog_save_file(const char *title, const char *default_name) {
    const char *res = tinyfd_saveFileDialog(title, default_name ? default_name : "",
                                             0, NULL, NULL);
    strncpy(g_result, res ? res : "", SILVER_PATH_MAX - 1);
    g_result[SILVER_PATH_MAX - 1] = '\0';
    return g_result;
}

/* ---- silver_dialog_open_folder(title) -> string ---- */
const char *silver_dialog_open_folder(const char *title) {
    const char *res = tinyfd_selectFolderDialog(title, "");
    strncpy(g_result, res ? res : "", SILVER_PATH_MAX - 1);
    g_result[SILVER_PATH_MAX - 1] = '\0';
    return g_result;
}

/* ---- silver_dialog_message(title, text, kind) -> bool ----
 * kind: "info" | "warning" | "error" | "question" — dla "question"
 * zwraca true = OK/Tak, false = Cancel/Nie. Dla reszty zawsze true. */
int silver_dialog_message(const char *title, const char *text, const char *kind) {
    const char *dtype = "ok";
    const char *icon  = "info";
    if (strcmp(kind, "question") == 0) { dtype = "yesno"; icon = "question"; }
    else if (strcmp(kind, "warning") == 0) icon = "warning";
    else if (strcmp(kind, "error") == 0)   icon = "error";

    int r = tinyfd_messageBox(title, text, dtype, icon, 1);
    return r != 0;
}
