#pragma once
#include <QMainWindow>
#include <QTimer>
#include "serialmaster.h"

namespace Ui { class MainWindow; }
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
private:
    void refreshPorts();
    void selectMode(int mode);
    void updateStatus();
    void setRunning(bool running);
    void saveSettings();
    void setDisplayUnit(bool angle);
    void updatePositionInputs();
    void positionEdited();
    void updateFeedback();
    void adaptLayout(int width);
    Ui::MainWindow *ui;
    SerialMaster m_master;
    QTimer m_scanTimer, m_statusTimer;
    QByteArray m_lastTx, m_lastRx;
    QString m_savedPort;
    bool m_angleMode = true, m_hasFeedback = false;
    int m_positionX = 0, m_positionY = 0;
    quint16 m_feedbackX = 0, m_feedbackY = 0;
    int m_layoutCategory = -1;
};
