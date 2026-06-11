import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    Layout.fillWidth: true
    spacing: Theme.gapSm

    GridLayout {
        columns: 3
        Layout.fillWidth: true
        rowSpacing: Theme.gapSm
        columnSpacing: Theme.gapSm

        StatBox {
            title: "正确"
            value: piano.correctCount
            accent: Theme.success
            fill: Theme.successContainer
        }
        StatBox {
            title: "错音"
            value: piano.wrongCount
            accent: Theme.warningText
            fill: Theme.warningContainer
        }
        StatBox {
            title: "漏弹"
            value: piano.missedCount
            accent: Theme.error
            fill: Theme.errorContainer
        }
    }

    Label {
        Layout.fillWidth: true
        text: piano.mode !== "auto" ? expectedLabel() : "自动播放会同步高亮键盘"
        color: Theme.textSecondary
        font.pixelSize: Theme.fontBody
        wrapMode: Text.WordWrap

        function expectedLabel() {
            if (piano.expectedNotes.length === 0)
                return "没有待弹音符"
            if (piano.expectedNotes.length === 1)
                return "当前应弹：1 个音"
            return "当前应弹：" + piano.expectedNotes.length + " 个音"
        }
    }

    component StatBox: Rectangle {
        required property string title
        required property int value
        required property color accent
        required property color fill

        Layout.fillWidth: true
        height: 64
        radius: Theme.radiusMedium
        color: fill

        Column {
            anchors.centerIn: parent
            spacing: 2

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: title
                color: Theme.textSecondary
                font.pixelSize: Theme.fontCaption
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
