#pragma once

#include <QObject>
#include <QString>

// Stage 2 smoke test only: proves the Android cross-compile chain (NDK,
// vendored Poppler-Qt6/libzip/Freetype) can open a real PDF through the
// unmodified desktop backend (core/Document.h's openDocument()) and render
// it to a QImage, end to end on-device. Not part of the eventual reader UI.
class SmokeTestBridge : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    // Extracts the bundled sample PDF (see CMakeLists.txt's qt_add_resources
    // call) to a real file — Poppler::Document::load() only understands
    // filesystem paths, not Qt resource paths — then opens it via the same
    // openDocument() the desktop app uses, and renders page 0 to prove
    // Poppler's rasterizer itself works, not just metadata parsing.
    Q_INVOKABLE QString openSamplePdfSummary() const;
};
