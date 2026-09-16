#pragma once

#include <QtCore/QElapsedTimer>
#include <QtCore/QHash>
#include <QtCore/QJsonObject>
#include <QtCore/QTimer>
#include <QtCore/QVariantMap>
#include <QtNetwork/QTcpSocket>
#include <QtQmlIntegration/QtQmlIntegration>

/// Receive-side WFB-NG statistics from the RADXA JSON line API.
class WfbLinkStatus : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(int level READ level NOTIFY statusChanged)
    Q_PROPERTY(bool hasRssi READ hasRssi NOTIFY statusChanged)
    Q_PROPERTY(int rssi READ rssi NOTIFY statusChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)
    Q_PROPERTY(QVariantMap values READ values NOTIFY statusChanged)
    Q_PROPERTY(QVariantMap packetLoss READ packetLoss NOTIFY packetLossChanged)

public:
    explicit WfbLinkStatus(QObject* parent = nullptr);
    ~WfbLinkStatus() override;

    int level() const { return _level; }

    bool hasRssi() const { return _hasRssi; }

    int rssi() const { return _rssi; }

    QString statusText() const { return _statusText; }

    QVariantMap values() const { return _values; }

    QVariantMap packetLoss() const { return _packetLoss; }

signals:
    void statusChanged();
    void packetLossChanged();

private:
    friend class WfbLinkStatusTest;

    struct Sample
    {
        qint64 received = 0;
        bool hasRssi = false;
        int rssi = -128;
        QVariantMap values;
    };

    static int _levelForRssi(int rssi);
    static int _levelForLoss(double percent);
    void _recordLoss(const QJsonObject& message);
    void _refreshLoss();
    void _connect();
    void _receive();
    void _consume(const QByteArray& line);
    void _refresh();
    void _lostConnection();
    void _publish(int level, bool hasRssi, int rssi, const QString& text, const QVariantMap& values = {});

    QTcpSocket _socket;
    QTimer _reconnect;
    QTimer _expiry;
    QTimer _connectTimeout;
    QTimer _dataTimeout;
    QTimer _lossTimer;
    QElapsedTimer _clock;
    QByteArray _buffer;
    QHash<QString, Sample> _samples;
    QString _host = QStringLiteral("10.10.10.13");
    quint16 _port = 9001;
    int _interval = 1000;
    int _freshness = 3000;
    int _level = -1;
    bool _hasRssi = false;
    int _rssi = -128;
    QString _statusText;
    QVariantMap _values;

    struct LossSample
    {
        qint64 received;
        double out;
        double lost;
        double fec;
    };

    QList<LossSample> _lossSamples;
    double _lastOut = -1;
    double _lastLost = -1;
    double _lastFec = -1;
    double _lastLossTimestamp = -1;
    qint64 _lastLossReceived = -1;
    bool _videoReceiving = false;
    QVariantMap _packetLoss;
};
