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
        height: 202
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
                text: mistakeSummary()
                color: "#cbd5e1"
                font.pixelSize: 11
                elide: Text.ElideRight
            }

            ListView {
                id: recentSessionList
                Layout.fillWidth: true
                Layout.preferredHeight: 70
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

    function mistakeSummary() {
        if (!piano.practiceReport.hasData || piano.practiceReport.mistakes.length === 0)
            return "易错音：暂无"
        var parts = []
        var count = Math.min(3, piano.practiceReport.mistakes.length)
        for (var i = 0; i < count; ++i) {
            var item = piano.practiceReport.mistakes[i]
            parts.push(item.note + " x" + item.total)
        }
        return "易错音：" + parts.join("  ")
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
