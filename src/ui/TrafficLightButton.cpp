#include "TrafficLightButton.h"

#include <QPainter>

namespace {
constexpr int kDiameter = 14;
}

TrafficLightButton::TrafficLightButton(const QColor &color, Glyph glyph, QWidget *parent)
    : QAbstractButton(parent)
    , m_color(color)
    , m_glyph(glyph)
{
    setCursor(Qt::ArrowCursor);
    setFixedSize(kDiameter, kDiameter);
}

QSize TrafficLightButton::sizeHint() const
{
    return QSize(kDiameter, kDiameter);
}

void TrafficLightButton::enterEvent(QEnterEvent *event)
{
    QAbstractButton::enterEvent(event);
    update();
}

void TrafficLightButton::leaveEvent(QEvent *event)
{
    QAbstractButton::leaveEvent(event);
    update();
}

void TrafficLightButton::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF circleRect(0.5, 0.5, kDiameter - 1.0, kDiameter - 1.0);
    painter.setPen(Qt::NoPen);
    painter.setBrush(isEnabled() ? m_color : QColor(0x9A, 0x99, 0x97));
    painter.drawEllipse(circleRect);

    if (!underMouse()) {
        return;
    }

    QPen glyphPen(QColor(0, 0, 0, 130), 1.3);
    glyphPen.setCapStyle(Qt::RoundCap);
    painter.setPen(glyphPen);
    painter.setBrush(Qt::NoBrush);

    const QPointF c = circleRect.center();
    const qreal r = 3.0;

    switch (m_glyph) {
    case Glyph::Close:
        painter.drawLine(QPointF(c.x() - r, c.y() - r), QPointF(c.x() + r, c.y() + r));
        painter.drawLine(QPointF(c.x() - r, c.y() + r), QPointF(c.x() + r, c.y() - r));
        break;
    case Glyph::Minimize:
        painter.drawLine(QPointF(c.x() - r, c.y()), QPointF(c.x() + r, c.y()));
        break;
    case Glyph::Zoom:
        painter.drawLine(QPointF(c.x() - r, c.y() + r), QPointF(c.x() + r, c.y() - r));
        painter.drawLine(QPointF(c.x() + r, c.y() - r), QPointF(c.x() + r - 1.3, c.y() - r));
        painter.drawLine(QPointF(c.x() + r, c.y() - r), QPointF(c.x() + r, c.y() - r + 1.3));
        painter.drawLine(QPointF(c.x() - r, c.y() + r), QPointF(c.x() - r + 1.3, c.y() + r));
        painter.drawLine(QPointF(c.x() - r, c.y() + r), QPointF(c.x() - r, c.y() + r - 1.3));
        break;
    }
}
