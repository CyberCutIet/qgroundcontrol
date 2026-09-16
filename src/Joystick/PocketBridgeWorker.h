#pragma once

#include <QtCore/QObject>
#include <QtCore/QSocketNotifier>
#include <QtCore/QTimer>
#include <QtNetwork/QHostAddress>
#include <QtNetwork/QUdpSocket>
#include <array>

/// Linux evdev input and 100 Hz forwarding run exclusively in the worker thread.
class PocketBridgeWorker : public QObject
{
    Q_OBJECT
public:
    explicit PocketBridgeWorker(QObject* parent = nullptr);
    ~PocketBridgeWorker() override;
    void start();
    void stop();

signals:
    void statusChanged(int state, const QString& detail);

private:
    friend class ControlBridgeStatusTest;

    struct Axis
    {
        int value = 0;
        int minimum = 0;
        int maximum = 0;
        bool present = false;
    };

    static int _scale(const Axis& axis);
    QByteArray _packet() const;
    void _scan();
    void _read();
    bool _snapshot();
    void _send();
    void _close();
    void _status(int state, const QString& detail);

    QTimer* _scanTimer = nullptr;
    QTimer* _sendTimer = nullptr;
    QUdpSocket* _socket = nullptr;
    QSocketNotifier* _notifier = nullptr;
    QHostAddress _target{QStringLiteral("10.10.10.13")};
    quint16 _port = 7002;
    int _fd = -1;
    QString _path;
    std::array<Axis, 8> _axes{};
    std::array<bool, 2> _buttons{};
    bool _syncDropped = false;
    int _state = -1;
    QString _detail;
};
