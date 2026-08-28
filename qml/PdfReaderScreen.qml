import QtQuick
import QtQuick.Controls.Basic

import "components"

Item {
    id: root

    required property var documentModel

    Component.onCompleted: {
        highlightsModel.bookHash = root.documentModel.bookHash
    }

    // Frees the Poppler document and blocks until any in-flight page
    // render finishes (see PdfPageImageProvider::setDocument) as soon as
    // this screen leaves the StackView, not just when the app closes.
    Component.onDestruction: documentModel.close()

    Connections {
        target: root.documentModel
        function onCurrentPageChanged() {
            // A selection belongs to one page (PdfSelectionController is
            // shared across all page delegates) — drop it when the page
            // changes rather than leave a stale selection the user can no
            // longer see the source words for.
            pdfSelectionController.clearSelection()
        }
    }

    Connections {
        target: root.documentModel
        function onRemoteProgressAvailable(position, zoom, deviceName) {
            syncPromptBar.showPrompt(qsTr("Synced position available: page %1 (from %2) — jump?")
                                      .arg(position + 1).arg(deviceName))
            root._pendingRemotePosition = position
            root._pendingRemoteZoom = zoom
        }
    }

    property int _pendingRemotePosition: -1
    property real _pendingRemoteZoom: 1.0

    Rectangle {
        anchors.fill: parent
        color: Theme.panel
    }

    SyncPromptBar {
        id: syncPromptBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        z: 10
        onJumpRequested: {
            if (root._pendingRemotePosition >= 0) {
                root.documentModel.zoom = root._pendingRemoteZoom
                root.documentModel.currentPage = root._pendingRemotePosition
            }
        }
    }

    SwipeView {
        id: swipeView
        anchors.top: syncPromptBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        currentIndex: root.documentModel.currentPage
        onCurrentIndexChanged: root.documentModel.currentPage = currentIndex

        Repeater {
            model: root.documentModel.pageCount
            delegate: PdfPageItem {
                // Qt 6's required-property delegate binding only injects
                // "index" automatically into a property actually named
                // "index" — pageIndex needs it declared here explicitly and
                // then forwarded.
                required property int index
                pageIndex: index
                documentModel: root.documentModel
            }
        }
    }

    Row {
        anchors.top: syncPromptBar.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 8
        spacing: 8

        Rectangle {
            width: pageLabel.implicitWidth + 24
            height: 40
            radius: 20
            color: Theme.base
            border.color: Theme.border
            border.width: 1

            Text {
                id: pageLabel
                anchors.centerIn: parent
                text: (root.documentModel.currentPage + 1) + " / " + root.documentModel.pageCount
                color: Theme.mutedText
                font.pixelSize: 13
            }
        }
    }
}
