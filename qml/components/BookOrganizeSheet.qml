import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// Modal sheet for assigning one book to shelves (collections) and editing
// its tags -- opened from a library card's "⋯" button (see
// LibraryScreen.qml). A full Popup rather than NotePopup.qml's lightweight
// Rectangle-in-content-space pattern: this needs to sit centered over the
// whole screen (not anchored to one specific item's rect inside a
// Flickable), own its own modal dimming, and hold more controls -- a
// checkable shelf list, a new-shelf field, and a tags field -- than a quick
// glance-and-dismiss popup does.
Popup {
    id: root

    property string bookHash: ""
    property var libraryModel: null
    property var _shelves: []

    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    width: Math.min(340, (parent ? parent.width : 340) - 40)
    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? (parent.height - height) / 2 : 0

    background: Rectangle {
        color: Theme.base
        radius: 12
        border.color: Theme.border
        border.width: 1
    }

    // Re-reads current shelf membership/tags for bookHash -- call right
    // before opening (see LibraryScreen.qml), since allCollections()/
    // collectionsForBook()/tagsForBook() are plain Q_INVOKABLEs with no
    // change notification this could bind to directly.
    function reload() {
        root._shelves = root.libraryModel ? root.libraryModel.allCollections() : []
        tagsField.text = root.libraryModel ? root.libraryModel.tagsForBook(root.bookHash).join(", ") : ""
    }

    contentItem: ColumnLayout {
        spacing: 10

        Text {
            text: qsTr("Organize Book")
            color: Theme.text
            font.pixelSize: 16
            font.bold: true
        }

        Text {
            text: qsTr("SHELVES")
            color: Theme.mutedText
            font.pixelSize: 11
            font.letterSpacing: 1
        }

        Text {
            Layout.fillWidth: true
            visible: root._shelves.length === 0
            text: qsTr("No shelves yet -- add one below.")
            color: Theme.mutedText
            font.pixelSize: 12
        }

        Column {
            Layout.fillWidth: true
            spacing: 2

            Repeater {
                model: root._shelves

                delegate: CheckBox {
                    required property string modelData
                    width: parent ? parent.width : implicitWidth
                    text: modelData
                    // Evaluated once when this delegate is (re)created --
                    // i.e. whenever reload() reassigns root._shelves, which
                    // LibraryScreen.qml does right before opening this sheet
                    // (see there). Toggling below then updates the backend
                    // directly; this binding intentionally doesn't track
                    // that back, the same way any plain Qt Quick CheckBox's
                    // checked ends up owned by user interaction once
                    // pressed.
                    checked: root.libraryModel
                             ? root.libraryModel.collectionsForBook(root.bookHash).indexOf(modelData) >= 0 : false
                    onToggled: {
                        if (checked) {
                            root.libraryModel.addBookToCollection(root.bookHash, modelData)
                        } else {
                            root.libraryModel.removeBookFromCollection(root.bookHash, modelData)
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            TextField {
                id: newShelfField
                Layout.fillWidth: true
                placeholderText: qsTr("New shelf name")
            }

            Button {
                text: qsTr("Add")
                enabled: newShelfField.text.trim().length > 0
                onClicked: {
                    const name = root.libraryModel.createCollection(newShelfField.text)
                    if (name.length > 0) {
                        root.libraryModel.addBookToCollection(root.bookHash, name)
                        newShelfField.text = ""
                        root.reload()
                    }
                }
            }
        }

        Text {
            text: qsTr("TAGS")
            color: Theme.mutedText
            font.pixelSize: 11
            font.letterSpacing: 1
        }

        TextField {
            id: tagsField
            Layout.fillWidth: true
            placeholderText: qsTr("comma, separated, tags")
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Item { Layout.fillWidth: true }

            Button {
                text: qsTr("Save Tags")
                onClicked: {
                    const tags = tagsField.text.split(",").map((t) => t.trim()).filter((t) => t.length > 0)
                    root.libraryModel.setTagsForBook(root.bookHash, tags)
                }
            }

            Button {
                text: qsTr("Close")
                onClicked: root.close()
            }
        }
    }
}
