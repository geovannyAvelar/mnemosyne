#include "MacTouchBar.h"

#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>

#include <QWindow>

namespace {
NSString *const kPreviousPageId = @"com.mnemosyne.touchbar.previousPage";
NSString *const kNextPageId = @"com.mnemosyne.touchbar.nextPage";
NSString *const kZoomOutId = @"com.mnemosyne.touchbar.zoomOut";
NSString *const kZoomInId = @"com.mnemosyne.touchbar.zoomIn";
// Any variable's address makes a fine, unique associated-object key -- its
// value is never read, only used as an identity.
char kControllerAssociationKey = 0;
}

// Bridges the four callbacks passed to installPdfControls() to real
// NSButton actions and NSTouchBarDelegate calls -- kept alive for as long
// as its owning NSWindow via an associated object (see installPdfControls()
// below), since NSTouchBar's own `delegate` property is a weak reference
// and would otherwise dangle the moment the block below returns.
@interface MnemosyneTouchBarController : NSObject <NSTouchBarDelegate>
@property(nonatomic, copy) void (^previousPage)();
@property(nonatomic, copy) void (^nextPage)();
@property(nonatomic, copy) void (^zoomOut)();
@property(nonatomic, copy) void (^zoomIn)();
- (NSTouchBar *)buildTouchBar;
@end

@implementation MnemosyneTouchBarController

- (NSTouchBar *)buildTouchBar
{
    NSTouchBar *bar = [[NSTouchBar alloc] init];
    bar.delegate = self;
    bar.defaultItemIdentifiers = @[ kPreviousPageId, kNextPageId, NSTouchBarItemIdentifierFixedSpaceSmall, kZoomOutId,
                                     kZoomInId ];
    return bar;
}

- (NSTouchBarItem *)touchBar:(NSTouchBar *)touchBar makeItemForIdentifier:(NSTouchBarItemIdentifier)identifier
{
    struct
    {
        __unsafe_unretained NSString *identifier;
        __unsafe_unretained NSString *symbolName;
        __unsafe_unretained NSString *fallbackTitle;
        SEL action;
    } specs[] = {
        {kPreviousPageId, @"chevron.left", @"‹", @selector(previousPageTapped)},
        {kNextPageId, @"chevron.right", @"›", @selector(nextPageTapped)},
        {kZoomOutId, @"minus.magnifyingglass", @"−", @selector(zoomOutTapped)},
        {kZoomInId, @"plus.magnifyingglass", @"+", @selector(zoomInTapped)},
    };

    for (const auto &spec : specs) {
        if (![identifier isEqualToString:spec.identifier]) {
            continue;
        }
        // SF Symbols (macOS 11+) look native in the Touch Bar; the plain
        // character is only a fallback for a system too old to have them.
        NSImage *image = [NSImage imageWithSystemSymbolName:spec.symbolName accessibilityDescription:nil];
        NSButton *button = image ? [NSButton buttonWithImage:image target:self action:spec.action]
                                  : [NSButton buttonWithTitle:spec.fallbackTitle target:self action:spec.action];
        NSCustomTouchBarItem *item = [[NSCustomTouchBarItem alloc] initWithIdentifier:identifier];
        item.view = button;
        return item;
    }
    return nil;
}

- (void)previousPageTapped
{
    if (self.previousPage) {
        self.previousPage();
    }
}

- (void)nextPageTapped
{
    if (self.nextPage) {
        self.nextPage();
    }
}

- (void)zoomOutTapped
{
    if (self.zoomOut) {
        self.zoomOut();
    }
}

- (void)zoomInTapped
{
    if (self.zoomIn) {
        self.zoomIn();
    }
}

@end

namespace {
NSWindow *nsWindowFor(QWindow *window)
{
    if (!window) {
        return nil;
    }
    NSView *view = reinterpret_cast<NSView *>(window->winId());
    return view.window;
}
}

namespace MacTouchBar {

void installPdfControls(QWindow *window, std::function<void()> previousPage, std::function<void()> nextPage,
                         std::function<void()> zoomOut, std::function<void()> zoomIn)
{
    NSWindow *nsWindow = nsWindowFor(window);
    if (!nsWindow) {
        return;
    }

    auto *controller = [[MnemosyneTouchBarController alloc] init];
    controller.previousPage = ^{
      previousPage();
    };
    controller.nextPage = ^{
      nextPage();
    };
    controller.zoomOut = ^{
      zoomOut();
    };
    controller.zoomIn = ^{
      zoomIn();
    };

    // Replacing the association (a fresh controller each call -- e.g. every
    // tab switch) releases whichever controller was previously here, along
    // with the tab's now-stale callbacks it was holding.
    objc_setAssociatedObject(nsWindow, &kControllerAssociationKey, controller, OBJC_ASSOCIATION_RETAIN);
    nsWindow.touchBar = [controller buildTouchBar];
}

void clearControls(QWindow *window)
{
    NSWindow *nsWindow = nsWindowFor(window);
    if (!nsWindow) {
        return;
    }
    objc_setAssociatedObject(nsWindow, &kControllerAssociationKey, nil, OBJC_ASSOCIATION_RETAIN);
    nsWindow.touchBar = nil;
}

} // namespace MacTouchBar
