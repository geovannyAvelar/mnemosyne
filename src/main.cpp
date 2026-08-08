#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>

#include "ui/MainWindow.h"

int main(int argc, char *argv[])
{
#ifdef MNEMOSYNE_ENABLE_HTML
    // Recommended by Qt WebEngine docs; must be set before QApplication is
    // constructed. Used for HtmlView's QWebEngineView.
    QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
#endif

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
    parser.setApplicationDescription("Mnemosyne — a PDF, EPUB, and HTML reader");
    parser.addPositionalArgument("files", "Documents to open (.pdf, .epub, .html)", "[files...]");
#else
    parser.setApplicationDescription("Mnemosyne — a PDF and EPUB reader");
    parser.addPositionalArgument("files", "Documents to open (.pdf, .epub)", "[files...]");
#endif
    parser.addHelpOption();
    parser.process(app);

    MainWindow window;
    window.resize(1024, 768);
    window.show();

    for (const QString &path : parser.positionalArguments()) {
        window.openPath(path);
    }

    return app.exec();
}
