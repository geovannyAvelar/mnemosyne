import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// Dismissible banner offering to jump to a reading position GoogleDriveSync
// found on another device — the QML port of desktop's SyncPromptBar
// (ui/SyncPromptBar.h/.cpp), used the same way: reader screens call
// showPrompt() when documentModel.remoteProgressAvailable fires, and
// connect to jumpRequested to act on it. Anchored above the reader
// screen's own content (not an overlay on top of it) so a visible prompt
// pushes pages/chapters down rather than covering the first few lines —
// callers anchor their content's top to this item's bottom, which tracks
// implicitHeight automatically as visible toggles.
Rectangle {
    id: root

    signal jumpRequested()
    signal dismissed()

    property alias message: messageText.text

    function showPrompt(text) {
        message = text
        visible = true
    }

    visible: false
    color: "#D97757"
    implicitHeight: visible ? contentRow.implicitHeight + 16 : 0
    clip: true

    // Blocks a tap on the message text (which has no input handling of
    // its own) from falling through to whatever's behind this banner —
    // callers may sit content directly underneath it.
    MouseArea { anchors.fill: parent }

    RowLayout {
        id: contentRow
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.margins: 12
        spacing: 8

        Text {
            id: messageText
            Layout.fillWidth: true
            color: "white"
            font.pixelSize: 13
            font.weight: Font.Medium
            wrapMode: Text.WordWrap
        }

        Button {
            text: qsTr("Jump")
            flat: true
            palette.buttonText: "white"
            onClicked: {
                root.visible = false
                root.jumpRequested()
            }
        }

        Button {
            text: qsTr("Dismiss")
            flat: true
            palette.buttonText: "white"
            onClicked: {
                root.visible = false
                root.dismissed()
            }
        }
    }
}
