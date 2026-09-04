#pragma once

#include <QString>

// The active search-dock query for PdfView's page overlay ("dauber"
// highlight of every match) -- see PdfPageStackView::setSearchTerm().
// Deliberately thin: search() / PdfView::searchFile() (the static, per-page
// text scan) are already fully decoupled from any view state and don't go
// through this at all.
class PdfSearchController
{
public:
    void setTerm(const QString &term) { m_term = term.trimmed(); }
    QString term() const { return m_term; }

private:
    QString m_term;
};
