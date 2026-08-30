#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QStyleFactory>
#ifdef Q_OS_MACOS
#include <QTimer>
#endif

#include "platform/SystemAppearance.h"
#include "ui/MainWindow.h"
#ifdef Q_OS_MACOS
#include "platform/MacWindowChrome.h"
#endif

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

    QApplication app(argc, argv);
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

    MainWindow window;
    window.resize(1024, 768); // fallback size if the platform ever ignores showMaximized()
    window.showMaximized();

#ifdef Q_OS_MACOS
    // Deferred: Qt's Cocoa platform plugin finishes its own native window
    // setup asynchronously around show(), and applying this any earlier gets
    // silently overwritten by that setup. A same-tick (0ms) deferral used to
    // be enough, but now that the window has a real toolbar (see TopBar's
    // addToolBar() call), Qt's own async setup takes longer than one
    // event-loop iteration, so this needs an actual delay to win the race.
    QTimer::singleShot(100, &window, [&window] {
        MacWindowChrome::integrateTitleBar(window.windowHandle());
    });
#endif

    for (const QString &path : parser.positionalArguments()) {
        window.openPath(path);
    }

    return app.exec();
}
