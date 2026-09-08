#pragma once

#include <QDateTime>
#include <QObject>
#include <QString>

// Tracks one "reading session" at a time -- the stretch between opening/
// switching to a book and switching away from/closing it -- and records it
// into ReadingStatsStore.h when it ends. One instance lives for the whole
// app's lifetime: MainWindow owns one on desktop (see its onTabChanged()/
// closeEvent()); mobile exposes one to QML as the "readingSessionTracker"
// context property (see main_android.cpp/main_ios.mm) for
// PdfReaderScreen.qml/EpubReaderScreen.qml to drive from their own
// onBackRequested/Component.onDestruction. A QObject (rather than a plain
// class, like most of app/'s other stores) purely so QML can call start()/
// stop() directly as Q_INVOKABLEs.
//
// Deliberately simple: no OS-focus/idle detection, so time spent with the
// app backgrounded while a book's screen is still the active one counts as
// reading time. This reflects what start()/stop() are actually told, not
// real attention -- a reasonable approximation for a personal reading-stats
// feature, not a productivity-tracking tool.
class ReadingSessionTracker : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    Q_INVOKABLE void start(const QString &bookHash, int position);

    // Records the open session into ReadingStatsStore under the day it
    // started on: elapsed wall-clock time since start(), and
    // max(0, position - startPosition) as "pages" (see
    // ReadingStatsStore::recordSession()'s doc comment on what that means
    // outside PDF/CBZ). Clears the open session either way, so calling
    // stop() with no session open (or twice in a row) is a harmless no-op.
    //
    // Callers switching directly from one book to another must call stop()
    // for the outgoing book (with *its own* current position) before
    // start() for the incoming one -- this class only ever tracks one
    // session, and has no way to know the outgoing book's position on its
    // own once the caller has moved on to a different book.
    Q_INVOKABLE void stop(int position);

    Q_INVOKABLE bool isActive() const { return !m_bookHash.isEmpty(); }

private:
    QString m_bookHash;
    QDateTime m_startedAt;
    int m_startPosition = 0;
};
