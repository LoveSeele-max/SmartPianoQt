pragma Singleton
import QtQuick

QtObject {
    property real speedScale: 1.0
    property real lookAheadBeats: 7.0
    property bool beatRulerVisible: true
    property int splitMidi: 60
    property string handDisplayMode: "target"
}
