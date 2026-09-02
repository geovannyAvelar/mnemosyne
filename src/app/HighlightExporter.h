#pragma once

#include "core/Highlight.h"

#include <QString>
#include <QVector>

// Turns a book's highlights into a portable file — no file I/O here, just
// formatting, so this is unit-testable directly and reusable from a future
// mobile export flow. See ui/MainWindow.cpp for where entries are built
// (positionLabel is filled in there, since it depends on which view/format
// is open) and where the resulting string gets written to disk.
namespace HighlightExporter {

struct ExportEntry
{
    Highlight highlight;
    // "Page 12", "Chapter 3", "Part 3", or empty when the format has no
    // meaningful discrete unit (Markdown/TXT).
    QString positionLabel;
};

// A Markdown document: one heading with bookTitle, then one section per
// entry (in the given order — callers pass entries already sorted by
// position) with the highlighted passage as a blockquote, the note if any,
// and the position label if any. Every entry is included, noted or not.
QString toMarkdown(const QString &bookTitle, const QVector<ExportEntry> &entries);

// Tab-separated, no header row — Anki's plain "Notes > Import" reads
// Front\tBack per line for the Basic note type. Front = highlighted
// passage, Back = note. Only entries with a non-empty note are included; a
// bare highlight has no natural "back" for a flashcard. Embedded tabs/
// newlines in either field are collapsed to a single space, since a raw
// TSV row can't represent them without a quoting scheme Anki's plain import
// doesn't expect.
QString toAnkiTsv(const QVector<ExportEntry> &entries);

} // namespace HighlightExporter
