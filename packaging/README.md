# packaging/ — pakowanie aplikacji Silver do dystrybucji

Nieprzetestowane, punkt wyjścia — pełny, zautomatyzowany packaging
(podpisywanie, notaryzacja macOS, .msi na Windows, publikacja) to
osobny, spory kawałek pracy (patrz ROADMAP.md, "Dystrybucja /
produkcja"). To co tu jest, to minimalny szablon na start dla Linuksa.

## Linux — AppImage (szkic)

1. `bytes build --release` (albo `h# compile --release`) -> binarka w `build/`.
2. Skopiuj binarkę + `libsilvershim.so` (+ opcjonalnie `libsilverjs.so`,
   `libsilverdialogs.so`) + `frontend/` do struktury AppDir:
   ```
   MojaApka.AppDir/
     AppRun              (skrypt uruchamiający binarkę z LD_LIBRARY_PATH=.)
     usr/bin/moja-apka
     usr/lib/libsilvershim.so
     moja-apka.desktop    (z app.desktop.template)
     moja-apka.png
   ```
3. `appimagetool MojaApka.AppDir` (https://github.com/AppImage/AppImageKit).

## Linux — .deb (szkic)

Standardowa struktura `debian/` z `dpkg-deb --build`; H# ma już coś
podobnego dla siebie w `config/packaging/` w repo H# — można się tym
wzorować.

## macOS / Windows

Nie zaczęte. macOS wymaga podpisywania + notaryzacji (Apple Developer
ID), Windows — podpisywania Authenticode i najlepiej instalatora
(.msi przez WiX albo Inno Setup). Oba wymagają też przetestowania
`native/silver_shim.c` na tych platformach (SDL2 jest cross-platform,
ale nie testowaliśmy).
