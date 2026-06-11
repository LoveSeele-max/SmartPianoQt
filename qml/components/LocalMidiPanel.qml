import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    Layout.fillWidth: true
    spacing: 8

    RowLayout {
        Layout.fillWidth: true

        Label {
            text: "Local MIDI"
            color: "#e4e4e7"
            font.pixelSize: 13
            font.bold: true
            Layout.fillWidth: true
        }

        Button {
            text: "刷新"
            Layout.preferredWidth: 58
            onClicked: piano.refreshLocalMidiLibrary()
        }

        Button {
            text: "目录"
            Layout.preferredWidth: 58
            onClicked: piano.openLocalMidiLibrary()
        }
    }

    ComboBox {
        id: categoryCombo
        Layout.fillWidth: true
        model: piano.sheetCategories
        textRole: "name"
        valueRole: "id"

        function syncCurrentCategory() {
            for (let i = 0; i < count; ++i) {
                if (Number(valueAt(i)) === piano.currentSheetCategoryId) {
                    currentIndex = i
                    return
                }
            }
            currentIndex = 0
        }

        Component.onCompleted: syncCurrentCategory()
        onActivated: piano.setLocalSheetCategory(Number(valueAt(index)))

        Connections {
            target: piano
            function onSheetCategoriesChanged() { categoryCombo.syncCurrentCategory() }
            function onSheetCategoryFilterChanged() { categoryCombo.syncCurrentCategory() }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 6

        TextField {
            id: newCategoryName
            Layout.fillWidth: true
            placeholderText: "新建分类"
            selectByMouse: true
            font.pixelSize: 11

            function submit() {
                if (text.trim().length === 0) {
                    return
                }
                piano.createSheetCategory(text)
                clear()
            }

            onAccepted: submit()
        }

        Button {
            id: createCategoryButton
            text: "创建"
            Layout.preferredWidth: 58
            enabled: newCategoryName.text.trim().length > 0
            onClicked: newCategoryName.submit()
        }
    }

    Label {
        Layout.fillWidth: true
        text: piano.localMidiLibraryPath
        color: "#71717a"
        font.pixelSize: 10
        elide: Text.ElideMiddle
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 170
        radius: 7
        color: "#111113"
        border.color: "#2f3036"
        clip: true

        ListView {
            id: localMidiList
            anchors.fill: parent
            anchors.margins: 4
            model: piano.localSheetModel
            spacing: 3
            boundsBehavior: Flickable.StopAtBounds

            delegate: Rectangle {
                id: sheetDelegate
                required property string fileName
                required property string name
                required property int sizeKb
                required property bool knownSheet
                required property int noteCount
                required property int sheetId
                required property var categoryIds
                required property var categoryNames
                required property int index

                function hasCategory(categoryId) {
                    return categoryIds && categoryIds.indexOf(categoryId) >= 0
                }

                function categoryText() {
                    return categoryNames && categoryNames.length > 0
                        ? categoryNames.join(" / ")
                        : "未分类"
                }

                width: localMidiList.width
                height: 54
                radius: 5
                color: mouseArea.containsMouse ? "#27272a" : "#18181b"
                border.color: "#27272a"

                MouseArea {
                    id: mouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: piano.loadLocalMidi(sheetDelegate.index)
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    spacing: 8

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Label {
                            Layout.fillWidth: true
                            text: fileName
                            color: "#e4e4e7"
                            font.pixelSize: 12
                            elide: Text.ElideRight
                        }

                        Label {
                            Layout.fillWidth: true
                            text: sheetDelegate.categoryText()
                            color: knownSheet ? "#a1a1aa" : "#71717a"
                            font.pixelSize: 10
                            elide: Text.ElideRight
                        }
                    }

                    Label {
                        text: knownSheet ? (noteCount > 0 ? noteCount + " notes" : "tracked") : "new"
                        color: knownSheet ? "#22c55e" : "#71717a"
                        font.pixelSize: 10
                    }

                    Label {
                        text: sizeKb + " KB"
                        color: "#71717a"
                        font.pixelSize: 10
                    }

                    Button {
                        text: "分类"
                        Layout.preferredWidth: 48
                        enabled: knownSheet
                        onClicked: categoryMenu.open()

                        Menu {
                            id: categoryMenu

                            Instantiator {
                                model: piano.sheetCategories

                                delegate: MenuItem {
                                    required property var modelData
                                    visible: Number(modelData.id) > 0
                                    text: modelData.name
                                    checkable: true
                                    checked: sheetDelegate.hasCategory(Number(modelData.id))
                                    onTriggered: piano.toggleLocalMidiCategory(sheetDelegate.index, Number(modelData.id))
                                }

                                onObjectAdded: (index, object) => categoryMenu.insertItem(index, object)
                                onObjectRemoved: (index, object) => categoryMenu.removeItem(object)
                            }
                        }
                    }
                }
            }

            Label {
                anchors.centerIn: parent
                visible: localMidiList.count === 0
                text: piano.currentSheetCategoryId === 0 ? "没有 MIDI 文件" : "此分类暂无 MIDI 曲谱"
                color: "#71717a"
                font.pixelSize: 12
            }
        }
    }
}
