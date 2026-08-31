#include "MacWindowChrome.h"

#import <Cocoa/Cocoa.h>

#include <QWindow>

namespace MacWindowChrome {

void integrateTitleBar(QWindow *window)
{
    if (!window) {
        return;
    }

    NSView *view = reinterpret_cast<NSView *>(window->winId());
    NSWindow *nsWindow = view.window;
    if (!nsWindow) {
        return;
    }

    nsWindow.titlebarAppearsTransparent = YES;
    nsWindow.titleVisibility = NSWindowTitleHidden;
    nsWindow.styleMask |= NSWindowStyleMaskFullSizeContentView;

    // Qt's Cocoa backend cached the old title-barred frame/content-rect
    // margins when the window was first shown, before this method (deferred
    // -- see main.cpp) removed the title bar. Nothing tells it to recompute
    // them afterward, so QWindow::position()/geometry() keep reporting the
    // window's content as starting ~32px (the old title bar height) below
    // where FullSizeContentView actually put it, and everything Qt lays out
    // -- including TopBar -- inherits that offset, leaving a dead strip at
    // the true top of the window that belongs to no widget and isn't
    // native-draggable either, since Qt's content view already covers it.
    // Re-announcing the frame makes AppKit redeliver the frame/resize
    // notifications Qt's QCocoaWindow uses to refresh those cached margins.
    // Setting the *same* frame is a no-op for AppKit (no notification is
    // sent unless the frame actually changes), so nudge the height by a
    // point and back to force two genuine frame-changed notifications.
    NSRect frame = nsWindow.frame;
    NSRect nudged = frame;
    nudged.size.height += 1;
    [nsWindow setFrame:nudged display:YES];
    [nsWindow setFrame:frame display:YES];

    // The app draws its own close/minimize/fullscreen buttons (see
    // TrafficLightButton), so get rid of the real ones entirely rather than
    // trying to keep a native and a custom set in visual sync. Just setting
    // .hidden doesn't stick -- AppKit's own window-button state machine
    // un-hides them again on events like becoming key or a resize, so they
    // reappear stacked above the custom row. Removing them from the view
    // hierarchy is more durable, but still not permanent: NSThemeFrame
    // recreates fresh standard-button instances during later layout passes
    // (e.g. the setWindowTitle() call MainWindow makes when a document tab
    // opens), so a single removal misses those. Re-running it on every
    // window update -- title changes, resizes, sheets closing, all funnel
    // through this notification -- keeps the buttons gone for the life of
    // the window instead of just at startup.
    void (^removeStandardButtons)() = ^{
        const NSWindowButton buttonTypes[] = {
            NSWindowCloseButton,
            NSWindowMiniaturizeButton,
            NSWindowZoomButton,
        };
        for (NSWindowButton type : buttonTypes) {
            if (NSButton *button = [nsWindow standardWindowButton:type]) {
                [button removeFromSuperview];
            }
        }
    };
    removeStandardButtons();
    [[NSNotificationCenter defaultCenter] addObserverForName:NSWindowDidUpdateNotification
                                                        object:nsWindow
                                                         queue:nil
                                                    usingBlock:^(NSNotification *) {
                                                        removeStandardButtons();
                                                    }];
}

} // namespace MacWindowChrome
