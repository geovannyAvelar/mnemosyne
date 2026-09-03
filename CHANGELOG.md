# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/).

## [1.11.0] - 2026-09-03

### Added

- Plugins can now add their own on-demand action under the **Plugins** menu (`mnemosyne.registerCommand`), receiving the open book's highlights (or `null`, when the Library tab is active) and able to report back via a new `mnemosyne.showMessage(text)`. See [docs/plugins.md](docs/plugins.md).
- Plugins can now add extra CSS to how a book renders (`mnemosyne.registerCssInjector`), for EPUB, MOBI, and Markdown — applied after Mnemosyne's own dark-mode styling, so a plugin can override it too. Plain text has no markup for CSS to target, so it's not included.
- Plugins can now show a simple input form (`mnemosyne.showForm`) — text, multi-line text, number, checkbox, and dropdown fields, rendered with Mnemosyne's own widgets rather than HTML, so there's no markup/script injection surface. Blocks until the user submits or cancels; the ~250ms per-hook time budget pauses while the dialog is open, so taking your time doesn't count against it.

### Fixed

- On macOS, Mnemosyne never showed up as an option to open PDF, EPUB, HTML, Markdown, MOBI, Kindle (AZW/AZW3), CBZ, or plain-text files — Finder's "Open With" menu and "Get Info" > "Open with" default-app picker didn't list it at all, and double-clicking a PDF never launched it. The app bundle now declares the document types it supports, so macOS registers it as an available viewer for all of them the next time it's launched.
- Desktop: reading position could revert to an earlier page/scroll spot after closing a tab or quitting shortly after turning a page or scrolling — each view only wrote it out up to 1.5s after the last change, and closing didn't wait for that save to fire. Closing a tab or quitting the app now flushes any pending position immediately, so reopening a book always resumes exactly where you left off.

## [1.10.0] - 2026-09-02

### Added

- Highlights and notes now sync across devices, the same way reading position already does — over a locally-synced folder (Google Drive/iCloud Drive/Dropbox on disk) and, when signed in, Google Drive's hidden app-data folder. Each highlight, note edit, color change, and deletion is logged per device and merged on a per-highlight basis (last edit wins; a delete beats a stale edit from a device that hasn't seen it yet), so two devices editing highlights independently converge without a manual conflict prompt. Highlights created before this update are picked up automatically the next time their book is opened. Included on Android/iOS too (Google Drive leg only, same as reading-position sync there), wired through the same shared `HighlightsModel` both platforms already use for their PDF/EPUB highlight overlays.
- Desktop: File > Export Notes exports the current book's highlights, either as a Markdown document (every highlight, with its note and page/chapter number where applicable) or as tab-separated Anki flashcards (highlighted passage as the front, note as the back — only highlights with a note become a card, ready for Anki's Notes > Import with the Basic note type). The same menu also has an "Export Library" pair covering every book in Recents with at least one highlight, in one combined file (Anki cards get their book title prefixed onto the front, so a mixed deck still shows what each card came from).
- Desktop: a QuickJS-based plugin system (new **Plugins** menu > Manage Plugins...). A plugin is a folder (`manifest.json` + a JS entry file) dropped into the app's Plugins folder and enabled from that dialog; it can register its own export format (shows up in File > Export Notes alongside Markdown/Anki) and listen for `documentOpened`/`highlightAdded`/`highlightChanged`/`highlightRemoved` events. No file, network, or process access is exposed to plugin code — only what's explicitly bound — and a plugin that throws or runs away is stopped (a ~250ms watchdog) and logged without affecting the app or other plugins. Commands/menu items and reader CSS customization are designed but not yet built (see the plugin system plan).
- Mnemosyne now restricts itself to a single running instance. Opening a file while it's already running (a second launch, a double-click, macOS's "open with") hands the file to the existing window and brings it to front instead of opening a duplicate.

### Fixed

- On macOS, the Sync menu could end up permanently empty and never appear in the menu bar at all, hiding Google Drive sign-in along with the rest of the sync options. It's now populated as soon as the app starts instead of waiting for the menu to be opened first.

## [1.9.0] - 2026-09-01

### Changed

