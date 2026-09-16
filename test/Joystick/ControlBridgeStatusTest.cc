#include "ControlBridgeStatusTest.h"

#include <QtCore/QtEndian>
#include <QtNetwork/QNetworkDatagram>

#include "ControlBridgeStatus.h"
#include "PocketBridgeWorker.h"
#ifdef Q_OS_LINUX
#include <fcntl.h>
#include <linux/input.h>
#include <unistd.h>
#endif

void ControlBridgeStatusTest::_packetCompatibility()
{
    PocketBridgeWorker worker;
    // Golden channel order from Python struct.pack("<16H", ...).
    for (size_t i = 0; i < worker._axes.size(); ++i) {
        worker._axes[i] = {int(i * 100), 0, 1000, true};
    }
    worker._buttons = {true, false};
    const std::array<quint16, 16> expected{192,  352, 512, 672, 1792, 192, 992, 1152,
                                           1312, 992, 992, 832, 992,  992, 992, 992};
    const auto bytes = worker._packet();
    QCOMPARE(bytes.size(), 32);
    for (size_t i = 0; i < expected.size(); ++i) {
        QCOMPARE(qFromLittleEndian<quint16>(bytes.constData() + 2 * i), expected[i]);
    }
    QCOMPARE(PocketBridgeWorker::_scale({-99, 0, 1000, true}), 192);
    QCOMPARE(PocketBridgeWorker::_scale({5000, 0, 1000, true}), 1792);
    QCOMPARE(PocketBridgeWorker::_scale({0, 0, 0, true}), 992);
    QCOMPARE(PocketBridgeWorker::_scale({999, 0, 1000, false}), 992);
    QCOMPARE(PocketBridgeWorker::_scale({0, -32768, 32767, true}), 992);
}

void ControlBridgeStatusTest::_disconnectStopsForwarding()
{
#ifdef Q_OS_LINUX
    int input[2];
    QVERIFY(pipe2(input, O_NONBLOCK | O_CLOEXEC) == 0);
    PocketBridgeWorker worker;
    worker._fd = input[0];
    worker._socket = new QUdpSocket(&worker);
    worker._target = QHostAddress::LocalHost;
    QUdpSocket receiver;
    QVERIFY(receiver.bind(QHostAddress::LocalHost, 0));
    worker._port = receiver.localPort();
    worker._send();
    QTRY_VERIFY(receiver.hasPendingDatagrams());
    QCOMPARE(receiver.receiveDatagram().data().size(), 32);
    QCOMPARE(worker._state, int(ControlBridgeStatus::Ready));
    ::close(input[1]);
    worker._send();
    QCOMPARE(worker._fd, -1);
    QCOMPARE(worker._state, int(ControlBridgeStatus::Disconnected));
    QVERIFY(!receiver.hasPendingDatagrams());
    worker._send();
    QVERIFY(!receiver.hasPendingDatagrams());
#else
    QSKIP("Linux evdev bridge");
#endif
}

void ControlBridgeStatusTest::_droppedEventsStopForwarding()
{
#ifdef Q_OS_LINUX
    int input[2];
    QVERIFY(pipe2(input, O_NONBLOCK | O_CLOEXEC) == 0);
    PocketBridgeWorker worker;
    worker._fd = input[0];
    worker._socket = new QUdpSocket(&worker);
    worker._target = QHostAddress::LocalHost;
    QUdpSocket receiver;
    QVERIFY(receiver.bind(QHostAddress::LocalHost, 0));
    worker._port = receiver.localPort();
    input_event dropped{};
    dropped.type = EV_SYN;
    dropped.code = SYN_DROPPED;
    QCOMPARE(::write(input[1], &dropped, sizeof(dropped)), ssize_t(sizeof(dropped)));
    worker._send();
    QVERIFY(worker._syncDropped);
    QVERIFY(!receiver.hasPendingDatagrams());
    QCOMPARE(worker._state, int(ControlBridgeStatus::Error));
    // A pipe cannot satisfy evdev resync ioctls: fail closed instead of sending stale controls.
    dropped.code = SYN_REPORT;
    QCOMPARE(::write(input[1], &dropped, sizeof(dropped)), ssize_t(sizeof(dropped)));
    worker._send();
    QCOMPARE(worker._fd, -1);
    QVERIFY(!receiver.hasPendingDatagrams());
    ::close(input[1]);
#else
    QSKIP("Linux evdev bridge");
#endif
}

UT_REGISTER_TEST_LIGHTWEIGHT(ControlBridgeStatusTest, TestLabel::Unit, TestLabel::Joystick)
