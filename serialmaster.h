#pragma once
#include "protocol.h"
#include <QObject>
#include <QSerialPort>
#include <QTimer>
#include <QElapsedTimer>

class SerialMaster : public QObject {
    Q_OBJECT
public:
    explicit SerialMaster(QObject *parent = nullptr);
    bool start(const QString &port, qint32 baud, quint8 deviceId);
    void stop();
    bool isRunning() const { return m_running; }
    bool setIntervalMs(int interval);
    int intervalMs() const { return m_timer.interval(); }
    static int minimumBaud(int interval);
    void setMode(int mode);
    void setVelocity(int x, int y);
    void setPosition(int x, int y);
    void setHoming(bool enabled);
    void setTrig(bool enabled);
    void setReserveValue(int value);
    QByteArray commandFrame() const;
    quint64 txCount() const { return m_tx; }
    quint64 rxCount() const { return m_rx; }
    quint64 badCount() const { return m_bad + m_decoder.rejected(); }
    qint64 responseAge() const { return m_lastRx.isValid() ? m_lastRx.elapsed() : -1; }
signals:
    void runningChanged(bool running);
    void received(quint16 x, quint16 y);
    void frameSent(const QByteArray &bytes);
    void frameReceived(const QByteArray &bytes);
    void error(const QString &message);
private:
    void transmit();
    void readIncoming();
    QSerialPort m_port;
    QTimer m_timer;
    Protocol::StreamDecoder m_decoder;
    Protocol::Packet m_command;
    QElapsedTimer m_lastRx;
    quint64 m_tx = 0, m_rx = 0, m_bad = 0;
    bool m_running = false;
};
