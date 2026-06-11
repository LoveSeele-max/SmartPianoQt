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
        visible: piano.mode === "auto"
        Layout.fillWidth: true
        text: "自动播放会同步高亮键盘"
        color: Theme.textSecondary
        font.pixelSize: Theme.fontBody
        wrapMode: Text.WordWrap
    }

    RowLayout {
        visible: piano.mode !== "auto"
        Layout.fillWidth: true
        spacing: Theme.gapSm

        HandHintBox {
            title: "左"
            notes: piano.expectedLeftNotes
            selected: piano.handPracticeEnabled && piano.handPracticeSide === "left"
            muted: piano.handPracticeEnabled && piano.handPracticeSide !== "left"
            accent: Theme.loop
            fill: Theme.loopContainer
        }

        HandHintBox {
            title: "右"
            notes: piano.expectedRightNotes
            selected: piano.handPracticeEnabled && piano.handPracticeSide === "right"
            muted: piano.handPracticeEnabled && piano.handPracticeSide !== "right"
            accent: Theme.primary
            fill: Theme.primaryContainer
        }
    }

    Label {
        visible: piano.mode !== "auto"
        Layout.fillWidth: true
        text: expectedLabel()
        color: Theme.textMuted
        font.pixelSize: Theme.fontCaption
        elide: Text.ElideRight
    }

    function expectedLabel() {
        if (piano.expectedNotes.length === 0)
            return piano.handPracticeEnabled ? "当前手部没有待弹音符" : "没有待弹音符"
        var mode = piano.handPracticeEnabled
            ? (piano.handPracticeSide === "left" ? "左手" : "右手")
            : "完整"
        return mode + "待弹：" + piano.expectedNotes.length + " 个音"
    }

    function handNotesText(notes) {
        if (!notes || notes.length === 0)
            return "--"
        var parts = []
        var count = Math.min(notes.length, 5)
        for (var i = 0; i < count; ++i)
            parts.push(notes[i].note || notes[i].midi)
        if (notes.length > count)
            parts.push("+" + (notes.length - count))
        return parts.join(" ")
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

    component HandHintBox: Rectangle {
        required property string title
        property var notes: []
        property bool selected: false
        property bool muted: false
        required property color accent
        required property color fill

        Layout.fillWidth: true
        height: 58
        radius: Theme.radiusMedium
        color: selected ? fill : Theme.surfaceContainer
        border.color: selected ? accent : Theme.outline
        border.width: selected ? 1 : 0
        opacity: muted ? 0.52 : 1.0

        Column {
            anchors.fill: parent
            anchors.margins: Theme.gapSm
            spacing: 3

            Label {
                text: title
                color: selected ? accent : Theme.textSecondary
                font.pixelSize: Theme.fontCaption
                font.bold: true
            }

            Label {
                width: parent.width
                text: handNotesText(notes)
                color: selected ? Theme.textPrimary : Theme.textSecondary
                font.pixelSize: Theme.fontBody
                font.bold: selected
                elide: Text.ElideRight
            }
        }
    }
}
