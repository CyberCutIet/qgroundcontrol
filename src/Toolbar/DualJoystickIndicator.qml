import QtQuick

import QGroundControl
import QGroundControl.Controls
import QGroundControl.Toolbar

Item {
    id: control
    width: indicators.width
    anchors.top: parent.top
    anchors.bottom: parent.bottom
    property bool showIndicator: true

    readonly property var tx12Names: joystickManager.availableJoystickNames.filter(function(name) {
        // Exact USB product name, with only QGC's duplicate-device suffix allowed.
        return /^OpenTX Radiomaster TX12 Joystick(?: #[0-9]+)?$/.test(name)
    })
    readonly property var tx12: tx12Names.length === 1 ? joystickManager.joystickByName(tx12Names[0]) : null
    readonly property bool tx12Enabled: !!tx12 && tx12 === joystickManager.activeJoystick && joystickManager.activeJoystickEnabledForActiveVehicle

    QGCPalette { id: palette }
    property var bridgeStatus: ControlBridgeStatus

    Row {
        id: indicators
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        spacing: ScreenTools.defaultFontPixelWidth

        JoystickIndicator {
            objectName: "tx12ConnectionIndicator"
            displayName: "ТХ-12"
            joystick: control.tx12
            showIndicator: true
            indicatorColor: control.tx12Names.length > 1 ? palette.colorOrange
                : !control.tx12 ? palette.colorRed
                : globals.activeVehicle && !control.tx12Enabled ? palette.colorOrange : palette.colorGreen
            connectionStatus: control.tx12Names.length > 1 ? qsTr("Multiple controllers")
                : !control.tx12 ? qsTr("Disconnected")
                : globals.activeVehicle && !control.tx12Enabled ? qsTr("Connected; control disabled") : qsTr("Connected")
            connectionDetail: control.tx12Names.length > 1 ? qsTr("More than one TX12 was found.")
                : !control.tx12 ? qsTr("Connect TX12 by USB in joystick mode.")
                : qsTr("USB controller detected by QGC. Aircraft link status is shown separately.")
        }

        JoystickIndicator {
            objectName: "pocketConnectionIndicator"
            displayName: "Pocket"
            joystick: null
            externalControl: true
            showIndicator: true
            indicatorColor: bridgeStatus.state === ControlBridgeStatus.Ready ? palette.colorGreen
                : bridgeStatus.state === ControlBridgeStatus.Disconnected ? palette.colorRed
                : bridgeStatus.state === ControlBridgeStatus.Error ? palette.colorOrange : palette.colorGrey
            connectionStatus: bridgeStatus.state === ControlBridgeStatus.Ready ? qsTr("Connected")
                : bridgeStatus.state === ControlBridgeStatus.Disconnected ? qsTr("Disconnected")
                : bridgeStatus.state === ControlBridgeStatus.Error ? qsTr("Input error") : qsTr("No bridge status")
            connectionDetail: bridgeStatus.detail
        }
    }
}
