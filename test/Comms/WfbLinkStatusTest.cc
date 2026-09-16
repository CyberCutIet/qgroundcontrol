#include "WfbLinkStatusTest.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtNetwork/QTcpServer>

#include "WfbLinkStatus.h"

namespace {
QByteArray rx(int rssi, int lost = 0, QString id = QStringLiteral("video_repiter rx"), bool receiving = true)
{
    const auto antenna = [rssi](int index, int delta) {
        return QJsonObject{{"ant", index},  {"pkt_recv", 100}, {"rssi_avg", rssi + delta},
                           {"snr_avg", 13}, {"freq", 5200},    {"mcs", 3},
                           {"bw", 40}};
    };
    return QJsonDocument(
               QJsonObject{{"type", "rx"},
                           {"id", id},
                           {"timestamp", 1},
                           {"rx_ant_stats", receiving ? QJsonArray{antenna(0, 0), antenna(1, -4)} : QJsonArray{}},
                           {"packets", QJsonObject{{"uniq", QJsonArray{receiving ? 100 : 0, 10000}},
                                                   {"lost", QJsonArray{lost, 10000}},
                                                   {"fec_rec", QJsonArray{1, 200}},
                                                   {"out_bytes", QJsonArray{125000, 1000000}}}},
                           {"session", QJsonObject{{"fec_k", 4}, {"fec_n", 6}}}})
        .toJson(QJsonDocument::Compact);
}
}  // namespace

void WfbLinkStatusTest::_rssiOnlyScale()
{
    WfbLinkStatus status;
    const QList<QPair<int, int>> ranges{{-100, 0}, {-86, 0}, {-85, 1}, {-76, 1}, {-75, 2},
                                        {-66, 2},  {-65, 3}, {-56, 3}, {-55, 4}, {-17, 4}};
    for (auto [rssi, level] : ranges) {
        status._consume(rx(rssi));
        QVERIFY(status.hasRssi());
        QCOMPARE(status.rssi(), rssi);
        QCOMPARE(status.level(), level);
        // Even severe packet loss must not change this explicitly RSSI-only scale.
        status._consume(rx(rssi, 500));
        QCOMPARE(status.level(), level);
        QCOMPARE(status.values().value("lost").toInt(), 500);
    }
    QCOMPARE(status.values().value("mbps").toDouble(), 1.0);
    QCOMPARE(status.values().value("fecScheme").toString(), QStringLiteral("4/6"));
    QVERIFY(status.values().value("antennas").toString().contains(QStringLiteral("-21 dBm")));
}

void WfbLinkStatusTest::_freshnessAndFallback()
{
    WfbLinkStatus status;
    status._consume(rx(-45));
    QCOMPARE(status.level(), 4);
    status._consume(rx(-100, 0, QStringLiteral("controller_FPV tx")));
    QCOMPARE(status.rssi(), -45);
    status._consume(rx(-70, 0, QStringLiteral("MAVLink rx")));
    QCOMPARE(status.rssi(), -45);
    status._consume(rx(-128, 0, QStringLiteral("video_repiter rx"), false));
    QCOMPARE(status.rssi(), -70);
    status._consume(rx(-128, 0, QStringLiteral("MAVLink rx"), false));
    QVERIFY(!status.hasRssi());
    QCOMPARE(status.level(), 0);
    for (auto& sample : status._samples) {
        sample.received -= status._freshness;
    }
    status._consume("{}");
    status._consume(R"({"type":"rx","id":"video_repiter rx"})");
    status._refresh();
    QCOMPARE(status.level(), -1);
    status._consume(rx(-128));
    QCOMPARE(status.level(), -1);
    status._consume(rx(-60));
    QVERIFY(status.hasRssi());
    QCOMPARE(status.level(), 3);
}

void WfbLinkStatusTest::_tcpFramesAndReconnect()
{
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost, 0));
    WfbLinkStatus status;
    status._host = QStringLiteral("127.0.0.1");
    status._port = server.serverPort();
    status._reconnect.setInterval(20);
    status._connect();
    QTRY_VERIFY(server.hasPendingConnections());
    QScopedPointer<QTcpSocket> peer(server.nextPendingConnection());
    const auto data = rx(-32);
    peer->write(data.left(data.size() / 2));
    peer->flush();
    QTRY_VERIFY(!status._buffer.isEmpty());
    QVERIFY(!status.hasRssi());
    peer->write(data.mid(data.size() / 2) + '\n' + rx(-44) + '\n');
    QTRY_COMPARE(status.rssi(), -44);
    peer->disconnectFromHost();
    QTRY_VERIFY(!status.hasRssi());
    QTRY_VERIFY_WITH_TIMEOUT(server.hasPendingConnections(), TestTimeout::mediumMs());
    peer.reset(server.nextPendingConnection());
    peer->write(rx(-57) + '\n');
    QTRY_COMPARE(status.rssi(), -57);
    // A connected server sending garbage cannot keep old RSSI alive.
    status._dataTimeout.start(100);
    peer->write("bad json\n{}\n");
    QTRY_VERIFY(!status.hasRssi());
    QCOMPARE(status.level(), -1);
}

