import QtQuick
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls

Item {
    id: control
    objectName: "radxaRssiIndicator"
    anchors.top: parent.top
    anchors.bottom: parent.bottom
    anchors.topMargin: -2
    anchors.bottomMargin: -4
    width: ScreenTools.defaultFontPixelWidth * 13
    property bool showIndicator: true
    property string deviceName: "Вынос"
    property Component indicatorPage: detailsPage
    property var linkStatus: WfbLinkStatus
    readonly property var metrics: linkStatus.values
    readonly property color signalColor: ["#ff2020", "#ff7900", "#ffe600", "#91ed00", "#00d52c"][Math.max(0, linkStatus.level)]

    Image {
        id: wifiIcon
        objectName: "radxaRssiIcon"
        anchors.top: nameLabel.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.leftMargin: ScreenTools.defaultFontPixelWidth * 0.5
        width: height * 1.2
        fillMode: Image.PreserveAspectFit
        sourceSize.height: height * 2
        source: "/qmlimages/WfbRssi" + (control.linkStatus.level < 0 ? "Unknown" : control.linkStatus.level) + ".svg"
    }

    QGCLabel {
        id: rssiLabel
        objectName: "radxaRssiLabel"
        anchors.left: wifiIcon.right
        anchors.leftMargin: 4
        anchors.right: parent.right
        anchors.rightMargin: ScreenTools.defaultFontPixelWidth * 0.5
        horizontalAlignment: Text.AlignLeft
        anchors.verticalCenter: wifiIcon.verticalCenter
        anchors.verticalCenterOffset: -implicitHeight / 2
        text: control.linkStatus.hasRssi ? "RSSI - " + Math.abs(control.linkStatus.rssi) + " dBm" : "RSSI —"
        font.pointSize: ScreenTools.smallFontPointSize
    }

    QGCLabel {
        id: snrLabel
        objectName: "radxaSnrLabel"
        anchors.top: rssiLabel.bottom
        anchors.left: rssiLabel.left
        anchors.right: rssiLabel.right
        horizontalAlignment: Text.AlignLeft
        text: control.linkStatus.hasRssi && typeof control.metrics.snr === "number" && isFinite(control.metrics.snr)
              ? "SNR - " + Math.round(control.metrics.snr) + " dB" : "SNR -"
        font.pointSize: ScreenTools.smallFontPointSize
    }

    QGCLabel {
        id: nameLabel
        objectName: "rssiDeviceName"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        transform: Translate { y: -nameLabel.implicitHeight / 2 }
        text: control.deviceName
        font.bold: true
        font.pointSize: ScreenTools.smallFontPointSize
    }

    QGCMouseArea {
        fillItem: control
        onClicked: mainWindow.showIndicatorDrawer(control.indicatorPage, control)
    }

    Component {
        id: detailsPage
        ToolIndicatorPage {
            showExpand: false
            contentComponent: SettingsGroupLayout {
                heading: "Вынос · приём на RADXA"
                QGCLabel {
                    text: control.linkStatus.statusText
                    color: control.linkStatus.level < 0 ? "#90969c" : control.signalColor
                }
                LabelledLabel { label: "RSSI:"; labelText: control.linkStatus.hasRssi ? control.linkStatus.rssi + " dBm" : "—" }
                LabelledLabel { label: "Поток:"; labelText: control.metrics.stream || "—" }
                QGCLabel {
                    text: control.metrics.antennas || "Нет измерений антенн"
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                LabelledLabel {
                    label: "Радиоканал:"
                    labelText: control.linkStatus.hasRssi ? control.metrics.frequency + " MHz · " + control.metrics.bandwidth + " MHz · MCS " + control.metrics.mcs : "—"
                }
                LabelledLabel { label: "FEC k/n:"; labelText: control.metrics.fecScheme || "—" }
                LabelledLabel {
                    label: "Принято за интервал:"
                    labelText: control.linkStatus.hasRssi ? control.metrics.packets + " пак. / " + control.metrics.interval + " с" : "—"
                }
                LabelledLabel {
                    label: "Потеряно:"
                    labelText: control.linkStatus.hasRssi ? control.metrics.lost + " за интервал · всего " + control.metrics.lostTotal : "—"
                }
                LabelledLabel {
                    label: "Восстановлено FEC:"
                    labelText: control.linkStatus.hasRssi ? control.metrics.fec + " за интервал · всего " + control.metrics.fecTotal : "—"
                }
                LabelledLabel {
                    label: "Выходной поток:"
                    labelText: control.linkStatus.hasRssi ? Number(control.metrics.mbps).toFixed(2) + " Мбит/с" : "—"
                }
                QGCLabel {
                    text: "Цвет — только RSSI лучшей принимающей антенны.\nГраницы шкалы: −85 / −75 / −65 / −55 dBm."
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                    Layout.maximumWidth: ScreenTools.defaultFontPixelWidth * 52
                    font.pointSize: ScreenTools.smallFontPointSize
                }
            }
        }
    }
}
