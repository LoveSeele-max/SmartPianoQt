import QtQuick
import QtQuick.Layouts

Rectangle {
    id: keyboard
    Layout.fillWidth: true
    Layout.preferredHeight: 168
    color: Theme.surface
    radius: Theme.radiusLarge
    border.color: Theme.outline
    clip: true

    readonly property int firstMidi: 36
    readonly property int lastMidi: 96
    readonly property int whiteKeyCount: 36
    readonly property int inset: 10
    property var activeSet: ({})
    property var expectedSet: ({})
    property var expectedLeftSet: ({})
    property var expectedRightSet: ({})
    readonly property real splitX: inset + keyX(piano.handSplitMidi, (width - inset * 2) / whiteKeyCount)

    function rebuildActiveSet() {
        var next = {}
        for (var i = 0; i < piano.activeNotes.length; ++i)
            next[piano.activeNotes[i]] = true
        activeSet = next
    }

    function rebuildExpectedSet() {
        var next = {}
        var left = {}
        var right = {}
        for (var i = 0; i < piano.expectedNotes.length; ++i)
            next[piano.expectedNotes[i].midi] = true
        for (var l = 0; l < piano.expectedLeftNotes.length; ++l)
            left[piano.expectedLeftNotes[l].midi] = true
        for (var r = 0; r < piano.expectedRightNotes.length; ++r)
            right[piano.expectedRightNotes[r].midi] = true
        expectedSet = next
        expectedLeftSet = left
        expectedRightSet = right
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
        function onHandPracticeChanged() {
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
            property real whiteW: (keyboard.width - keyboard.inset * 2) / whiteKeyCount
            property bool active: activeSet[midi] === true
            property bool expectedLeft: expectedLeftSet[midi] === true && piano.mode !== "auto"
            property bool expectedRight: expectedRightSet[midi] === true && piano.mode !== "auto"
            property bool expected: expectedSet[midi] === true && piano.mode !== "auto"
            property color expectedAccent: expectedLeft ? Theme.loop : Theme.primary
            property color expectedFill: expectedLeft ? Theme.loopContainer : Theme.primaryContainer

            z: black ? 4 : 1
            x: keyboard.inset + keyX(midi, whiteW)
            y: keyboard.inset
            width: black ? Math.max(12, whiteW * 0.64) : whiteW
            height: black ? (keyboard.height - keyboard.inset * 2) * 0.60 : keyboard.height - keyboard.inset * 2
            radius: black ? 4 : 7
            visible: midi >= firstMidi && midi <= lastMidi
            border.color: keyItem.expected ? keyItem.expectedAccent
                         : keyItem.active ? Theme.activeKey
                         : black ? "#3C4043"
                         : Theme.outline
            border.width: 1

            gradient: Gradient {
                orientation: keyItem.black ? Gradient.Horizontal : Gradient.Vertical
                GradientStop {
                    position: 0.0
                    color: keyItem.black ? "#202124"
                                         : Theme.darkMode ? "#FFFDF4" : Theme.surface
                }
                GradientStop {
                    position: keyItem.black ? 0.35 : 0.18
                    color: keyItem.black ? "#111315"
                                         : Theme.darkMode ? "#F8F3E5" : "#F4F1E7"
                }
                GradientStop {
                    position: keyItem.black ? 1.0 : 1.0
                    color: keyItem.black ? "#2F3136"
                                         : Theme.darkMode ? "#EFE8D6" : "#E8E3D3"
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
                color: keyItem.expected ? keyItem.expectedFill : Theme.activeKeyContainer
                opacity: keyItem.expected ? 0.78 : 1.0
                border.color: keyItem.expected ? keyItem.expectedAccent : Theme.activeKey
                border.width: 2
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: keyItem.black ? Math.max(18, parent.height * 0.24)
                                                     : Math.max(18, parent.height * 0.18)
                text: noteName(keyItem.midi)
                color: keyItem.active || keyItem.expected ? Theme.textPrimary
                      : keyItem.black ? "#B6BBC5"
                      : Theme.textSecondary
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

    Rectangle {
        visible: piano.mode !== "auto"
        z: 20
        x: Math.max(keyboard.inset, Math.min(keyboard.width - keyboard.inset, keyboard.splitX))
        y: keyboard.inset
        width: 1
        height: keyboard.height - keyboard.inset * 2
        color: Theme.outline
        opacity: 0.7
    }

    Rectangle {
        visible: piano.mode !== "auto"
        z: 21
        x: keyboard.inset + 8
        y: keyboard.inset + 8
        width: 42
        height: 24
        radius: Theme.radiusPill
        color: Theme.loopContainer
        border.color: piano.handPracticeEnabled && piano.handPracticeSide === "left" ? Theme.loop : "transparent"

        Text {
            anchors.centerIn: parent
            text: "左"
            color: Theme.loop
            font.pixelSize: Theme.fontCaption
            font.bold: true
        }
    }

    Rectangle {
        visible: piano.mode !== "auto"
        z: 21
        x: Math.max(keyboard.inset + 58, Math.min(keyboard.width - keyboard.inset - width - 8, keyboard.splitX + 8))
        y: keyboard.inset + 8
        width: 42
        height: 24
        radius: Theme.radiusPill
        color: Theme.primaryContainer
        border.color: piano.handPracticeEnabled && piano.handPracticeSide === "right" ? Theme.primary : "transparent"

        Text {
            anchors.centerIn: parent
            text: "右"
            color: Theme.primary
            font.pixelSize: Theme.fontCaption
            font.bold: true
        }
    }
}
