#include "ControlBridgeStatus.h"

#include "PocketBridgeWorker.h"
#include "QGCApplication.h"

ControlBridgeStatus::ControlBridgeStatus(QObject* parent)
    : QObject(parent), _detail(tr("Starting the integrated Pocket bridge."))
{
    // Tests must never open physical controllers or transmit to the aircraft network.
    if (qgcApp() && qgcApp()->runningUnitTests()) {
        return;
    }
    _worker = new PocketBridgeWorker();
    _worker->moveToThread(&_thread);
    connect(&_thread, &QThread::started, _worker, &PocketBridgeWorker::start);
    connect(&_thread, &QThread::finished, _worker, &QObject::deleteLater);
    connect(_worker, &PocketBridgeWorker::statusChanged, this, [this](int state, const QString& detail) {
        _state = static_cast<State>(state);
        _detail = detail;
        emit statusChanged();
    });
    connect(qgcApp(), &QCoreApplication::aboutToQuit, this, &ControlBridgeStatus::_stop);
    _thread.setObjectName(QStringLiteral("PocketControlBridge"));
    _thread.start();
}

ControlBridgeStatus::~ControlBridgeStatus()
{
    _stop();
}

void ControlBridgeStatus::_stop()
{
    if (_thread.isRunning()) {
        QMetaObject::invokeMethod(_worker, &PocketBridgeWorker::stop, Qt::BlockingQueuedConnection);
        _thread.quit();
        _thread.wait();
    }
    _worker = nullptr;
}
