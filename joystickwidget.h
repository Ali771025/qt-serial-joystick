#pragma once
#include <QWidget>
#include <QPoint>

class JoystickWidget : public QWidget {
    Q_OBJECT
public:
    explicit JoystickWidget(QWidget *parent = nullptr);
    QSize sizeHint() const override { return QSize(280, 280); }
    QPoint values() const { return m_values; }
public slots:
    void setValues(int x, int y);
signals:
    void valuesChanged(int x, int y);
protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
private:
    void moveTo(const QPointF &point);
    QPoint m_values;
    bool m_dragging = false;
};
