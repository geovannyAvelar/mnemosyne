#pragma once

#include <QAbstractButton>
#include <QColor>

// A macOS-traffic-light-styled button: a small colored circle that reveals
// a glyph (×, −, or the fullscreen arrows) on hover, matching the native
// close/minimize/zoom buttons this replaces — but drawn as a real widget so
// it's genuinely part of the app's own GUI instead of native window chrome.
class TrafficLightButton : public QAbstractButton
{
    Q_OBJECT

public:
    enum class Glyph { Close, Minimize, Zoom };

    TrafficLightButton(const QColor &color, Glyph glyph, QWidget *parent = nullptr);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QColor m_color;
    Glyph m_glyph;
};
