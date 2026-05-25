import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    Layout.fillWidth: true
    spacing: 8

    Label {
        text: "MIDI Input"
        color: "#e4e4e7"
        font.pixelSize: 13
        font.bold: true
    }

    ComboBox {
        id: midiInputBox
        Layout.fillWidth: true
        model: midiInput.inputPorts
        enabled: midiInput.inputPorts.length > 0
        displayText: midiInput.inputPorts.length > 0 ? currentText : "未检测到设备"
    }

    RowLayout {
        Layout.fillWidth: true

        Button {
            text: "刷新"
            Layout.fillWidth: true
            onClicked: midiInput.refreshPorts()
        }

        Button {
            text: "连接"
            Layout.fillWidth: true
            enabled: midiInput.inputPorts.length > 0
            onClicked: midiInput.openPort(midiInputBox.currentIndex)
        }

        Button {
            text: "关闭"
            Layout.fillWidth: true
            onClicked: midiInput.close()
        }
    }

    Label {
        Layout.fillWidth: true
        text: midiInput.statusText
        color: "#71717a"
        font.pixelSize: 10
        elide: Text.ElideRight
    }
}
