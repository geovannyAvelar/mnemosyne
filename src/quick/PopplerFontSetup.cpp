#include "PopplerFontSetup.h"

#include <GlobalParams.h>

#include <QCoreApplication>

void setupPopplerBase14Fonts()
{
    // Held for the whole process lifetime. Every Poppler::Document
    // (poppler-qt6's DocumentData) privately inherits its own
    // GlobalParamsIniter, which ref-counts a *shared* globalParams and
    // tears it down via globalParams.reset() the moment that count drops
    // back to zero -- i.e. whenever no document happens to be open,
    // including between this app's own close()-then-open() calls (see
    // PdfDocumentModel::open()). Without a long-lived guard of our own
    // here, the very next document rebuilds a fresh, unconfigured
    // GlobalParams from scratch, silently discarding setupBaseFonts()
    // below before it ever has a chance to matter -- which is exactly why
    // the "No display font" spam kept happening even after this file was
    // first added. Constructing ours first (count 0->1) means every
    // document's own guard only ever increments/decrements an already-
    // nonzero count, so the shared instance below survives for good.
    static GlobalParamsIniter keepGlobalParamsAlive(nullptr);

    // MACOSX_PACKAGE_LOCATION "pdf-base14-fonts" (src/CMakeLists.txt) copies
    // the .pfb files directly into the app bundle's root alongside the
    // executable, which is what applicationDirPath() returns on iOS.
    const QString fontsDir = QCoreApplication::applicationDirPath() + QStringLiteral("/pdf-base14-fonts");
    globalParams->setupBaseFonts(fontsDir.toUtf8().constData());
}
