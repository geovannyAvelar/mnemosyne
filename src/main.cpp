#include <QApplication>
#include <QCommandLineParser>

#include "ui/MainWindow.h"

int main(int argc, char *argv[])
{
    // Recommended by Qt WebEngine docs; must be set before QApplication is
    // constructed. Used for HtmlView's QWebEngineView.
    QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    QApplication app(argc, argv);
    QApplication::setApplicationName("Mnemosyne");
    QApplication::setOrganizationName("Mnemosyne");

    QCommandLineParser parser;
    parser.setApplicationDescription("Mnemosyne — a PDF, EPUB, and HTML reader");
    parser.addHelpOption();
    parser.addPositionalArgument("files", "Documents to open (.pdf, .epub, .html)", "[files...]");
    parser.process(app);

    MainWindow window;
    window.resize(1024, 768);
    window.show();

    for (const QString &path : parser.positionalArguments()) {
        window.openPath(path);
    }

    return app.exec();
}
