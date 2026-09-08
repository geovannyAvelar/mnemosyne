import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

import "components"

Item {
    id: root

    signal fileActivated(string filePath, string title, string format)
    signal settingsRequested()

    // Shelves/tags (see app/CollectionStore.h, app/TagStore.h) filter the
    // grid below via libraryModel's own collectionFilter/tagFilter
    // properties -- these two just cache what to show as filter chips
    // (allCollections()/allTags() are plain Q_INVOKABLEs, not NOTIFYing
    // properties a Repeater could bind to directly). Refreshed on load and
    // whenever BookOrganizeSheet closes, since that's the only place a
    // shelf gets created or a tag gets typed for the first time.
    property var _shelfNames: []
    property var _tagNames: []
    function refreshFilterChips() {
        root._shelfNames = libraryModel.allCollections()
        root._tagNames = libraryModel.allTags()
    }

    BookOrganizeSheet {
        id: organizeSheet
        parent: root
        libraryModel: libraryModel
        onClosed: root.refreshFilterChips()
    }

    property string _errorMessage: ""
    function showError(message) {
        root._errorMessage = message
        errorClearTimer.restart()
    }
    Timer {
        id: errorClearTimer
        interval: 5000
        onTriggered: root._errorMessage = ""
    }

    function guessFormat(name) {
        const lower = name.toLowerCase()
        if (lower.endsWith(".epub")) return "epub"
        if (lower.endsWith(".pdf")) return "pdf"
        return "unknown"
    }

    Connections {
        target: documentPicker
        function onDocumentPicked(uri, displayName) {
            libraryModel.recordOpened(uri, displayName, root.guessFormat(displayName))
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.window
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 16

        RowLayout {
            Layout.fillWidth: true

            Text {
                text: qsTr("Library")
                color: Theme.text
                font.pixelSize: 32
                font.bold: true
            }

            Item { Layout.fillWidth: true }

            Button {
                text: "⚙"
                flat: true
                onClicked: root.settingsRequested()
            }
        }

        Button {
            id: openButton
            text: qsTr("Open Document…")
            onClicked: documentPicker.pickDocument()

            background: Rectangle {
                implicitHeight: 44
                radius: 10
                color: openButton.pressed ? Theme.accentPressed
                     : openButton.hovered ? Theme.accentHover
                                           : Theme.accent
            }
            contentItem: Text {
                text: openButton.text
                color: Theme.accentText
                font.pixelSize: 16
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }

        Rectangle {
            Layout.fillWidth: true
            visible: root._errorMessage.length > 0
            implicitHeight: errorText.implicitHeight + 20
            radius: 10
            color: Theme.panel
            border.color: Theme.border
            border.width: 1

            Text {
                id: errorText
                anchors.fill: parent
                anchors.margins: 10
                text: root._errorMessage
                color: Theme.text
                font.pixelSize: 13
                wrapMode: Text.WordWrap
            }
        }

        // Shelves and tags both filter the same grid below (AND'd together
        // when both are set) via libraryModel.collectionFilter/tagFilter --
        // "All"/"All Tags" (empty string) is always the first chip in each
        // row.
        ListView {
            Layout.fillWidth: true
            visible: root._shelfNames.length > 0
            implicitHeight: 32
            orientation: ListView.Horizontal
            spacing: 6
            model: [""].concat(root._shelfNames)
            delegate: Rectangle {
                required property string modelData
                readonly property bool isSelected: libraryModel.collectionFilter === modelData
                height: 28
                width: chipLabel.implicitWidth + 20
                radius: height / 2
                color: isSelected ? Theme.accent : Theme.panel
                border.color: Theme.border
                border.width: isSelected ? 0 : 1

                Text {
                    id: chipLabel
                    anchors.centerIn: parent
                    text: modelData.length > 0 ? modelData : qsTr("All")
                    color: parent.isSelected ? Theme.accentText : Theme.text
                    font.pixelSize: 12
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: libraryModel.collectionFilter = parent.modelData
                }
            }
        }

        ListView {
            Layout.fillWidth: true
            visible: root._tagNames.length > 0
            implicitHeight: 32
            orientation: ListView.Horizontal
            spacing: 6
            model: [""].concat(root._tagNames)
            delegate: Rectangle {
                required property string modelData
                readonly property bool isSelected: libraryModel.tagFilter === modelData
                height: 28
                width: tagChipLabel.implicitWidth + 20
                radius: height / 2
                color: isSelected ? Theme.accent : Theme.panel
                border.color: Theme.border
                border.width: isSelected ? 0 : 1

                Text {
                    id: tagChipLabel
                    anchors.centerIn: parent
                    text: modelData.length > 0 ? modelData : qsTr("All Tags")
                    color: parent.isSelected ? Theme.accentText : Theme.text
                    font.pixelSize: 12
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: libraryModel.tagFilter = parent.modelData
                }
            }
        }

        Text {
            text: qsTr("RECENT DOCUMENTS")
            color: Theme.mutedText
            font.pixelSize: 12
            font.letterSpacing: 1
        }

        Text {
            visible: libraryGrid.count === 0
            text: libraryModel.collectionFilter.length === 0 && libraryModel.tagFilter.length === 0
                  ? qsTr("No recent documents yet.") : qsTr("No books match this filter.")
            color: Theme.mutedText
            font.pixelSize: 14
        }

        GridView {
            id: libraryGrid
            Layout.fillWidth: true
            Layout.fillHeight: true
            cellWidth: 168
            cellHeight: 220
            model: libraryModel
            clip: true

            delegate: Item {
                width: libraryGrid.cellWidth
                height: libraryGrid.cellHeight

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 8
                    radius: 12
                    color: Theme.base
                    border.color: Theme.border
                    border.width: 1

                    MouseArea {
                        anchors.fill: parent
                        onClicked: root.fileActivated(model.filePath, model.title, model.format)
                    }

                    // Opens BookOrganizeSheet for this card's book -- a
                    // dedicated small button rather than a long-press,
                    // since Library has no other gesture affordance to
                    // establish that convention (unlike e.g.
                    // PdfContinuousPageItem's long-press-to-select, already
                    // meaningful there). Hidden for a pre-hash Recent entry
                    // (empty contentHash -- see LibraryModel::ContentHashRole's
                    // doc comment): there's nothing stable to key a shelf/tag
                    // assignment on until it's re-opened and re-recorded.
                    Button {
                        visible: model.contentHash.length > 0
                        anchors.top: parent.top
                        anchors.right: parent.right
                        anchors.margins: 4
                        text: "⋯"
                        flat: true
                        implicitWidth: 28
                        implicitHeight: 28
                        onClicked: {
                            organizeSheet.bookHash = model.contentHash
                            organizeSheet.reload()
                            organizeSheet.open()
                        }
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 6

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 120
                            radius: 8
                            color: Theme.panel

                            Text {
                                anchors.centerIn: parent
                                text: model.format.toUpperCase()
                                color: Theme.mutedText
                                font.pixelSize: 14
                                font.bold: true
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: model.title
                            color: Theme.text
                            font.pixelSize: 14
                            elide: Text.ElideRight
                            maximumLineCount: 2
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }
        }
    }

    Component.onCompleted: {
        libraryModel.refresh()
        root.refreshFilterChips()
    }
}
