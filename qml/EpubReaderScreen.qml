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
// it — the bottom nav bar and top action row are siblings occupying their
// own screen regions, not overlays, for exactly that reason. Chapter
// navigation is explicit prev/next buttons rather than swipe, since a
// swipe gesture would conflict with the WebView's own internal scrolling.
//
// Text selection: unlike the PDF reader, no custom touch handling is
// needed here at all — WebView is a real native android.webkit.WebView, so
// Android's own long-press-to-select (native handles, magnifier, the OS
// selection toolbar) already works for free. Qt's WebView QML API exposes
// no selection-changed signal and no way to add a custom entry to that
// native toolbar, though, so "Highlight" is a QML button the reader taps
// *after* selecting text natively — it just asks the page for whatever's
// currently selected via runJavaScript(), rather than trying to react to
// the selection changing.
Item {
    id: root

    required property var documentModel

    Component.onDestruction: documentModel.close()

    Connections {
        target: highlightsModel
        function onModelReset() { root.applyHighlightsToChapter() }
    }

    Connections {
        target: root.documentModel
        function onRemoteProgressAvailable(spineIndex, deviceName) {
            syncPromptBar.showPrompt(qsTr("Synced position available: chapter %1 (from %2) — jump?")
                                      .arg(spineIndex + 1).arg(deviceName))
            root._pendingRemoteSpineIndex = spineIndex
        }
    }

    property int _pendingRemoteSpineIndex: -1

    Rectangle {
        anchors.fill: parent
        color: Theme.base
    }

    SyncPromptBar {
        id: syncPromptBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        z: 10
        onJumpRequested: {
            if (root._pendingRemoteSpineIndex >= 0) {
                root.documentModel.currentSpineIndex = root._pendingRemoteSpineIndex
            }
        }
    }

    WebView {
        id: webView
        anchors.top: topBar.bottom
        anchors.bottom: navBar.top
        anchors.left: parent.left
        anchors.right: parent.right
        onLoadingChanged: (loadRequest) => {
            if (loadRequest.status === WebView.LoadSucceededStatus) {
                root.applyHighlightsToChapter()
            }
        }
    }

    function loadCurrentChapter() {
        webView.loadHtml(root.documentModel.currentChapterHtml)
    }

    // Wraps every occurrence of each persisted highlight's text in a
    // <mark> via a small DOM-walking script — the JS equivalent of desktop
    // EpubView::applyHighlightsToBrowser()'s QTextDocument::find() loop,
    // since there's no QTextDocument here to search. Per-text-node matching
    // only (doesn't span element boundaries), same practical scope as
    // desktop's approach not handling every conceivable rich-text edge case.
    function applyHighlightsToChapter() {
        const entries = highlightsModel.highlightsForTarget(root.documentModel.currentSpineIndex)
        const texts = entries.map((entry) => entry.text).filter((text) => text.length > 0)
        if (texts.length === 0) {
            return
        }
        const script = "(function(){" +
            "var highlights=" + JSON.stringify(texts) + ";" +
            "function walk(node){" +
            "  if(node.nodeType===3){" +
            "    var text=node.nodeValue;" +
            "    for(var i=0;i<highlights.length;i++){" +
            "      var h=highlights[i];" +
            "      var idx=text.indexOf(h);" +
            "      if(idx>=0){" +
            "        var before=text.substring(0,idx);" +
            "        var after=text.substring(idx+h.length);" +
            "        var mark=document.createElement('mark');" +
            "        mark.style.backgroundColor='rgba(217,119,86,0.4)';" +
            "        mark.textContent=text.substring(idx,idx+h.length);" +
            "        var afterNode=document.createTextNode(after);" +
            "        var parent=node.parentNode;" +
            "        parent.insertBefore(document.createTextNode(before),node);" +
            "        parent.insertBefore(mark,node);" +
            "        parent.insertBefore(afterNode,node);" +
            "        parent.removeChild(node);" +
            "        walk(afterNode);" +
            "        return;" +
            "      }" +
            "    }" +
            "  } else if(node.nodeType===1 && node.tagName!=='SCRIPT' && node.tagName!=='STYLE' && node.tagName!=='MARK'){" +
            "    var children=[];" +
            "    for(var j=0;j<node.childNodes.length;j++){children.push(node.childNodes[j]);}" +
            "    for(var k=0;k<children.length;k++){walk(children[k]);}" +
            "  }" +
            "}" +
            "walk(document.body);" +
            "})();"
        webView.runJavaScript(script)
    }

    Connections {
        target: root.documentModel
        function onCurrentSpineIndexChanged() { root.loadCurrentChapter() }
    }

    Component.onCompleted: {
        highlightsModel.bookHash = root.documentModel.bookHash
        loadCurrentChapter()
    }

    // Highlight action, separate from chapter navigation below — combining
    // both rows overflowed a single bottom bar (Qt Quick Layouts collapses
    // items that don't fit to zero size rather than wrapping or clipping,
    // which silently made the Next button untappable).
    Rectangle {
        id: topBar
        anchors.top: syncPromptBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 48
        color: Theme.panel
        border.color: Theme.border
        border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.margins: 4
            spacing: 2

            Button {
                text: qsTr("Highlight")
                flat: true
                onClicked: {
                    webView.runJavaScript("window.getSelection().toString()", (result) => {
                        if (result && result.length > 0) {
                            highlightsModel.addHighlight(root.documentModel.currentSpineIndex, Qt.rect(0, 0, 0, 0), result)
                            webView.runJavaScript("window.getSelection().removeAllRanges()")
                        }
                    })
                }
            }

            Item { Layout.fillWidth: true }
        }
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

            Text {
                text: (root.documentModel.currentSpineIndex + 1) + " / " + root.documentModel.spineCount
                color: Theme.mutedText
                font.pixelSize: 13
                Layout.alignment: Qt.AlignVCenter
            }

            Item { Layout.fillWidth: true }

            Button {
                text: "Next ›"
                enabled: root.documentModel.currentSpineIndex < root.documentModel.spineCount - 1
                onClicked: root.documentModel.nextChapter()
            }
        }
    }
}
