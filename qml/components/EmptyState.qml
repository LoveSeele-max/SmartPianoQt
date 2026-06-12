import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property string iconText: "•"
    property string title: ""
    property string detail: ""
    property string actionText: ""
    signal action()

    implicitHeight: actionText.length > 0 ? 116 : 92
    Layout.fillWidth: true

    RowLayout {
        anchors.fill: parent
        anchors.margins: Theme.gapMd
        spacing: Theme.gapMd

        Rectangle {
            Layout.preferredWidth: 44
            Layout.preferredHeight: 44
            radius: Theme.radiusPill
            color: Theme.surfaceContainerHigh

            Behavior on color {
                ColorAnimation {
                    duration: 160
                    easing.type: Easing.OutCubic
                }
            }

            Label {
                anchors.centerIn: parent
                text: root.iconText
                color: Theme.primary
                font.pixelSize: 20
                font.bold: true
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 3

            Label {
                Layout.fillWidth: true
                text: root.title
                color: Theme.textPrimary
                font.pixelSize: Theme.fontBody
                font.bold: true
                elide: Text.ElideRight
            }

            Label {
                Layout.fillWidth: true
                text: root.detail
                color: Theme.textMuted
                font.pixelSize: Theme.fontCaption
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }
        }

        TonalButton {
            visible: root.actionText.length > 0
            text: root.actionText
            Layout.preferredWidth: 76
            onClicked: root.action()
        }
    }
}
