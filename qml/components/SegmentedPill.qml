import QtQuick
import QtQuick.Layouts

Item {
    id: root

    property var options: []
    property var currentValue: ""
    signal selected(var value)

    implicitHeight: 36
    Layout.fillWidth: true

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusPill
        color: Theme.surfaceContainer
        border.color: Theme.darkMode ? "#2A3952" : "#E3E7ED"
        border.width: 1

        Behavior on color {
            ColorAnimation {
                duration: 150
                easing.type: Easing.OutCubic
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 3
        spacing: 3

        Repeater {
            model: root.options

            delegate: Rectangle {
                required property var modelData

                readonly property bool selected: String(modelData.value) === String(root.currentValue)

                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: Theme.radiusPill
                color: selected ? Theme.primary : "transparent"

                Behavior on color {
                    ColorAnimation {
                        duration: 150
                        easing.type: Easing.OutCubic
                    }
                }

                Text {
                    anchors.centerIn: parent
                    width: parent.width - 10
                    text: modelData.label
                    color: parent.selected ? Theme.textOnPrimary : Theme.textSecondary
                    font.pixelSize: Theme.fontCaption
                    font.bold: parent.selected
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

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.selected(modelData.value)
                }
            }
        }
    }
}
