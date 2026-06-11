import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    Layout.fillWidth: true
    spacing: Theme.gapSm

    Label {
        text: "MIDI Input"
        color: Theme.textPrimary
        font.pixelSize: Theme.fontBody
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
        spacing: Theme.gapSm

        TonalButton {
            text: "刷新"
            Layout.fillWidth: true
            onClicked: midiInput.refreshPorts()
        }

        PrimaryButton {
            text: "连接"
            Layout.fillWidth: true
            enabled: midiInput.inputPorts.length > 0
            onClicked: midiInput.openPort(midiInputBox.currentIndex)
        }

        TonalButton {
            text: "关闭"
            Layout.fillWidth: true
            onClicked: midiInput.close()
        }
    }

    Label {
        Layout.fillWidth: true
        text: midiInput.backendName + " · " + midiInput.statusText
        color: Theme.textMuted
        font.pixelSize: Theme.fontCaption
        elide: Text.ElideRight
    }
}