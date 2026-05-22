import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import SmartPianoQt.Controls 1.0

ApplicationWindow {
    id: root
    width: 1180
    height: 760
    visible: true
    title: "SmartPianoQt"
    color: "#101114"

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

    FileDialog {
        id: openDialog
        title: "选择曲谱文件"
        nameFilters: ["曲谱文件 (*.json *.mid *.midi)", "JSON (*.json)", "MIDI (*.mid *.midi)", "所有文件 (*)"]
        onAccepted: piano.loadSheet(selectedFile)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                Label {
                    text: piano.songTitle
                    color: "#f4f4f5"
                    font.pixelSize: 24
                    font.bold: true
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                Label {
                    text: piano.statusMessage
                    color: "#a1a1aa"
                    font.pixelSize: 13
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }

            Button {
                text: piano.playing ? "暂停" : "播放"
                onClicked: piano.playPause()
            }

            Button {
                text: "停止"
                onClicked: piano.stop()
            }

            Button {
                text: "示例曲"
                onClicked: piano.loadDemoSong()
            }

            Button {
                text: "导入"
                onClicked: openDialog.open()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 14

            Frame {
                Layout.preferredWidth: 260
                Layout.fillHeight: true
                background: Rectangle {
                    color: "#18181b"
                    border.color: "#2f3036"
                    radius: 8
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 18

                    Label {
                        text: "Control"
                        color: "#e4e4e7"
                        font.pixelSize: 15
                        font.bold: true
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Label {
                            text: "Mode"
                            color: "#a1a1aa"
                            font.pixelSize: 12
                        }

                        ComboBox {
                            id: modeBox
                            Layout.fillWidth: true
                            textRole: "label"
                            valueRole: "value"
                            model: [
                                { label: "自动播放", value: "auto" },
                                { label: "练习等待", value: "practice" }
                            ]
                            currentIndex: piano.mode === "practice" ? 1 : 0
                            onActivated: piano.mode = currentValue
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        RowLayout {
                            Layout.fillWidth: true
                            Label {
                                text: "BPM"
                                color: "#a1a1aa"
                                font.pixelSize: 12
                                Layout.fillWidth: true
                            }
                            Label {
                                text: piano.bpm
                                color: "#f4f4f5"
                                font.pixelSize: 14
                                font.bold: true
                            }
                        }

                        Slider {
                            Layout.fillWidth: true
                            from: 40
                            to: 220
                            stepSize: 1
                            value: piano.bpm
                            onMoved: piano.bpm = Math.round(value)
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Label {
                            text: "Progress"
                            color: "#a1a1aa"
                            font.pixelSize: 12
                        }

                        Slider {
                            Layout.fillWidth: true
                            from: 0
                            to: Math.max(1, piano.totalBeats)
                            value: piano.currentBeat
                            onMoved: piano.seekBeat(value)
                        }

                        Label {
                            text: piano.currentBeat.toFixed(1) + " / " + piano.totalBeats.toFixed(1) + " beats"
                            color: "#d4d4d8"
                            font.pixelSize: 12
                        }
                    }

                    GridLayout {
                        columns: 3
                        Layout.fillWidth: true
                        rowSpacing: 8
                        columnSpacing: 8

                        StatBox { title: "正确"; value: piano.correctCount; accent: "#22c55e" }
                        StatBox { title: "错音"; value: piano.wrongCount; accent: "#f97316" }
                        StatBox { title: "漏弹"; value: piano.missedCount; accent: "#ef4444" }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: "#2f3036"
                    }

                    Label {
                        Layout.fillWidth: true
                        text: piano.mode === "practice" ? expectedLabel() : "自动播放会同步高亮键盘"
                        color: "#cbd5e1"
                        font.pixelSize: 13
                        wrapMode: Text.WordWrap

                        function expectedLabel() {
                            if (piano.expectedNotes.length === 0)
                                return "没有待弹音符"
                            var names = []
                            for (var i = 0; i < piano.expectedNotes.length; ++i)
                                names.push(piano.expectedNotes[i].note)
                            return "当前应弹：" + names.join(" + ")
                        }
                    }

                    Item { Layout.fillHeight: true }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 14

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "#09090b"
                    radius: 8
                    border.color: "#2f3036"
                    clip: true

                    PianoRollView {
                        anchors.fill: parent
                        controller: piano
                    }
                }

                Rectangle {
                    id: keyboard
                    Layout.fillWidth: true
                    Layout.preferredHeight: 190
                    color: "#111113"
                    radius: 8
                    border.color: "#2f3036"
                    clip: true

                    Repeater {
                        model: lastMidi - firstMidi + 1
                        delegate: Rectangle {
                            required property int index
                            property int midi: firstMidi + index
                            property bool black: isBlackMidi(midi)
                            property real whiteW: keyboard.width / whiteKeyCount
                            property bool active: activeSet[midi] === true
                            property bool expected: expectedSet[midi] === true && piano.mode === "practice"

                            z: black ? 2 : 1
                            x: keyX(midi, whiteW)
                            y: black ? 0 : 0
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
                                onPressed: piano.noteOn(parent.midi)
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
            }
        }
    }

    component StatBox: Rectangle {
        required property string title
        required property int value
        required property color accent

        Layout.fillWidth: true
        height: 64
        radius: 7
        color: "#111113"
        border.color: "#2f3036"

        Column {
            anchors.centerIn: parent
            spacing: 2

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: title
                color: "#a1a1aa"
                font.pixelSize: 11
            }

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: value
                color: parent.parent.accent
                font.pixelSize: 20
                font.bold: true
            }
        }
    }
}
