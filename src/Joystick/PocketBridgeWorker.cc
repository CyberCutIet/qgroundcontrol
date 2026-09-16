#include "PocketBridgeWorker.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QtEndian>
#include <algorithm>
#include <cerrno>
#include <cstring>

#include "ControlBridgeStatus.h"

#ifdef Q_OS_LINUX
#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace {
constexpr auto PocketName = "EdgeTX Radiomaster Pocket Joystick";
constexpr std::array<int, 8> AxisChannels{0, 1, 2, 3, 11, 6, 7, 8};
}  // namespace

PocketBridgeWorker::PocketBridgeWorker(QObject* parent) : QObject(parent) {}

PocketBridgeWorker::~PocketBridgeWorker()
{
    stop();
}

void PocketBridgeWorker::start()
{
    if (_scanTimer) {
        return;
    }
    _socket = new QUdpSocket(this);
    _sendTimer = new QTimer(this);
    _sendTimer->setTimerType(Qt::PreciseTimer);
    _sendTimer->setInterval(10);
    connect(_sendTimer, &QTimer::timeout, this, &PocketBridgeWorker::_send);
    _scanTimer = new QTimer(this);
    _scanTimer->setInterval(500);
    connect(_scanTimer, &QTimer::timeout, this, &PocketBridgeWorker::_scan);
    _scanTimer->start();
    _scan();
}

void PocketBridgeWorker::stop()
{
    if (_scanTimer) {
        _scanTimer->stop();
    }
    _close();
}

int PocketBridgeWorker::_scale(const Axis& axis)
{
    if (!axis.present || axis.maximum <= axis.minimum) {
        return 992;
    }
    const qint64 value = std::clamp(axis.value, axis.minimum, axis.maximum);
    return 192 + int((value - axis.minimum) * 1600 / (qint64(axis.maximum) - axis.minimum));
}

QByteArray PocketBridgeWorker::_packet() const
{
    std::array<quint16, 16> channels;
    channels.fill(992);
    for (size_t i = 0; i < _axes.size(); ++i) {
        channels[AxisChannels[i]] = _scale(_axes[i]);
    }
    channels[4] = _buttons[0] ? 1792 : 192;
    channels[5] = _buttons[1] ? 1792 : 192;
    QByteArray packet(32, Qt::Uninitialized);
    for (size_t i = 0; i < channels.size(); ++i) {
        qToLittleEndian(channels[i], packet.data() + i * 2);
    }
    return packet;
}

void PocketBridgeWorker::_status(int state, const QString& detail)
{
    if (state != _state || detail != _detail) {
        _state = state;
        _detail = detail;
        emit statusChanged(state, detail);
    }
}

void PocketBridgeWorker::_close()
{
    if (_sendTimer) {
        _sendTimer->stop();
    }
    if (_notifier) {
        _notifier->setEnabled(false);
        _notifier->deleteLater();
        _notifier = nullptr;
    }
#ifdef Q_OS_LINUX
    if (_fd >= 0) {
        ioctl(_fd, EVIOCGRAB, 0);
        ::close(_fd);
    }
#endif
    _fd = -1;
    _path.clear();
    _syncDropped = false;
    _axes = {};
    _buttons = {};
}

