import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Frame {
    id: root
    property bool expanded: false

    width: expanded ? 278 : 92
    height: expanded ? Math.max(420, (parent ? parent.height : 760) - 130) : 38
    padding: expanded ? 12 : 0

    background: Rectangle {
        color: "#18181b"
        border.color: root.expanded ? "#3f3f46" : "#2f3036"
        radius: 8
    }

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

        Rectangle {
            anchors.fill: parent
            anchors.margins: 6
            radius: 6
            color: collapsedMouse.containsMouse ? "#27272a" : "transparent"
            border.color: collapsedMouse.containsMouse ? "#52525b" : "transparent"
        }

        Label {
            anchors.centerIn: parent
            text: "快捷键"
            color: "#e4e4e7"
            font.pixelSize: 13
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
        }

        MouseArea {
            id: collapsedMouse
            anchors.fill: parent
            hoverEnabled: true
            onClicked: root.expanded = true
        }
    }

    ColumnLayout {
        visible: root.expanded
        anchors.fill: parent
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                Layout.fillWidth: true
                text: "快捷键"
                color: "#f4f4f5"
                font.pixelSize: 16
                font.bold: true
            }

            Button {
                Layout.preferredWidth: 58
                text: "收起"
                onClicked: root.expanded = false
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#2f3036"
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
                spacing: 8

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
                    }

                    delegate: Rectangle {
                        required property string keyText
                        required property string actionText

                        Layout.fillWidth: true
                        implicitHeight: Math.max(34, actionLabel.implicitHeight + 14)
                        radius: 6
                        color: "#202024"
                        border.color: "#2f3036"

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 7
                            spacing: 8

                            Rectangle {
                                Layout.preferredWidth: 58
                                Layout.preferredHeight: 24
                                radius: 5
                                color: "#27272a"
                                border.color: "#52525b"

                                Label {
                                    anchors.centerIn: parent
                                    text: keyText
                                    color: "#f8fafc"
                                    font.pixelSize: 12
                                    font.bold: true
                                    elide: Text.ElideRight
                                }
                            }

                            Label {
                                id: actionLabel
                                Layout.fillWidth: true
                                text: actionText
                                color: "#d4d4d8"
                                font.pixelSize: 12
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }
            }
        }
    }
}
