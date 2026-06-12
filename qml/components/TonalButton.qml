import QtQuick
import QtQuick.Controls

Button {
    id: root

    property color buttonColor: root.highlighted ? Theme.primary : Theme.primaryContainer
    property color textColor: root.highlighted ? Theme.textOnPrimary : Theme.primary

    implicitHeight: 36
    font.pixelSize: Theme.fontBody
    font.bold: true

    background: Rectangle {
        radius: Theme.radiusPill
        color: !root.enabled ? Theme.surfaceContainerHigh
             : root.down ? Qt.darker(root.buttonColor, 1.08)
             : root.hovered ? Qt.lighter(root.buttonColor, 1.04)
             : root.buttonColor
        border.color: root.highlighted ? Theme.primary : "transparent"
        border.width: root.highlighted ? 1 : 0

        Behavior on color {
            ColorAnimation {
                duration: 140
                easing.type: Easing.OutCubic
            }
        }
    }

    contentItem: Text {
        text: root.text
        color: root.enabled ? root.textColor : Theme.textMuted
        font: root.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight

        Behavior on color {
            ColorAnimation {
                duration: 140
                easing.type: Easing.OutCubic
            }
        }
    }
}