- Desktop PDF and EPUB reading now scrolls continuously across page and chapter boundaries, loading the next one in as you scroll near it, instead of snapping the scrollbar to the top/bottom of the next page or chapter.

## [1.8.0] - 2026-08-31

### Added

- Notes: bookmarks are replaced by colorable highlight notes, browsable in a new Notes tab. Selecting text now offers "Add Note..." alongside "Highlight", opening one dialog with both the note text and a color palette (five presets plus a full picker) together — the same dialog edits an existing note. Every highlight now carries its own color instead of one fixed yellow for all of them. The sidebar's Bookmarks tab becomes a Notes tab listing every highlighted note with Edit/Remove and jump-to-page, and it updates live the moment a note is added, edited, or removed from the document itself.
- A working iOS build: Poppler/libzip/freetype cross-compiled for arm64, a native app target, and a document-picker bridge that copies picked files into the app's own sandbox (iOS has no persistable content URI the way Android's SAF does). The mobile PDF reader is rewritten as a continuous vertical scroll (Adobe Reader-style) instead of one-page-at-a-time swiping, with pinch-zoom anchored to the page center, a persistent zoom/page-jump toolbar, and an in-app dark mode toggle sharing desktop's setting. Also fixes duplicated recent-documents entries on iOS (deduped by content hash instead of path, since the picker copies to a fresh path every time) and a broken recent entry that failed to open with no feedback.
- A new app icon, replacing the Noto Emoji-derived book stack, masked to a circular badge across every platform and size. On Windows and Linux, the taskbar icon now switches between light/dark variants automatically to match the OS's own appearance setting.

### Fixed

- On macOS, a dead strip above the custom title bar that wasn't covered by any widget and couldn't be used to drag the window. Qt's Cocoa backend was caching title-bar frame margins from before the native title bar got stripped; the window frame is now nudged to force those margins to recompute, and clicks landing outside any child widget in that region now start a native window move.
- Cross-device reading-progress sync no longer shows a spurious "jump to other device's position?" prompt when the other device's hostname is (or resolves to) "localhost", as dev/test machines often do.

### Removed

- The Noto Emoji attribution from the About dialog, no longer needed now that the app icon doesn't use Noto Emoji artwork.

## [1.7.0] - 2026-08-27

### Added

- Thumbnails on the Library's recent-documents grid: a real preview of the first page (PDF/CBZ) or first chapter (EPUB/MOBI/Markdown/TXT), rendered off the UI thread and cached to disk so reopening the library doesn't re-render every entry. Falls back to a drawn placeholder (a dog-eared page bearing the format label) until the real thumbnail is ready, or when no cheap render is available (HTML).
- A Book Info panel for EPUB and MOBI, showing title/authors/publisher/description/cover pulled straight from the file's own OPF/EXTH metadata — no network access needed. When the book has an ISBN and "Look Up Book Info Online" is turned on (off by default, since it's the app's only network call unrelated to Drive sync), an Open Library lookup enhances that on top, field-by-field, and the result is disk-cached by ISBN. The panel joins the existing Contents/Bookmarks/Search sidebar group — it only appears when the current tab actually has something to show, and hides/shows together with the rest via the sidebar toggle.
- EPUB `<video>` elements now play: since the EPUB chapter renderer has no video support at all (the tag previously just vanished silently), each one becomes a "▶ Play Video" link that extracts the file (preferring an MP4 source) from the archive to a cache file and hands it to the OS's default video player.
- A way to remove books from the Library's recent-documents list: right-click an entry for a "Remove from Recent" action, which confirms first (defaulting to No) before removing just that entry. The file itself is untouched.
- Opening files by dragging them onto the window, one or more at a time — reuses the same open path as File > Open, so an unsupported file still gets the usual warning instead of failing silently.

### Fixed

- An oversized EPUB image (e.g. a full-bleed cover several thousand pixels wide) no longer overflows and clips past the page. The EPUB renderer ignores percentage-based CSS image sizing entirely — even when the book's own stylesheet already specifies `max-width:100%` — so such images are now capped with explicit pixel dimensions (aspect ratio preserved) during rendering.

## [1.6.2] - 2026-08-26

### Fixed