void PocketBridgeWorker::_scan()
{
#ifdef Q_OS_LINUX
    QStringList paths;
    const QDir inputs(QStringLiteral("/sys/class/input"));
    for (const QString& entry : inputs.entryList({QStringLiteral("event*")}, QDir::Dirs)) {
        QFile name(inputs.filePath(entry + QStringLiteral("/device/name")));
        if (name.open(QIODevice::ReadOnly) && name.readAll().trimmed() == PocketName) {
            paths.append(QStringLiteral("/dev/input/") + entry);
        }
    }
    if (paths.size() != 1) {
        _close();
        _status(paths.isEmpty() ? ControlBridgeStatus::Disconnected : ControlBridgeStatus::Error,
                paths.isEmpty() ? tr("Connect Pocket by USB in joystick mode.")
                                : tr("Multiple Pocket controllers found. Connect only the intended controller."));
        return;
    }
    if (_fd >= 0 && _path == paths.first()) {
        return;
    }
    _close();
    _fd = ::open(QFile::encodeName(paths.first()).constData(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (_fd < 0) {
        _status(ControlBridgeStatus::Error,
                tr("Cannot open Pocket USB input: %1").arg(QString::fromLocal8Bit(strerror(errno))));
        return;
    }
    // Event numbers may be reused during hotplug; verify the opened node before grabbing it.
    char name[256]{};
    if (ioctl(_fd, EVIOCGNAME(sizeof(name)), name) < 0 || QByteArray(name) != PocketName) {
        _close();
        _status(ControlBridgeStatus::Error, tr("Pocket input changed during connection; retrying."));
        return;
    }
    if (ioctl(_fd, EVIOCGRAB, 1) < 0) {
        _close();
        _status(ControlBridgeStatus::Error,
                tr("Pocket is busy. Close the separate control bridge or other input reader."));
        return;
    }
    _path = paths.first();
    if (!_snapshot()) {
        _close();
        _status(ControlBridgeStatus::Error, tr("Cannot read Pocket axes and buttons."));
        return;
    }
    _notifier = new QSocketNotifier(_fd, QSocketNotifier::Read, this);
    connect(_notifier, &QSocketNotifier::activated, this, &PocketBridgeWorker::_read);
    _sendTimer->start();
#else
    _status(ControlBridgeStatus::Error, tr("The integrated Pocket bridge requires Linux evdev."));
#endif
}

bool PocketBridgeWorker::_snapshot()
{
#ifdef Q_OS_LINUX
    for (size_t i = 0; i < _axes.size(); ++i) {
        input_absinfo info{};
        const bool present = ioctl(_fd, EVIOCGABS(i), &info) >= 0;
        _axes[i] = {info.value, info.minimum, info.maximum, present};
        if (i < 4 && (!present || info.maximum <= info.minimum)) {
            return false;
        }
    }
    std::array<unsigned char, (KEY_MAX + 8) / 8> keys{};
    if (ioctl(_fd, EVIOCGKEY(keys.size()), keys.data()) < 0) {
        return false;
    }
    _buttons[0] = keys[BTN_SOUTH / 8] & (1 << (BTN_SOUTH % 8));
    _buttons[1] = keys[BTN_EAST / 8] & (1 << (BTN_EAST % 8));
    return true;
#else
    return false;
#endif
}

void PocketBridgeWorker::_read()
{
#ifdef Q_OS_LINUX
    input_event events[64];
    while (_fd >= 0) {
        const ssize_t count = ::read(_fd, events, sizeof(events));
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return;
        }
        if (count <= 0 || count % sizeof(input_event) != 0) {
            _close();
            _status(ControlBridgeStatus::Disconnected, tr("Pocket USB input disconnected; waiting for reconnection."));
            return;
        }
        for (size_t i = 0; i < size_t(count) / sizeof(input_event); ++i) {
            const auto& event = events[i];
            if (event.type == EV_SYN && event.code == SYN_DROPPED) {
                _syncDropped = true;
                _status(ControlBridgeStatus::Error, tr("Resynchronizing Pocket USB input."));
            } else if (_syncDropped) {
                if (event.type == EV_SYN && event.code == SYN_REPORT) {
                    if (!_snapshot()) {
                        _close();
                        _status(ControlBridgeStatus::Error, tr("Pocket input resynchronization failed."));
                        return;
                    }
                    _syncDropped = false;
                }
            } else if (event.type == EV_ABS && event.code < _axes.size()) {
                _axes[event.code].value = event.value;
            } else if (event.type == EV_KEY && (event.code == BTN_SOUTH || event.code == BTN_EAST)) {
                _buttons[event.code == BTN_EAST] = event.value != 0;
            }
        }
    }
#endif
}

void PocketBridgeWorker::_send()
{
    // Drain disconnect/drop notifications before sending cached input.
    _read();
    if (_fd < 0 || _syncDropped || !_socket) {
        return;
    }
    const QByteArray data = _packet();
    if (_socket->writeDatagram(data, _target, _port) != data.size()) {
        _status(ControlBridgeStatus::Error, tr("Pocket forwarding error: %1").arg(_socket->errorString()));
        return;
    }
    _status(ControlBridgeStatus::Ready,
            tr("Pocket USB input → RADXA 10.10.10.13:7002, 100 Hz. Integrated QGC bridge."));
}
