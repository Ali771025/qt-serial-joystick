#include "serialmaster.h"

SerialMaster::SerialMaster(QObject *parent) : QObject(parent)
{
    m_timer.setInterval(40); m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &SerialMaster::transmit);
    connect(&m_port, &QSerialPort::readyRead, this, &SerialMaster::readIncoming);
    connect(&m_port, &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError e) {
        if (e == QSerialPort::NoError || !m_running) return;
        const QString message = m_port.errorString();
        stop(); emit error(message);
    });
}
bool SerialMaster::start(const QString &port, qint32 baud, quint8 deviceId)
{
    if (m_running) return true;
    const int requiredBaud = minimumBaud(intervalMs());
    if (baud < requiredBaud) {
        emit error(QString("33-byte frames every %1 ms require at least %2 baud (8N1).")
                   .arg(intervalMs()).arg(requiredBaud)); return false;
    }
    m_port.setPortName(port);
    if (!m_port.setBaudRate(baud) || !m_port.setDataBits(QSerialPort::Data8)
            || !m_port.setParity(QSerialPort::NoParity) || !m_port.setStopBits(QSerialPort::OneStop)
            || !m_port.setFlowControl(QSerialPort::NoFlowControl)) {
        emit error(m_port.errorString()); return false;
    }
    if (!m_port.open(QIODevice::ReadWrite)) { emit error(m_port.errorString()); return false; }
    m_command.id = 1; m_command.deviceId = deviceId; m_command.counter = 0;
    m_decoder.reset(); m_tx = m_rx = m_bad = 0; m_lastRx.invalidate();
    m_port.clear(); m_running = true; emit runningChanged(true);
    transmit();
    if (m_running) m_timer.start();
    return m_running;
}
void SerialMaster::stop()
{
    m_timer.stop();
    const bool wasRunning = m_running; m_running = false;
    if (m_port.isOpen()) { m_port.clear(QSerialPort::Output); m_port.close(); }
    if (wasRunning) emit runningChanged(false);
}
bool SerialMaster::setIntervalMs(int interval)
{
    if (m_running || interval < 5 || interval > 1000) return false;
    m_timer.setInterval(interval);
    return true;
}
int SerialMaster::minimumBaud(int interval)
{
    if (interval < 5 || interval > 1000) return -1;
    const int bitsPerSecondNumerator = Protocol::PacketSize * 10 * 1000;
    return (bitsPerSecondNumerator + interval - 1) / interval;
}
void SerialMaster::setMode(int mode) { m_command.reserve[0] = mode == 1 ? 0 : 1; }
void SerialMaster::setVelocity(int x, int y)
{
    m_command.velocityX = qint16(qBound(-99, x, 99));
    m_command.velocityY = qint16(qBound(-99, y, 99));
}
void SerialMaster::setPosition(int x, int y)
{
    m_command.positionX = quint16(qBound(0, x, 65535));
    m_command.positionY = quint16(qBound(0, y, 65535));
}
void SerialMaster::setHoming(bool enabled) { m_command.reserve[1] = enabled ? 1 : 0; }
void SerialMaster::setTrig(bool enabled) { m_command.reserve[2] = enabled ? 1 : 0; }
void SerialMaster::setReserveValue(int value) { m_command.reserve[3] = quint8(qBound(0, value, 255)); }
QByteArray SerialMaster::commandFrame() const
{
    Protocol::Packet p = m_command;
    p.dataValue = p.reserve[0] == 0 ? 12 : 3;
    if (p.reserve[0] == 0) p.positionX = p.positionY = 0;
    else p.velocityX = p.velocityY = 0;
    return Protocol::encode(p);
}
void SerialMaster::transmit()
{
    if (!m_running) return;
    if (m_port.bytesToWrite() > 0) {
        const int interval = intervalMs();
        stop(); emit error(QString("Serial output cannot sustain the %1 ms cycle. Check baud rate or adapter.").arg(interval)); return;
    }
    const QByteArray bytes = commandFrame();
    if (m_port.write(bytes) != bytes.size() || !m_running) {
        if (m_running) { const QString message = m_port.errorString(); stop(); emit error(message); }
        return;
    }
    ++m_tx; ++m_command.counter;
    emit frameSent(bytes);
}
void SerialMaster::readIncoming()
{
    const auto frames = m_decoder.append(m_port.readAll());
    for (const QByteArray &bytes : frames) {
        Protocol::Packet p;
        Protocol::decode(bytes, p);
        if (p.id != 1 || p.deviceId != m_command.deviceId) { ++m_bad; continue; }
        ++m_rx; m_lastRx.restart();
        emit received(p.positionX, p.positionY); emit frameReceived(bytes);
    }
}
