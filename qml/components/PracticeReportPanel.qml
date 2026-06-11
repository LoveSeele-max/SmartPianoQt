import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    Layout.fillWidth: true
    spacing: Theme.gapSm

    Label {
        text: "Practice Report"
        color: Theme.textPrimary
        font.pixelSize: Theme.fontBody
        font.bold: true
    }

    MaterialCard {
        Layout.fillWidth: true
        height: 304
        padding: Theme.gapSm
        cardColor: Theme.surfaceContainer
        strokeColor: "transparent"

        ColumnLayout {
            anchors.fill: parent
            spacing: Theme.gapSm

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.gapSm

                ReportMetric {
                    title: "平均"
                    value: piano.practiceReport.hasData ? piano.practiceReport.averageScore + "%" : "--"
                    accent: Theme.primary
                    fill: Theme.primaryContainer
                }
                ReportMetric {
                    title: "最近"
                    value: piano.practiceReport.hasData ? piano.practiceReport.latest.score + "%" : "--"
                    accent: Theme.success
                    fill: Theme.successContainer
                }
                ReportMetric {
                    title: "次数"
                    value: piano.practiceReport.hasData ? piano.practiceReport.sessionCount + "" : "--"
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

            Label {
                Layout.fillWidth: true
                text: trendSummary()
                color: Theme.primary
                font.pixelSize: Theme.fontCaption
                elide: Text.ElideRight
            }

            Label {
                Layout.fillWidth: true
                text: topWrongSummary()
                color: Theme.error
                font.pixelSize: Theme.fontCaption
                elide: Text.ElideRight
            }

            Label {
                Layout.fillWidth: true
                text: topMissedSummary()
                color: Theme.warningText
                font.pixelSize: Theme.fontCaption
                elide: Text.ElideRight
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

    function latestSummary() {
        if (!piano.practiceReport.hasData)
            return "暂无练习记录"
        var latest = piano.practiceReport.latest
        return latest.startedAt + "  正 " + latest.correct + " / 错 " + latest.wrong + " / 漏 " + latest.missed
    }

    function trendSummary() {
        var trend = piano.practiceReport.scoreTrend || []
        if (!piano.practiceReport.hasData || trend.length === 0)
            return "最近 5 次趋势：暂无"
        var parts = []
        for (var i = 0; i < trend.length; ++i)
            parts.push(trend[i].score + "%")
        return "最近 5 次趋势：" + parts.join(" -> ")
    }

    function topWrongSummary() {
        var list = piano.practiceReport.topWrongNotes || []
        if (!piano.practiceReport.hasData || list.length === 0)
            return "常错音 Top 5：暂无"
        var parts = []
        var count = Math.min(5, list.length)
        for (var i = 0; i < count; ++i)
            parts.push((list[i].note || "#") + " x" + list[i].wrong)
        return "常错音 Top 5：" + parts.join("  ")
    }

    function topMissedSummary() {
        var list = piano.practiceReport.topMissedNotes || []
        if (!piano.practiceReport.hasData || list.length === 0)
            return "漏弹音 Top 5：暂无"
        var parts = []
        var count = Math.min(5, list.length)
        for (var i = 0; i < count; ++i)
            parts.push((list[i].note || "#") + " x" + list[i].missed)
        return "漏弹音 Top 5：" + parts.join("  ")
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
}
