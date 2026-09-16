#include "DualJoystickIndicatorTest.h"

#include <QtCore/QDir>
#include <QtGui/QColor>
#include <QtGui/QImage>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlEngine>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>

#include "ColoredSvgImageProvider.h"
#include "ControlBridgeStatus.h"

void DualJoystickIndicatorTest::_connectionStatesWithoutVehicle()
{
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral("qrc:/qml"));
    engine.addImageProvider(QLatin1String(ColoredSvgImageProvider::ProviderId), new ColoredSvgImageProvider());
    QQuickWindow window;
    window.resize(240, 64);
    QQmlComponent component(&engine);
    component.setData(R"QML(
        import QtQuick
        import QGroundControl.Toolbar

        Rectangle {
            id: testRoot
            width: 240
            height: 64
            color: "#303030"
            property alias names: joystickManager.availableJoystickNames
            property alias bridgeState: bridge.state
            property bool vehicleConnected: false
            property alias controlEnabled: joystickManager.activeJoystickEnabledForActiveVehicle
            QtObject {
                id: globals
                property var activeVehicle: testRoot.vehicleConnected ? ({}) : null
            }
            QtObject { id: tx12; property string name: "OpenTX Radiomaster TX12 Joystick" }
            QtObject {
                id: joystickManager
                property var availableJoystickNames: []
                property var activeJoystick: availableJoystickNames.indexOf(tx12.name) !== -1 ? tx12 : null
                property bool activeJoystickEnabledForActiveVehicle: false
                function joystickByName(name) { return tx12 }
            }
            QtObject {
                id: bridge
                property int state: 0
                property string detail: "Test bridge"
            }
            DualJoystickIndicator {
                anchors.horizontalCenter: parent.horizontalCenter
                bridgeStatus: bridge
            }
        }
    )QML",
                      QUrl(QStringLiteral("qrc:/DualJoystickIndicatorTest.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QScopedPointer<QObject> root(component.create());
    QVERIFY2(root, qPrintable(component.errorString()));
    auto* item = qobject_cast<QQuickItem*>(root.data());
    QVERIFY(item);
    item->setParentItem(window.contentItem());
    window.show();
    auto* tx12 = root->findChild<QQuickItem*>(QStringLiteral("tx12ConnectionIndicator"));
    auto* pocket = root->findChild<QQuickItem*>(QStringLiteral("pocketConnectionIndicator"));
    QVERIFY(tx12);
    QVERIFY(pocket);
    QVERIFY(tx12->isVisible());
    QVERIFY(pocket->isVisible());
    QCOMPARE(tx12->property("displayName").toString(), QStringLiteral("ТХ-12"));
    QCOMPARE(pocket->property("displayName").toString(), QStringLiteral("Pocket"));
    for (auto* indicator : {tx12, pocket}) {
        const QUrl source = indicator->property("iconSource").toUrl();
        QCOMPARE(source, QUrl(QStringLiteral("/qmlimages/Joystick.png")));
        QVERIFY(indicator->property("showLabel").toBool());
        QVERIFY(!indicator->property("framedIcon").toBool());
        QVERIFY(!indicator->property("iconWhiteMask").toBool());
        int iconCount = 0;
        for (auto* child : indicator->findChildren<QQuickItem*>()) {
            if (child->property("source").toUrl() == source) {
                ++iconCount;
                QTRY_COMPARE_WITH_TIMEOUT(child->property("status").toInt(), 1, TestTimeout::mediumMs());
            }
        }
        QCOMPARE(iconCount, 1);
    }
    const QColor disconnectedColor = tx12->property("indicatorColor").value<QColor>();
    const QColor unknownColor = pocket->property("indicatorColor").value<QColor>();

    const QString tx12Name = QStringLiteral("OpenTX Radiomaster TX12 Joystick");
    const QString pocketName = QStringLiteral("EdgeTX Radiomaster Pocket Joystick");
    QVERIFY(root->setProperty("names", QStringList{pocketName, QStringLiteral("Unrelated TX12 Controller")}));
    QCOMPARE(tx12->property("indicatorColor").value<QColor>(), disconnectedColor);
    QVERIFY(root->setProperty("names", QStringList{tx12Name, pocketName}));
    QTRY_VERIFY(tx12->property("indicatorColor").value<QColor>() != disconnectedColor);
    const QColor readyColor = tx12->property("indicatorColor").value<QColor>();
    QVERIFY(root->setProperty("controlEnabled", true));
    QVERIFY(root->setProperty("vehicleConnected", true));
    QCOMPARE(tx12->property("indicatorColor").value<QColor>(), readyColor);
    QVERIFY(root->setProperty("bridgeState", int(ControlBridgeStatus::Ready)));
    QTRY_COMPARE(pocket->property("indicatorColor").value<QColor>(), readyColor);

    // Optional visual artifact from the actual QGC QML, for local review.
    const QString screenshotDir = qEnvironmentVariable("QGC_INDICATOR_SCREENSHOT_DIR");
    if (!screenshotDir.isEmpty()) {
        QImage image;
        QTRY_VERIFY_WITH_TIMEOUT(!(image = window.grabWindow()).isNull(), TestTimeout::mediumMs());
        QVERIFY(QDir().mkpath(screenshotDir));
        QVERIFY(image.save(screenshotDir + QStringLiteral("/controllers-ready.png")));
    }

    QVERIFY(root->setProperty("names", QStringList{tx12Name, tx12Name + QStringLiteral(" #2"), pocketName}));
    QTRY_COMPARE(tx12->property("connectionStatus").toString(), QStringLiteral("Multiple controllers"));
    QVERIFY(root->setProperty("names", QStringList{pocketName, tx12Name}));
    QTRY_COMPARE(tx12->property("indicatorColor").value<QColor>(), readyColor);
    QVERIFY(root->setProperty("names", QStringList{pocketName}));
    QTRY_COMPARE(tx12->property("indicatorColor").value<QColor>(), disconnectedColor);
    QVERIFY(tx12->isVisible());
    QVERIFY(root->setProperty("names", QStringList{tx12Name, pocketName}));
    QTRY_COMPARE(tx12->property("indicatorColor").value<QColor>(), readyColor);
    QVERIFY(root->setProperty("controlEnabled", false));
    QTRY_COMPARE(tx12->property("connectionStatus").toString(), QStringLiteral("Connected; control disabled"));
    QVERIFY(root->setProperty("bridgeState", int(ControlBridgeStatus::Unknown)));
    QTRY_COMPARE_WITH_TIMEOUT(pocket->property("indicatorColor").value<QColor>(), unknownColor,
                              TestTimeout::mediumMs());
    QVERIFY(pocket->isVisible());
}

void DualJoystickIndicatorTest::_rssiStates()
{
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral("qrc:/qml"));
    QQmlComponent component(&engine);
    component.setData(R"QML(
        import QtQuick
        import QGroundControl.Toolbar
        Rectangle {
            width: 160; height: 48
            property alias level: model.level
            QtObject {
                id: model
                property int level: -1
                property bool hasRssi: level >= 0
                property int rssi: -100 + level * 15
                property string statusText: "Test RSSI"
                property var values: ({stream: "video_repiter rx", antennas: "Антенна 0: -40 dBm · SNR 13 dB\nАнтенна 1: -44 dBm · SNR 12 dB",
                    snr: 13, frequency: 5200, bandwidth: 40, mcs: 3, fecScheme: "4/6", packets: 650,
                    interval: 1, lost: 0, lostTotal: 10292, fec: 21, fecTotal: 195818, mbps: 4.26})
            }
            RadxaRSSIIndicator { linkStatus: model }
        }
    )QML",
                      QUrl(QStringLiteral("qrc:/RssiTest.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QScopedPointer<QObject> root(component.create());
    QVERIFY2(root, qPrintable(component.errorString()));
    auto* icon = root->findChild<QQuickItem*>(QStringLiteral("radxaRssiIcon"));
    auto* label = root->findChild<QQuickItem*>(QStringLiteral("radxaRssiLabel"));
    QVERIFY(icon);
    QVERIFY(label);
    auto* snr = root->findChild<QQuickItem*>(QStringLiteral("radxaSnrLabel"));
    QVERIFY(snr);
    for (int level = -1; level <= 4; ++level) {
        QVERIFY(root->setProperty("level", level));
        QCOMPARE(snr->property("text").toString(), level < 0 ? QStringLiteral("SNR -") : QStringLiteral("SNR - 13 dB"));
        const auto filename =
            QStringLiteral("WfbRssi%1.svg").arg(level < 0 ? QStringLiteral("Unknown") : QString::number(level));
        QVERIFY(icon->property("source").toUrl().toString().endsWith(filename));
        QTRY_COMPARE(icon->property("status").toInt(), 1);
        QCOMPARE(label->property("text").toString(),
                 level < 0 ? QStringLiteral("RSSI —") : QStringLiteral("RSSI - %1 dBm").arg(100 - level * 15));
    }
    auto* indicator = root->findChild<QQuickItem*>(QStringLiteral("radxaRssiIndicator"));
    QVERIFY(indicator);
    auto* details = indicator->property("indicatorPage").value<QQmlComponent*>();
    QVERIFY(details);
    QScopedPointer<QObject> page(details->create(details->creationContext()));
    QVERIFY2(page, qPrintable(details->errorString()));
    QStringList texts;
    for (auto* child : page->findChildren<QObject*>()) {
        if (child->property("text").isValid())
            texts.append(child->property("text").toString());
    }
    QVERIFY(texts.contains(QStringLiteral("video_repiter rx")));
    QVERIFY(texts.contains(QStringLiteral("5200 MHz · 40 MHz · MCS 3")));
    QVERIFY(texts.contains(QStringLiteral("4/6")));
    QVERIFY(texts.contains(QStringLiteral("4.26 Мбит/с")));
    QVERIFY(!texts.join(' ').contains(QStringLiteral("undefined")));
    const QString screenshotDir = qEnvironmentVariable("QGC_INDICATOR_SCREENSHOT_DIR");
    if (!screenshotDir.isEmpty()) {
        QQuickWindow window;
        window.setColor(QColor(QStringLiteral("#303030")));
        auto* pageItem = qobject_cast<QQuickItem*>(page.data());
        QVERIFY(pageItem);
        pageItem->setParentItem(window.contentItem());
        QTRY_VERIFY(pageItem->implicitWidth() > 0 && pageItem->implicitHeight() > 0);
        pageItem->setSize(QSizeF(pageItem->implicitWidth(), pageItem->implicitHeight()));
        pageItem->setPosition(QPointF(12, 12));
        window.resize(qCeil(pageItem->width()) + 24, qCeil(pageItem->height()) + 24);
        window.show();
        QImage image;
        QTRY_VERIFY_WITH_TIMEOUT(!(image = window.grabWindow()).isNull(), TestTimeout::mediumMs());
        QVERIFY(QDir().mkpath(screenshotDir));
        QVERIFY(image.save(screenshotDir + QStringLiteral("/rssi-details.png")));
        pageItem->setParentItem(nullptr);
    }
}

void DualJoystickIndicatorTest::_repeaterAndLossStates()
{
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral("qrc:/qml"));
    QQmlComponent component(&engine);
    component.setData(R"QML(
        import QtQuick
        import QGroundControl.Toolbar
        Rectangle {
            width: 400; height: 64
            property alias lossLevel: model.lossLevel
            property alias lossPercent: model.lossPercent
            QtObject {
                id: model
                property int lossLevel: -1
                property real lossPercent: 5
                property int level: 4
                property bool hasRssi: true
                property int rssi: -31
                property string statusText: "Приём IP-туннеля"
                property var values: ({stream: "ip_tunnel rx", streams: [
                    {stream: "MAVLink_uplink rx", fresh: true, hasRssi: false, out: 0, lost: 0, fec_rec: 0, interval: 1},
                    {stream: "ip_tunnel rx", fresh: true, hasRssi: true, antennas: "Антенна 1: -31 dBm", out: 2, lost: 0, fec_rec: 0, interval: 1}]})
                property var packetLoss: ({hasData: lossLevel >= 0, level: lossLevel, percent: lossPercent, displayPercent: Math.ceil(lossPercent * 10) / 10, out: 95, lost: 5, fec: 10, interval: 1, status: "Оценка потерь"})
            }
            RpiRSSIIndicator { linkStatus: model }
            PacketLossIndicator { x: 200; linkStatus: model }
        }
    )QML",
                      QUrl(QStringLiteral("qrc:/RepeaterLossTest.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QScopedPointer<QObject> root(component.create());
    QVERIFY2(root, qPrintable(component.errorString()));
    auto* rpi = root->findChild<QQuickItem*>(QStringLiteral("rpiRssiIndicator"));
    auto* loss = root->findChild<QQuickItem*>(QStringLiteral("packetLossIndicator"));
    QVERIFY(rpi);
    QVERIFY(loss);
    QCOMPARE(rpi->property("deviceName").toString(), QStringLiteral("Ретранслятор"));
    auto* icon = loss->findChild<QQuickItem*>(QStringLiteral("packetLossIcon"));
    auto* value = loss->findChild<QQuickItem*>(QStringLiteral("packetLossValue"));
    QVERIFY(icon);
    QVERIFY(value);
    for (int level = -1; level <= 4; ++level) {
        QVERIFY(root->setProperty("lossLevel", level));
        QTRY_COMPARE(icon->property("status").toInt(), 1);
        QVERIFY(icon->property("source").toUrl().toString().endsWith(
            QStringLiteral("PacketLoss%1.svg").arg(level < 0 ? QStringLiteral("Unknown") : QString::number(level))));
        QCOMPARE(value->property("text").toString(), level < 0 ? QStringLiteral("—") : QStringLiteral("- 5.0%"));
    }
    const QList<QPair<double, QString>> lossLabels{{0, QStringLiteral("- 0.0%")},    {0.01, QStringLiteral("- 0.1%")},
                                                   {0.23, QStringLiteral("- 0.3%")}, {0.99, QStringLiteral("- 1.0%")},
                                                   {1, QStringLiteral("- 1.0%")},    {12.6, QStringLiteral("- 12.6%")},
                                                   {100, QStringLiteral("- 100.0%")}};
    for (const auto& [percent, expected] : lossLabels) {
        QVERIFY(root->setProperty("lossPercent", percent));
        QCOMPARE(value->property("text").toString(), expected);
    }
    for (auto* indicator : {rpi, loss}) {
        auto* details = indicator->property("indicatorPage").value<QQmlComponent*>();
        QVERIFY(details);
        QScopedPointer<QObject> page(details->create(details->creationContext()));
        QVERIFY2(page, qPrintable(details->errorString()));
        QStringList texts;
        for (auto* child : page->findChildren<QObject*>()) {
            if (child->property("text").isValid())
                texts.append(child->property("text").toString());
        }
        QVERIFY(!texts.join(' ').contains(QStringLiteral("undefined")));
    }
}

UT_REGISTER_TEST(DualJoystickIndicatorTest, TestLabel::Unit, TestLabel::Joystick)
