#pragma once

#include <QDateTime>
#include <QString>

struct Bookmark
{
    int targetIndex = -1; // PDF page index or EPUB spine index
    QString label;
    QDateTime createdAt;
};
