import QtQuick
import QtQuick.Layouts
import QGroundControl
import QGroundControl.Controls

Item {
    id: control
    objectName: "packetLossIndicator"
    anchors.top: parent.top
    anchors.bottom: parent.bottom
    anchors.topMargin: -2
    anchors.bottomMargin: -4
    width: ScreenTools.defaultFontPixelWidth * 13
    property bool showIndicator: true
    property var linkStatus: WfbLinkStatus
    readonly property var metrics: linkStatus.packetLoss
    readonly property bool valid: metrics.hasData === true
    readonly property int level: valid ? metrics.level : -1
    readonly property real lossPercent: Math.max(0, Math.min(100, Number(metrics.displayPercent)))
    readonly property Component indicatorPage: detailsPage
    Image {
        id: lossIcon
        objectName: "packetLossIcon"
        anchors.top: nameLabel.bottom
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.horizontalCenterOffset: -(valueLabel.implicitWidth + 4) / 2
        width: height * 1.2
        fillMode: Image.PreserveAspectFit
        sourceSize.height: height * 2
        source: "/qmlimages/PacketLoss" + (control.level < 0 ? "Unknown" : control.level) + ".svg"
    }
    QGCLabel {
        id: valueLabel
        objectName: "packetLossValue"
        anchors.left: lossIcon.right
        anchors.leftMargin: 4
        horizontalAlignment: Text.AlignHCenter
        anchors.verticalCenter: lossIcon.verticalCenter
        text: control.valid ? "- " + control.lossPercent.toFixed(1) + "%" : "—"
        font.pointSize: ScreenTools.smallFontPointSize
    }
    QGCLabel {
        id: nameLabel
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        transform: Translate { y: -nameLabel.implicitHeight / 2 }
        text: "Packet Loss"
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
                heading: "Packet Loss · видео на выносе"
                QGCLabel { text: control.metrics.status || "Ожидание видео RADXA" }
                LabelledLabel { label: "Поток:"; labelText: "video_repiter rx · UDP 5602" }
                LabelledLabel { label: "Потери после FEC:"; labelText: valueLabel.text }
                LabelledLabel { label: "Доставлено / потеряно:"; labelText: control.valid ? control.metrics.out + " / " + control.metrics.lost : "—" }
                LabelledLabel { label: "Восстановлено FEC:"; labelText: control.valid ? String(control.metrics.fec) : "—" }
                LabelledLabel { label: "Интервал источника:"; labelText: control.metrics.interval ? control.metrics.interval + " с" : "—" }
                QGCLabel {
                    text: "Оценка: 100 × lost / (out + lost), окно 3 с.\nОкругление вверх до десятых; цвет — по показанному значению.\nЗелёный: 0%; салатовый: >0–<5%; жёлтый: 5–<20%; оранжевый: 20–<50%; красный: ≥50%.\nЧем больше потерь, тем меньше столбиков.\nСлужебные FEC-пакеты могут влиять на точность оценки. Нет данных — не 0% потерь."
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                    Layout.maximumWidth: ScreenTools.defaultFontPixelWidth * 52
                    font.pointSize: ScreenTools.smallFontPointSize
                }
            }
        }
    }
}
