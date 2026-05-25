import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    Layout.fillWidth: true
    spacing: 10

    GridLayout {
        columns: 3
        Layout.fillWidth: true
        rowSpacing: 8
        columnSpacing: 8

        StatBox { title: "正确"; value: piano.correctCount; accent: "#22c55e" }
        StatBox { title: "错音"; value: piano.wrongCount; accent: "#f97316" }
        StatBox { title: "漏弹"; value: piano.missedCount; accent: "#ef4444" }
    }

    Label {
        Layout.fillWidth: true
        text: piano.mode !== "auto" ? expectedLabel() : "自动播放会同步高亮键盘"
        color: "#cbd5e1"
        font.pixelSize: 13
        wrapMode: Text.WordWrap

        function expectedLabel() {
            if (piano.expectedNotes.length === 0)
                return "没有待弹音符"
            var names = []
            for (var i = 0; i < piano.expectedNotes.length; ++i)
                names.push(piano.expectedNotes[i].note)
            return "当前应弹：" + names.join(" + ")
        }
    }

    component StatBox: Rectangle {
        required property string title
        required property int value
        required property color accent

        Layout.fillWidth: true
        height: 64
        radius: 7
        color: "#111113"
        border.color: "#2f3036"

        Column {
            anchors.centerIn: parent
            spacing: 2

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: title
                color: "#a1a1aa"
                font.pixelSize: 11
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
