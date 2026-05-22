import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

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
            rollCanvas.requestPaint()
        }
        function onPracticeChanged() {
            rebuildExpectedSet()
            rollCanvas.requestPaint()
        }
        function onNotesChanged() {
            rollCanvas.requestPaint()
        }
        function onPositionChanged() {
            rollCanvas.requestPaint()
        }
        function onModeChanged() {
            rebuildExpectedSet()
            rollCanvas.requestPaint()
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

                    Canvas {
                        id: rollCanvas
                        anchors.fill: parent
                        antialiasing: true

                        onPaint: {
                            var ctx = getContext("2d")
                            var w = width
                            var h = height
                            ctx.clearRect(0, 0, w, h)
                            ctx.fillStyle = "#09090b"
                            ctx.fillRect(0, 0, w, h)

                            var gutter = 58
                            var ruler = 34
                            var bottomPad = 24
                            var trackTop = ruler + 12
                            var trackBottom = h - bottomPad
                            var trackH = Math.max(120, trackBottom - trackTop)
                            var playheadX = gutter + (w - gutter) * 0.42
                            var pixelsPerBeat = Math.max(58, Math.min(120, (w - gutter) / 11))
                            var minMidi = 36
                            var maxMidi = 96
                            var span = maxMidi - minMidi
                            var current = piano.currentBeat

                            var grad = ctx.createLinearGradient(0, 0, w, h)
                            grad.addColorStop(0, "rgba(18, 161, 177, 0.16)")
                            grad.addColorStop(0.45, "rgba(24, 24, 27, 0.82)")
                            grad.addColorStop(1, "rgba(12, 14, 18, 1.0)")
                            ctx.fillStyle = grad
                            ctx.fillRect(0, 0, w, h)

                            ctx.fillStyle = "rgba(24,24,27,0.92)"
                            ctx.fillRect(0, 0, gutter, h)
                            ctx.fillStyle = "rgba(9,9,11,0.78)"
                            ctx.fillRect(gutter, 0, w - gutter, ruler)

                            for (var midi = minMidi; midi <= maxMidi; ++midi) {
                                var y = trackTop + (1 - (midi - minMidi) / span) * trackH
                                var octave = midi % 12 === 0
                                ctx.strokeStyle = octave ? "rgba(20,184,166,0.22)" : "rgba(255,255,255,0.045)"
                                ctx.lineWidth = octave ? 1 : 0.5
                                ctx.beginPath()
                                ctx.moveTo(gutter, y)
                                ctx.lineTo(w, y)
                                ctx.stroke()

                                if (octave) {
                                    ctx.fillStyle = "rgba(228,228,231,0.72)"
                                    ctx.font = "10px Segoe UI"
                                    ctx.textAlign = "right"
                                    ctx.textBaseline = "middle"
                                    ctx.fillText(noteName(midi), gutter - 9, y)
                                }
                            }

                            var startBeat = Math.floor(current - 5)
                            var endBeat = Math.ceil(current + 8)
                            for (var b = Math.max(0, startBeat); b <= Math.min(piano.totalBeats + 1, endBeat); ++b) {
                                var x = (b - current) * pixelsPerBeat + playheadX
                                var measure = b % 4 === 0
                                ctx.strokeStyle = measure ? "rgba(20,184,166,0.35)" : "rgba(255,255,255,0.08)"
                                ctx.lineWidth = measure ? 1.2 : 0.5
                                ctx.beginPath()
                                ctx.moveTo(x, ruler)
                                ctx.lineTo(x, h)
                                ctx.stroke()

                                ctx.fillStyle = measure ? "rgba(153,246,228,0.94)" : "rgba(161,161,170,0.58)"
                                ctx.font = measure ? "bold 10px Segoe UI" : "9px Segoe UI"
                                ctx.textAlign = "center"
                                ctx.textBaseline = "middle"
                                ctx.fillText(measure ? "M" + (Math.floor(b / 4) + 1) : "" + (b + 1), x, ruler / 2)
                            }

                            var notes = piano.notes
                            for (var i = 0; i < notes.length; ++i) {
                                var n = notes[i]
                                var nx = (n.startBeat - current) * pixelsPerBeat + playheadX
                                var nw = Math.max(n.durationBeat * pixelsPerBeat - 10, 20)
                                if (nx < gutter - nw - 70 || nx > w + 70)
                                    continue

                                var ny = trackTop + (1 - (n.midi - minMidi) / span) * trackH
                                var active = current >= n.startBeat - 0.001 && current <= n.startBeat + n.durationBeat
                                var expected = expectedSet[n.midi] && piano.mode === "practice" && Math.abs(n.startBeat - current) < 0.01
                                var played = n.played
                                var fill = expected ? "#facc15" : active ? "#5eead4" : played ? "#64748b" : "#38bdf8"
                                var edge = expected ? "#fef08a" : active ? "#ccfbf1" : "#bae6fd"

                                ctx.shadowColor = fill
                                ctx.shadowBlur = expected ? 18 : active ? 14 : 4
                                ctx.fillStyle = fill
                                ctx.strokeStyle = edge
                                ctx.lineWidth = 1

                                ctx.beginPath()
                                ctx.roundedRect(nx, ny - 9, nw, 18, 5, 5)
                                ctx.fill()
                                ctx.shadowBlur = 0
                                ctx.stroke()

                                if (nw > 34) {
                                    ctx.fillStyle = "#020617"
                                    ctx.font = "bold 10px Segoe UI"
                                    ctx.textAlign = "center"
                                    ctx.textBaseline = "middle"
                                    ctx.fillText(n.note, nx + nw / 2, ny)
                                }
                            }

                            ctx.strokeStyle = "#f8fafc"
                            ctx.lineWidth = 2
                            ctx.shadowColor = "#f8fafc"
                            ctx.shadowBlur = 10
                            ctx.beginPath()
                            ctx.moveTo(playheadX, ruler)
                            ctx.lineTo(playheadX, h)
                            ctx.stroke()
                            ctx.shadowBlur = 0
                        }
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
