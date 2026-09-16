#pragma once

#include <QtCore/QElapsedTimer>
#include <QtCore/QHash>
#include <QtCore/QTimer>
#include <QtCore/QVariantMap>
#include <QtNetwork/QUdpSocket>
#include <QtQmlIntegration/QtQmlIntegration>

/// RPi RX reports delivered by RADXA over the dedicated WFB statistics stream.
class RpiLinkStatus : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(int level READ level NOTIFY statusChanged)
    Q_PROPERTY(bool hasRssi READ hasRssi NOTIFY statusChanged)
    Q_PROPERTY(int rssi READ rssi NOTIFY statusChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)
    Q_PROPERTY(QVariantMap values READ values NOTIFY statusChanged)
public:
    explicit RpiLinkStatus(QObject* parent = nullptr);

    int level() const { return _level; }

    bool hasRssi() const { return _hasRssi; }

    int rssi() const { return _rssi; }

    QString statusText() const { return _statusText; }

    QVariantMap values() const { return _values; }

signals:
    void statusChanged();

private:
    friend class RpiLinkStatusTest;
    void _bind();
    void _receive();
    void _consume(const QByteArray& data, const QHostAddress& sender);
    void _refresh();

    struct Sample
    {
        qint64 received = 0;
        qint64 seq = 0;
        int freshness = 3000;
        int rssi = -128;
        QVariantMap values;
    };

    QUdpSocket _socket;
    QTimer _timer;
    QTimer _retry;
    QElapsedTimer _clock;
    QHash<QString, Sample> _samples;
    QString _session;
    QStringList _retiredSessions;
    QHostAddress _sender{QStringLiteral("10.10.10.13")};
    quint16 _port = 5800;
    int _level = -1;
    int _rssi = -128;
    bool _hasRssi = false;
    bool _bindError = false;
    QString _statusText;
    QVariantMap _values;
};
