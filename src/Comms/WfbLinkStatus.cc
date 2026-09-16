#include "WfbLinkStatus.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <algorithm>
#include <cmath>

#include "QGCApplication.h"

namespace {
const QStringList RxStreams{QStringLiteral("video_repiter rx"), QStringLiteral("MAVLink rx"),
                            QStringLiteral("video_fpv rx"), QStringLiteral("ip_tunnel rx")};

bool counter(const QJsonObject& counters, const QString& name, double& recent, double& total)
{
    const auto pair = counters.value(name).toArray();
    if (pair.size() != 2 || !pair[0].isDouble() || !pair[1].isDouble()) {
        return false;
    }
    recent = pair[0].toDouble();
    total = pair[1].toDouble();
    return std::isfinite(recent) && std::isfinite(total) && recent >= 0 && total >= recent;
}
}  // namespace

WfbLinkStatus::WfbLinkStatus(QObject* parent) : QObject(parent), _statusText(tr("Ожидание RADXA"))
{
    _clock.start();
    _socket.setReadBufferSize(262144);
    _reconnect.setInterval(2000);
    _reconnect.setSingleShot(true);
    _connectTimeout.setInterval(3000);
    _connectTimeout.setSingleShot(true);
    _dataTimeout.setSingleShot(true);
    _expiry.setInterval(250);
    _lossTimer.setInterval(500);
    connect(&_lossTimer, &QTimer::timeout, this, &WfbLinkStatus::_refreshLoss);
    connect(&_reconnect, &QTimer::timeout, this, &WfbLinkStatus::_connect);
    connect(&_expiry, &QTimer::timeout, this, &WfbLinkStatus::_refresh);
    connect(&_connectTimeout, &QTimer::timeout, this, &WfbLinkStatus::_lostConnection);
    connect(&_dataTimeout, &QTimer::timeout, this, &WfbLinkStatus::_lostConnection);
    connect(&_socket, &QTcpSocket::connected, this, [this]() { _dataTimeout.start(_freshness); });
    connect(&_socket, &QTcpSocket::connected, &_connectTimeout, &QTimer::stop);
    connect(&_socket, &QTcpSocket::readyRead, this, &WfbLinkStatus::_receive);
    connect(&_socket, &QTcpSocket::disconnected, this, &WfbLinkStatus::_lostConnection);
    connect(&_socket, &QTcpSocket::errorOccurred, this, &WfbLinkStatus::_lostConnection);
    if (!qgcApp() || !qgcApp()->runningUnitTests()) {
        _expiry.start();
        _lossTimer.start();
        _connect();
    }
}

WfbLinkStatus::~WfbLinkStatus()
{
    // Socket destruction can emit disconnected after later-declared state/timers have been destroyed.
    _socket.disconnect();
    _socket.abort();
}

void WfbLinkStatus::_connect()
{
    _reconnect.stop();
    _buffer.clear();
    _connectTimeout.start();
    _socket.connectToHost(_host, _port);
}

void WfbLinkStatus::_lostConnection()
{
    _connectTimeout.stop();
    _dataTimeout.stop();
    // Mark before abort(): abort may synchronously emit disconnected.
    const bool alreadyScheduled = _reconnect.isActive();
    _reconnect.start();
    if (!alreadyScheduled) {
        _socket.abort();
    }
    _buffer.clear();
    _samples.clear();
    _lossSamples.clear();
    _lastLossTimestamp = -1;
    _lastLossReceived = -1;
    _lastOut = _lastLost = _lastFec = -1;
    _videoReceiving = false;
    _refreshLoss();
    _publish(-1, false, -128, tr("Нет данных RADXA"));
}

void WfbLinkStatus::_receive()
{
    _buffer += _socket.readAll();
    if (_buffer.size() > 262144) {
        _lostConnection();
        return;
    }
    qsizetype end;
    while ((end = _buffer.indexOf('\n')) >= 0) {
        const QByteArray line = _buffer.left(end);
        _buffer.remove(0, end + 1);
        if (line.size() <= 65536) {
            _consume(line);
        }
    }
    if (_buffer.size() > 65536) {
        _lostConnection();
    }
}

