import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

MaterialCard {
    Layout.preferredWidth: 300
    Layout.minimumWidth: 280
    Layout.fillHeight: true
    padding: Theme.gapMd

    ScrollView {
        id: controlScroll
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            width: controlScroll.availableWidth
            spacing: Theme.gapMd

            Label {
                text: "Control"
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSection
                font.bold: true
            }

            PlaybackControls {}
            PracticeStatusPanel {}
            PracticeReportPanel {}

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: Theme.outline
            }

            MidiInputPanel {}
            LocalMidiPanel {}

            Item { height: 1 }
        }
    }
}
