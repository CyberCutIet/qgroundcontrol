#include "RpiLinkStatusTest.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

#include "RpiLinkStatus.h"

namespace {
QByteArray report(QString stream, int seq, int rssi = -50, QString session = QString(32, 'a'))
{
    QJsonObject packets;
    for (const QString& key : {QStringLiteral("out"), QStringLiteral("lost"), QStringLiteral("fec_rec"),
                               QStringLiteral("uniq"), QStringLiteral("dec_err"), QStringLiteral("bad")}) {
        packets[key] = QJsonArray{0, 0};
    }
    QJsonArray antennas;
    if (rssi > -128)
        antennas.append(QJsonObject{{"ant", 1}, {"pkt_recv", 10}, {"rssi_avg", rssi}, {"snr_avg", 20}});
    return QJsonDocument(QJsonObject{{"version", 1},
                                     {"source", "rpi"},
                                     {"stream", stream},
                                     {"seq", seq},
                                     {"session", session},
                                     {"interval_ms", 1000},
                                     {"has_rssi", rssi > -128},
                                     {"antennas", antennas},
                                     {"packets", packets}})
        .toJson(QJsonDocument::Compact);
}
}  // namespace

void RpiLinkStatusTest::_datagramsAndFreshness()
{
    RpiLinkStatus status;
    status._sender = QHostAddress::LocalHost;
    status._port = 0;
    status._bind();
    QUdpSocket peer;
    const auto tunnel = report(QStringLiteral("ip_tunnel rx"), 1, -31);
    QVERIFY(peer.writeDatagram(tunnel, QHostAddress::LocalHost, status._socket.localPort()) > 0);
    QTRY_COMPARE(status.rssi(), -31);
    QCOMPARE(status.values().value("snr").toInt(), 20);
    QVERIFY(status.statusText().contains(QStringLiteral("IP-туннеля")));
    status._consume(report(QStringLiteral("MAVLink_uplink rx"), 2, -128), QHostAddress::LocalHost);
    QCOMPARE(status.rssi(), -31);
    status._consume(report(QStringLiteral("MAVLink_uplink rx"), 3, -76), QHostAddress::LocalHost);
    QCOMPARE(status.rssi(), -76);
    QCOMPARE(status.level(), 1);
    status._consume(report(QStringLiteral("controller_FPV rx"), 4, -20), QHostAddress(QStringLiteral("10.9.9.9")));
    QCOMPARE(status.rssi(), -76);
    status._consume(QByteArray(1401, 'x'), QHostAddress::LocalHost);
    for (auto& sample : status._samples)
        sample.received -= sample.freshness;
    status._refresh();
    QCOMPARE(status.level(), -1);
    QVERIFY(!status.hasRssi());
    status._consume(report(QStringLiteral("MAVLink_uplink rx"), 5, -128), QHostAddress::LocalHost);
    QCOMPARE(status.level(), 0);
    QVERIFY(!status.hasRssi());
}

void RpiLinkStatusTest::_orderingAndSessions()
{
    RpiLinkStatus status;
    const QString id = QStringLiteral("MAVLink_uplink rx");
    status._consume(report(id, 10, -40), status._sender);
    status._consume(report(id, 9, -90), status._sender);
    status._consume(report(id, 10, -90), status._sender);
    QCOMPARE(status.rssi(), -40);
    auto bad = QJsonDocument::fromJson(report(id, 11, -90)).object();
    bad["has_rssi"] = false;
    status._consume(QJsonDocument(bad).toJson(), status._sender);
    QCOMPARE(status.rssi(), -40);
    status._consume(report(id, 1, -60, QString(32, 'b')), status._sender);
    QCOMPARE(status.rssi(), -60);
    status._consume(report(id, 100, -90), status._sender);
    QCOMPARE(status.rssi(), -60);
    status._consume(report(QStringLiteral("ip_tunnel rx"), 3, -20, QString(32, 'b')), status._sender);
    // Datagram ordering is per stream: another stream's later seq must not hide this one.
    status._consume(report(id, 2, -55, QString(32, 'b')), status._sender);
    QCOMPARE(status.rssi(), -55);
}

UT_REGISTER_TEST_LIGHTWEIGHT(RpiLinkStatusTest, TestLabel::Unit, TestLabel::Comms)
