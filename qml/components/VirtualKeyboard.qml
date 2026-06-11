import QtQuick
import QtQuick.Layouts

Rectangle {
    id: keyboard
    Layout.fillWidth: true
    Layout.preferredHeight: 168
    color: "#050506"
    radius: 4
    border.color: "#0b0b0d"
    clip: true

    readonly property int firstMidi: 36
    readonly property int lastMidi: 96
    readonly property int whiteKeyCount: 36
    property var activeSet: ({})
    property var expectedSet: ({})

    function rebuildActiveSet() {
        var next = {}
        for (var i = 0; i < piano.activeNotes.length; ++i)
            next[piano.activeNotes[i]] = true
        activeSet = next
    }

    function rebuildExpectedSet() {
        var next = {}
        for (var i = 0; i < piano.expectedNotes.length; ++i)
            next[piano.expectedNotes[i].midi] = true
        expectedSet = next
    }

    function isBlackMidi(midi) {
        var pc = ((midi % 12) + 12) % 12
        return pc === 1 || pc === 3 || pc === 6 || pc === 8 || pc === 10
    }

    function noteName(midi) {
        var names = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
        return names[midi % 12] + (Math.floor(midi / 12) - 1)
    }

    function whiteIndexBefore(midi) {
        var count = 0
        for (var n = firstMidi; n < midi; ++n) {
            if (!isBlackMidi(n))
                ++count
        }
        return count
    }

    function keyX(midi, whiteW) {
        if (!isBlackMidi(midi))
            return whiteIndexBefore(midi) * whiteW
        return whiteIndexBefore(midi) * whiteW - whiteW * 0.32
    }

    Connections {
        target: piano
        function onActiveNotesChanged() {
            rebuildActiveSet()
        }
        function onPracticeChanged() {
            rebuildExpectedSet()
        }
        function onModeChanged() {
            rebuildExpectedSet()
        }
    }

    Component.onCompleted: {
        rebuildActiveSet()
        rebuildExpectedSet()
    }

    Repeater {
        model: lastMidi - firstMidi + 1
        delegate: Rectangle {
            id: keyItem
            required property int index
            property int midi: firstMidi + index
            property bool black: isBlackMidi(midi)
            property real whiteW: keyboard.width / whiteKeyCount
            property bool active: activeSet[midi] === true
            property bool expected: expectedSet[midi] === true && piano.mode !== "auto"

            z: black ? 4 : 1
            x: keyX(midi, whiteW)
            y: 0
            width: black ? Math.max(12, whiteW * 0.64) : whiteW
            height: black ? keyboard.height * 0.60 : keyboard.height
            radius: black ? 2 : 3
            visible: midi >= firstMidi && midi <= lastMidi
            border.color: black ? "#020203" : "#1c1c1a"
            border.width: 1

            gradient: Gradient {
                orientation: keyItem.black ? Gradient.Horizontal : Gradient.Vertical
                GradientStop {
                    position: 0.0
                    color: keyItem.black ? "#202124" : "#f4f1e7"
                }
                GradientStop {
                    position: keyItem.black ? 0.35 : 0.18
                    color: keyItem.black ? "#070708" : "#e4dfcf"
                }
                GradientStop {
                    position: keyItem.black ? 1.0 : 1.0
                    color: keyItem.black ? "#121316" : "#cfc8b8"
                }
            }

            Rectangle {
                visible: !keyItem.black
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                width: 2
                color: "#0000002f"
            }

            Rectangle {
                visible: !keyItem.black
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 5
                color: "#00000034"
            }

            Rectangle {
                visible: keyItem.black
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 8
                color: "#000000aa"
                radius: parent.radius
            }

            Rectangle {
                visible: keyItem.active || keyItem.expected
                anchors.fill: parent
                radius: parent.radius
                color: keyItem.expected ? "#facc1558" : "#14b8a658"
                border.color: keyItem.expected ? "#fef08a" : "#99f6e4"
                border.width: 2
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: keyItem.black ? Math.max(18, parent.height * 0.24)
                                                     : Math.max(18, parent.height * 0.18)
                text: noteName(keyItem.midi)
                color: keyItem.black ? "#7b7c82" : "#858576"
                font.pixelSize: keyItem.black ? 10 : 11
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
            }

            Behavior on color {
                ColorAnimation { duration: 90 }
            }

            MouseArea {
                anchors.fill: parent
                onPressed: piano.noteOn(parent.midi, 112)
                onReleased: piano.noteOff(parent.midi)
                onCanceled: piano.noteOff(parent.midi)
                onExited: {
                    if (pressed)
                        piano.noteOff(parent.midi)
                }
            }
        }
    }
}
