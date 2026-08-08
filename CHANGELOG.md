# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/).

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
