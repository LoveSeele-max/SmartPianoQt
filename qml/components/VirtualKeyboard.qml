import QtQuick
import QtQuick.Layouts

Rectangle {
    id: keyboard
    Layout.fillWidth: true
    Layout.preferredHeight: 168
    color: "#111113"
    radius: 8
    border.color: "#2f3036"
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
            required property int index
            property int midi: firstMidi + index
            property bool black: isBlackMidi(midi)
            property real whiteW: keyboard.width / whiteKeyCount
            property bool active: activeSet[midi] === true
            property bool expected: expectedSet[midi] === true && piano.mode !== "auto"

            z: black ? 2 : 1
            x: keyX(midi, whiteW)
            y: 0
            width: black ? whiteW * 0.64 : whiteW
            height: black ? keyboard.height * 0.62 : keyboard.height - 10
            radius: black ? 4 : 5
            visible: midi >= firstMidi && midi <= lastMidi
            color: expected ? "#facc15" : active ? "#14b8a6" : black ? "#18181b" : "#f8fafc"
            border.color: expected ? "#fef08a" : active ? "#99f6e4" : black ? "#3f3f46" : "#d4d4d8"
            border.width: expected || active ? 2 : 1

            Behavior on color {
                ColorAnimation { duration: 90 }
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: parent.black ? 8 : 12
                text: parent.black ? "" : noteName(parent.midi)
                color: parent.active || parent.expected ? "#042f2e" : "#52525b"
                font.pixelSize: 10
                font.bold: parent.active || parent.expected
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
