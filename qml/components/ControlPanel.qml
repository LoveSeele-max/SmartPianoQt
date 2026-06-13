import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

MaterialCard {
    id: root

    property bool collapsed: false
    property int currentTab: 0
    signal focusRequested()

    function currentSoundFontIndex() {
        const files = piano.soundFontFiles
        for (let i = 0; i < files.length; ++i) {
            if (files[i].current)
                return i
        }
        return -1
    }

    Layout.preferredWidth: collapsed ? 68 : 300
    Layout.minimumWidth: collapsed ? 68 : 280
    Layout.maximumWidth: collapsed ? 68 : 320
    Layout.fillHeight: true
    padding: collapsed ? Theme.gapSm : Theme.gapMd
    cardColor: Theme.surface
    strokeColor: Theme.darkMode ? "#26344A" : "#EEF1F5"
    clip: true

    FileDialog {
        id: soundFontDialog
        title: "选择 SoundFont"
        nameFilters: [ "SoundFont (*.sf2 *.sf3)", "All files (*)" ]
        onAccepted: piano.loadSoundFont(selectedFile)
    }

    function tabTitle(index) {
        if (index === 1)
            return "曲谱"
        if (index === 2)
            return "设备"
        return "练习"
    }

    ColumnLayout {
        visible: root.collapsed
        anchors.fill: parent
        spacing: Theme.gapSm

        TonalButton {
            text: "›"
            Layout.fillWidth: true
            Layout.preferredHeight: 42
            onClicked: root.collapsed = false

            ToolTip.text: "展开侧栏"
            ToolTip.visible: hovered
        }

        Repeater {
            model: ListModel {
                ListElement { glyph: "♪"; tip: "练习"; tabIndex: 0 }
                ListElement { glyph: "▤"; tip: "曲谱库"; tabIndex: 1 }
                ListElement { glyph: "◉"; tip: "设备"; tabIndex: 2 }
            }

            delegate: TonalButton {
                required property string glyph
                required property string tip
                required property int tabIndex

                text: glyph
                Layout.fillWidth: true
                Layout.preferredHeight: 42
                highlighted: root.currentTab === tabIndex
                onClicked: {
                    root.currentTab = tabIndex
                    root.collapsed = false
                }

                ToolTip.text: tip
                ToolTip.visible: hovered
            }
        }

        Item {
            Layout.fillHeight: true
        }

        TonalButton {
            text: "⛶"
            Layout.fillWidth: true
            Layout.preferredHeight: 42
            onClicked: root.focusRequested()

            ToolTip.text: "专注模式"
            ToolTip.visible: hovered
        }
    }

    ColumnLayout {
        visible: !root.collapsed
        anchors.fill: parent
        spacing: Theme.gapMd

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gapSm

            Label {
                text: root.tabTitle(root.currentTab)
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSection
                font.bold: true
                Layout.fillWidth: true
            }

            TonalButton {
                text: "专注"
                Layout.preferredWidth: 70
                onClicked: root.focusRequested()
            }

            TonalButton {
                text: "收起"
                Layout.preferredWidth: 70
                onClicked: root.collapsed = true
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gapSm

            Repeater {
                model: ListModel {
                    ListElement { label: "练习"; tabIndex: 0 }
                    ListElement { label: "曲谱"; tabIndex: 1 }
                    ListElement { label: "设备"; tabIndex: 2 }
                }

                delegate: TonalButton {
                    required property string label
                    required property int tabIndex

                    text: label
                    Layout.fillWidth: true
                    highlighted: root.currentTab === tabIndex
                    onClicked: root.currentTab = tabIndex
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.outline
            opacity: Theme.darkMode ? 0.55 : 0.45
        }

        ScrollView {
            id: panelScroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            Loader {
                id: tabLoader
                width: panelScroll.availableWidth
                height: item ? item.implicitHeight : 0
                sourceComponent: root.currentTab === 1 ? sheetTab
                               : root.currentTab === 2 ? deviceTab
                               : practiceTab
            }
        }
    }

    Component {
        id: practiceTab

        ColumnLayout {
            width: panelScroll.availableWidth
            spacing: Theme.gapMd

            PlaybackControls {}
            PracticeStatusPanel {}
            PracticeReportPanel {}

            Item { height: 1 }
        }
    }

    Component {
        id: sheetTab

        ColumnLayout {
            width: panelScroll.availableWidth
            spacing: Theme.gapMd

            LocalMidiPanel {}

            Item { height: 1 }
        }
    }

    Component {
        id: deviceTab

        ColumnLayout {
            width: panelScroll.availableWidth
            spacing: Theme.gapMd

            MidiInputPanel {}

            MaterialCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 476
                padding: Theme.gapSm
                cardColor: Theme.surfaceContainer
                strokeColor: "transparent"

                ColumnLayout {
                    anchors.fill: parent
                    spacing: Theme.gapSm

                    Label {
                        text: "音源状态"
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontBody
                        font.bold: true
                    }

                    Label {
                        Layout.fillWidth: true
                        text: piano.audioStatus
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontCaption
                        wrapMode: Text.WordWrap
                    }

                    Label {
                        Layout.fillWidth: true
                        text: piano.soundFontName.length > 0
                              ? "当前：" + piano.soundFontName
                              : "当前：内置柔和钢琴"
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontCaption
                        font.bold: true
                        elide: Text.ElideMiddle
                    }

                    ComboBox {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 36
                        model: piano.soundFontFiles.length > 0
                               ? piano.soundFontFiles
                               : [ { name: "未扫描到 SoundFont", path: "" } ]
                        textRole: "name"
                        currentIndex: Math.max(0, root.currentSoundFontIndex())
                        enabled: piano.soundFontFiles.length > 0
                        font.pixelSize: Theme.fontCaption
                        onActivated: {
                            const item = piano.soundFontFiles[index]
                            if (item && item.path)
                                piano.loadSoundFontPath(item.path)
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.gapSm

                        TonalButton {
                            text: "选择"
                            Layout.fillWidth: true
                            onClicked: soundFontDialog.open()
                        }

                        TonalButton {
                            text: "重扫"
                            Layout.fillWidth: true
                            onClicked: piano.rescanSoundFonts()
                        }

                        TonalButton {
                            text: "试听"
                            Layout.fillWidth: true
                            onClicked: piano.previewCurrentSound()
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        text: "Volume " + Math.round(piano.volume * 100 / 127) + "%"
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontCaption
                        font.bold: true
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
                            text: "静音练习"
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontBody
                            Layout.fillWidth: true
                        }

                        Switch {
                            checked: piano.silentPracticeEnabled
                            onToggled: piano.silentPracticeEnabled = checked
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        text: "力度曲线"
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontCaption
                        font.bold: true
                    }

                    SegmentedPill {
                        options: [
                            { label: "线性", value: "linear" },
                            { label: "柔和", value: "soft" },
                            { label: "明亮", value: "bright" },
                            { label: "压缩", value: "compressed" }
                        ]
                        currentValue: piano.velocityCurve
                        onSelected: value => piano.velocityCurve = value
                    }

                    Label {
                        Layout.fillWidth: true
                        text: "延迟模式"
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontCaption
                        font.bold: true
                    }

                    SegmentedPill {
                        options: [
                            { label: "低延迟", value: "low" },
                            { label: "稳定", value: "stable" },
                            { label: "兼容", value: "compatible" }
                        ]
                        currentValue: piano.latencyMode
                        onSelected: value => piano.latencyMode = value
                    }

                }
            }

            Item { height: 1 }
        }
    }
}
