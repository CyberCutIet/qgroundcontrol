#pragma once

#include <QtCore/QObject>
#include <QtCore/QThread>
#include <QtQmlIntegration/QtQmlIntegration>

class PocketBridgeWorker;

/// Embedded Pocket USB-to-UDP bridge; describes local forwarding, not radio reception.
class ControlBridgeStatus : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(State state READ state NOTIFY statusChanged)
    Q_PROPERTY(QString detail READ detail NOTIFY statusChanged)

public:
    enum State
    {
        Unknown,
        Disconnected,
        Ready,
        Error
    };
    Q_ENUM(State)
    explicit ControlBridgeStatus(QObject* parent = nullptr);
    ~ControlBridgeStatus() override;

    State state() const { return _state; }

    QString detail() const { return _detail; }

signals:
    void statusChanged();

private:
    void _stop();
    QThread _thread;
    PocketBridgeWorker* _worker = nullptr;
    State _state = Unknown;
    QString _detail;
};
