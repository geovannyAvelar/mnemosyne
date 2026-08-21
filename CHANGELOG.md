# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/).

## [Unreleased]

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
