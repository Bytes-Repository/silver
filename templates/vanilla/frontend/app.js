// Prawdziwy JS jest dostępny przez native/silver_quickjs_shim.c
// (od v0.2 linkowany statycznie, jak reszta shimów Silver),
// ale trzeba go jawnie włączyć w src/main.h#:
//
//   a = app::load_js(a, ["<ścieżka-do-pakietu-silver>/src/js/silver_api.js",
//                        "frontend/app.js"])
//
// Domyślnie ten szablon używa prostszego on-click="..." w HTML, bez JS
// (patrz frontend/index.html) — mniej ruchomych części na start.
//
// Docelowe API (już gotowe w silver_api.js):
//   const res = await silver.invoke("greet", {});
//   silver.listen("some-event", (payload) => { ... });
