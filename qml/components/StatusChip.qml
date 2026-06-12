import QtQuick
import QtQuick.Controls

Rectangle {
    id: root

    property alias text: label.text
    property color chipColor: Theme.surfaceContainer
    property color textColor: Theme.textSecondary
    property int horizontalPadding: 10

    implicitWidth: label.implicitWidth + horizontalPadding * 2
    implicitHeight: 26
    radius: Theme.radiusPill
    color: chipColor

    Behavior on color {
        ColorAnimation {
            duration: 150
            easing.type: Easing.OutCubic
        }
    }

    Label {
        id: label
        anchors.fill: parent
        anchors.leftMargin: root.horizontalPadding
        anchors.rightMargin: root.horizontalPadding
        color: root.textColor
        font.pixelSize: Theme.fontCaption
        font.bold: true
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight

        Behavior on color {
            ColorAnimation {
                duration: 150
                easing.type: Easing.OutCubic
            }
        }
    }
}
