import QtQuick

Rectangle {
    id: root

    default property alias content: content.data
    property int padding: 14
    property color cardColor: Theme.surface
    property color strokeColor: Theme.outline

    color: cardColor
    radius: Theme.radiusLarge
    border.color: strokeColor
    border.width: 1

    Item {
        id: content
        anchors.fill: parent
        anchors.margins: root.padding
    }
}
