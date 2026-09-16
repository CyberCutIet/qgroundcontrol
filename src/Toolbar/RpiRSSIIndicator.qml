import QtQuick
import QtQuick.Layouts
import QGroundControl
import QGroundControl.Controls

RadxaRSSIIndicator {
    id: control
    objectName: "rpiRssiIndicator"
    deviceName: "Ретранслятор"
    linkStatus: RpiLinkStatus
    indicatorPage: Component {
        ToolIndicatorPage {
            showExpand: false
            contentComponent: SettingsGroupLayout {
                heading: "Ретранслятор · приём на RPi"
                QGCLabel {
                    text: control.linkStatus.statusText
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                    Layout.maximumWidth: ScreenTools.defaultFontPixelWidth * 52
                }
                LabelledLabel { label: "Источник RSSI:"; labelText: control.metrics.stream || "—" }
                Repeater {
                    model: control.metrics.streams || []
                    delegate: ColumnLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        QGCLabel {
                            text: modelData.stream === "MAVLink_uplink rx" ? "TX-12 / MAVLink" :
                                  modelData.stream === "controller_FPV rx" ? "Pocket" : "IP-туннель"
                            font.bold: true
                        }
                        QGCLabel {
                            text: !modelData.fresh ? "Нет свежих данных" :
                                  !modelData.hasRssi ? "Нет приёма" : modelData.antennas
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                        QGCLabel {
                            visible: modelData.fresh
                            text: modelData.fresh ? "Выдано: " + modelData.out + " · потеряно: " + modelData.lost +
                                  " · FEC: " + modelData.fec_rec + " / " + modelData.interval + " с" : ""
                            font.pointSize: ScreenTools.smallFontPointSize
                        }
                    }
                }
                QGCLabel {
                    text: "Цвет — по RSSI. Статистика доставляется отдельным потоком WFB №6.\nRSSI IP-туннеля не подтверждает работу пультов."
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                    Layout.maximumWidth: ScreenTools.defaultFontPixelWidth * 52
                    font.pointSize: ScreenTools.smallFontPointSize
                }
            }
        }
    }
}