void WfbLinkStatusTest::_packetLossWindow()
{
    WfbLinkStatus status;
    const auto feed = [&](double stamp, int out, int outTotal, int lost, int lostTotal, int fec = 0) {
        auto message = QJsonDocument::fromJson(rx(-25)).object();
        message["timestamp"] = stamp;
        auto packets = message["packets"].toObject();
        packets["out"] = QJsonArray{out, outTotal};
        packets["lost"] = QJsonArray{lost, lostTotal};
        packets["fec_rec"] = QJsonArray{fec, fec};
        message["packets"] = packets;
        status._consume(QJsonDocument(message).toJson());
        status._refreshLoss();
    };
    feed(1, 990, 990, 10, 10, 50);
    QVERIFY(status.packetLoss().value("hasData").toBool());
    QCOMPARE(status.packetLoss().value("percent").toDouble(), 1.0);
    QCOMPARE(status.packetLoss().value("level").toInt(), 3);
    QCOMPARE(status.level(), 4);     // Packet loss cannot change RSSI.
    feed(1, 990, 990, 10, 10, 50);   // Duplicate report.
    QCOMPARE(status.packetLoss().value("out").toInt(), 990);
    feed(3, 1000, 2990, 0, 10, 50);  // One source report missing: cumulative delta retains delivery.
    QCOMPARE(status.packetLoss().value("out").toInt(), 2990);
    QCOMPARE(status.packetLoss().value("lost").toInt(), 10);
    QCOMPARE(status.packetLoss().value("level").toInt(), 3);
    status._lossSamples.first().received -= 3000;
    status._refreshLoss();
    QCOMPARE(status.packetLoss().value("percent").toDouble(), 0.0);
    feed(4, 10, 10, 10, 10);  // Counter reset: do not mix service lifetimes.
    QCOMPARE(status.packetLoss().value("percent").toDouble(), 50.0);
    QCOMPARE(status.packetLoss().value("level").toInt(), 0);
    feed(5, 0, 10, 0, 10);
    QVERIFY(!status.packetLoss().value("hasData").toBool());
    feed(6, 100, 110, 0, 10);
    QVERIFY(status.packetLoss().value("hasData").toBool());
    status._lastLossReceived -= status._freshness;
    status._refreshLoss();
    QVERIFY(!status.packetLoss().value("hasData").toBool());
    // Exercise source reports at display-rounding and colour boundaries.
    for (const auto& [lost, displayed] : QList<QPair<int, double>>{{0, 0},
                                                                   {1, 0.1},
                                                                   {23, 0.3},
                                                                   {490, 4.9},
                                                                   {491, 5},
                                                                   {1990, 19.9},
                                                                   {1991, 20},
                                                                   {4990, 49.9},
                                                                   {4991, 50},
                                                                   {10000, 100}}) {
        WfbLinkStatus rounded;
        auto message = QJsonDocument::fromJson(rx(-25)).object();
        message["timestamp"] = 1;
        auto packets = message["packets"].toObject();
        packets["out"] = QJsonArray{10000 - lost, 10000 - lost};
        packets["lost"] = QJsonArray{lost, lost};
        message["packets"] = packets;
        rounded._consume(QJsonDocument(message).toJson());
        rounded._refreshLoss();
        QVERIFY(rounded.packetLoss().value("hasData").toBool());
        QCOMPARE(rounded.packetLoss().value("displayPercent").toDouble(), displayed);
        QCOMPARE(rounded.packetLoss().value("level").toInt(), WfbLinkStatus::_levelForLoss(displayed));
    }
    for (const auto& [loss, level] : QList<QPair<double, int>>{
             {0, 4}, {0.1, 3}, {4.9, 3}, {5, 2}, {19.9, 2}, {20, 1}, {49.9, 1}, {50, 0}, {100, 0}}) {
        QCOMPARE(WfbLinkStatus::_levelForLoss(loss), level);
    }
}

UT_REGISTER_TEST_LIGHTWEIGHT(WfbLinkStatusTest, TestLabel::Unit, TestLabel::Comms)
