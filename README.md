# Mnemosyne

[![CI](https://github.com/geovannyAvelar/mnemosyne/actions/workflows/ci.yml/badge.svg)](https://github.com/geovannyAvelar/mnemosyne/actions/workflows/ci.yml)

A native desktop reader for PDF, EPUB, and HTML files, built with C++17 and Qt6.

## Features

- **PDF** — rendered via Poppler, with zoom, table of contents navigation, and text search.
- **EPUB** — chapter navigation via table of contents / spine, zoom (font size), dark mode, and highlights.
- **HTML** — rendered with a real Chromium engine (`QWebEngineView`), including JavaScript execution.
- **Library / tabs** — open multiple books at once in tabs, with a persistent Library tab and recent-files list.
- **Bookmarks and highlights**, stored locally per book.
- **Scroll-to-turn-page** — scrolling past the top/bottom edge of a page or chapter advances to the next/previous one.
- **Collapsible sidebar** — hide the table of contents / bookmarks / search dock with a single chevron toggle.
- **Cross-device reading progress sync** — point Mnemosyne at a cloud-synced folder (Google Drive, iCloud Drive, Dropbox, etc.) and it writes an append-only per-device log of page/zoom changes. Other devices reading the same folder detect newer progress and prompt before jumping to it. Books are matched across devices by content hash, not file path.

## Requirements

- CMake 3.20+
- A C++17 compiler
- Qt6 (`Widgets`, `Test`, `WebEngineWidgets`)
- Poppler-Qt6
- libzip

On macOS with Homebrew:

```bash
brew install qt poppler-qt6 libzip
```

`qt`, `poppler-qt6`, and `libzip` are keg-only, so `CMakeLists.txt` already adds their Homebrew prefixes (`/usr/local/opt/...`) to `CMAKE_PREFIX_PATH`. If Homebrew is installed elsewhere (e.g. Apple Silicon's default `/opt/homebrew`), pass the right prefixes explicitly, e.g.:

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH="/opt/homebrew/opt/qt;/opt/homebrew/opt/poppler-qt6;/opt/homebrew/opt/libzip"
```

## Building

```bash
cmake -S . -B build
cmake --build build
```

This produces the `Mnemosyne` app bundle inside `build/src/`.

## Running

```bash
open build/src/Mnemosyne.app
```

or pass one or more files to open on launch:

```bash
build/src/Mnemosyne.app/Contents/MacOS/Mnemosyne path/to/book.epub path/to/document.pdf
```

## Testing

The test suite uses Qt Test and is registered with CTest:

```bash
ctest --test-dir build --output-on-failure
```

## Packaging

Prebuilt packages for every tagged release are attached to the corresponding
[GitHub release](https://github.com/geovannyAvelar/mnemosyne/releases). To
build one yourself:

### Linux (.deb)

The build additionally installs a binary and a `.desktop` file, and CPack can produce a `.deb`:

```bash
cmake -S . -B build
cmake --build build
cd build
cpack -G DEB
```

This produces `mnemosyne_1.0.0_<arch>.deb` in `build/`, installable with:

```bash
sudo dpkg -i mnemosyne_1.0.0_<arch>.deb
sudo apt -f install   # pull in any missing runtime dependencies
```

Runtime dependencies (Qt6 Widgets/WebEngineWidgets, Poppler-Qt6, libzip) are detected automatically via `dpkg-shlibdeps` at package-build time.

### macOS (.dmg)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./scripts/package-macos.sh build
```

Produces `build/Mnemosyne-macos.dmg` — a self-contained, drag-and-drop
install with Qt and all other dependencies bundled in (`macdeployqt` +
`dylibbundler`), so Homebrew isn't required on the machine running it.

### Windows (.zip)

Built with [MSYS2](https://www.msys2.org/)'s MinGW64 environment (`pacman -S
mingw-w64-x86_64-{toolchain,cmake,ninja,qt6-base,poppler-qt6,libzip,pkgconf,ntldd}
zip`):

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./scripts/package-windows.sh build
```

Produces `build/Mnemosyne-windows.zip`, with Qt and all other dependencies
bundled in (`windeployqt6` + `ntldd`).

**HTML support is not available on Windows** — QtWebEngine (Chromium) ships
no prebuilt Windows binaries at all, for either MinGW or MSVC, only a
multi-hour from-source build. PDF and EPUB are unaffected. This is
controlled by the `MNEMOSYNE_ENABLE_HTML` CMake option, which defaults to
`OFF` on Windows and `ON` everywhere else.

## License

GPLv3 — see [LICENSE](LICENSE). QtWebEngine and Poppler are both GPL-licensed, so this project is too.

## Syncing reading progress

Sync is opt-in and configured per-machine from the **Sync** menu: choose a folder that's already synced by a cloud provider (e.g. a Google Drive or iCloud Drive folder on disk). Mnemosyne creates a `MnemosyneSync/` subfolder there and writes one JSONL log file per device — each device only ever appends to its own file, so there's no write-write conflict between devices. When another device's log has a newer position for a book you're currently viewing, a banner prompts you to jump to it; nothing is applied automatically.
