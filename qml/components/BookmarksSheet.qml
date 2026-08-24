import QtQuick
import QtQuick.Controls.Basic

// Modal list of bookmarks for whichever document BookmarksModel is
// currently scoped to (see BookmarksModel.bookHash, set by the reader
// screen that opens this sheet). Replaces desktop's BookmarksDock —
// a tabified sidebar makes no sense on a phone-sized screen, so this is a
// modal popup instead, opened on demand rather than always visible.
Popup {
    id: sheet

    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    width: parent ? Math.min(420, parent.width * 0.92) : 320
    height: parent ? Math.min(560, parent.height * 0.8) : 420
    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? (parent.height - height) / 2 : 0

    // targetIndex: PDF page index or EPUB spine index to jump to.
    signal jumpRequested(int targetIndex)

    background: Rectangle {
        color: Theme.base
        radius: 14
        border.color: Theme.border
        border.width: 1
    }

    contentItem: Column {
        spacing: 12

        Text {
            text: qsTr("Bookmarks")
            font.pixelSize: 18
            font.bold: true
            color: Theme.text
        }

        Text {
            visible: bookmarksListView.count === 0
            text: qsTr("No bookmarks yet.")
            color: Theme.mutedText
            font.pixelSize: 14
        }

        ListView {
            id: bookmarksListView
            width: sheet.width - 32 // sheet's own left/right padding
            height: sheet.height - 80
            clip: true
            visible: count > 0
            model: bookmarksModel

            delegate: Rectangle {
                width: bookmarksListView.width
                height: 52
                color: rowMouseArea.pressed ? Theme.raisedHover : "transparent"
                radius: 8

                MouseArea {
                    id: rowMouseArea
                    anchors.fill: parent
                    anchors.rightMargin: 44
                    onClicked: {
                        sheet.jumpRequested(model.targetIndex)
                        sheet.close()
                    }
                }

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 8
                    anchors.right: removeButton.left
                    anchors.verticalCenter: parent.verticalCenter
                    elide: Text.ElideRight
                    text: model.label.length > 0
                          ? model.label
                          : qsTr("Position %1").arg(model.targetIndex + 1)
                    color: Theme.text
                    font.pixelSize: 15
                }

                Button {
                    id: removeButton
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    text: "✕"
                    flat: true
                    onClicked: bookmarksModel.removeBookmarkAt(index)
                }
            }
        }
    }
}
