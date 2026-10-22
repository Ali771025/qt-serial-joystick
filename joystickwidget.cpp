#include "joystickwidget.h"
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QtMath>

JoystickWidget::JoystickWidget(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(220, 220);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setFocusPolicy(Qt::StrongFocus);
    setAccessibleName("Circular velocity joystick");
    setToolTip("Drag the ball. Releasing the mouse centers the ball and zeros X / Y. Arrow keys: move; Home or double click: center.");
}
void JoystickWidget::setValues(int x, int y)
{
    const double length = qSqrt(double(x)*x + double(y)*y);
    if (length > 99) {
        x = int(x * 99.0 / length); y = int(y * 99.0 / length);
    }
    const QPoint next(x, y);
    if (next == m_values) return;
    m_values = next;
    update();
    emit valuesChanged(x, y);
}
void JoystickWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    const QPointF center(width()/2.0, height()/2.0);
    const double radius = qMin(width(), height())/2.0 - 26;
    const double travel = radius - 18;
    p.setOpacity(isEnabled() ? 1 : .4);
    QRadialGradient field(center, radius);
    field.setColorAt(0, QColor("#192d47")); field.setColorAt(1, QColor("#0c1728"));
    p.setBrush(field); p.setPen(QPen(QColor("#38516e"), 2));
    p.drawEllipse(center, radius, radius);
    p.setBrush(Qt::NoBrush); p.setPen(QPen(QColor("#263c55"), 1, Qt::DashLine));
    p.drawEllipse(center, travel/2, travel/2);
    p.drawLine(QPointF(center.x()-radius+8, center.y()), QPointF(center.x()+radius-8, center.y()));
    p.drawLine(QPointF(center.x(), center.y()-radius+8), QPointF(center.x(), center.y()+radius-8));
    p.setPen(QColor("#8aa0b9"));
    p.drawText(QRectF(center.x()-20, 1, 40, 22), Qt::AlignCenter, "-Y");
    p.drawText(QRectF(width()-28, center.y()-12, 28, 24), Qt::AlignCenter, "+X");
    p.drawText(QRectF(0, center.y()-12, 28, 24), Qt::AlignCenter, "-X");
    p.drawText(QRectF(center.x()-20, height()-23, 40, 22), Qt::AlignCenter, "+Y");
    const QPointF ball = center + QPointF(m_values.x()*travel/99.0, m_values.y()*travel/99.0);
    p.setPen(Qt::NoPen); p.setBrush(QColor(0, 0, 0, 65));
    p.drawEllipse(ball + QPointF(0, 5), 19, 19);
    QRadialGradient sphere(ball - QPointF(6, 7), 25);
    sphere.setColorAt(0, QColor("#bcfff4")); sphere.setColorAt(.4, QColor("#46d9bd"));
    sphere.setColorAt(1, QColor("#087e7d"));
    p.setBrush(sphere); p.setPen(QPen(QColor("#7ce9d5"), 1)); p.drawEllipse(ball, 17, 17);
}
void JoystickWidget::moveTo(const QPointF &point)
{
    const double travel = qMin(width(), height())/2.0 - 44;
    setValues(qRound((point.x()-width()/2.0)*99/travel),
              qRound((point.y()-height()/2.0)*99/travel));
}
void JoystickWidget::mousePressEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton) return;
    setFocus(); m_dragging = true; moveTo(e->localPos());
}
void JoystickWidget::mouseMoveEvent(QMouseEvent *e) { if (m_dragging) moveTo(e->localPos()); }
void JoystickWidget::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) { m_dragging = false; setValues(0, 0); }
}
void JoystickWidget::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) { m_dragging = false; setValues(0, 0); }
}
void JoystickWidget::keyPressEvent(QKeyEvent *e)
{
    const int step = e->modifiers().testFlag(Qt::ShiftModifier) ? 10 : 1;
    switch (e->key()) {
    case Qt::Key_Left: setValues(m_values.x()-step, m_values.y()); break;
    case Qt::Key_Right: setValues(m_values.x()+step, m_values.y()); break;
    case Qt::Key_Up: setValues(m_values.x(), m_values.y()-step); break;
    case Qt::Key_Down: setValues(m_values.x(), m_values.y()+step); break;
    case Qt::Key_Home: setValues(0, 0); break;
    default: QWidget::keyPressEvent(e); break;
    }
}
