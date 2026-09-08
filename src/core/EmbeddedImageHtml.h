#pragma once

#include <QByteArray>
#include <QString>

// Shared HTML-rewriting helpers for embedding a document format's own
// binary-referenced images as self-contained data: URIs, so a plain
// QTextBrowser (used by EpubView/MobiView/etc., with no concept of the
// source archive/container's own resource scheme) can render them with no
// custom resource loader. Deliberately format-agnostic and libmobi/
// libzip-free: MobiDocument owns resolving *which* image a tag refers to
// (its "recindex"/"kindle:embed:N" scheme, resolved via libmobi against
// rawml->resources); this just does the actual tag rewrite once that
// resolution has already produced the image's bytes -- the part that's
// cheap to give real unit-test coverage without needing a real parsed
// document.
namespace EmbeddedImageHtml {

// Returns the "recindex" attribute value if present (Mobipocket/KF7 style,
// e.g. <img recindex="00001">), else the numeric id inside a
// "kindle:embed:NNNN" src (KF8 style, e.g.
// <img src="kindle:embed:0001?mime=image/jpeg">, leading zeros stripped).
// Empty if tag has neither -- e.g. a plain EPUB-style <img src="foo.png">
// (not this scheme at all) or a genuinely broken/absent reference.
QString mobiImageReference(const QString &imgTag);

// Rewrites imgTag's src to a "data:<mimeType>;base64,..." URI embedding
// imageData, replacing any existing src attribute or inserting one if the
// tag had none (e.g. a bare <img recindex="..."> with no src at all).
// Removes any "recindex" attribute, since it's meaningless once a real src
// exists. If imageData decodes to a QImage wider than maxWidth, also caps
// it with explicit pixel width/height attributes, computed from its own
// aspect ratio, replacing any existing width/height attributes -- necessary
// because QTextDocument (this app's HTML renderer for every non-PDF format)
// doesn't honor CSS percentage sizing on <img> at all, confirmed
// empirically: an oversized image with only percentage-based sizing still
// renders at native pixel size, overflowing the reading pane.
QString rewriteImgTagWithDataUri(QString imgTag, const QByteArray &imageData, const QString &mimeType, int maxWidth);

} // namespace EmbeddedImageHtml
