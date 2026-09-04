#pragma once

#include <functional>

class QWindow;

// Installs a small custom Touch Bar (previous/next page, zoom out/in) on
// window's native NSWindow, wired to the given callbacks -- for MacBooks
// with a physical Touch Bar (2016-2019 models; Apple dropped it from the
// lineup in 2021, replacing it with physical function keys). This file is
// only compiled for macOS (see src/CMakeLists.txt's `if(APPLE)` guard
// around it, itself inside the desktop-UI block that already excludes iOS).
namespace MacTouchBar {

// Call whenever the active tab becomes one this applies to (currently just
// PdfView) -- e.g. from MainWindow::onTabChanged(). Safe to call again with
// fresh callbacks to point an already-installed Touch Bar at a newly active
// tab's PdfView instead.
void installPdfControls(QWindow *window, std::function<void()> previousPage, std::function<void()> nextPage,
                         std::function<void()> zoomOut, std::function<void()> zoomIn);

// Reverts window's Touch Bar to AppKit's default -- call when the active
// tab is no longer one with custom controls (Library, non-PDF documents),
// so the Touch Bar doesn't keep calling into a tab that's no longer active.
void clearControls(QWindow *window);

} // namespace MacTouchBar
