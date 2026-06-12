import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    Layout.fillWidth: true
    spacing: Theme.gapSm

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 40
        radius: Theme.radiusPill
        color: Theme.surfaceContainer
        border.color: Theme.darkMode ? "#2A3952" : "#E3E7ED"
        border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Theme.gapMd
            anchors.rightMargin: Theme.gapMd
            spacing: Theme.gapXs

            StatText {
                title: "正确"
                value: piano.correctCount
                accent: Theme.success
            }

            Dot {}

            StatText {
                title: "错音"
                value: piano.wrongCount
                accent: Theme.warningText
            }

            Dot {}

            StatText {
                title: "漏弹"
                value: piano.missedCount
                accent: Theme.error
            }
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

    component Dot: Label {
        text: "·"
        color: Theme.textMuted
        font.pixelSize: Theme.fontBody
        Layout.alignment: Qt.AlignVCenter
    }

    component StatText: RowLayout {
        required property string title
        required property int value
        required property color accent

        spacing: 3
        Layout.fillWidth: true

        Label {
            text: title
            color: Theme.textSecondary
            font.pixelSize: Theme.fontCaption
            Layout.alignment: Qt.AlignVCenter
        }

        Label {
            text: value
            color: parent.accent
            font.pixelSize: Theme.fontBody
            font.bold: true
            Layout.alignment: Qt.AlignVCenter
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
        Layout.preferredHeight: 44
        radius: Theme.radiusPill
        color: selected ? fill : Theme.surfaceContainer
        border.color: selected ? accent : Theme.outline
        border.width: selected ? 1 : 0
        opacity: muted ? 0.52 : 1.0

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Theme.gapSm
            anchors.rightMargin: Theme.gapSm
            spacing: Theme.gapSm

            Label {
                text: title
                color: selected ? accent : Theme.textSecondary
                font.pixelSize: Theme.fontCaption
                font.bold: true
                Layout.alignment: Qt.AlignVCenter
            }

            Label {
                Layout.fillWidth: true
                text: handNotesText(notes)
                color: selected ? Theme.textPrimary : Theme.textSecondary
                font.pixelSize: Theme.fontCaption
                font.bold: selected
                elide: Text.ElideRight
                Layout.alignment: Qt.AlignVCenter
            }
        }
    }
}
