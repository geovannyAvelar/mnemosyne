#include "IOSStorageAccess.h"

#include <QDir>
#include <QMetaObject>
#include <QStandardPaths>
#include <QUuid>

#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

namespace {

QString importedDirPath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/Imported");
    QDir().mkpath(dir);
    return dir;
}

UIViewController *rootViewController()
{
    for (UIScene *scene in [UIApplication sharedApplication].connectedScenes) {
        if (![scene isKindOfClass:[UIWindowScene class]]) {
            continue;
        }
        for (UIWindow *window in ((UIWindowScene *)scene).windows) {
            if (window.isKeyWindow) {
                return window.rootViewController;
            }
        }
    }
    return nil;
}

} // namespace

// UIDocumentPickerViewController hands back a security-scoped URL to the
// file's original location (Files app, iCloud Drive, another app's
// provider), not a copy -- unlike Android's SAF flow, there's no
// persistable-permission equivalent that survives relaunch without also
// keeping a security-scoped bookmark alive. Copying into this app's own
// Documents/Imported directory up front sidesteps bookmark management
// entirely and matches what every downstream consumer (FileIdentity,
// RecentFiles, PdfDocumentModel, EpubReaderModel) already expects: a
// plain, always-readable filesystem path, the same shape a desktop path
// takes.
@interface MnemosyneDocumentPickerDelegate : NSObject <UIDocumentPickerDelegate>
@property (nonatomic, assign) IOSStorageAccess *owner;
@end

@implementation MnemosyneDocumentPickerDelegate

- (void)reportCancelled
{
    IOSStorageAccess *owner = self.owner;
    if (!owner) {
        return;
    }
    QMetaObject::invokeMethod(owner, [owner] { emit owner->pickCancelled(); }, Qt::QueuedConnection);
}

- (void)documentPicker:(UIDocumentPickerViewController *)controller
    didPickDocumentsAtURLs:(NSArray<NSURL *> *)urls
{
    Q_UNUSED(controller);

    if (urls.count == 0) {
        [self reportCancelled];
        return;
    }

    NSURL *sourceUrl = urls.firstObject;
    NSString *displayName = sourceUrl.lastPathComponent;
    const QString displayNameQ = QString::fromNSString(displayName);

    const BOOL accessing = [sourceUrl startAccessingSecurityScopedResource];
    const QString destPath = importedDirPath() + QLatin1Char('/')
        + QUuid::createUuid().toString(QUuid::WithoutBraces) + QLatin1Char('-') + displayNameQ;
    NSString *destNsPath = destPath.toNSString();

    [[NSFileManager defaultManager] removeItemAtPath:destNsPath error:nil];
    NSError *error = nil;
    const BOOL copied = [[NSFileManager defaultManager] copyItemAtURL:sourceUrl
                                                                  toURL:[NSURL fileURLWithPath:destNsPath]
                                                                  error:&error];

    if (accessing) {
        [sourceUrl stopAccessingSecurityScopedResource];
    }

    IOSStorageAccess *owner = self.owner;
    if (!copied || !owner) {
        [self reportCancelled];
        return;
    }

    QMetaObject::invokeMethod(
        owner, [owner, destPath, displayNameQ] { emit owner->documentPicked(destPath, displayNameQ); },
        Qt::QueuedConnection);
}

- (void)documentPickerWasCancelled:(UIDocumentPickerViewController *)controller
{
    Q_UNUSED(controller);
    [self reportCancelled];
}

@end

void IOSStorageAccess::pickDocument()
{
    UIViewController *rootVC = rootViewController();
    if (!rootVC) {
        emit pickCancelled();
        return;
    }

    NSArray<UTType *> *contentTypes = @[
        UTTypePDF,
        [UTType typeWithIdentifier:@"org.idpf.epub-container"],
    ];

    UIDocumentPickerViewController *picker =
        [[UIDocumentPickerViewController alloc] initForOpeningContentTypes:contentTypes];
    picker.allowsMultipleSelection = NO;

    // Retained for the picker's lifetime via the delegate property below;
    // this static holds the strong reference ARC would otherwise drop as
    // soon as this function returns, since picker.delegate is weak/unsafe
    // and pickDocument() itself returns immediately (the actual pick
    // happens later, off a UIKit callback).
    static MnemosyneDocumentPickerDelegate *delegate;
    delegate = [[MnemosyneDocumentPickerDelegate alloc] init];
    delegate.owner = this;
    picker.delegate = delegate;

    [rootVC presentViewController:picker animated:YES completion:nil];
}
