import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

MaterialCard {
    id: root

    property bool collapsed: false
    property int currentTab: 0
    signal focusRequested()

    Layout.preferredWidth: collapsed ? 68 : 300
    Layout.minimumWidth: collapsed ? 68 : 280
    Layout.maximumWidth: collapsed ? 68 : 320
    Layout.fillHeight: true
    padding: collapsed ? Theme.gapSm : Theme.gapMd
    cardColor: Theme.surface
    strokeColor: Theme.darkMode ? "#26344A" : "#EEF1F5"
    clip: true

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
            text: ">"
            Layout.fillWidth: true
            Layout.preferredHeight: 42
            onClicked: root.collapsed = false
        }

        Repeater {
            model: ListModel {
                ListElement { label: "练"; tabIndex: 0 }
                ListElement { label: "谱"; tabIndex: 1 }
                ListElement { label: "设"; tabIndex: 2 }
            }

            delegate: TonalButton {
                required property string label
                required property int tabIndex

                text: label
                Layout.fillWidth: true
                Layout.preferredHeight: 42
                highlighted: root.currentTab === tabIndex
                onClicked: {
                    root.currentTab = tabIndex
                    root.collapsed = false
                }
            }
        }

        Item {
            Layout.fillHeight: true
        }

        TonalButton {
            text: "专"
            Layout.fillWidth: true
            Layout.preferredHeight: 42
            onClicked: root.focusRequested()
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
                Layout.preferredHeight: 116
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
                        text: "当前音量 " + Math.round(piano.volume * 100 / 127) + "%"
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontCaption
                    }
                }
            }

            Item { height: 1 }
        }
    }
}
