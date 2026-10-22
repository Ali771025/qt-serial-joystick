#pragma once
#include <QByteArray>
#include <QtGlobal>
#include <QList>

namespace Protocol {
constexpr int ReserveCount = 5;
constexpr int ReserveOffset = 26;
constexpr int CrcOffset = ReserveOffset + ReserveCount;
constexpr int FooterOffset = CrcOffset + 1;
constexpr int PacketSize = FooterOffset + 1;
struct Packet {
    quint8 id = 1, deviceId = 0, counter = 0;
    quint16 dataValue = 0;
    quint16 positionX = 0, positionY = 0;
    qint16 velocityX = 0, velocityY = 0;
    quint16 targetX = 0;
    qint16 targetY = 0;
    quint16 speed = 0, range = 0, mk = 0, discrete = 0;
    quint8 reserve[ReserveCount] = {0, 0, 0, 0, 0};
};
quint8 checksum(const QByteArray &bytes);
QByteArray encode(const Packet &packet);
bool decode(const QByteArray &bytes, Packet &packet);
// Retains incomplete frames and recovers alignment after noise or damaged frames.
class StreamDecoder {
public:
    QList<QByteArray> append(const QByteArray &bytes);
    void reset() { m_buffer.clear(); m_rejected = 0; }
    quint64 rejected() const { return m_rejected; }
private:
    QByteArray m_buffer;
    quint64 m_rejected = 0;
};
}
