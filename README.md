# Mnemosyne

[![CI](https://github.com/geovannyAvelar/mnemosyne/actions/workflows/ci.yml/badge.svg)](https://github.com/geovannyAvelar/mnemosyne/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/geovannyAvelar/mnemosyne)](https://github.com/geovannyAvelar/mnemosyne/releases/latest)

A native desktop reader for PDF, EPUB, HTML, Markdown, MOBI/AZW, CBZ comic, and plain text files, built with C++17 and Qt6.

## Features

- **PDF** — rendered via Poppler, with zoom, table of contents navigation, and text search.
- **EPUB** — chapter navigation via table of contents / spine, zoom (font size), dark mode, and highlights.
- **HTML** — rendered with a real Chromium engine (`QWebEngineView`), including JavaScript execution.
- **Markdown** — rendered via Qt's own Markdown support (`QTextDocument::setMarkdown()`), with a heading-derived table of contents, search, zoom, dark mode, and highlights — same feature set as EPUB.
- **MOBI/AZW/AZW3** — rendered via [libmobi](https://github.com/bfabiszewski/libmobi) (LGPLv3), with the same chapter/table-of-contents/zoom/dark-mode/highlight feature set as EPUB. **DRM-protected files are refused outright** — Mnemosyne never attempts to decrypt anything; it's a reader for files you already have unencrypted access to, not a DRM-removal tool. Embedded images aren't rendered yet (text/formatting only).
- **CBZ comics** — a zip of page images, reusing the same archive-reading code EPUB uses (no new dependency). Page navigation and zoom like PDF; no table of contents, search, or highlights, since there's no text layer to drive them.
- **Plain text (.txt)** — search, zoom, dark mode, highlights, and reading-progress sync, same as EPUB/Markdown, just addressed by raw character offset instead of a chapter/heading since plain text has no structure to derive one from.
- **Library / tabs** — open multiple books at once in tabs, with a persistent Library tab and recent-files list.
- **Highlights and notes**, synced across devices — highlight a passage in a color of your choosing, optionally attach a note to it, and browse every note in the sidebar's Notes tab. Exportable via File > Export Notes, as a Markdown document or as Anki flashcards, for the open book or your whole library at once.
- **Scroll-to-turn-page** — scrolling past the top/bottom edge of a page or chapter advances to the next/previous one.
- **Collapsible sidebar** — hide the table of contents / notes / search dock with a single chevron toggle.
- **Cross-device sync** — point Mnemosyne at a cloud-synced folder (Google Drive, iCloud Drive, Dropbox, etc.) and/or sign in with Google Drive, and it syncs reading position (page/zoom) and highlights/notes across every device reading the same book. Books are matched by content hash, not file path; a same-highlight edit made on two devices resolves last-edit-wins.
- **Plugins** (desktop) — drop a folder (`manifest.json` + a JS entry file) into the Plugins folder and enable it from Plugins > Manage Plugins... to add your own export format or react to reading/highlighting activity, via a small QuickJS-based JavaScript API with no file/network/process access unless explicitly granted. See [docs/plugins.md](docs/plugins.md).

## Requirements

- CMake 3.20+
- A C++17 compiler
- Qt6 (`Widgets`, `Test`, `WebEngineWidgets`)
- Poppler-Qt6
- libzip
- libmobi

On macOS with Homebrew:

```bash
brew install qt poppler-qt6 libzip libmobi
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

The plugin system (`MNEMOSYNE_ENABLE_PLUGINS`, on by default on
Windows/macOS/Linux — see [docs/plugins.md](docs/plugins.md)) fetches
[quickjs-ng](https://github.com/quickjs-ng/quickjs) via CMake's
`FetchContent` on first configure, so that first `cmake -S . -B build` needs
network access. It's the only dependency handled this way; everything else
is a system package. Pass `-DMNEMOSYNE_ENABLE_PLUGINS=OFF` to skip it
entirely.

## Running

```bash
open build/src/Mnemosyne.app
```

or pass one or more files to open on launch:

```bash
build/src/Mnemosyne.app/Contents/MacOS/Mnemosyne path/to/book.epub path/to/document.pdf path/to/book.mobi path/to/comic.cbz path/to/notes.txt
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

### APT repository (Debian / Ubuntu)

Every release also publishes to an APT repository, updated automatically on
each tag:

```bash
curl -fsSL https://geovannyavelar.github.io/mnemosyne/pubkey.gpg | sudo gpg --dearmor -o /usr/share/keyrings/mnemosyne.gpg
echo "deb [signed-by=/usr/share/keyrings/mnemosyne.gpg] https://geovannyavelar.github.io/mnemosyne stable main" | sudo tee /etc/apt/sources.list.d/mnemosyne.list
sudo apt update
sudo apt install mnemosyne-pdf
```

There's also an `unstable` distribution, rebuilt from the latest commit on
`main` on every push (see the "Unstable (latest main)" release) — always a
single package, no version history, and may be broken. Use the same key,
pointed at `unstable` instead of `stable`:

```bash
curl -fsSL https://geovannyavelar.github.io/mnemosyne/pubkey.gpg | sudo gpg --dearmor -o /usr/share/keyrings/mnemosyne.gpg
echo "deb [signed-by=/usr/share/keyrings/mnemosyne.gpg] https://geovannyavelar.github.io/mnemosyne unstable main" | sudo tee /etc/apt/sources.list.d/mnemosyne-unstable.list
sudo apt update
sudo apt install mnemosyne-pdf
```

Don't add both `.list` files at once — they publish the same package name at
different, conflicting versions, and apt will just pick whichever it prefers.

### Linux (.deb, manual build)

The build additionally installs a binary and a `.desktop` file, and CPack can produce a `.deb`:

```bash
cmake -S . -B build
cmake --build build
cd build
cpack -G DEB
```

This produces `mnemosyne-pdf_1.0.0_<arch>.deb` in `build/`, installable with:

```bash
sudo dpkg -i mnemosyne-pdf_1.0.0_<arch>.deb
sudo apt -f install   # pull in any missing runtime dependencies
```

Runtime dependencies (Qt6 Widgets/WebEngineWidgets, Poppler-Qt6, libzip, libmobi) are detected automatically via `dpkg-shlibdeps` at package-build time.

### macOS (.dmg)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./scripts/package-macos.sh build
```

Produces `build/Mnemosyne-macos-<arch>.dmg` (e.g. `Mnemosyne-macos-arm64.dmg`
on Apple Silicon) — a self-contained, drag-and-drop install with Qt and all
other dependencies bundled in (`macdeployqt` + `dylibbundler`), so Homebrew
isn't required on the machine running it.

### Windows (.zip and installer)

Built with [MSYS2](https://www.msys2.org/), either its MinGW64 environment
for x86_64 (`pacman -S
mingw-w64-x86_64-{toolchain,cmake,ninja,qt6-base,poppler-qt6,libzip,pkgconf,ntldd,nsis}
zip`) or its CLANGARM64 environment for arm64 (same package list with the
`mingw-w64-clang-aarch64-` prefix instead — except `nsis`, which has no
CLANGARM64 build; the installer step always falls back to the MinGW64 one,
which runs fine under Windows 11 ARM's x64 emulation). `scripts/package-windows*.sh`
detect which environment they're running in via `$MSYSTEM` and adjust
package names/paths accordingly.

MSYS2 doesn't package libmobi, so build it from source once (Autotools, not
its CMake support — that's flagged upstream as "not tested and not updated
regularly", and doesn't actually install anything). Substitute
`mingw-w64-clang-aarch64-{autotools,libtool}` and `--prefix=/clangarm64` on
arm64:

```bash
pacman -S mingw-w64-x86_64-autotools mingw-w64-x86_64-libtool
git clone --depth 1 https://github.com/bfabiszewski/libmobi.git
cd libmobi && ./autogen.sh && ./configure --prefix=/mingw64 && make && make install
```

Then the regular build:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./scripts/package-windows.sh build
./scripts/package-windows-installer.sh build
```

`package-windows.sh` produces `build/Mnemosyne-windows-<arch>.zip` — a
portable, extract-and-run package with Qt and all other dependencies bundled
in (`windeployqt6` + `ntldd`). `package-windows-installer.sh` wraps that same
staged copy into `build/Mnemosyne-Setup-<arch>.exe` (via [NSIS](https://nsis.sourceforge.io/)),
a proper per-machine installer with a Start Menu shortcut, an optional
Desktop shortcut, and an uninstaller registered in Add/Remove Programs.

Because these DLLs are unsigned MinGW builds, Windows SmartScreen may warn
on first run of either the installer or the unpacked `.zip` — this is
normal for small, freshly-built open-source binaries and isn't specific to
Mnemosyne.

**HTML support is not available on Windows** — QtWebEngine (Chromium) ships
no prebuilt Windows binaries at all, for either MinGW or MSVC, only a
multi-hour from-source build. PDF and EPUB are unaffected. This is
controlled by the `MNEMOSYNE_ENABLE_HTML` CMake option, which defaults to
`OFF` on Windows and `ON` everywhere else.

## License

GPLv3 — see [LICENSE](LICENSE). QtWebEngine and Poppler are both GPL-licensed, so this project is too.

## Syncing reading progress

Sync is opt-in and configured per-machine from the **Sync** menu: choose a folder that's already synced by a cloud provider (e.g. a Google Drive or iCloud Drive folder on disk). Mnemosyne creates a `MnemosyneSync/` subfolder there and writes one JSONL log file per device — each device only ever appends to its own file, so there's no write-write conflict between devices. When another device's log has a newer position for a book you're currently viewing, a banner prompts you to jump to it; nothing is applied automatically.
