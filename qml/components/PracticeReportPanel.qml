import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    Layout.fillWidth: true
    spacing: 8

    Label {
        text: "Practice Report"
        color: "#e4e4e7"
        font.pixelSize: 13
        font.bold: true
    }

    Rectangle {
        Layout.fillWidth: true
        height: 286
        radius: 7
        color: "#111113"
        border.color: "#2f3036"

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 8

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                ReportMetric {
                    title: "平均"
                    value: piano.practiceReport.hasData ? piano.practiceReport.averageScore + "%" : "--"
                    accent: "#38bdf8"
                }
                ReportMetric {
                    title: "最近"
                    value: piano.practiceReport.hasData ? piano.practiceReport.latest.score + "%" : "--"
                    accent: "#22c55e"
                }
                ReportMetric {
                    title: "次数"
                    value: piano.practiceReport.hasData ? piano.practiceReport.sessionCount + "" : "--"
                    accent: "#facc15"
                }
            }

            Label {
                Layout.fillWidth: true
                text: latestSummary()
                color: "#a1a1aa"
                font.pixelSize: 11
                elide: Text.ElideRight
            }

            Label {
                Layout.fillWidth: true
                text: trendSummary()
                color: "#cbd5e1"
                font.pixelSize: 11
                elide: Text.ElideRight
            }

            Label {
                Layout.fillWidth: true
                text: topWrongSummary()
                color: "#fca5a5"
                font.pixelSize: 11
                elide: Text.ElideRight
            }

            Label {
                Layout.fillWidth: true
                text: topMissedSummary()
                color: "#fcd34d"
                font.pixelSize: 11
                elide: Text.ElideRight
            }

            Label {
                Layout.fillWidth: true
                text: piano.practiceReport.teacherTip || "完成几次练习后，我会给出更具体的复练建议。"
                color: "#cbd5e1"
                font.pixelSize: 11
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }

            ListView {
                id: recentSessionList
                Layout.fillWidth: true
                Layout.preferredHeight: 62
                model: piano.practiceReport.sessions
                spacing: 3
                interactive: false
                clip: true

                delegate: RowLayout {
                    required property string startedAt
                    required property string mode
                    required property int score
                    required property int correct
                    required property int wrong
                    required property int missed
                    required property int activeDurationSeconds

                    width: recentSessionList.width
                    height: 20
                    spacing: 6

                    Label {
                        text: startedAt
                        color: "#a1a1aa"
                        font.pixelSize: 10
                        Layout.preferredWidth: 54
                    }

                    Label {
                        text: mode
                        color: "#38bdf8"
                        font.pixelSize: 10
                        Layout.preferredWidth: 42
                        elide: Text.ElideRight
                    }

                    Label {
                        text: score + "%"
                        color: "#22c55e"
                        font.pixelSize: 10
                        font.bold: true
                        Layout.preferredWidth: 34
                    }

                    Label {
                        text: "OK " + correct + "  W " + wrong + "  M " + missed
                        color: "#cbd5e1"
                        font.pixelSize: 10
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }

                    Label {
                        text: activeDurationSeconds + "s"
                        color: "#71717a"
                        font.pixelSize: 10
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
            parts.push(list[i].note + " x" + list[i].wrong)
        return "常错音 Top 5：" + parts.join("  ")
    }

    function topMissedSummary() {
        var list = piano.practiceReport.topMissedNotes || []
        if (!piano.practiceReport.hasData || list.length === 0)
            return "漏弹音 Top 5：暂无"
        var parts = []
        var count = Math.min(5, list.length)
        for (var i = 0; i < count; ++i)
            parts.push(list[i].note + " x" + list[i].missed)
        return "漏弹音 Top 5：" + parts.join("  ")
    }

    component ReportMetric: ColumnLayout {
        required property string title
        required property string value
        required property color accent

        Layout.fillWidth: true
        spacing: 1

        Label {
            Layout.alignment: Qt.AlignHCenter
            text: title
            color: "#71717a"
            font.pixelSize: 10
        }

        Label {
            Layout.alignment: Qt.AlignHCenter
            text: value
            color: accent
            font.pixelSize: 18
            font.bold: true
        }
    }
}
