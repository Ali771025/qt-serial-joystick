#include "protocol.h"
#include <QtEndian>

namespace Protocol {
quint8 checksum(const QByteArray &bytes)
{
    quint8 crc = 0;
    for (int i = 1; i < CrcOffset && i < bytes.size(); ++i)
        crc ^= static_cast<quint8>(bytes.at(i));
    return crc;
}
QByteArray encode(const Packet &p)
{
    QByteArray b(PacketSize, '\0');
    const auto put = [&](int offset, quint16 value) {
        // Every 16-bit field is LSB first, independently of the host CPU.
        qToLittleEndian<quint16>(value, b.data() + offset);
    };
    b[0] = char(0xa5); b[1] = char(p.id); b[2] = char(p.deviceId); b[3] = char(p.counter);
    put(4, p.dataValue); put(6, p.positionX); put(8, p.positionY);
    put(10, quint16(p.velocityX)); put(12, quint16(p.velocityY));
    put(14, p.targetX); put(16, quint16(p.targetY));
    put(18, p.speed); put(20, p.range); put(22, p.mk); put(24, p.discrete);
    for (int i = 0; i < ReserveCount; ++i) b[ReserveOffset + i] = char(p.reserve[i]);
    b[CrcOffset] = char(checksum(b)); b[FooterOffset] = char(0x5a);
    return b;
}
bool decode(const QByteArray &b, Packet &p)
{
    if (b.size() != PacketSize || quint8(b[0]) != 0xa5 || quint8(b[FooterOffset]) != 0x5a
            || quint8(b[CrcOffset]) != checksum(b)) return false;
    const auto get = [&](int offset) -> quint16 {
        return qFromLittleEndian<quint16>(b.constData() + offset);
    };
    const auto signedValue = [](quint16 v) -> qint16 {
        return qint16(v < 0x8000 ? int(v) : int(v) - 65536);
    };
    p.id = quint8(b[1]); p.deviceId = quint8(b[2]); p.counter = quint8(b[3]);
    p.dataValue = get(4); p.positionX = get(6); p.positionY = get(8);
    p.velocityX = signedValue(get(10)); p.velocityY = signedValue(get(12));
    p.targetX = get(14); p.targetY = signedValue(get(16));
    p.speed = get(18); p.range = get(20); p.mk = get(22); p.discrete = get(24);
    for (int i = 0; i < ReserveCount; ++i) p.reserve[i] = quint8(b[ReserveOffset + i]);
    return true;
}
QList<QByteArray> StreamDecoder::append(const QByteArray &bytes)
{
    m_buffer += bytes;
    QList<QByteArray> frames;
    while (!m_buffer.isEmpty()) {
        const int header = m_buffer.indexOf(char(0xa5));
        if (header < 0) { m_buffer.clear(); break; }
        if (header > 0) m_buffer.remove(0, header);
        if (m_buffer.size() < PacketSize) break;
        const QByteArray frame = m_buffer.left(PacketSize);
        Packet packet;
        if (!decode(frame, packet)) {
            ++m_rejected; m_buffer.remove(0, 1); continue;
        }
        frames.append(frame); m_buffer.remove(0, PacketSize);
    }
    return frames;
}
}
