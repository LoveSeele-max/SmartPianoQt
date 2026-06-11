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
    color: "#101114"

    FileDialog {
        id: openDialog
        title: "选择曲谱文件"
        nameFilters: ["曲谱文件 (*.json *.mid *.midi)", "JSON (*.json)", "MIDI (*.mid *.midi)", "所有文件 (*)"]
        onAccepted: piano.loadSheet(selectedFile)
    }

    AppShortcut {
        sequence: "Space"
        onActivated: piano.playPause()
    }

    AppShortcut {
        sequence: "S"
        onActivated: piano.stop()
    }

    AppShortcut {
        sequence: "A"
        onActivated: piano.setLoopStartAtCurrent()
    }

    AppShortcut {
        sequence: "B"
        onActivated: piano.setLoopEndAtCurrent()
    }

    AppShortcut {
        sequence: "L"
        onActivated: piano.toggleLoopPractice()
    }

    AppShortcut {
        sequence: "C"
        onActivated: piano.clearLoopPractice()
    }

    AppShortcut {
        sequence: "Left"
        onActivated: piano.seekPreviousMeasure()
    }

    AppShortcut {
        sequence: "Right"
        onActivated: piano.seekNextMeasure()
    }

    AppShortcut {
        sequence: "-"
        onActivated: piano.adjustPlaybackSpeed(-5)
    }

    AppShortcut {
        sequence: "="
        onActivated: piano.adjustPlaybackSpeed(5)
    }

    AppShortcut {
        sequence: "1"
        onActivated: piano.mode = "auto"
    }

    AppShortcut {
        sequence: "2"
        onActivated: piano.mode = "practice"
    }

    AppShortcut {
        sequence: "3"
        onActivated: piano.mode = "rhythm"
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
                    text: piano.statusMessage + "  |  " + piano.audioStatus
                    color: "#a1a1aa"
                    font.pixelSize: 13
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }

            Button {
                text: piano.countdownActive ? "取消" : (piano.playing ? "暂停" : "播放")
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

            ControlPanel {}

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

                    Rectangle {
                        visible: piano.countdownActive
                        anchors.centerIn: parent
                        width: Math.min(parent.width * 0.42, 260)
                        height: 132
                        radius: 8
                        color: "#18181bcc"
                        border.color: "#3f3f46"

                        Label {
                            anchors.centerIn: parent
                            text: piano.countdownText
                            color: "#f8fafc"
                            font.pixelSize: piano.countdownText === "开始" ? 42 : 72
                            font.bold: true
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
        anchors.topMargin: 112
        anchors.rightMargin: 18
        z: 20
    }

    component AppShortcut: Shortcut {
        context: Qt.ApplicationShortcut
    }
}
