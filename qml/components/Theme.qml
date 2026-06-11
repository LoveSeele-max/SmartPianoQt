pragma Singleton
import QtQuick

QtObject {
    property bool darkMode: false

    readonly property color window: darkMode ? "#0B1020" : "#F8FAFD"
    readonly property color surface: darkMode ? "#111827" : "#FFFFFF"
    readonly property color surfaceContainer: darkMode ? "#182235" : "#F1F4F9"
    readonly property color surfaceContainerHigh: darkMode ? "#223047" : "#E8EEF7"
    readonly property color outline: darkMode ? "#334155" : "#DADCE0"

    readonly property color textPrimary: darkMode ? "#F8FAFC" : "#202124"
    readonly property color textSecondary: darkMode ? "#CBD5E1" : "#5F6368"
    readonly property color textMuted: darkMode ? "#94A3B8" : "#80868B"

    readonly property color primary: "#1A73E8"
    readonly property color primaryContainer: darkMode ? "#173B6D" : "#D3E3FD"
    readonly property color textOnPrimary: "#FFFFFF"

    readonly property color success: darkMode ? "#4ADE80" : "#188038"
    readonly property color successContainer: darkMode ? "#173F2A" : "#CEEAD6"
    readonly property color warning: "#F9AB00"
    readonly property color warningContainer: darkMode ? "#4A3413" : "#FEEFC3"
    readonly property color warningText: darkMode ? "#FCD34D" : "#8A5A00"
    readonly property color error: darkMode ? "#F87171" : "#D93025"
    readonly property color errorContainer: darkMode ? "#4B1E24" : "#FAD2CF"
    readonly property color activeKey: darkMode ? "#2DD4BF" : "#00A389"
    readonly property color activeKeyContainer: darkMode ? "#143D3A" : "#D1F3EC"
    readonly property color loop: darkMode ? "#C4B5FD" : "#7E57C2"
    readonly property color loopContainer: darkMode ? "#33224F" : "#EADDFF"
    readonly property color hoverSurface: darkMode ? "#1E293B" : "#F7FAFF"

    readonly property int radiusSmall: 8
    readonly property int radiusMedium: 14
    readonly property int radiusLarge: 22
    readonly property int radiusPill: 999

    readonly property int gapXs: 4
    readonly property int gapSm: 8
    readonly property int gapMd: 12
    readonly property int gapLg: 18
    readonly property int gapXl: 24

    readonly property int fontTitle: 24
    readonly property int fontSection: 16
    readonly property int fontBody: 13
    readonly property int fontCaption: 11
}
