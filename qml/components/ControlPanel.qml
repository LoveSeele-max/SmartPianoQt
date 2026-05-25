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

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

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

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: "MIDI Input"
                color: "#e4e4e7"
                font.pixelSize: 13
                font.bold: true
            }

            ComboBox {
                id: midiInputBox
                Layout.fillWidth: true
                model: midiInput.inputPorts
                enabled: midiInput.inputPorts.length > 0
                displayText: midiInput.inputPorts.length > 0 ? currentText : "未检测到设备"
            }

            RowLayout {
                Layout.fillWidth: true

                Button {
                    text: "刷新"
                    Layout.fillWidth: true
                    onClicked: midiInput.refreshPorts()
                }

                Button {
                    text: "连接"
                    Layout.fillWidth: true
                    enabled: midiInput.inputPorts.length > 0
                    onClicked: midiInput.openPort(midiInputBox.currentIndex)
                }

                Button {
                    text: "关闭"
                    Layout.fillWidth: true
                    onClicked: midiInput.close()
                }
            }

            Label {
                Layout.fillWidth: true
                text: midiInput.statusText
                color: "#71717a"
                font.pixelSize: 10
                elide: Text.ElideRight
            }
        }

        LocalMidiPanel {}

        Item { Layout.fillHeight: true }
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
