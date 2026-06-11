import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import SmartPianoQt.Controls 1.0
import "components"

ApplicationWindow {
    id: root
    width: 1180
    height: 760
    visible: true
    title: "SmartPianoQt"
    color: Theme.window
    readonly property bool globalShortcutsEnabled: !textInputHasFocus(activeFocusItem)

    function textInputHasFocus(item) {
        while (item) {
            if (item instanceof TextInput ||
                item instanceof TextEdit ||
                item.objectName === "globalShortcutTextInput") {
                return true
            }
            item = item.parent
        }
        return false
    }

    function modeLabel() {
        if (piano.mode === "practice")
            return "等待练习"
        if (piano.mode === "rhythm")
            return "节奏练习"
        return "自动播放"
    }

    function modeChipColor() {
        if (piano.mode === "rhythm")
            return Theme.warningContainer
        if (piano.mode === "practice")
            return Theme.primaryContainer
        return Theme.surfaceContainerHigh
    }

    function modeTextColor() {
        if (piano.mode === "rhythm")
            return Theme.warningText
        if (piano.mode === "practice")
            return Theme.primary
        return Theme.textSecondary
    }

    FileDialog {
        id: openDialog
        title: "选择曲谱文件"
        nameFilters: ["曲谱文件 (*.json *.mid *.midi)", "JSON (*.json)", "MIDI (*.mid *.midi)", "所有文件 (*)"]
        onAccepted: piano.loadSheet(selectedFile)
    }

    AppShortcut {
        sequence: "Space"
        enabled: root.globalShortcutsEnabled
        onActivated: piano.playPause()
    }

    AppShortcut {
        sequence: "S"
        enabled: root.globalShortcutsEnabled
        onActivated: piano.stop()
    }

    AppShortcut {
        sequence: "A"
        enabled: root.globalShortcutsEnabled
        onActivated: piano.setLoopStartAtCurrent()
    }

    AppShortcut {
        sequence: "B"
        enabled: root.globalShortcutsEnabled
        onActivated: piano.setLoopEndAtCurrent()
    }

    AppShortcut {
        sequence: "L"
        enabled: root.globalShortcutsEnabled
        onActivated: piano.toggleLoopPractice()
    }

    AppShortcut {
        sequence: "C"
        enabled: root.globalShortcutsEnabled
        onActivated: piano.clearLoopPractice()
    }

    AppShortcut {
        sequence: "Left"
        enabled: root.globalShortcutsEnabled
        onActivated: piano.seekPreviousMeasure()
    }

    AppShortcut {
        sequence: "Right"
        enabled: root.globalShortcutsEnabled
        onActivated: piano.seekNextMeasure()
    }

    AppShortcut {
        sequence: "-"
        enabled: root.globalShortcutsEnabled
        onActivated: piano.adjustPlaybackSpeed(-5)
    }

    AppShortcut {
        sequence: "="
        enabled: root.globalShortcutsEnabled
        onActivated: piano.adjustPlaybackSpeed(5)
    }

    AppShortcut {
        sequence: "1"
        enabled: root.globalShortcutsEnabled
        onActivated: piano.mode = "auto"
    }

    AppShortcut {
        sequence: "2"
        enabled: root.globalShortcutsEnabled
        onActivated: piano.mode = "practice"
    }

    AppShortcut {
        sequence: "3"
        enabled: root.globalShortcutsEnabled
        onActivated: piano.mode = "rhythm"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.gapLg
        spacing: Theme.gapLg

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gapMd

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.gapSm

                Label {
                    text: piano.songTitle
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontTitle
                    font.bold: true
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.gapSm

                    StatusChip {
                        text: root.modeLabel()
                        chipColor: root.modeChipColor()
                        textColor: root.modeTextColor()
                    }

                    StatusChip {
                        visible: piano.loopPracticeEnabled
                        text: "循环中"
                        chipColor: Theme.loopContainer
                        textColor: Theme.loop
                    }

                    StatusChip {
                        visible: piano.silentPracticeEnabled
                        text: "静音练习"
                        chipColor: Theme.surfaceContainerHigh
                        textColor: Theme.textSecondary
                    }

                    Label {
                        text: piano.statusMessage + "  |  " + piano.audioStatus
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontBody
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }
            }

            PrimaryButton {
                text: piano.countdownActive ? "取消" : (piano.playing ? "暂停" : "播放")
                Layout.preferredWidth: 88
                onClicked: piano.playPause()
            }

            TonalButton {
                text: "停止"
                Layout.preferredWidth: 78
                onClicked: piano.stop()
            }

            TonalButton {
                text: "示例曲"
                Layout.preferredWidth: 86
                onClicked: piano.loadDemoSong()
            }

            TonalButton {
                text: "导入"
                Layout.preferredWidth: 78
                onClicked: openDialog.open()
            }

            TonalButton {
                text: Theme.darkMode ? "浅色" : "深色"
                Layout.preferredWidth: 78
                highlighted: Theme.darkMode
                onClicked: Theme.darkMode = !Theme.darkMode
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.gapLg

            ControlPanel {}

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: Theme.gapLg

                MaterialCard {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    padding: 10

                    Rectangle {
                        anchors.fill: parent
                        color: "#101418"
                        radius: Theme.radiusMedium
                        clip: true

                        PianoRollView {
                            anchors.fill: parent
                            controller: piano
                            rollSpeedScale: PianoRollSettings.speedScale
                            lookAheadBeats: PianoRollSettings.lookAheadBeats
                            showBeatRuler: PianoRollSettings.beatRulerVisible
                            splitMidi: PianoRollSettings.splitMidi
                        }

                        Rectangle {
                            visible: piano.countdownActive
                            anchors.centerIn: parent
                            width: Math.min(parent.width * 0.42, 260)
                            height: 132
                            radius: Theme.radiusLarge
                            color: Theme.darkMode ? "#111827E8" : "#FFFFFFE8"
                            border.color: Theme.outline

                            Label {
                                anchors.centerIn: parent
                                text: piano.countdownText
                                color: Theme.primary
                                font.pixelSize: piano.countdownText === "开始" ? 42 : 72
                                font.bold: true
                            }
                        }
                    }
                }

                VirtualKeyboard {}
            }
        }
    }

    ShortcutSidebar {
        id: shortcutSidebar
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: 104
        anchors.rightMargin: Theme.gapLg
        z: 20
    }

    component AppShortcut: Shortcut {
        context: Qt.ApplicationShortcut
    }
}
