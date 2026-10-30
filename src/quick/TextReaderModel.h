#pragma once

#include "TextParts.h"

#include <QObject>
#include <QString>
#include <QTimer>

// Markdown and plain text counterpart to EpubReaderModel, exposing the same
// properties so qml/EpubReaderScreen.qml can drive either one. A "spine
// index" here is an index into TextParts::parts; what gets persisted and
// synced is that part's TextPart::position, so progress stays compatible
// with desktop's MarkdownView and TxtView.
class TextReaderModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isOpen READ isOpen NOTIFY documentChanged)
    Q_PROPERTY(int spineCount READ spineCount NOTIFY documentChanged)
    Q_PROPERTY(QString title READ title NOTIFY documentChanged)
    Q_PROPERTY(QString bookHash READ bookHash NOTIFY documentChanged)
    Q_PROPERTY(QString filePath READ filePath NOTIFY documentChanged)
    Q_PROPERTY(int currentSpineIndex READ currentSpineIndex WRITE setCurrentSpineIndex NOTIFY currentSpineIndexChanged)
    Q_PROPERTY(int highlightTarget READ highlightTarget CONSTANT)
    Q_PROPERTY(QString currentChapterHtml READ currentChapterHtml NOTIFY currentSpineIndexChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)

public:
    explicit TextReaderModel(QObject *parent = nullptr);
    ~TextReaderModel() override;

    bool isOpen() const { return !m_document.parts.isEmpty(); }
    int spineCount() const { return m_document.parts.size(); }
    QString title() const { return m_document.title; }
    QString bookHash() const { return m_bookHash; }
    QString filePath() const { return m_filePath; }
    int currentSpineIndex() const { return m_currentSpineIndex; }
    void setCurrentSpineIndex(int index);
    // Always 0: desktop keeps every Markdown/TXT highlight on target 0 (see
    // core/Highlight.h), and relocates them by text.
    int highlightTarget() const { return 0; }
    QString currentChapterHtml() const;
    QString errorMessage() const { return m_errorMessage; }

    // filePathOrUri: a real path (desktop) or a content:// URI (Android) —
    // see ContentUriCache. format is the file's lowercase suffix.
    Q_INVOKABLE bool open(const QString &filePathOrUri, const QString &format);
    Q_INVOKABLE void close();
    Q_INVOKABLE void nextChapter();
    Q_INVOKABLE void previousChapter();

signals:
    void documentChanged();
    void currentSpineIndexChanged();
    void errorMessageChanged();
    void remoteProgressAvailable(int spineIndex, const QString &deviceName);

private:
    void setErrorMessage(const QString &message);
    void restoreProgress();
    void saveProgressNow();

    TextParts m_document;
    QString m_bookHash;
    QString m_filePath;
    int m_currentSpineIndex = 0;
    // Desktop's font zoom steps. Mobile has no zoom control, but a save here
    // must not reset what the desktop app stored.
    qreal m_zoom = 0;
    QString m_errorMessage;
    QTimer m_progressSaveTimer;
};
