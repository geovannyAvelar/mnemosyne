#include "Theme.h"

namespace Theme {

namespace {

struct Colors
{
    QColor window;       // app background
    QColor panel;        // sidebar / dock / tab-pane background
    QColor base;         // inputs, cards, the "raised" surface
    QColor raisedHover;   // hover state for base surfaces
    QColor border;
    QColor text;
    QColor mutedText;
    QColor accent;        // terracotta brand color
    QColor accentHover;
    QColor accentPressed;
    QColor accentText;    // text/icon color on top of accent
};

const Colors &lightColors()
{
    static const Colors c{
        QColor(0xFA, 0xF9, 0xF5),
        QColor(0xF5, 0xF3, 0xEC),
        QColor(0xFF, 0xFF, 0xFF),
        QColor(0xEE, 0xEC, 0xE3),
        QColor(0xE5, 0xE2, 0xD9),
        QColor(0x2B, 0x2A, 0x27),
        QColor(0x8A, 0x87, 0x7C),
        QColor(0xD9, 0x77, 0x56),
        QColor(0xC2, 0x60, 0x3F),
        QColor(0xAD, 0x54, 0x35),
        QColor(0xFF, 0xFF, 0xFF),
    };
    return c;
}

const Colors &darkColors()
{
    static const Colors c{
        QColor(0x26, 0x26, 0x24),
        QColor(0x2A, 0x2A, 0x28),
        QColor(0x30, 0x30, 0x2E),
        QColor(0x38, 0x38, 0x35),
        QColor(0x3D, 0x3D, 0x3A),
        QColor(0xF5, 0xF4, 0xEF),
        QColor(0x9B, 0x99, 0x93),
        QColor(0xD9, 0x77, 0x56),
        QColor(0xE2, 0x8A, 0x6C),
        QColor(0xC2, 0x60, 0x3F),
        QColor(0xFF, 0xFF, 0xFF),
    };
    return c;
}

QPalette buildPalette(const Colors &c)
{
    QPalette p;
    p.setColor(QPalette::Window, c.window);
    p.setColor(QPalette::WindowText, c.text);
    p.setColor(QPalette::Base, c.base);
    p.setColor(QPalette::AlternateBase, c.raisedHover);
    p.setColor(QPalette::ToolTipBase, c.panel);
    p.setColor(QPalette::ToolTipText, c.text);
    p.setColor(QPalette::Text, c.text);
    p.setColor(QPalette::Button, c.panel);
    p.setColor(QPalette::ButtonText, c.text);
    p.setColor(QPalette::BrightText, Qt::red);
    p.setColor(QPalette::Link, c.accent);
    p.setColor(QPalette::Highlight, c.accent);
    p.setColor(QPalette::HighlightedText, c.accentText);
    p.setColor(QPalette::PlaceholderText, c.mutedText);
    p.setColor(QPalette::Disabled, QPalette::Text, c.mutedText);
    p.setColor(QPalette::Disabled, QPalette::WindowText, c.mutedText);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, c.mutedText);
    return p;
}

QString buildStyleSheet(const Colors &c)
{
    QString sheet = QStringLiteral(R"(
QMainWindow, QDialog, QMessageBox {
    background: $WINDOW$;
}

QMenuBar {
    background: $WINDOW$;
    color: $TEXT$;
    border: none;
    padding: 2px;
}
QMenuBar::item {
    padding: 4px 10px;
    border-radius: 6px;
    background: transparent;
}
QMenuBar::item:selected {
    background: $RAISED_HOVER$;
}
QMenu {
    background: $PANEL$;
    color: $TEXT$;
    border: 1px solid $BORDER$;
    border-radius: 10px;
    padding: 6px;
}
QMenu::item {
    padding: 6px 24px 6px 12px;
    border-radius: 6px;
}
QMenu::item:selected {
    background: $ACCENT$;
    color: $ACCENT_TEXT$;
}
QMenu::separator {
    height: 1px;
    background: $BORDER$;
    margin: 6px 4px;
}

QDockWidget {
    color: $TEXT$;
    background: $PANEL$;
    border: none;
}
QDockWidget::title {
    background: $PANEL$;
    padding: 12px 12px 8px 12px;
    font-weight: 600;
    border: none;
}

QTabWidget::pane {
    border: 1px solid $BORDER$;
    border-radius: 12px;
    background: $WINDOW$;
    top: -1px;
}
QTabBar {
    background: transparent;
}
QTabBar::tab {
    background: transparent;
    color: $MUTED_TEXT$;
    padding: 7px 16px;
    margin: 6px 3px 4px 3px;
    border-radius: 8px;
    font-weight: 500;
}
QTabBar::tab:selected {
    background: $PANEL$;
    color: $TEXT$;
}
QTabBar::tab:hover:!selected {
    background: $RAISED_HOVER$;
    color: $TEXT$;
}
QTabBar::close-button {
    subcontrol-position: right;
}

QTreeWidget, QListWidget, QTreeView, QListView {
    background: transparent;
    color: $TEXT$;
    border: none;
    outline: none;
    padding: 4px;
}
QTreeWidget::item, QListWidget::item {
    padding: 9px 10px;
    border-radius: 8px;
    margin: 1px 2px;
}
QTreeWidget::item:hover, QListWidget::item:hover {
    background: $RAISED_HOVER$;
}
QTreeWidget::item:selected, QListWidget::item:selected {
    background: $ACCENT$;
    color: $ACCENT_TEXT$;
}

QLineEdit {
    background: $BASE$;
    color: $TEXT$;
    border: 1px solid $BORDER$;
    border-radius: 8px;
    padding: 7px 10px;
    selection-background-color: $ACCENT$;
}
QLineEdit:focus {
    border: 1px solid $ACCENT$;
}

QPushButton {
    background: $BASE$;
    color: $TEXT$;
    border: 1px solid $BORDER$;
    border-radius: 8px;
    padding: 7px 14px;
}
QPushButton:hover {
    background: $RAISED_HOVER$;
}
QPushButton:pressed {
    background: $BORDER$;
}
QPushButton:disabled {
    color: $MUTED_TEXT$;
}
QPushButton#primaryButton {
    background: $ACCENT$;
    color: $ACCENT_TEXT$;
    border: none;
    font-weight: 600;
    padding: 8px 18px;
}
QPushButton#primaryButton:hover {
    background: $ACCENT_HOVER$;
}
QPushButton#primaryButton:pressed {
    background: $ACCENT_PRESSED$;
}