void WfbLinkStatus::_consume(const QByteArray& line)
{
    const auto document = QJsonDocument::fromJson(line);
    if (!document.isObject()) {
        return;
    }
    const auto message = document.object();
    const QString type = message.value(QStringLiteral("type")).toString();
    if (type == QStringLiteral("settings")) {
        const int interval = message.value(QStringLiteral("settings"))
                                 .toObject()
                                 .value(QStringLiteral("common"))
                                 .toObject()
                                 .value(QStringLiteral("log_interval"))
                                 .toInt(-1);
        if (interval >= 100 && interval <= 5000) {
            _interval = interval;
            _freshness = std::max(3000, interval * 3);
        }
        return;
    }
    const QString id = message.value(QStringLiteral("id")).toString();
    if (type != QStringLiteral("rx") || !RxStreams.contains(id) ||
        !message.value(QStringLiteral("rx_ant_stats")).isArray()) {
        return;
    }
    const auto packets = message.value(QStringLiteral("packets")).toObject();
    double received, receivedTotal, lost, lostTotal, fec, fecTotal, bytes, bytesTotal;
    if (!counter(packets, QStringLiteral("uniq"), received, receivedTotal) ||
        !counter(packets, QStringLiteral("lost"), lost, lostTotal) ||
        !counter(packets, QStringLiteral("fec_rec"), fec, fecTotal) ||
        !counter(packets, QStringLiteral("out_bytes"), bytes, bytesTotal)) {
        return;
    }
    Sample sample;
    sample.received = _clock.elapsed();
    sample.values = {{QStringLiteral("stream"), id},
                     {QStringLiteral("packets"), received},
                     {QStringLiteral("lost"), lost},
                     {QStringLiteral("lostTotal"), lostTotal},
                     {QStringLiteral("fec"), fec},
                     {QStringLiteral("fecTotal"), fecTotal},
                     {QStringLiteral("mbps"), bytes * 8 / (_interval * 1000.0)},
                     {QStringLiteral("interval"), _interval / 1000.0}};
    QStringList antennas;
    const auto antennaStats = message.value(QStringLiteral("rx_ant_stats")).toArray();
    for (const auto& entry : antennaStats) {
        const auto ant = entry.toObject();
        const auto rssiValue = ant.value(QStringLiteral("rssi_avg"));
        const auto packetValue = ant.value(QStringLiteral("pkt_recv"));
        if (!rssiValue.isDouble() || !packetValue.isDouble() || packetValue.toDouble() <= 0) {
            continue;
        }
        const double rssi = rssiValue.toDouble();
        if (!std::isfinite(rssi) || rssi < -127 || rssi >= 0) {
            continue;
        }
        antennas.append(tr("Антенна %1: %2 dBm · SNR %3 dB")
                            .arg(ant.value(QStringLiteral("ant")).toVariant().toString())
                            .arg(qRound(rssi))
                            .arg(ant.value(QStringLiteral("snr_avg")).toVariant().toString()));
        if (received > 0 && (!sample.hasRssi || rssi > sample.rssi)) {
            sample.hasRssi = true;
            sample.rssi = qRound(rssi);
            sample.values[QStringLiteral("snr")] = ant.value(QStringLiteral("snr_avg")).toVariant();
            sample.values[QStringLiteral("frequency")] = ant.value(QStringLiteral("freq")).toVariant();
            sample.values[QStringLiteral("mcs")] = ant.value(QStringLiteral("mcs")).toVariant();
            sample.values[QStringLiteral("bandwidth")] = ant.value(QStringLiteral("bw")).toVariant();
        }
    }
    // Malformed antenna measurements must not be interpreted as a valid loss-of-signal report.
    if (received > 0 && !sample.hasRssi) {
        return;
    }
    sample.values[QStringLiteral("antennas")] = antennas.join(QLatin1Char('\n'));
    const auto session = message.value(QStringLiteral("session")).toObject();
    sample.values[QStringLiteral("fecScheme")] = QStringLiteral("%1/%2")
                                                     .arg(session.value(QStringLiteral("fec_k")).toInt())
                                                     .arg(session.value(QStringLiteral("fec_n")).toInt());
    _samples.insert(id, sample);
    if (id == QStringLiteral("video_repiter rx")) {
        _recordLoss(message);
    }
    _dataTimeout.start(_freshness);
    _refresh();
}

int WfbLinkStatus::_levelForLoss(double percent)
{
    if (percent >= 50)
        return 0;
    if (percent >= 20)
        return 1;
    if (percent >= 5)
        return 2;
    if (percent > 0)
        return 3;
    return 4;
}

