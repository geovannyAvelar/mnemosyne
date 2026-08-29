#pragma once

#include <QObject>

// QML-facing bridge for the dark-mode preference, persisted under the same
// QSettings "darkMode" key desktop's MainWindow menu action uses (see
// src/ui/MainWindow.cpp) — reusing the same key means the preference is
// stored consistently even though each platform reads/writes it through a
// different front end (a menu action there, qml/Theme.qml here).
class ThemeSettings : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool dark READ dark WRITE setDark NOTIFY darkChanged)

public:
    using QObject::QObject;

    bool dark() const;
    void setDark(bool dark);

signals:
    void darkChanged();
};
