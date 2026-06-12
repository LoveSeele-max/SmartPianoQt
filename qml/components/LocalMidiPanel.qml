import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    Layout.fillWidth: true
    spacing: Theme.gapSm

    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.gapSm

        Label {
            text: "Local MIDI"
            color: Theme.textPrimary
            font.pixelSize: Theme.fontBody
            font.bold: true
            Layout.fillWidth: true
        }

        TonalButton {
            text: "刷新"
            Layout.preferredWidth: 58
            onClicked: piano.refreshLocalMidiLibrary()
        }

        TonalButton {
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
        spacing: Theme.gapSm

        TextField {
            id: newCategoryName
            objectName: "globalShortcutTextInput"
            Layout.fillWidth: true
            placeholderText: "新建分类"
            selectByMouse: true
            font.pixelSize: Theme.fontCaption

            function submit() {
                if (text.trim().length === 0) {
                    return
                }
                piano.createSheetCategory(text)
                clear()
            }

            onAccepted: submit()
        }

        PrimaryButton {
            id: createCategoryButton
            text: "创建"
            Layout.preferredWidth: 62
            enabled: newCategoryName.text.trim().length > 0
            onClicked: newCategoryName.submit()
        }
    }

    Label {
        Layout.fillWidth: true
        text: piano.localMidiLibraryPath
        color: Theme.textMuted
        font.pixelSize: 10
        elide: Text.ElideMiddle
    }

    MaterialCard {
        Layout.fillWidth: true
        Layout.preferredHeight: 390
        padding: 6
        cardColor: Theme.surfaceContainer
        strokeColor: "transparent"

        ListView {
            id: localMidiList
            anchors.fill: parent
            model: piano.localSheetModel
            spacing: 5
            boundsBehavior: Flickable.StopAtBounds
            clip: true

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

                function detailText() {
                    var details = knownSheet && noteCount > 0 ? noteCount + " notes" : sizeKb + " KB"
                    return knownSheet ? details : details + " · 新曲"
                }

                width: localMidiList.width
                height: 68
                radius: Theme.radiusMedium
                color: mouseArea.containsMouse ? Theme.hoverSurface : Theme.surface
                border.color: mouseArea.containsMouse ? Theme.primaryContainer : Theme.outline

                MouseArea {
                    id: mouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: piano.loadLocalMidi(sheetDelegate.index)
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.gapSm
                    anchors.rightMargin: Theme.gapSm
                    spacing: Theme.gapSm

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 3

                        Label {
                            Layout.fillWidth: true
                            text: fileName
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontBody
                            font.bold: true
                            elide: Text.ElideRight
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 5

                            StatusChip {
                                text: sheetDelegate.categoryText()
                                chipColor: Theme.surfaceContainerHigh
                                textColor: Theme.textSecondary
                                horizontalPadding: 8
                                Layout.maximumWidth: 126
                            }

                            StatusChip {
                                text: knownSheet ? "已练过" : "新曲"
                                chipColor: knownSheet ? Theme.successContainer : Theme.surfaceContainer
                                textColor: knownSheet ? Theme.success : Theme.textSecondary
                                horizontalPadding: 8
                            }

                            Label {
                                Layout.fillWidth: true
                                text: sheetDelegate.detailText()
                                color: Theme.textMuted
                                font.pixelSize: 10
                                elide: Text.ElideRight
                            }
                        }
                    }

                    TonalButton {
                        text: "⋮"
                        Layout.preferredWidth: 38
                        Layout.preferredHeight: 34
                        onClicked: sheetMenu.open()

                        Menu {
                            id: sheetMenu

                            MenuItem {
                                text: "加载"
                                onTriggered: piano.loadLocalMidi(sheetDelegate.index)
                            }

                            MenuSeparator {}

                            Instantiator {
                                model: piano.sheetCategories

                                delegate: MenuItem {
                                    required property var modelData
                                    visible: Number(modelData.id) > 0
                                    enabled: sheetDelegate.knownSheet
                                    text: modelData.name
                                    checkable: true
                                    checked: sheetDelegate.hasCategory(Number(modelData.id))
                                    onTriggered: piano.toggleLocalMidiCategory(sheetDelegate.index, Number(modelData.id))
                                }

                                onObjectAdded: (index, object) => sheetMenu.insertItem(index + 2, object)
                                onObjectRemoved: (index, object) => sheetMenu.removeItem(object)
                            }
                        }
                    }
                }
            }

            Label {
                anchors.centerIn: parent
                visible: localMidiList.count === 0
                text: piano.currentSheetCategoryId === 0 ? "没有 MIDI 文件" : "此分类暂无 MIDI 曲谱"
                color: Theme.textMuted
                font.pixelSize: Theme.fontBody
            }
        }
    }
}