- On Linux, Google Drive sync could appear to sign itself out after the app had been open for a while, even though nothing had actually been revoked. The refresh token is kept in the desktop's secret store (gnome-keyring via libsecret), which can become transiently locked mid-session (e.g. after the screen locks or the machine wakes from suspend); the app was treating that as an outright sign-out instead of a temporary hiccup. Signed-in state now only changes on an explicit sign-out or a real revocation from Google.

## [1.6.1] - 2026-08-24

### Added

- Intel (x86_64) macOS builds are back — a new `macos-x86_64` CI job on GitHub's `macos-15-intel` runner, alongside the existing native Apple Silicon build. (GitHub retired the old `macos-13` Intel image in December 2025; `macos-15-intel` is its replacement and, per GitHub's own roadmap, will be the last x86_64 image it ships at all, retiring in turn around August 2027.) Every release now includes both a `Mnemosyne-macos-arm64.dmg` and a `Mnemosyne-macos-x86_64.dmg`.

## [1.6.0] - 2026-08-24

### Added

- Native arm64 packages for Linux and Windows, alongside macOS (which already built natively on arm64 since GitHub's `macos-latest` runners moved off Intel — that artifact is now just labeled explicitly). New `linux-arm64` (`ubuntu-24.04-arm`) and `windows-arm64` (`windows-11-arm`, MSYS2 CLANGARM64) CI jobs build and package alongside the existing x86_64 ones, and the Windows packaging scripts are now architecture-aware via `$MSYSTEM`. Every release now includes an arm64 `.deb`, `.dmg`, `.zip`, and installer.

## [1.5.1] - 2026-08-21

### Fixed

- The sidebar's saved open/closed state (added in 1.4.0) never actually restored correctly on launch — the app always came up with the sidebar visible regardless of what was saved. Two compounding bugs: (1) `toggleSidebar()`'s visibility check used `isVisible()`, which depends on the whole window already being shown and is always `false` during construction, so the startup restore call always took the wrong branch; (2) even after fixing that, `QMainWindowLayout` re-asserts its own default dock visibility during the main window's first `show()`, silently undoing a `hide()` called beforehand — fixed by deferring the restore to the next event-loop iteration, after that first show cycle completes. Also fixed a related desync: clicking the search icon while the sidebar was hidden showed it again without updating the saved preference, so it would incorrectly revert to hidden on the next launch.

## [1.5.0] - 2026-08-21

### Added

- Markdown (`.md`/`.markdown`) support, with the same feature set as EPUB: heading-derived table of contents, full-text search, bookmarks and reading-progress sync (local folder + Google Drive), highlights, dark mode, and Ctrl+scroll zoom. Rendered via Qt's own Markdown support (`QTextDocument::setMarkdown()`), so no new dependency was needed.
- MOBI/AZW/AZW3 support, via [libmobi](https://github.com/bfabiszewski/libmobi) (LGPLv3) — same feature set as EPUB again (chapter/table-of-contents navigation, search, bookmarks/sync, highlights, dark mode, zoom). DRM-protected files are refused outright (checked before any parsing is attempted); Mnemosyne never calls libmobi's decrypt functions. Embedded images aren't rendered yet.
- CBZ comic archive support — page-by-page image viewing (zoom, scroll-to-turn-page, bookmarks, reading-progress sync) reusing the same page-canvas widget PDF uses and the same zip-reading code EPUB uses, so no new dependency was needed. No table of contents, search, or highlights, since a comic has no text layer to drive them.
- Plain text (`.txt`) support, with the same feature set as Markdown (search, zoom, dark mode, highlights, bookmarks, reading-progress sync — local folder + Google Drive), addressed by raw character offset instead of a heading index since plain text has no structure to derive one from.

### Changed

- Split the `mnemosynecore` static library in two: a Widgets-free `mnemosynecore` (document parsing/rendering, bookmarks/highlights, sync, OAuth) and a new `mnemosynedesktopui` layered on top of it for everything Qt Widgets-based. No user-visible change — this just enforces, as a real build dependency, the boundary a future mobile (Qt Quick/QML) front end would need to link against instead of the desktop UI.

## [1.4.0] - 2026-08-20

### Added

- Windows installer (`Mnemosyne-Setup.exe`), alongside the existing portable `.zip`. Built with NSIS via `scripts/package-windows-installer.sh`; installs per-machine with a Start Menu shortcut, an optional Desktop shortcut, and a proper uninstaller in Add/Remove Programs.
- Ctrl+scroll wheel now zooms in/out in the PDF and EPUB views, matching the convention in most other apps, instead of scrolling or turning pages.
- The sidebar's open/closed state is now remembered across restarts.

### Changed

- Google Drive sign-in no longer asks each user to create their own Google Cloud OAuth client and paste in a Client ID/Secret. Mnemosyne now ships a shared OAuth client of its own, so "Sign in with Google Drive..." works with one click. (Maintainers building from source: see `docs/google-drive-setup.md` for the one-time step of provisioning that shared client.)

## [1.3.0] - 2026-08-13

### Added

- Search now highlights every match on the current page/chapter in a bold yellow overlay while you search, and jumps straight to the first result instead of waiting for you to click it.
- Search runs on a background thread with a progress spinner in the Search dock, so scanning a large document no longer freezes the UI.
- Emoji licensing attribution (Noto Emoji, Apache 2.0) added to the About dialog.

### Changed

- The main window now opens maximized instead of at a fixed 1024x768.

## [1.2.0] - 2026-08-12

### Added

- Google Drive sign-in as a second, optional cross-device sync backend, alongside the existing "point at a locally-synced folder" approach. Uses OAuth 2.0 with PKCE and a system-browser/loopback-redirect flow — no embedded webview, and Mnemosyne ships no credentials of its own; see `docs/google-drive-setup.md` for creating your own Google Cloud OAuth client. Reading progress syncs through the signed-in account's hidden Drive app-data folder, and refresh tokens are stored in the OS's own secret store (Keychain on macOS, Credential Manager on Windows, Secret Service via libsecret on Linux) rather than in plain settings.

## [1.1.1] - 2026-08-12

### Changed

- Renamed the `.deb`/APT package from `mnemosyne` to `mnemosyne-pdf` — Ubuntu's official repos already ship an unrelated package named `mnemosyne` (a flashcard app), so ours collided with it. The application, binary, and `.desktop` entry are unaffected; only the package identifier and install instructions change (`sudo apt install mnemosyne-pdf`).

## [1.1.0] - 2026-08-08

### Added

- APT repository, published to GitHub Pages and rebuilt automatically on every tagged release: `https://geovannyavelar.github.io/mnemosyne`. Signed with a dedicated GPG key; see the README for setup instructions.
- CI now skips on documentation-only changes (`**.md`, `LICENSE`) to `main` and pull requests.

## [1.0.0] - 2026-08-08

Initial release.

### Added

- PDF reading via Poppler-Qt6, with zoom, table of contents navigation, and text search.
- EPUB reading with chapter navigation, zoom (font size), dark mode, and highlights.
- HTML reading via a real Chromium engine (`QWebEngineView`), including JavaScript execution — macOS and Linux only, see Known Limitations below.
- Library / tabs — open multiple books at once, with a persistent Library tab and recent-files list.
- Bookmarks and highlights, stored locally per book.
- Scroll-to-turn-page: scrolling past a page/chapter edge advances to the next/previous one.
- Collapsible sidebar for the table of contents / bookmarks / search dock.
- Cross-device reading progress sync over a cloud-synced folder (Google Drive, iCloud Drive, Dropbox, etc.), matching books across devices by content hash rather than file path.
- Application icon (Noto Emoji "books", Apache 2.0).
- Packaging for all three desktop platforms:
  - Linux: `.deb` via CPack, with a `.desktop` entry and `hicolor` icon theme integration.
  - macOS: a self-contained, drag-and-drop `.dmg` (Qt and all other dependencies bundled via `macdeployqt`/`dylibbundler`).
  - Windows: a self-contained portable `.zip` (Qt and all other dependencies bundled via `windeployqt6`/`ntldd`).
- GitHub Actions CI building, testing, and packaging on Linux, macOS, and Windows for every push and pull request.

### Known Limitations

- HTML support is unavailable in the Windows build. QtWebEngine (Chromium) ships no prebuilt Windows binaries at all, for either MinGW or MSVC — only a multi-hour from-source build, which isn't part of this release's CI. PDF and EPUB are unaffected.
