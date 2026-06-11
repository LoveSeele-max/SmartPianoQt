import QtQuick
import QtQuick.Controls

Button {
    id: root

    property color buttonColor: Theme.primary
    property color textColor: Theme.textOnPrimary

    implicitHeight: 38
    font.pixelSize: Theme.fontBody
    font.bold: true

    background: Rectangle {
        radius: Theme.radiusPill
        color: !root.enabled ? Theme.surfaceContainerHigh
             : root.down ? Qt.darker(root.buttonColor, 1.12)
             : root.hovered ? Qt.lighter(root.buttonColor, 1.06)
             : root.buttonColor
        border.color: "transparent"
    }

    contentItem: Text {
        text: root.text
        color: root.enabled ? root.textColor : Theme.textMuted
        font: root.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
