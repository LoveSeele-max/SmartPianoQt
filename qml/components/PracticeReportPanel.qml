import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root

    property bool expanded: false
    property int reportHeight: !expanded ? 78 : (piano.practiceReport.hasData ? 488 : 204)

    Layout.fillWidth: true
    spacing: Theme.gapSm

    Behavior on reportHeight {
        NumberAnimation {
            duration: 150
            easing.type: Easing.OutCubic
        }
    }

    MaterialCard {
        id: reportCard
        Layout.fillWidth: true
        Layout.preferredHeight: root.reportHeight
        padding: Theme.gapSm
        cardColor: Theme.surfaceContainer
        strokeColor: "transparent"
        clip: true

        ColumnLayout {
            anchors.fill: parent
            spacing: Theme.gapSm

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.gapSm

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Label {
                        text: "Practice Report"
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontBody
                        font.bold: true
                    }

                    Label {
                        Layout.fillWidth: true
                        text: summaryText()
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontCaption
                        elide: Text.ElideRight
                    }
                }

                TonalButton {
                    text: root.expanded ? "收起" : "展开"
                    Layout.preferredWidth: 64
                    onClicked: root.expanded = !root.expanded
                }
            }

            ColumnLayout {
                visible: root.expanded
                Layout.fillWidth: true
                spacing: Theme.gapSm

                EmptyState {
                    visible: !piano.practiceReport.hasData
                    iconText: "▁"
                    title: "暂无练习记录"
                    detail: "完成一次等待练习或节奏练习后，这里会生成分数趋势和错漏音分析。"
                }

                ColumnLayout {
                    visible: piano.practiceReport.hasData
                    Layout.fillWidth: true
                    spacing: Theme.gapSm

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.gapSm

                        ReportMetric {
                            title: "平均"
                            value: piano.practiceReport.averageScore + "%"
                            accent: Theme.primary
                            fill: Theme.primaryContainer
                        }
                        ReportMetric {
                            title: "最近"
                            value: piano.practiceReport.latest.score + "%"
                            accent: Theme.success
                            fill: Theme.successContainer
                        }
                        ReportMetric {
                            title: "次数"
                            value: piano.practiceReport.sessionCount + ""
                            accent: Theme.warningText
                            fill: Theme.warningContainer
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        text: latestSummary()
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontCaption
                        elide: Text.ElideRight
                    }

                    TrendStrip {
                        trend: piano.practiceReport.scoreTrend || []
                    }

                    NoteChipGroup {
                        title: "常错音"
                        notes: piano.practiceReport.topWrongNotes || []
                        countKey: "wrong"
                        accent: Theme.error
                        fill: Theme.errorContainer
                    }

                    NoteChipGroup {
                        title: "漏弹音"
                        notes: piano.practiceReport.topMissedNotes || []
                        countKey: "missed"
                        accent: Theme.warningText
                        fill: Theme.warningContainer
                    }

                    Label {
                        Layout.fillWidth: true
                        text: piano.practiceReport.teacherTip || "完成几次练习后，我会给出更具体的复练建议。"
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontCaption
                        wrapMode: Text.WordWrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                    }

                    ListView {
                        id: recentSessionList
                        Layout.fillWidth: true
                        Layout.preferredHeight: 70
                        model: piano.practiceReport.sessions
                        spacing: 4
                        interactive: false
                        clip: true

                        delegate: Rectangle {
                            required property string startedAt
                            required property string mode
                            required property int score
                            required property int correct
                            required property int wrong
                            required property int missed
                            required property int activeDurationSeconds

                            width: recentSessionList.width
                            height: 22
                            radius: Theme.radiusSmall
                            color: Theme.surface

                            Behavior on color {
                                ColorAnimation {
                                    duration: 150
                                    easing.type: Easing.OutCubic
                                }
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: Theme.gapSm
                                anchors.rightMargin: Theme.gapSm
                                spacing: Theme.gapSm

                                Label {
                                    text: startedAt
                                    color: Theme.textSecondary
                                    font.pixelSize: 10
                                    Layout.preferredWidth: 58
                                }

                                Label {
                                    text: mode
                                    color: Theme.primary
                                    font.pixelSize: 10
                                    Layout.preferredWidth: 42
                                    elide: Text.ElideRight
                                }

                                Label {
                                    text: score + "%"
                                    color: Theme.success
                                    font.pixelSize: 10
                                    font.bold: true
                                    Layout.preferredWidth: 36
                                }

                                Label {
                                    text: "OK " + correct + "  W " + wrong + "  M " + missed
                                    color: Theme.textPrimary
                                    font.pixelSize: 10
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }

                                Label {
                                    text: activeDurationSeconds + "s"
                                    color: Theme.textMuted
                                    font.pixelSize: 10
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    function summaryText() {
        if (!piano.practiceReport.hasData)
            return "暂无练习记录"
        var latest = piano.practiceReport.latest
        return "平均 " + piano.practiceReport.averageScore + "% · 最近 " + latest.score + "% · 练习 " + piano.practiceReport.sessionCount + " 次"
    }

    function latestSummary() {
        if (!piano.practiceReport.hasData)
            return "暂无练习记录"
        var latest = piano.practiceReport.latest
        return latest.startedAt + "  正 " + latest.correct + " / 错 " + latest.wrong + " / 漏 " + latest.missed
    }

    component ReportMetric: Rectangle {
        required property string title
        required property string value
        required property color accent
        required property color fill

        Layout.fillWidth: true
        height: 58
        radius: Theme.radiusMedium
        color: fill

        Behavior on color {
            ColorAnimation {
                duration: 150
                easing.type: Easing.OutCubic
            }
        }

        Column {
            anchors.centerIn: parent
            spacing: 1

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: title
                color: Theme.textSecondary
                font.pixelSize: Theme.fontCaption
            }

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: value
                color: accent
                font.pixelSize: 18
                font.bold: true
            }
        }
    }

    component TrendStrip: ColumnLayout {
        id: trendRoot

        required property var trend

        Layout.fillWidth: true
        spacing: Theme.gapXs

        Label {
            Layout.fillWidth: true
            text: "最近 5 次趋势"
            color: Theme.textSecondary
            font.pixelSize: Theme.fontCaption
            font.bold: true
        }

        RowLayout {
            visible: trendRoot.trend && trendRoot.trend.length > 0
            Layout.fillWidth: true
            Layout.preferredHeight: 54
            spacing: Theme.gapSm

            Repeater {
                model: trendRoot.trend || []

                delegate: ColumnLayout {
                    id: scoreColumn

                    required property var modelData

                    readonly property int scoreValue: Math.max(0, Math.min(100, Number(modelData.score || 0)))

                    Layout.fillWidth: true
                    spacing: 2

                    Label {
                        Layout.fillWidth: true
                        text: scoreColumn.scoreValue
                        color: Theme.textMuted
                        font.pixelSize: 10
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 34

                        Rectangle {
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.bottom: parent.bottom
                            width: 16
                            height: Math.max(4, Math.round(parent.height * scoreColumn.scoreValue / 100))
                            radius: 8
                            color: scoreColumn.scoreValue >= 90 ? Theme.success
                                 : scoreColumn.scoreValue >= 75 ? Theme.primary
                                 : Theme.warningText

                            Behavior on height {
                                NumberAnimation {
                                    duration: 180
                                    easing.type: Easing.OutCubic
                                }
                            }

                            Behavior on color {
                                ColorAnimation {
                                    duration: 150
                                    easing.type: Easing.OutCubic
                                }
                            }
                        }
                    }
                }
            }
        }

        Label {
            visible: !trendRoot.trend || trendRoot.trend.length === 0
            Layout.fillWidth: true
            text: "暂无趋势"
            color: Theme.textMuted
            font.pixelSize: Theme.fontCaption
        }
    }

    component NoteChipGroup: ColumnLayout {
        id: chipRoot

        required property string title
        required property var notes
        required property string countKey
        required property color accent
        required property color fill

        Layout.fillWidth: true
        spacing: Theme.gapXs

        Label {
            Layout.fillWidth: true
            text: chipRoot.title
            color: Theme.textSecondary
            font.pixelSize: Theme.fontCaption
            font.bold: true
        }

        Flow {
            visible: chipRoot.notes && chipRoot.notes.length > 0
            Layout.fillWidth: true
            Layout.preferredHeight: childrenRect.height
            spacing: 6

            Repeater {
                model: chipRoot.notes || []

                delegate: Rectangle {
                    required property var modelData

                    readonly property int hitCount: Number(modelData[chipRoot.countKey] || 0)

                    width: chipText.implicitWidth + 16
                    height: 24
                    radius: Theme.radiusPill
                    color: chipRoot.fill

                    Behavior on color {
                        ColorAnimation {
                            duration: 150
                            easing.type: Easing.OutCubic
                        }
                    }

                    Label {
                        id: chipText
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        text: (modelData.note || "#") + " ×" + hitCount
                        color: chipRoot.accent
                        font.pixelSize: 10
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }

        Label {
            visible: !chipRoot.notes || chipRoot.notes.length === 0
            Layout.fillWidth: true
            text: "暂无"
            color: Theme.textMuted
            font.pixelSize: Theme.fontCaption
        }
    }
}
