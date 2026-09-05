#include <QApplication>
#include <QCommandLineParser>
#include <QFileOpenEvent>
#include <QIcon>
#include <QStyleFactory>

#include "app/SingleInstanceGuard.h"
#include "platform/SystemAppearance.h"
#include "ui/MainWindow.h"

namespace {

// Brings an already-open window to the front, restoring it first if it was
// minimized (see MainWindow::changeEvent() for why a plain showNormal() is
// enough to also put a maximized window back the way it was).
void bringToFront(MainWindow &window)
{
    if (window.isMinimized()) {
        window.showNormal();
    }
    window.raise();
    window.activateWindow();
}

} // namespace

// QApplication subclass that catches QFileOpenEvent -- how macOS asks a
// running app to open a file (Finder's "Open With", a Dock drop, a second
// double-click of a document whose app is already running) instead of
// spawning a new process with the path on argv. Harmless no-op on other
// platforms, which never send this event.
class Application : public QApplication
{
public:
    using QApplication::QApplication;

    // Null until main() finishes constructing MainWindow. A FileOpen event
    // that arrives before then (macOS can deliver one for the file that
    // launched the app in the first place, queued before the event loop
    // even starts) is buffered here and drained once the window exists.
    MainWindow *window = nullptr;
    QStringList pendingFileOpenPaths;

protected:
    bool event(QEvent *event) override
    {
        if (event->type() == QEvent::FileOpen) {
            const QString path = static_cast<QFileOpenEvent *>(event)->file();
            if (window) {
                window->openPath(path);
                bringToFront(*window);
            } else {
                pendingFileOpenPaths << path;
            }
            return true;
        }
        return QApplication::event(event);
    }
};

int main(int argc, char *argv[])
{
#ifdef Q_OS_LINUX
    // Prefer XWayland (the xcb backend) over Qt's native Wayland QPA plugin.
    // Qt's Wayland integration has long-standing bugs restoring a window's
    // geometry after minimize/unminimize (see e.g. QTBUG-100263) -- confirmed
    // reproducible on stock Ubuntu/GNOME, where the window comes back at some
    // small, centered size instead of staying maximized. XWayland doesn't
    // have this problem. A user who's explicitly set QT_QPA_PLATFORM (e.g.
    // to force native Wayland back on) keeps that choice.
    if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "xcb");
    }
#endif

#ifdef MNEMOSYNE_ENABLE_HTML
    // Recommended by Qt WebEngine docs; must be set before QApplication is
    // constructed. Used for HtmlView's QWebEngineView.
    QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
#endif

    // Fusion draws all its own controls (no native-style icons like the
    // toolbar overflow chevron, which currently crashes via AppKit's symbol
    // image renderer on this machine) and matches better with Theme's heavy
    // QSS reskinning anyway.
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    Application app(argc, argv);
    QApplication::setApplicationName("Mnemosyne");
    QApplication::setOrganizationName("Mnemosyne");

#ifndef Q_OS_MACOS
    // On macOS the app bundle's .icns (see MACOSX_BUNDLE_ICON_FILE) already
    // provides the Dock/Finder icon, and the OS applies its own rounded-square
    // mask and shadow to it. Setting a window icon here would override that
    // with the icon used for the Windows/Linux taskbar, which also tracks the
    // OS's own light/dark appearance setting (see SystemAppearance) so it
    // matches the taskbar it sits in.
    auto buildAppIcon = [](bool dark) {
        QIcon icon;
        for (int size : {16, 32, 48, 64, 128, 256, 512}) {
            const QString suffix = dark ? QStringLiteral("_dark") : QString();
            icon.addFile(QStringLiteral(":/icons/mnemosyne_%1%2.png").arg(size).arg(suffix), QSize(size, size));
        }
        return icon;
    };
    QApplication::setWindowIcon(buildAppIcon(SystemAppearance::instance().isDarkMode()));
    QObject::connect(&SystemAppearance::instance(), &SystemAppearance::darkModeChanged, &app,
                      [buildAppIcon](bool dark) { QApplication::setWindowIcon(buildAppIcon(dark)); });
#endif

    QCommandLineParser parser;
#ifdef MNEMOSYNE_ENABLE_HTML
    parser.setApplicationDescription("Mnemosyne — a PDF, EPUB, HTML, Markdown, MOBI/AZW, CBZ comic, and plain text reader");
    parser.addPositionalArgument("files", "Documents to open (.pdf, .epub, .html, .md, .mobi, .azw, .azw3, .cbz, .txt)", "[files...]");
#else
    parser.setApplicationDescription("Mnemosyne — a PDF, EPUB, Markdown, MOBI/AZW, CBZ comic, and plain text reader");
    parser.addPositionalArgument("files", "Documents to open (.pdf, .epub, .md, .mobi, .azw, .azw3, .cbz, .txt)", "[files...]");
#endif
    parser.addHelpOption();
    parser.process(app);

    // Only one Mnemosyne process runs at a time. If another one is already
    // up, hand it our files (if any -- a plain re-launch with none just
    // brings it to front) and exit without ever creating a window.
    SingleInstanceGuard instanceGuard;
    if (!instanceGuard.tryBecomePrimary(parser.positionalArguments())) {
        return 0;
    }

    MainWindow window;
    window.resize(1024, 768); // fallback size if the platform ever ignores showMaximized()
    window.showMaximized();

    app.window = &window;
    for (const QString &path : app.pendingFileOpenPaths) {
        window.openPath(path);
    }
    app.pendingFileOpenPaths.clear();

    QObject::connect(&instanceGuard, &SingleInstanceGuard::filesReceived, &window, [&window](const QStringList &paths) {
        for (const QString &path : paths) {
            window.openPath(path);
        }
        bringToFront(window);
    });

    for (const QString &path : parser.positionalArguments()) {
        window.openPath(path);
    }

    return app.exec();
}
