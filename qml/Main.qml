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
    property bool focusMode: false
    property bool rollSettingsOpen: false
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

    function setFocusMode(enabled) {
        focusMode = enabled
        if (focusMode) {
            rollSettingsOpen = false
            shortcutSidebar.expanded = false
        }
        if (!focusMode)
            focusShortcutSidebar.expanded = false
    }

    function handTargetLabel() {
        if (!piano.handPracticeEnabled)
            return "双手"
        return piano.handPracticeSide === "left" ? "左手" : "右手"
    }

    function focusHudText() {
        var parts = [
            modeLabel(),
            handTargetLabel(),
            piano.playbackSpeed + "%"
        ]
        if (piano.loopPracticeEnabled && piano.loopRangeValid)
            parts.push("循环 A " + piano.loopStartBeat.toFixed(1) + " / B " + piano.loopEndBeat.toFixed(1))
        parts.push("正 " + piano.correctCount + " 错 " + piano.wrongCount + " 漏 " + piano.missedCount)
        return parts.join(" · ")
    }

    FileDialog {
        id: openDialog
        title: "选择曲谱文件"
        nameFilters: ["曲谱文件 (*.json *.mid *.midi)", "JSON (*.json)", "MIDI (*.mid *.midi)", "所有文件 (*)"]
        onAccepted: piano.loadSheet(selectedFile)
    }

    Component.onCompleted: {
        piano.handSplitMidi = PianoRollSettings.splitMidi
    }

    Connections {
        target: PianoRollSettings
        function onSplitMidiChanged() {
            piano.handSplitMidi = PianoRollSettings.splitMidi
        }
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

    AppShortcut {
        sequence: "F11"
        enabled: root.globalShortcutsEnabled
        onActivated: root.setFocusMode(!root.focusMode)
    }

    AppShortcut {
        sequence: "Esc"
        enabled: root.focusMode
        onActivated: root.setFocusMode(false)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: root.focusMode ? Theme.gapMd : Theme.gapLg
        spacing: root.focusMode ? Theme.gapSm : Theme.gapLg

        RowLayout {
            id: topBar
            Layout.fillWidth: true
            spacing: Theme.gapMd
            opacity: root.focusMode ? 0.68 : 1.0

            ColumnLayout {
                Layout.fillWidth: true
                spacing: root.focusMode ? 4 : Theme.gapSm

                Label {
                    text: piano.songTitle
                    color: Theme.textPrimary
                    font.pixelSize: root.focusMode ? 18 : Theme.fontTitle
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

                    StatusChip {
                        visible: piano.handPracticeEnabled
                        text: piano.handPracticeSide === "left" ? "左手练习" : "右手练习"
                        chipColor: piano.handPracticeSide === "left" ? Theme.loopContainer : Theme.primaryContainer
                        textColor: piano.handPracticeSide === "left" ? Theme.loop : Theme.primary
                    }

                    Label {
                        text: piano.statusMessage
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontBody
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }
            }

            PrimaryButton {
                text: piano.countdownActive ? "取消" : (piano.playing ? "暂停" : "播放")
                visible: !root.focusMode
                Layout.preferredWidth: 88
                onClicked: piano.playPause()
            }

            TonalButton {
                text: "停止"
                visible: !root.focusMode
                Layout.preferredWidth: 78
                onClicked: piano.stop()
            }

            TonalButton {
                text: "示例曲"
                visible: !root.focusMode
                Layout.preferredWidth: 86
                onClicked: piano.loadDemoSong()
            }

            TonalButton {
                text: "导入"
                visible: !root.focusMode
                Layout.preferredWidth: 78
                onClicked: openDialog.open()
            }

            TonalButton {
                text: Theme.darkMode ? "浅色" : "深色"
                visible: !root.focusMode
                Layout.preferredWidth: 78
                highlighted: Theme.darkMode
                onClicked: Theme.darkMode = !Theme.darkMode
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: root.focusMode ? 0 : Theme.gapLg

            ShortcutSidebar {
                id: focusShortcutSidebar
                visible: root.focusMode && expanded
                Layout.alignment: Qt.AlignTop
                z: 8

                onVisibleChanged: {
                    if (!visible)
                        expanded = false
                }
            }

            ControlPanel {
                id: controlPanel
                visible: !root.focusMode
                onFocusRequested: root.setFocusMode(true)
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: root.focusMode ? 4 : 8

                MaterialCard {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    padding: root.focusMode ? 6 : 10

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
                            handDisplayMode: PianoRollSettings.handDisplayMode
                        }

                        TonalButton {
                            id: rollSettingsButton
                            visible: !root.focusMode
                            text: "⚙"
                            anchors.top: parent.top
                            anchors.right: parent.right
                            anchors.topMargin: 10
                            anchors.rightMargin: 10
                            width: 42
                            height: 36
                            z: 12
                            highlighted: root.rollSettingsOpen
                            onClicked: root.rollSettingsOpen = !root.rollSettingsOpen

                            ToolTip.text: "瀑布设置"
                            ToolTip.visible: hovered
                        }

                        PianoRollSettingsPanel {
                            visible: root.rollSettingsOpen && !root.focusMode
                            anchors.top: rollSettingsButton.bottom
                            anchors.right: parent.right
                            anchors.topMargin: Theme.gapSm
                            anchors.rightMargin: 10
                            z: 13
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

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 2
                    radius: 1
                    color: Theme.activeKey
                    opacity: Theme.darkMode ? 0.32 : 0.22
                }

                VirtualKeyboard {
                    Layout.preferredHeight: root.focusMode ? 176 : 168
                }
            }
        }
    }

    ShortcutSidebar {
        id: shortcutSidebar
        visible: !root.focusMode && !controlPanel.collapsed
        x: parent ? controlPanel.mapToItem(parent, Theme.gapMd, 0).x : 0
        y: parent ? controlPanel.mapToItem(parent, 0, controlPanel.height - height - Theme.gapMd).y : 0
        z: 20

        onVisibleChanged: {
            if (!visible)
                expanded = false
        }
    }

    RowLayout {
        visible: root.focusMode
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.rightMargin: Theme.gapLg
        anchors.topMargin: Theme.gapMd
        spacing: Theme.gapSm
        z: 30

        TonalButton {
            text: "?"
            Layout.preferredWidth: 48
            highlighted: focusShortcutSidebar.expanded
            onClicked: focusShortcutSidebar.expanded = !focusShortcutSidebar.expanded
        }

        PrimaryButton {
            text: "退出专注"
            Layout.preferredWidth: 96
            onClicked: root.setFocusMode(false)
        }
    }

    Rectangle {
        visible: root.focusMode
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: Theme.gapMd
        width: Math.min(parent.width - 260, focusHudLabel.implicitWidth + 36)
        height: 34
        radius: Theme.radiusPill
        color: Theme.darkMode ? "#111827D9" : "#FFFFFFDB"
        border.color: Theme.darkMode ? "#334155" : "#DADCE0"
        border.width: 1
        z: 25

        Label {
            id: focusHudLabel
            anchors.fill: parent
            anchors.leftMargin: Theme.gapMd
            anchors.rightMargin: Theme.gapMd
            text: root.focusHudText()
            color: Theme.textPrimary
            font.pixelSize: Theme.fontCaption
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }

    component AppShortcut: Shortcut {
        context: Qt.ApplicationShortcut
    }
}
