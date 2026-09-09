// Opcjonalny frontend JS dla tego przykładu (patrz komentarz w
// src/main.h# — odkomentuj app::load_js, żeby to zaczęło działać).
// Gdyby przyciski w HTML NIE miały on-click, tak wyglądałoby to samo
// przez prawdziwy silver.invoke():
//
// document.querySelector(".plus").addEventListener("click", async () => {
//   const res = await silver.invoke("increment", {});
//   console.log("nowa wartość:", res.count);
// });
