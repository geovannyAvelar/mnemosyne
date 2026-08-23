import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtWebView

import "components"

// WebView renders EpubDocument::chapterHtml()'s already-self-contained
// HTML (inlined CSS, data: URI images) via the platform's native
// android.webkit.WebView — closer to desktop's QTextBrowser rendering
// contract than QML's own limited rich-text engine. Trade-off accepted per
// the mobile-port plan: WebView is a native view composited outside Qt
// Quick's scene graph, so nothing can draw a translucent overlay on top of
// it — the nav bar below is a sibling occupying its own screen region, not
// an overlay, for exactly that reason. Chapter navigation is explicit
// prev/next buttons rather than swipe, since a swipe gesture would conflict
// with the WebView's own internal scrolling.
Item {
    id: root

    required property var documentModel

    property bool currentChapterBookmarked: false
    function refreshBookmarkedState() {
        currentChapterBookmarked = bookmarksModel.isBookmarked(root.documentModel.currentSpineIndex)
    }

    Component.onDestruction: documentModel.close()

    Connections {
        target: bookmarksModel
        function onModelReset() { root.refreshBookmarkedState() }
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.base
    }

    WebView {
        id: webView
        anchors.top: parent.top
        anchors.bottom: navBar.top
        anchors.left: parent.left
        anchors.right: parent.right
    }

    function loadCurrentChapter() {
        webView.loadHtml(root.documentModel.currentChapterHtml)
        root.refreshBookmarkedState()
    }

    Connections {
        target: root.documentModel
        function onCurrentSpineIndexChanged() { root.loadCurrentChapter() }
    }

    Component.onCompleted: {
        bookmarksModel.bookHash = root.documentModel.bookHash
        loadCurrentChapter()
    }

    Rectangle {
        id: navBar
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 56
        color: Theme.panel
        border.color: Theme.border
        border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.margins: 8

            Button {
                text: "‹ Prev"
                enabled: root.documentModel.currentSpineIndex > 0
                onClicked: root.documentModel.previousChapter()
            }

            Item { Layout.fillWidth: true }

            Button {
                text: "☰"
                flat: true
                onClicked: bookmarksSheet.open()
            }

            Text {
                text: (root.documentModel.currentSpineIndex + 1) + " / " + root.documentModel.spineCount
                color: Theme.mutedText
                font.pixelSize: 13
                Layout.alignment: Qt.AlignVCenter
            }

            Button {
                text: "★"
                flat: true
                palette.buttonText: root.currentChapterBookmarked ? Theme.accent : Theme.mutedText
                onClicked: bookmarksModel.toggleBookmark(
                    root.documentModel.currentSpineIndex,
                    root.documentModel.title)
            }

            Item { Layout.fillWidth: true }

            Button {
                text: "Next ›"
                enabled: root.documentModel.currentSpineIndex < root.documentModel.spineCount - 1
                onClicked: root.documentModel.nextChapter()
            }
        }
    }

    BookmarksSheet {
        id: bookmarksSheet
        parent: root
        onJumpRequested: (targetIndex) => root.documentModel.currentSpineIndex = targetIndex
    }
}
