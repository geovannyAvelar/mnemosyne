import QtQuick
import QtQuick.Controls.Basic

import "components"

Item {
    id: root

    required property var documentModel

    // Frees the Poppler document and blocks until any in-flight page
    // render finishes (see PdfPageImageProvider::setDocument) as soon as
    // this screen leaves the StackView, not just when the app closes.
    Component.onDestruction: documentModel.close()

    Rectangle {
        anchors.fill: parent
        color: Theme.panel
    }

    SwipeView {
        id: swipeView
        anchors.fill: parent
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

    Rectangle {
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 8
        width: pageLabel.implicitWidth + 24
        height: pageLabel.implicitHeight + 10
        radius: height / 2
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
