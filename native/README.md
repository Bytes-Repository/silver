# native/ — natywna warstwa Silver

`silver_shim.c` to **jedyny** kawałek C w Silver. Nie jest to WebView
ani przeglądarka — to ~250 linii kodu owijające SDL2 w płaskie funkcje
(int/string), żeby dało się je wywołać z H# przez `extern dynamic`
(zobacz komentarz na górze `silver_shim.c` po "dlaczego").

Cały DOM/CSS/layout/paint pipeline (parsowanie HTML/CSS, box model,
kolejność rysowania, hit-testing kliknięć) jest po stronie H#, w
`src/engine/*.h#` — to jest właśnie ten "własny lekki silnik".

## Zależności

- SDL2 (`libsdl2-dev` / `SDL2-devel`)
- SDL2_ttf (`libsdl2-ttf-dev` / `SDL2_ttf-devel`)
- SDL2_image (`libsdl2-image-dev` / `SDL2_image-devel`) — od v0.3, dla `<img>`

## Build

```bash
gcc -shared -fPIC -O2 silver_shim.c -o libsilvershim.so \
    $(pkg-config --cflags --libs sdl2 SDL2_ttf SDL2_image)
```

Umieść `libsilvershim.so` tam, gdzie H# runtime szuka bibliotek
dynamicznych ładowanych przez `extern dynamic [c, "silvershim"]`
(zwykle katalog binarki, albo `LD_LIBRARY_PATH`) — dokładny mechanizm
resolvingu zależy od implementacji `extern dynamic` w H# (na dzień
pisania tego kodu FFI był w README opisany, ale nie widziałem w
repo faktycznego resolvera bibliotek dynamicznych w akcji — jeśli
`bytes`/`h#` szuka bibliotek w innym miejscu niż cwd, dopasuj ścieżkę).

## Status

Zaimplementowane i (na poziomie C, kompiluje się z SDL2 zainstalowanym)
gotowe do testu:
- tworzenie/zamykanie okna, event loop (quit/mysz/klawiatura/resize)
- prymitywy rysujące: `clear`, `fill_rect`, `stroke_rect`, `draw_text`,
  `measure_text`
- ładowanie fontu TTF

**Nie zostało to skompilowane ani przetestowane w tym środowisku** —
brak tu zainstalowanego SDL2/toolchaina H# (LLVM 21 + Rust 1.85+,
patrz README H#). Przed pierwszym użyciem: zbuduj `.so` lokalnie i
przejdź przez `h# check src/` w projekcie Silver, żeby złapać
ewentualne niezgodności składni z aktualną wersją kompilatora — H# to
język w bardzo aktywnym rozwoju (v0.9) i część API stdlib użytego tu
(`array_push`, `string_slice`, metody na fn-typach w polach structów)
nie ma w repo przykładu 1:1 identycznego z tym, jak używa go Silver.
Zobacz `ROADMAP.md` → "Rzeczy do zweryfikowania przy pierwszym buildzie".

## native/silver_quickjs_shim.c — most JS

Podłączony w `src/js/js_bridge.h#` + `app::load_js`/`app::run`
(patrz `app.h#`). Wymaga zainstalowanego QuickJS (nagłówek `quickjs.h`
+ biblioteka) — np. z github.com/quickjs-ng/quickjs albo dystrybucji.

```bash
gcc -shared -fPIC -O2 silver_quickjs_shim.c -o libsilverjs.so \
    -I/path/do/quickjs -lquickjs -lm -lpthread
```

Jak `silver_shim.c` — **niesprawdzone w praktyce** (brak tu
zainstalowanego QuickJS). Zakłada standardowe API QuickJS
(`JS_NewRuntime`, `JS_NewCFunction`, `JS_Call`, ...) — różne forki
(bellard/quickjs vs quickjs-ng) mogą się nieznacznie różnić, dopasuj
przy pierwszym buildzie z realnym `quickjs.h`.

## native/silver_dialogs_shim.c — natywne dialogi systemowe

Podłączony w `src/ffi_dialogs.h#` + `src/dialog.h#` (odpowiednik
`@tauri-apps/plugin-dialog`). Wymaga `tinyfiledialogs.h`/`.c` (MIT,
pobierz osobno — https://sourceforge.net/projects/tinyfiledialogs/,
nie dołączone do tego repo).

```bash
gcc -shared -fPIC -O2 silver_dialogs_shim.c tinyfiledialogs.c \
    -o libsilverdialogs.so
```

Jak reszta shimów — niesprawdzone w praktyce.
