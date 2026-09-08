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
    Q_PROPERTY(bool pdfPageDark READ pdfPageDark WRITE setPdfPageDark NOTIFY pdfPageDarkChanged)

public:
    using QObject::QObject;

    bool dark() const;
    void setDark(bool dark);

    // Persisted under "pdfPageInvertColors" -- deliberately a different key
    // from "dark" above (see PdfView::setupUi()'s "Invert" button on the
    // desktop side, which reads/writes the same key): this is the per-page
    // color-inversion preference, independent of app-wide Dark Mode.
    bool pdfPageDark() const;
    void setPdfPageDark(bool dark);

signals:
    void darkChanged();
    void pdfPageDarkChanged();
};