void WfbLinkStatus::_recordLoss(const QJsonObject& message)
{
    const auto packets = message.value(QStringLiteral("packets")).toObject();
    double out, outTotal, lost, lostTotal, fec, fecTotal;
    const auto timestamp = message.value(QStringLiteral("timestamp"));
    if (!timestamp.isDouble() || !std::isfinite(timestamp.toDouble()) ||
        !counter(packets, QStringLiteral("out"), out, outTotal) ||
        !counter(packets, QStringLiteral("lost"), lost, lostTotal) ||
        !counter(packets, QStringLiteral("fec_rec"), fec, fecTotal)) {
        return;
    }
    const double stamp = timestamp.toDouble();
    const qint64 now = _clock.elapsed();
    const bool reset = _lastOut < 0 || outTotal < _lastOut || lostTotal < _lastLost || fecTotal < _lastFec ||
                       (_lastLossReceived >= 0 && now - _lastLossReceived >= _freshness);
    if (!reset && stamp <= _lastLossTimestamp)
        return;
    if (reset) {
        _lossSamples.clear();
    } else {
        // Use cumulative differences so a missing API report is not counted as a radio loss.
        out = outTotal - _lastOut;
        lost = lostTotal - _lastLost;
        fec = fecTotal - _lastFec;
    }
    _lastOut = outTotal;
    _lastLost = lostTotal;
    _lastFec = fecTotal;
    _lastLossTimestamp = stamp;
    _lastLossReceived = now;
    _videoReceiving = out + lost > 0;
    if (!_videoReceiving)
        _lossSamples.clear();
    else
        _lossSamples.append({now, out, lost, fec});
    while (_lossSamples.size() > 40)
        _lossSamples.removeFirst();
}

void WfbLinkStatus::_refreshLoss()
{
    const qint64 now = _clock.elapsed();
    while (!_lossSamples.isEmpty() && now - _lossSamples.first().received >= 3000) {
        _lossSamples.removeFirst();
    }
    const bool fresh = _lastLossReceived >= 0 && now - _lastLossReceived < _freshness;
    double out = 0, lost = 0, fec = 0;
    for (const auto& sample : _lossSamples) {
        out += sample.out;
        lost += sample.lost;
        fec += sample.fec;
    }
    const bool valid = fresh && _videoReceiving && out + lost > 0;
    const double percent = valid ? 100.0 * lost / (out + lost) : 0;
    const double displayPercent = valid ? std::ceil(1000.0 * lost / (out + lost)) / 10.0 : 0;
    const QVariantMap values{{QStringLiteral("hasData"), valid},
                             {QStringLiteral("percent"), percent},
                             {QStringLiteral("displayPercent"), displayPercent},
                             {QStringLiteral("level"), valid ? _levelForLoss(displayPercent) : -1},
                             {QStringLiteral("out"), out},
                             {QStringLiteral("lost"), lost},
                             {QStringLiteral("fec"), fec},
                             {QStringLiteral("stream"), QStringLiteral("video_repiter rx")},
                             {QStringLiteral("interval"), _interval / 1000.0},
                             {QStringLiteral("status"), !fresh   ? tr("Нет свежих данных видео")
                                                        : !valid ? tr("Нет пакетов видео")
                                                                 : tr("Оценка потерь после FEC · окно 3 с")}};
    if (_packetLoss != values) {
        _packetLoss = values;
        emit packetLossChanged();
    }
}

int WfbLinkStatus::_levelForRssi(int rssi)
{
    // Display thresholds, not a prediction of decoder failure. No packet/FEC/SNR inputs.
    if (rssi < -85)
        return 0;
    if (rssi < -75)
        return 1;
    if (rssi < -65)
        return 2;
    if (rssi < -55)
        return 3;
    return 4;
}

void WfbLinkStatus::_refresh()
{
    bool fresh = false;
    const qint64 now = _clock.elapsed();
    // Prefer the repeater video; another received stream can prove reception when that video is off.
    for (const QString& id : RxStreams) {
        const auto sample = _samples.constFind(id);
        if (sample == _samples.cend() || now - sample->received >= _freshness) {
            continue;
        }
        fresh = true;
        if (sample->hasRssi) {
            const int level = _levelForRssi(sample->rssi);
            const QStringList states{tr("Критически слабый RSSI"), tr("Слабый RSSI"), tr("Средний RSSI"),
                                     tr("Хороший RSSI"), tr("Сильный RSSI")};
            _publish(level, true, sample->rssi, states[level], sample->values);
            return;
        }
    }
    _publish(fresh ? 0 : -1, false, -128, fresh ? tr("Нет приёма WFB-NG") : tr("Нет свежих данных RADXA"));
}

void WfbLinkStatus::_publish(int level, bool hasRssi, int rssi, const QString& text, const QVariantMap& values)
{
    if (_level == level && _hasRssi == hasRssi && _rssi == rssi && _statusText == text && _values == values) {
        return;
    }
    _level = level;
    _hasRssi = hasRssi;
    _rssi = rssi;
    _statusText = text;
    _values = values;
    emit statusChanged();
}
