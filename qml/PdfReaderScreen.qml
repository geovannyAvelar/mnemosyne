import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

import "components"

// Adobe Acrobat Reader-style continuous vertical scroll through the whole
// document, replacing the old SwipeView's one-page-at-a-time paging.
// pageList (a ListView, which is itself a Flickable) owns all scrolling;
// each PdfContinuousPageItem delegate is a plain Item, not its own
// Flickable — nesting a Flickable-per-page inside pageList would create a
// gesture-ownership conflict between the two for vertical drags, so
// per-page horizontal panning of an over-wide zoomed page isn't supported
// yet (pages center and clip instead) pending a follow-up.
Item {
    id: root

    required property var documentModel

    signal backRequested()

    // Zoom row + page indicator auto-hide after a few seconds so they don't
    // sit over the page while reading, and reappear on a double-tap
    // anywhere on screen (the back button stays visible either way — see
    // topBar below). hideHintVisible briefly explains that gesture right
    // as the controls disappear, since there's otherwise no other affordance
    // hinting a double-tap does anything.
    property bool controlsVisible: true
    property bool hideHintVisible: false

    function showControls() {
        root.controlsVisible = true
        hideControlsTimer.restart()
    }

    Timer {
        id: hideControlsTimer
        interval: 3000
        running: true
        onTriggered: {
            // Don't yank the page-jump field away mid-edit.
            if (root._editingPage) {
                restart()
                return
            }
            root.controlsVisible = false
        }
    }

    onControlsVisibleChanged: {
        if (!controlsVisible) {
            hideHintVisible = true
            hideHintTimer.restart()
        } else {
            hideHintVisible = false
        }
    }

    Timer {
        id: hideHintTimer
        interval: 2000
        onTriggered: root.hideHintVisible = false
    }

    TapHandler {
        // Not a MouseArea, same reasoning as PdfContinuousPageItem's own
        // TapHandler (see there) — doesn't take an exclusive grab, so it
        // coexists with pageList's Flickable drag and the per-page
        // long-press selection handler instead of starving them.
        onDoubleTapped: root.showControls()
    }

    // Every document opens at its natural 100% size (1 PDF point = 1 QML
    // pixel) — no fit-to-width shrinking, regardless of whatever zoom
    // ReadingProgressStore restored from a past session (a user decision,
    // not an oversight).
    Component.onCompleted: {
        highlightsModel.bookHash = root.documentModel.bookHash
        root.documentModel.zoom = 1.0
        pageList.committedZoom = 1.0
        pageList.positionViewAtIndex(root.documentModel.currentPage, ListView.Beginning)
    }

    // Frees the Poppler document and blocks until any in-flight page
    // render finishes (see PdfPageImageProvider::setDocument) as soon as
    // this screen leaves the StackView, not just when the app closes.
    Component.onDestruction: documentModel.close()

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

    // The most a pinch (or the toolbar's − button) can zoom OUT to: the
    // first page's width exactly filling the viewport — zooming out any
    // further would just shrink pages inside empty space rather than show
    // more of them, since the whole document is already visible via
    // scrolling. Using page 0 as the representative width (rather than
    // per-visible-page, which doesn't make sense once several pages of
    // possibly-differing sizes can be on screen at once) is a deliberate,
    // accepted simplification — see the mobile-port plan.
    readonly property real minZoom: documentModel.pageCount > 0 && documentModel.pageSizePoints(0).width > 0
        ? pageList.width / documentModel.pageSizePoints(0).width : 0.25
    readonly property real maxZoom: 4.0

    // Applies newZoom (clamped to [minZoom, maxZoom]), keeping whichever
    // page is at the viewport's vertical center — and how far scrolled
    // into that specific page — the same afterward. This does NOT touch
    // pageList.committedZoom (see its own doc comment) — callers decide
    // separately whether this zoom level is "live" (still gesturing) or
    // should also commit a fresh render.
    //
    // Earlier this rescaled contentY by the raw newZoom/oldZoom ratio
    // against the document's total height, assuming every page (not just
    // the handful currently instantiated within cacheBuffer) scales
    // uniformly. For a long book most pages aren't measured yet — ListView
    // estimates their height until they're actually realized — and that
    // estimate drifts enough in practice to land the view on a visibly
    // different page after zooming, not just a mildly different scroll
    // fraction. Anchoring on the one specific page/item actually visible
    // right now sidesteps that: whatever ListView currently reports for
    // this real, already-measured item's position is authoritative,
    // rather than trusting an estimate spanning the whole document.
    function setZoom(newZoomRaw) {
        const newZoom = Math.min(Math.max(newZoomRaw, root.minZoom), root.maxZoom)
        const oldZoom = root.documentModel.zoom
        if (oldZoom <= 0 || newZoom === oldZoom) {
            return
        }

        const anchorY = pageList.contentY + pageList.height / 2
        const anchorItem = pageList.itemAt(pageList.width / 2, anchorY)
        const fraction = anchorItem ? (anchorY - anchorItem.y) / anchorItem.height : 0

        root.documentModel.zoom = newZoom

        if (anchorItem) {
            // anchorItem.y/height already reflect the new zoom -- their
            // bindings recompute synchronously off documentModel.zoom
            // above -- so this puts the same fractional point on this
            // exact page back under the viewport's center.
            pageList.contentY = anchorItem.y + fraction * anchorItem.height - pageList.height / 2
        }
        pageList.returnToBounds()
    }

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
                pageList.committedZoom = root._pendingRemoteZoom
                root.documentModel.currentPage = root._pendingRemotePosition
                pageList.positionViewAtIndex(root._pendingRemotePosition, ListView.Beginning)
            }
        }
    }

    Rectangle {
        id: topBar
        anchors.top: syncPromptBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 48
        color: Theme.panel
        border.color: Theme.border
        border.width: 1
        z: 10

        RowLayout {
            anchors.fill: parent
            anchors.margins: 4
            spacing: 2

            // Always visible, unlike the zoom controls below — it's the
            // only way back to the library, so auto-hiding it along with
            // the rest would strand the user.
            Button {
                text: "‹"
                flat: true
                onClicked: root.backRequested()
            }

            Item { Layout.fillWidth: true }

            RowLayout {
                spacing: 2
                opacity: root.controlsVisible ? 1 : 0
                visible: opacity > 0
                Layout.alignment: Qt.AlignVCenter
                Behavior on opacity { NumberAnimation { duration: 150 } }

                Button {
                    text: "−" // minus sign
                    flat: true
                    onClicked: {
                        root.setZoom(root.documentModel.zoom - 0.25)
                        pageList.committedZoom = root.documentModel.zoom
                    }
                }

                Text {
                    text: Math.round(root.documentModel.zoom * 100) + "%"
                    color: Theme.mutedText
                    font.pixelSize: 13
                    Layout.alignment: Qt.AlignVCenter
                }

                Button {
                    text: "+"
                    flat: true
                    onClicked: {
                        root.setZoom(root.documentModel.zoom + 0.25)
                        pageList.committedZoom = root.documentModel.zoom
                    }
                }
            }

            Item { Layout.fillWidth: true }
        }
    }

    ListView {
        id: pageList
        anchors.top: topBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: navBar.top
        clip: true
        model: root.documentModel.pageCount
        spacing: 6
        // Keeps roughly two screens' worth of pages warm off-screen in
        // each scroll direction instead of instantiating the whole
        // document at once — essential for a real multi-hundred-page book
        // (this app has been tested against a ~1300-page technical book).
        cacheBuffer: height * 2
        // Qt6 defaults ListView to pooling/reusing delegate instances
        // (resetting their properties for a new index) rather than
        // destroying them when scrolled off-screen — which would mean
        // PdfContinuousPageItem's Component.onDestruction cleanup (for a
        // selection whose page scrolls away) might never fire. Disabling
        // reuse trades a little recycle-construction cost for that
        // cleanup actually being reliable.
        reuseItems: false

        // The resolution delegates should actually re-render Poppler at —
        // deliberately NOT the same as documentModel.zoom (which delegate
        // height/width bind to directly and must update every frame for
        // smooth live pinch feedback). Re-rendering on every pinch frame
        // would be far too slow; this only updates once a gesture (or a
        // toolbar zoom-button tap) settles, matching the single-page
        // reader's original renderScale-tracks-committed-zoom design.
        property real committedZoom: 1.0

        onContentYChanged: {
            // During a live pinch, setZoom() rewrites contentY on every
            // frame to keep scroll position proportional (see its own
            // comment) -- that's an incidental side effect of the zoom
            // math, not the user navigating, so it shouldn't be treated as
            // a page change (the toolbar's "N / total" indicator would
            // otherwise flicker through page numbers while the user is
            // just trying to zoom, which reads as "pages are changing").
            if (pinchHandler.active) {
                return
            }
            const idx = pageList.indexAt(pageList.width / 2, pageList.contentY)
            if (idx >= 0) {
                root.documentModel.currentPage = idx
            }
        }

        delegate: PdfContinuousPageItem {
            documentModel: root.documentModel
            renderScale: Math.max(2.0, pageList.committedZoom)
        }

        PinchHandler {
            id: pinchHandler
            target: null
            minimumRotation: 0
            maximumRotation: 0
            property real baseZoom: 1.0

            onActiveChanged: {
                if (active) {
                    baseZoom = root.documentModel.zoom
                } else {
                    pageList.committedZoom = root.documentModel.zoom
                }
            }
            onScaleChanged: {
                root.setZoom(baseZoom * pinchHandler.scale)
            }
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
            opacity: root.controlsVisible ? 1 : 0
            visible: opacity > 0
            Behavior on opacity { NumberAnimation { duration: 150 } }

            Item { Layout.fillWidth: true }

            // Tap the page number to jump to a specific page — the one
            // navigation affordance the old floating pill didn't have.
            Loader {
                Layout.alignment: Qt.AlignVCenter
                sourceComponent: root._editingPage ? pageJumpField : pageLabel
            }

            Item { Layout.fillWidth: true }
        }
    }

    // Brief explanation of the reappear gesture, shown for a couple of
    // seconds right as the controls above fade out — otherwise a
    // double-tap to bring them back isn't discoverable at all.
    Rectangle {
        id: hideHint
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: navBar.top
        anchors.bottomMargin: 16
        width: hintText.implicitWidth + 24
        height: hintText.implicitHeight + 16
        radius: 8
        color: Theme.panel
        border.color: Theme.border
        border.width: 1
        opacity: root.hideHintVisible ? 0.9 : 0
        visible: opacity > 0
        z: 10
        Behavior on opacity { NumberAnimation { duration: 200 } }

        Text {
            id: hintText
            anchors.centerIn: parent
            text: qsTr("Double-tap to show controls")
            color: Theme.mutedText
            font.pixelSize: 13
        }
    }

    property bool _editingPage: false

    Component {
        id: pageLabel
        Text {
            text: (root.documentModel.currentPage + 1) + " / " + root.documentModel.pageCount
            color: Theme.mutedText
            font.pixelSize: 13
            TapHandler {
                onTapped: root._editingPage = true
            }
        }
    }

    Component {
        id: pageJumpField
        TextField {
            text: (root.documentModel.currentPage + 1).toString()
            font.pixelSize: 13
            horizontalAlignment: TextInput.AlignHCenter
            implicitWidth: 60
            selectByMouse: true
            Component.onCompleted: { forceActiveFocus(); selectAll() }
            onAccepted: {
                const target = Math.min(Math.max(parseInt(text, 10) - 1, 0), root.documentModel.pageCount - 1)
                if (!isNaN(target)) {
                    root.documentModel.currentPage = target
                    pageList.positionViewAtIndex(target, ListView.Beginning)
                }
                root._editingPage = false
            }
            onActiveFocusChanged: {
                if (!activeFocus) {
                    root._editingPage = false
                }
            }
        }
    }
}
