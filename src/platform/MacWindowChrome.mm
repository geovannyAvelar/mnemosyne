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

    // The app draws its own close/minimize/fullscreen buttons (see
    // TrafficLightButton), so hide the real ones entirely rather than
    // trying to keep a native and a custom set in visual sync.
    const NSWindowButton buttonTypes[] = {
        NSWindowCloseButton,
        NSWindowMiniaturizeButton,
        NSWindowZoomButton,
    };
    for (NSWindowButton type : buttonTypes) {
        if (NSButton *button = [nsWindow standardWindowButton:type]) {
            button.hidden = YES;
        }
    }
}

} // namespace MacWindowChrome
