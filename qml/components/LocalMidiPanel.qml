import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    Layout.fillWidth: true
    spacing: 8

    RowLayout {
        Layout.fillWidth: true

        Label {
            text: "Local MIDI"
            color: "#e4e4e7"
            font.pixelSize: 13
            font.bold: true
            Layout.fillWidth: true
        }

        Button {
            text: "刷新"
            Layout.preferredWidth: 58
            onClicked: piano.refreshLocalMidiLibrary()
        }

        Button {
            text: "目录"
            Layout.preferredWidth: 58
            onClicked: piano.openLocalMidiLibrary()
        }
    }

    Label {
        Layout.fillWidth: true
        text: piano.localMidiLibraryPath
        color: "#71717a"
        font.pixelSize: 10
        elide: Text.ElideMiddle
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 150
        radius: 7
        color: "#111113"
        border.color: "#2f3036"
        clip: true

        ListView {
            id: localMidiList
            anchors.fill: parent
            anchors.margins: 4
            model: piano.localMidiFiles
            spacing: 3
            boundsBehavior: Flickable.StopAtBounds

            delegate: Rectangle {
                required property var modelData
                required property int index

                width: localMidiList.width
                height: 34
                radius: 5
                color: mouseArea.containsMouse ? "#27272a" : "#18181b"
                border.color: "#27272a"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    spacing: 8

                    Label {
                        text: modelData.fileName
                        color: "#e4e4e7"
                        font.pixelSize: 12
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }

                    Label {
                        text: modelData.sizeKb + " KB"
                        color: "#71717a"
                        font.pixelSize: 10
                    }
                }

                MouseArea {
                    id: mouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: piano.loadLocalMidi(index)
                }
            }

            Label {
                anchors.centerIn: parent
                visible: piano.localMidiFiles.length === 0
                text: "没有 MIDI 文件"
                color: "#71717a"
                font.pixelSize: 12
            }
        }
    }
}
