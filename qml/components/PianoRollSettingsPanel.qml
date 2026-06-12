import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

MaterialCard {
    id: root

    width: 292
    height: 330
    padding: Theme.gapMd
    cardColor: Theme.darkMode ? "#111827E8" : "#FFFFFFEA"
    strokeColor: Theme.darkMode ? "#334155" : "#DADCE0"

    function splitName() {
        if (PianoRollSettings.splitMidi === 48)
            return "C3"
        if (PianoRollSettings.splitMidi === 60)
            return "C4"
        if (PianoRollSettings.splitMidi === 72)
            return "C5"
        return "MIDI " + PianoRollSettings.splitMidi
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.gapMd

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gapSm

            Label {
                text: "瀑布设置"
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSection
                font.bold: true
                Layout.fillWidth: true
            }

            Label {
                text: root.splitName() + " 分割"
                color: Theme.textMuted
                font.pixelSize: Theme.fontCaption
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.gapSm

            ValueHeader {
                title: "显示方式"
                value: PianoRollSettings.handDisplayMode === "target" ? "仅目标"
                     : PianoRollSettings.handDisplayMode === "dim" ? "淡显"
                     : "全部"
            }

            SegmentedPill {
                options: [
                    { label: "仅目标", value: "target" },
                    { label: "淡显", value: "dim" },
                    { label: "全部", value: "all" }
                ]
                currentValue: PianoRollSettings.handDisplayMode
                onSelected: value => PianoRollSettings.handDisplayMode = value
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.gapXs

            ValueHeader {
                title: "瀑布速度"
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
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.gapXs

            ValueHeader {
                title: "提前显示"
                value: PianoRollSettings.lookAheadBeats.toFixed(1) + " 拍"
            }

            Slider {
                Layout.fillWidth: true
                from: 3
                to: 12
                stepSize: 0.5
                value: PianoRollSettings.lookAheadBeats
                onMoved: PianoRollSettings.lookAheadBeats = value
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gapSm

            Label {
                text: "小节线"
                color: Theme.textSecondary
                font.pixelSize: Theme.fontBody
                Layout.fillWidth: true
            }

            Switch {
                checked: PianoRollSettings.beatRulerVisible
                onToggled: PianoRollSettings.beatRulerVisible = checked
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.gapSm

            ValueHeader {
                title: "左右手识别"
                value: root.splitName() + " 分割"
            }

            SegmentedPill {
                options: [
                    { label: "C3", value: 48 },
                    { label: "C4", value: 60 },
                    { label: "C5", value: 72 }
                ]
                currentValue: PianoRollSettings.splitMidi
                onSelected: value => PianoRollSettings.splitMidi = Number(value)
            }
        }
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
            font.pixelSize: Theme.fontCaption
            font.bold: true
        }
    }
}
