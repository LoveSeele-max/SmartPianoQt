import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Frame {
    Layout.preferredWidth: 260
    Layout.fillHeight: true
    background: Rectangle {
        color: "#18181b"
        border.color: "#2f3036"
        radius: 8
    }

    ScrollView {
        id: controlScroll
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            width: controlScroll.availableWidth
            spacing: 12

            Label {
                text: "Control"
                color: "#e4e4e7"
                font.pixelSize: 15
                font.bold: true
            }

            PlaybackControls {}
            PracticeStatusPanel {}

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: "#2f3036"
            }

            MidiInputPanel {}
            LocalMidiPanel {}

            Item { height: 1 }
        }
    }
}
