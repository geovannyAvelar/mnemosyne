#pragma once

#include "app/BookMetadataClient.h" // BookMetadata

#include <QDockWidget>

class QLabel;

// Shows title/authors/publisher/description/cover art for the current
// tab's EPUB/MOBI document: local metadata straight from the file's own
// OPF/EXTH records, shown immediately with no network access, upgraded
// field-by-field with an Open Library lookup when an ISBN is present and
// the user has opted into that -- see MainWindow's "Look Up Book Info
// Online" toggle.
class BookInfoDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit BookInfoDock(QWidget *parent = nullptr);

    void setLoading();
    void setMetadata(const BookMetadata &metadata);
    void setUnavailable(const QString &reason);

    // No document open, or the current one has neither local metadata nor
    // an ISBN to look up -- nothing at all to show.
    void clear();

private:
    void showStatus(const QString &text);

    QLabel *m_coverLabel;
    QLabel *m_titleLabel;
    QLabel *m_authorsLabel;
    QLabel *m_publisherLabel;
    QLabel *m_publishDateLabel;
    QLabel *m_descriptionLabel;
    QLabel *m_statusLabel;
};
