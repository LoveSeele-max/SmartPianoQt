import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

MaterialCard {
    id: root
    property bool expanded: false

    width: expanded ? 278 : 56
    height: expanded ? Math.min(460, Math.max(360, (parent ? parent.height : 760) - 220)) : 56
    padding: expanded ? Theme.gapMd : 0
    radius: expanded ? Theme.radiusLarge : 28
    cardColor: expanded ? Theme.surface : Theme.primary
    strokeColor: expanded ? Theme.outline : "transparent"

    Behavior on width {
        NumberAnimation {
            duration: 140
            easing.type: Easing.OutCubic
        }
    }

    Behavior on height {
        NumberAnimation {
            duration: 140
            easing.type: Easing.OutCubic
        }
    }

    Item {
        visible: !root.expanded
        anchors.fill: parent

        Label {
            anchors.centerIn: parent
            text: "?"
            color: Theme.textOnPrimary
            font.pixelSize: 24
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.expanded = true
        }
    }

    ColumnLayout {
        visible: root.expanded
        anchors.fill: parent
        spacing: Theme.gapMd

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gapSm

            Label {
                Layout.fillWidth: true
                text: "快捷键"
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSection
                font.bold: true
            }

            TonalButton {
                Layout.preferredWidth: 64
                text: "收起"
                onClicked: root.expanded = false
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.outline
        }

        ScrollView {
            id: shortcutScroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ColumnLayout {
                width: shortcutScroll.availableWidth
                spacing: Theme.gapSm

                Repeater {
                    model: ListModel {
                        ListElement { keyText: "Space"; actionText: "播放 / 暂停 / 取消倒计时" }
                        ListElement { keyText: "S"; actionText: "停止" }
                        ListElement { keyText: "A"; actionText: "设为循环 A 点" }
                        ListElement { keyText: "B"; actionText: "设为循环 B 点" }
                        ListElement { keyText: "L"; actionText: "开启 / 关闭循环练习" }
                        ListElement { keyText: "C"; actionText: "清除 A/B 循环" }
                        ListElement { keyText: "←"; actionText: "回退上一小节" }
                        ListElement { keyText: "→"; actionText: "进入下一小节" }
                        ListElement { keyText: "-"; actionText: "速度 -5%" }
                        ListElement { keyText: "="; actionText: "速度 +5%" }
                        ListElement { keyText: "1"; actionText: "自动播放" }
                        ListElement { keyText: "2"; actionText: "等待练习" }
                        ListElement { keyText: "3"; actionText: "节奏练习" }
                        ListElement { keyText: "F11"; actionText: "进入 / 退出专注模式" }
                        ListElement { keyText: "Esc"; actionText: "退出专注模式" }
                    }

                    delegate: Rectangle {
                        required property string keyText
                        required property string actionText

                        Layout.fillWidth: true
                        implicitHeight: Math.max(36, actionLabel.implicitHeight + 14)
                        radius: Theme.radiusMedium
                        color: Theme.surfaceContainer

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 7
                            spacing: Theme.gapSm

                            Rectangle {
                                Layout.preferredWidth: 58
                                Layout.preferredHeight: 24
                                radius: Theme.radiusSmall
                                color: Theme.primaryContainer

                                Label {
                                    anchors.centerIn: parent
                                    text: keyText
                                    color: Theme.primary
                                    font.pixelSize: Theme.fontCaption
                                    font.bold: true
                                    elide: Text.ElideRight
                                }
                            }

                            Label {
                                id: actionLabel
                                Layout.fillWidth: true
                                text: actionText
                                color: Theme.textSecondary
                                font.pixelSize: Theme.fontCaption
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }
            }
        }
    }
}