QToolBar {
    background: $WINDOW$;
    border: none;
    spacing: 4px;
    padding: 6px;
}
QToolButton {
    background: transparent;
    color: $TEXT$;
    border: none;
    border-radius: 8px;
    padding: 6px 10px;
}
QToolButton:hover {
    background: $RAISED_HOVER$;
}

QScrollBar:vertical {
    background: transparent;
    width: 12px;
    margin: 2px;
}
QScrollBar::handle:vertical {
    background: $BORDER$;
    border-radius: 5px;
    min-height: 24px;
}
QScrollBar::handle:vertical:hover {
    background: $MUTED_TEXT$;
}
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
    height: 0;
}
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
    background: transparent;
}

QScrollBar:horizontal {
    background: transparent;
    height: 12px;
    margin: 2px;
}
QScrollBar::handle:horizontal {
    background: $BORDER$;
    border-radius: 5px;
    min-width: 24px;
}
QScrollBar::handle:horizontal:hover {
    background: $MUTED_TEXT$;
}
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
    width: 0;
}
QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {
    background: transparent;
}

QLabel#libraryTitle {
    font-size: 22px;
    font-weight: 700;
    color: $TEXT$;
}
QLabel#sectionLabel {
    color: $MUTED_TEXT$;
    font-weight: 600;
    font-size: 12px;
}

QCheckBox, QRadioButton {
    color: $TEXT$;
    spacing: 8px;
}
)");

    sheet.replace(QStringLiteral("$WINDOW$"), c.window.name());
    sheet.replace(QStringLiteral("$PANEL$"), c.panel.name());
    sheet.replace(QStringLiteral("$BASE$"), c.base.name());
    sheet.replace(QStringLiteral("$RAISED_HOVER$"), c.raisedHover.name());
    sheet.replace(QStringLiteral("$BORDER$"), c.border.name());
    sheet.replace(QStringLiteral("$TEXT$"), c.text.name());
    sheet.replace(QStringLiteral("$MUTED_TEXT$"), c.mutedText.name());
    sheet.replace(QStringLiteral("$ACCENT$"), c.accent.name());
    sheet.replace(QStringLiteral("$ACCENT_HOVER$"), c.accentHover.name());
    sheet.replace(QStringLiteral("$ACCENT_PRESSED$"), c.accentPressed.name());
    sheet.replace(QStringLiteral("$ACCENT_TEXT$"), c.accentText.name());
    return sheet;
}

} // namespace

QPalette lightPalette()
{
    return buildPalette(lightColors());
}

QPalette darkPalette()
{
    return buildPalette(darkColors());
}

QString styleSheet(bool dark)
{
    return buildStyleSheet(dark ? darkColors() : lightColors());
}

} // namespace Theme
