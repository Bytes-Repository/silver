# Silver

Lekki framework do natywnych aplikacji desktopowych dla **H#** —
architektura jak Tauri (frontend HTML/CSS/JS, backend w języku
systemowym, komunikacja przez komendy), ale **bez WebView**. Zamiast
osadzać przeglądarkę, Silver ma własny, mały silnik: parsuje podzbiór
HTML/CSS, liczy layout i rysuje bezpośrednio przez SDL2 (cienki, ~250-
liniowy shim C — nie przeglądarka, nie webview).

```
frontend/ (HTML + CSS + JS)          src/ (H#)
   index.html  ──┐                      app.h#      — builder, event loop
   style.css   ──┼─► engine/*.h# ──►     window.h#   — okno (FFI/SDL2)
   app.js      ──┘   (Twój, w H#)        ipc.h#      — komendy invoke()
                                          engine/     — DOM/CSS/layout/paint
```

## Status: v0.3 — rozbudowany silnik, coraz bliżej Tauri

To jest uczciwa informacja, nie skromność. Ta biblioteka **nie została
skompilowana ani przetestowana** w środowisku, w którym powstała (brak
tu toolchaina H# — LLVM 21 + Rust 1.85+ — i SDL2/QuickJS/tinyfiledialogs).
Kod jest napisany najbliżej jak się dało realnej, potwierdzonej składni
H# (wzorowany na faktycznym kodzie źródłowym `bytes`), ale zanim
zaczniesz budować na tym produkcyjną aplikację, przeczytaj
**`ROADMAP.md`** — sekcje "Blokery po stronie samego H#" i "Rzeczy do
zweryfikowania przy pierwszym buildzie" (w v0.3 doszedł m.in.
rekurencyjny enum w `engine/json.h#` — to najbardziej ryzykowna
konstrukcja typu w całym projekcie). Część fundamentów, na których
stoi Silver (`std -> mem`, `std -> async_`, `std -> gtk`), jest w samym
H# jeszcze niedokończona lub w ogóle nie istnieje.

Co doszło w v0.3: realny pomiar tekstu (nie szacowanie), flexbox
(uproszczony), `<img>`, scroll, edytowalne `<input>`, natywne dialogi
systemowe (open/save/dialog przez tinyfiledialogs), pełny zagnieżdżony
JSON (`engine/json.h#`, niezależny od stubowego `std -> json`), hot
reload frontendu, prosty system uprawnień komend, testy jednostkowe
parserów, oraz naprawiony podwójny layout na klik. Pełna lista w
`ROADMAP.md`.

Co działa dziś (na poziomie architektury/kodu źródłowego): HTML/CSS z
dziedziczeniem koloru/rozmiaru czcionki i podstawowym zawijaniem
tekstu, renderowane na ekran; kliknięcia w elementy z
`on-click="komenda"` **oraz** prawdziwe `await silver.invoke("komenda")`
z JS (silnik QuickJS, `app::load_js`) wołają te same zarejestrowane
funkcje H#; reaktywne `{{klucz}}` w HTML aktualizowane co klatkę ze
stanu aplikacji.

## Szybki start

```bash
cd native
gcc -shared -fPIC -O2 silver_shim.c -o libsilvershim.so \
    $(pkg-config --cflags --libs sdl2 SDL2_ttf SDL2_image)
# opcjonalnie, dla prawdziwego JS w frontend/app.js:
gcc -shared -fPIC -O2 silver_quickjs_shim.c -o libsilverjs.so \
    -I/path/do/quickjs -lquickjs -lm -lpthread
# skopiuj .so tam, gdzie H# ładuje extern dynamic

silver new moja-apka
cd moja-apka
# dołącz jakiś .ttf do frontend/fonts/ (tekst się nie narysuje bez fontu)
bytes add silver
bytes run
```

## Przykład — minimalna aplikacja (on-click, bez JS)

```hsharp
use "silver -> app" from "app"

fn greet(args_json: string) -> string is
    return "{\"message\": \"Cześć z H#!\"}"
end

fn main() is
    let mut a = app::new("Moja Apka", 800, 600)
    a = app::with_font(a, "frontend/fonts/DejaVuSans.ttf", 16)
    a = app::load_html(a, "frontend/index.html")
    a = app::load_css(a, "frontend/style.css")
    a = app::command(a, "greet", greet)
    app::run(a)
end
```

```html
<!-- frontend/index.html -->
<div class="card">
    <h1>Witaj</h1>
    <div class="button" on-click="greet"><span>Kliknij</span></div>
</div>
```

