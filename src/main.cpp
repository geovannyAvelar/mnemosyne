#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QStyleFactory>
#ifdef Q_OS_MACOS
#include <QTimer>
#endif

#include "ui/MainWindow.h"
#ifdef Q_OS_MACOS
#include "platform/MacWindowChrome.h"
#endif

int main(int argc, char *argv[])
{
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
    // with the flat, background-less icon used for the Windows/Linux taskbar.
    QIcon appIcon;
    for (int size : {16, 32, 48, 64, 128, 256, 512}) {
        appIcon.addFile(QString(":/icons/mnemosyne_%1.png").arg(size), QSize(size, size));
    }
    QApplication::setWindowIcon(appIcon);
#endif

    QCommandLineParser parser;
#ifdef MNEMOSYNE_ENABLE_HTML
    parser.setApplicationDescription("Mnemosyne — a PDF, EPUB, HTML, and Markdown reader");
    parser.addPositionalArgument("files", "Documents to open (.pdf, .epub, .html, .md)", "[files...]");
#else
    parser.setApplicationDescription("Mnemosyne — a PDF, EPUB, and Markdown reader");
    parser.addPositionalArgument("files", "Documents to open (.pdf, .epub, .md)", "[files...]");
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
