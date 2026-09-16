#include "RpiLinkStatus.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QRegularExpression>
#include <QtNetwork/QNetworkDatagram>
#include <algorithm>
#include <cmath>

#include "QGCApplication.h"

namespace {
const QStringList Streams{QStringLiteral("MAVLink_uplink rx"), QStringLiteral("controller_FPV rx"),
                          QStringLiteral("ip_tunnel rx")};

bool integer(const QJsonValue& value, double minimum, double maximum)
{
    const double n = value.toDouble(-1);
    return value.isDouble() && std::isfinite(n) && n >= minimum && n <= maximum && std::floor(n) == n;
}
}  // namespace

RpiLinkStatus::RpiLinkStatus(QObject* parent) : QObject(parent), _statusText(tr("Ожидание ретранслятора"))
{
    _clock.start();
    _timer.setInterval(250);
    _retry.setInterval(2000);
    connect(&_timer, &QTimer::timeout, this, &RpiLinkStatus::_refresh);
    connect(&_retry, &QTimer::timeout, this, &RpiLinkStatus::_bind);
    connect(&_socket, &QUdpSocket::readyRead, this, &RpiLinkStatus::_receive);
    if (!qgcApp() || !qgcApp()->runningUnitTests()) {
        _bind();
        _timer.start();
    }
}

void RpiLinkStatus::_bind()
{
    if (_socket.state() == QAbstractSocket::BoundState)
        return;
    _bindError = !_socket.bind(QHostAddress::AnyIPv4, _port, QUdpSocket::DontShareAddress);
    if (_bindError)
        _retry.start();
    else
        _retry.stop();
    _refresh();
}

void RpiLinkStatus::_receive()
{
    // Bound the event-loop work even if an unrelated sender floods this port.
    int remaining = 64;
    while (_socket.hasPendingDatagrams() && remaining-- > 0) {
        const auto datagram = _socket.receiveDatagram(1401);
        _consume(datagram.data(), datagram.senderAddress());
    }
    if (_socket.hasPendingDatagrams())
        QTimer::singleShot(0, this, &RpiLinkStatus::_receive);
}

void RpiLinkStatus::_consume(const QByteArray& data, const QHostAddress& sender)
{
    if (sender != _sender || data.size() > 1400)
        return;
    const auto doc = QJsonDocument::fromJson(data);
    if (!doc.isObject())
        return;
    const auto m = doc.object();
    const QString stream = m.value("stream").toString();
    const QString session = m.value("session").toString();
    static const QRegularExpression sessionPattern(QStringLiteral("^[0-9a-f]{32}$"));
    if (m.value("version").toInt(-1) != 1 || m.value("source").toString() != QStringLiteral("rpi") ||
        !Streams.contains(stream) || !sessionPattern.match(session).hasMatch() ||
        !integer(m.value("seq"), 1, 9007199254740991.0) || !integer(m.value("interval_ms"), 100, 5000) ||
        !m.value("antennas").isArray() || !m.value("has_rssi").isBool())
        return;
    const qint64 seq = static_cast<qint64>(m.value("seq").toDouble());
    if (_retiredSessions.contains(session) ||
        (session == _session && _samples.contains(stream) && seq <= _samples[stream].seq))
        return;
    Sample sample;
    sample.seq = seq;
    sample.received = _clock.elapsed();
    sample.freshness = std::max(3000, m.value("interval_ms").toInt() * 3);
    sample.values[QStringLiteral("stream")] = stream;
    sample.values[QStringLiteral("interval")] = m.value("interval_ms").toInt() / 1000.0;
    const auto packets = m.value("packets").toObject();
    for (const QString& name : {QStringLiteral("out"), QStringLiteral("lost"), QStringLiteral("fec_rec"),
                                QStringLiteral("uniq"), QStringLiteral("dec_err"), QStringLiteral("bad")}) {
        const auto pair = packets.value(name).toArray();
        if (pair.size() != 2 || !integer(pair[0], 0, 9007199254740991.0) ||
            !integer(pair[1], pair[0].toDouble(), 9007199254740991.0))
            return;
        sample.values[name] = pair[0].toDouble();
        sample.values[name + QStringLiteral("Total")] = pair[1].toDouble();
    }
    QStringList antennas;
    for (const auto& entry : m.value("antennas").toArray()) {
        const auto ant = entry.toObject();
        const double rssi = ant.value("rssi_avg").toDouble(-128);
        if (!ant.value("rssi_avg").isDouble() || !std::isfinite(rssi) || rssi < -127 || rssi >= 0 ||
            !integer(ant.value("ant"), 0, 65535) || !integer(ant.value("pkt_recv"), 1, 9007199254740991.0))
            return;
        antennas.append(tr("Антенна %1: %2 dBm · SNR %3 dB")
                            .arg(ant.value("ant").toInt())
                            .arg(qRound(rssi))
                            .arg(ant.value("snr_avg").toVariant().toString()));
        if (qRound(rssi) > sample.rssi) {
            sample.rssi = qRound(rssi);
            sample.values[QStringLiteral("snr")] = ant.value("snr_avg").toVariant();
        }
    }
    if (m.value("has_rssi").toBool() != !antennas.isEmpty())
        return;
    sample.values[QStringLiteral("antennas")] = antennas.join(QLatin1Char('\n'));
    if (session != _session) {
        if (!_session.isEmpty())
            _retiredSessions.append(_session);
        while (_retiredSessions.size() > 8)
            _retiredSessions.removeFirst();
        _session = session;
        _samples.clear();
    }
    _samples.insert(stream, sample);
    _refresh();
}

void RpiLinkStatus::_refresh()
{
    const qint64 now = _clock.elapsed();
    int rssi = -128;
    bool fresh = false;
    QVariantMap values;
    QVariantList streams;
    for (const QString& id : Streams) {
        const auto it = _samples.constFind(id);
        const bool current = it != _samples.cend() && now - it->received < it->freshness;
        QVariantMap row = current ? it->values : QVariantMap{{QStringLiteral("stream"), id}};
        row[QStringLiteral("fresh")] = current;
        row[QStringLiteral("hasRssi")] = current && it->rssi > -128;
        row[QStringLiteral("rssi")] = current ? it->rssi : -128;
        streams.append(row);
        fresh |= current;
        if (current && it->rssi > -128 && rssi == -128) {
            rssi = it->rssi;
            values = it->values;
        }
    }
    values[QStringLiteral("streams")] = streams;
    const bool hasRssi = rssi > -128;
    const int level = !hasRssi     ? (fresh ? 0 : -1)
                      : rssi < -85 ? 0
                      : rssi < -75 ? 1
                      : rssi < -65 ? 2
                      : rssi < -55 ? 3
                                   : 4;
    const QString text = _bindError ? tr("Не удалось открыть UDP 5800")
                         : !fresh   ? tr("Нет данных ретранслятора")
                         : !hasRssi ? tr("Нет приёма на ретрансляторе")
                         : values.value("stream").toString() == QStringLiteral("ip_tunnel rx")
                             ? tr("RSSI IP-туннеля · не подтверждает приём управления")
                             : tr("Приём на ретрансляторе");
    if (_level == level && _hasRssi == hasRssi && _rssi == rssi && _statusText == text && _values == values)
        return;
    _level = level;
    _hasRssi = hasRssi;
    _rssi = rssi;
    _values = values;
    _statusText = text;
    emit statusChanged();
}