Pełniejszy przykład ze stanem aplikacji i `{{count}}`: `examples/counter/`.
Flex, edytowalny `<input>` i natywny dialog: `examples/kitchen_sink/`.

## Przykład — prawdziwy JS zamiast on-click

```hsharp
use "silver -> app" from "app"

fn greet(args_json: string) -> string is
    return "{\"message\": \"Cześć z H#!\"}"
end

fn main() is
    let mut a = app::new("Moja Apka", 800, 600)
    a = app::with_font(a, "frontend/fonts/DejaVuSans.ttf", 16)
    a = app::load_html(a, "frontend/index.html")
    a = app::load_css(a, "frontend/style.css")
    a = app::command(a, "greet", greet)
    a = app::load_js(a, ["frontend/silver_api.js", "frontend/app.js"])
    app::run(a)
end
```

```js
// frontend/app.js
document.querySelector("#greet-btn")?.addEventListener("click", async () => {
    const res = await silver.invoke("greet", {});
    console.log(res.message);
});
```

`silver.invoke`/`silver.listen` (`src/js/silver_api.js`) idą przez
QuickJS (`native/silver_quickjs_shim.c`) do tego samego `ipc::Router`
co `on-click` — możesz mieszać oba style w jednej aplikacji.

## Dlaczego "własny silnik", a nie WebView — i dlaczego to *nie* jest
##  "przeglądarka od zera"

WebView (jak w Tauri) daje pełny CSS/JS/DOM za cenę: dużego binarnego
zależnościowego drzewa (systemowy WebKit/WebView2), niespójności
między platformami, i narzutu procesu przeglądarki. Silver idzie w
drugą stronę — **podzbiór** HTML/CSS wystarczający do UI typu
aplikacja (karty, formularze, przyciski, listy), renderowany
bezpośrednio przez SDL2. To radykalnie mniej kodu i mniej zależności,
kosztem: brak pełnego CSS, brak prawdziwego silnika JS na starcie,
brak zgodności z dowolną stroną WWW. To świadomy kompromis, opisany
szczegółowo w `ROADMAP.md`.

## Struktura repo

```
Bytes.hk                    manifest pakietu (biblioteka)
native/
  silver_shim.c               SDL2(+_image) → płaskie funkcje C wołane przez FFI
  silver_quickjs_shim.c        QuickJS → poll-queue most JS ↔ H#
  silver_dialogs_shim.c         tinyfiledialogs → natywne dialogi
  README.md                      jak zbudować wszystkie .so
src/
  ffi_shim.h#                 extern dynamic (SDL2) — okno/rysowanie/obrazki
  ffi_dialogs.h#                extern dynamic (tinyfiledialogs)
  window.h#                      wygodne API okna
  dialog.h#                        natywne dialogi (open/save/confirm)
  ipc.h#                              rejestr komend + Origin (Both/HtmlOnly/JsOnly)
  app.h#                                builder + główna pętla + most JS + scroll/input/hot-reload
  lib.h#                                  punkt wejścia pakietu
  engine/
    dom.h#      parser HTML → płaskie drzewo węzłów (+ src/value)
    css.h#       parser CSS → reguły + kaskada (+ flex/gap/justify/align)
    style.h#      CssDecl → ComputedStyle (kolory, box model, dziedziczenie)
    layout.h#      block stack + inline flow + flexbox + realny pomiar tekstu
    paint.h#         rysowanie (+ obrazki, edytowalne <input>, scroll)
    template.h#        {{klucz}} → wartość z app::store (reaktywny tekst)
    json.h#              pełny, zagnieżdżony JSON (niezależny od std -> json)
  js/
    js_bridge.h#    extern dynamic (QuickJS) — most invoke/listen, podłączony
    silver_api.js    frontendowe silver.invoke()/silver.listen()
  cli/
    silver_cli.h#     `silver new <nazwa>`
templates/vanilla/       szablon dla `silver new`
examples/counter/         przykład ze stanem + {{count}}
examples/kitchen_sink/     flex + <input> + natywny dialog
tests/engine_tests.h#       testy jednostkowe parserów DOM/CSS/JSON
packaging/                  szkic AppImage/.deb (Linux, nieprzetestowane)
.github/workflows/ci.yml     szkic CI (kroki TODO tam, gdzie zależą od toolchaina H#)
ROADMAP.md                   co działa, co nie, co dalej — czytaj to
```

## Licencja

MIT (zgodnie z resztą ekosystemu bytes.io).
