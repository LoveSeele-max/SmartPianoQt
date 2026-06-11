import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    Layout.fillWidth: true
    spacing: Theme.gapMd

    ColumnLayout {
        Layout.fillWidth: true
        spacing: Theme.gapSm

        SectionTitle { text: "Mode" }

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
        spacing: Theme.gapSm

        SectionTitle { text: "练习对象" }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gapSm

            TonalButton {
                text: "双手"
                Layout.fillWidth: true
                highlighted: !piano.handPracticeEnabled
                onClicked: piano.handPracticeEnabled = false
            }

            TonalButton {
                text: "右"
                Layout.fillWidth: true
                highlighted: piano.handPracticeEnabled && piano.handPracticeSide === "right"
                onClicked: {
                    piano.handPracticeSide = "right"
                    piano.handPracticeEnabled = true
                }
            }

            TonalButton {
                text: "左"
                Layout.fillWidth: true
                highlighted: piano.handPracticeEnabled && piano.handPracticeSide === "left"
                onClicked: {
                    piano.handPracticeSide = "left"
                    piano.handPracticeEnabled = true
                }
            }
        }

        SectionTitle { text: "瀑布显示" }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gapSm

            TonalButton {
                text: "目标"
                Layout.fillWidth: true
                highlighted: PianoRollSettings.handDisplayMode === "target"
                onClicked: PianoRollSettings.handDisplayMode = "target"
            }

            TonalButton {
                text: "淡显"
                Layout.fillWidth: true
                highlighted: PianoRollSettings.handDisplayMode === "dim"
                onClicked: PianoRollSettings.handDisplayMode = "dim"
            }

            TonalButton {
                text: "全部"
                Layout.fillWidth: true
                highlighted: PianoRollSettings.handDisplayMode === "all"
                onClicked: PianoRollSettings.handDisplayMode = "all"
            }
        }

        Label {
            Layout.fillWidth: true
            text: piano.handPracticeEnabled
                  ? "按 C4 自动分割，只判定" + (piano.handPracticeSide === "left" ? "左手" : "右手") + "音符"
                  : "按完整和弦判定，左右手以 C4 分割显示"
            color: Theme.textMuted
            font.pixelSize: Theme.fontCaption
            elide: Text.ElideRight
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: Theme.gapSm

        ValueHeader {
            title: "Waterfall"
            value: Math.round(PianoRollSettings.speedScale * 100) + "%"
        }

        Slider {
            Layout.fillWidth: true
            from: 65
            to: 155
            stepSize: 5
            value: PianoRollSettings.speedScale * 100
            onMoved: PianoRollSettings.speedScale = Math.round(value) / 100
        }

        ValueHeader {
            title: "Look Ahead"
            value: PianoRollSettings.lookAheadBeats.toFixed(1) + " beats"
        }

        Slider {
            Layout.fillWidth: true
            from: 3
            to: 12
            stepSize: 0.5
            value: PianoRollSettings.lookAheadBeats
            onMoved: PianoRollSettings.lookAheadBeats = value
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gapSm

            Label {
                text: "Beat ruler"
                color: Theme.textSecondary
                font.pixelSize: Theme.fontBody
                Layout.fillWidth: true
            }

            Switch {
                checked: PianoRollSettings.beatRulerVisible
                onToggled: PianoRollSettings.beatRulerVisible = checked
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: Theme.gapSm

        ValueHeader {
            title: "Volume"
            value: Math.round(piano.volume * 100 / 127) + "%"
        }

        Slider {
            Layout.fillWidth: true
            from: 0
            to: 127
            stepSize: 1
            value: piano.volume
            onMoved: piano.volume = Math.round(value)
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gapSm

            Label {
                text: "Silent Practice"
                color: Theme.textSecondary
                font.pixelSize: Theme.fontBody
                Layout.fillWidth: true
            }

            Switch {
                checked: piano.silentPracticeEnabled
                onToggled: piano.silentPracticeEnabled = checked
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: Theme.gapSm

        ValueHeader {
            title: "Speed"
            value: piano.playbackSpeed + "%"
        }

        Slider {
            Layout.fillWidth: true
            from: 50
            to: 150
            stepSize: 1
            value: piano.playbackSpeed
            onMoved: piano.playbackSpeed = Math.round(value)
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gapSm

            Repeater {
                model: [50, 75, 100, 125]

                delegate: TonalButton {
                    required property int modelData

                    text: modelData + "%"
                    Layout.fillWidth: true
                    highlighted: piano.playbackSpeed === modelData
                    onClicked: piano.playbackSpeed = modelData
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gapSm

            TonalButton {
                text: "-5%"
                Layout.fillWidth: true
                onClicked: piano.adjustPlaybackSpeed(-5)
            }

            TonalButton {
                text: "+5%"
                Layout.fillWidth: true
                onClicked: piano.adjustPlaybackSpeed(5)
            }
        }

        Label {
            Layout.fillWidth: true
            text: "Tempo " + piano.bpm + " BPM"
            color: Theme.textMuted
            font.pixelSize: Theme.fontCaption
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: Theme.gapSm

        SectionTitle { text: "Progress" }

        Slider {
            Layout.fillWidth: true
            from: 0
            to: Math.max(1, piano.totalBeats)
            value: piano.currentBeat
            onMoved: piano.seekBeat(value)
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gapSm

            TonalButton {
                text: "设为 A"
                Layout.fillWidth: true
                onClicked: piano.setLoopStartAtCurrent()
            }

            TonalButton {
                text: "设为 B"
                Layout.fillWidth: true
                onClicked: piano.setLoopEndAtCurrent()
            }

            TonalButton {
                text: piano.loopPracticeEnabled ? "停止循环" : "循环练习"
                Layout.fillWidth: true
                enabled: piano.loopRangeValid || piano.loopPracticeEnabled
                highlighted: piano.loopPracticeEnabled
                onClicked: piano.toggleLoopPractice()
            }
        }

        Label {
            text: piano.currentBeat.toFixed(1) + " / " + piano.totalBeats.toFixed(1) + " beats"
            color: Theme.textSecondary
            font.pixelSize: Theme.fontCaption
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gapSm

            Label {
                Layout.fillWidth: true
                text: piano.loopStatus
                color: piano.loopPracticeEnabled ? Theme.loop : Theme.textSecondary
                font.pixelSize: Theme.fontCaption
                elide: Text.ElideRight
            }

            TonalButton {
                text: "清除"
                Layout.preferredWidth: 72
                enabled: piano.loopRangeValid || piano.loopPracticeEnabled
                onClicked: piano.clearLoopPractice()
            }
        }
    }

    component SectionTitle: Label {
        color: Theme.textSecondary
        font.pixelSize: Theme.fontCaption
        font.bold: true
    }

    component ValueHeader: RowLayout {
        required property string title
        required property string value

        Layout.fillWidth: true

        Label {
            text: title
            color: Theme.textSecondary
            font.pixelSize: Theme.fontCaption
            font.bold: true
            Layout.fillWidth: true
        }

        Label {
            text: value
            color: Theme.textPrimary
            font.pixelSize: Theme.fontBody
            font.bold: true
        }
    }
}
