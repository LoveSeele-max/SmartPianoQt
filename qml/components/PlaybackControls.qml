import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    Layout.fillWidth: true
    spacing: 12

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 8

        Label {
            text: "Mode"
            color: "#a1a1aa"
            font.pixelSize: 12
        }

        ComboBox {
            Layout.fillWidth: true
            textRole: "label"
            valueRole: "value"
            model: [
                { label: "自动播放", value: "auto" },
                { label: "练习等待", value: "practice" },
                { label: "节奏练习", value: "rhythm" }
            ]
            currentIndex: modeIndex()
            onActivated: piano.mode = currentValue

            function modeIndex() {
                if (piano.mode === "practice")
                    return 1
                if (piano.mode === "rhythm")
                    return 2
                return 0
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: "Volume"
                color: "#a1a1aa"
                font.pixelSize: 12
                Layout.fillWidth: true
            }
            Label {
                text: Math.round(piano.volume * 100 / 127) + "%"
                color: "#f4f4f5"
                font.pixelSize: 14
                font.bold: true
            }
        }

        Slider {
            Layout.fillWidth: true
            from: 0
            to: 127
            stepSize: 1
            value: piano.volume
            onMoved: piano.volume = Math.round(value)
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: "Speed"
                color: "#a1a1aa"
                font.pixelSize: 12
                Layout.fillWidth: true
            }
            Label {
                text: piano.playbackSpeed + "%"
                color: "#f4f4f5"
                font.pixelSize: 14
                font.bold: true
            }
        }

        Slider {
            Layout.fillWidth: true
            from: 50
            to: 150
            stepSize: 1
            value: piano.playbackSpeed
            onMoved: piano.playbackSpeed = Math.round(value)
        }

        Label {
            Layout.fillWidth: true
            text: "Tempo " + piano.bpm + " BPM"
            color: "#71717a"
            font.pixelSize: 11
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

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Button {
                text: "设为 A"
                Layout.fillWidth: true
                onClicked: piano.setLoopStartAtCurrent()
            }

            Button {
                text: "设为 B"
                Layout.fillWidth: true
                onClicked: piano.setLoopEndAtCurrent()
            }

            Button {
                text: piano.loopPracticeEnabled ? "停止循环" : "循环练习"
                Layout.fillWidth: true
                enabled: piano.loopRangeValid || piano.loopPracticeEnabled
                highlighted: piano.loopPracticeEnabled
                onClicked: piano.toggleLoopPractice()
            }
        }

        Label {
            text: piano.currentBeat.toFixed(1) + " / " + piano.totalBeats.toFixed(1) + " beats"
            color: "#d4d4d8"
            font.pixelSize: 12
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                Layout.fillWidth: true
                text: piano.loopStatus
                color: piano.loopPracticeEnabled ? "#86efac" : "#a1a1aa"
                font.pixelSize: 11
                elide: Text.ElideRight
            }

            Button {
                text: "清除"
                enabled: piano.loopRangeValid || piano.loopPracticeEnabled
                onClicked: piano.clearLoopPractice()
            }
        }
    }
}
