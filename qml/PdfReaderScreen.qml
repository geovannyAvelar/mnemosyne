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

    // Top bar (back + zoom) and bottom nav bar (page indicator) toggle
    // together on a single tap anywhere on the document — no auto-hide
    // timer; the reader stays however the user last left it. Each bar
    // slides fully off-screen as a whole (top bar up, bottom bar down)
    // rather than just fading its contents in place, matching Acrobat's
    // mobile reader. The OS back gesture still works while the bars are
    // hidden.
    property bool controlsVisible: true

    function toggleControls() {
        root.controlsVisible = !root.controlsVisible
    }

    onControlsVisibleChanged: root.updateSyncPromptVisibility()

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
            root._pendingSyncMessage = qsTr("Synced position available: page %1 (from %2) — jump?")
                                        .arg(position + 1).arg(deviceName)
            root._pendingRemotePosition = position
            root._pendingRemoteZoom = zoom
            root.updateSyncPromptVisibility()
        }
    }

    property int _pendingRemotePosition: -1
    property real _pendingRemoteZoom: 1.0
    property string _pendingSyncMessage: ""

    // Keeps the sync banner from popping up over the page uninvited while
    // the top/bottom bars are hidden — it only shows alongside them (once
    // the user taps to reveal controls), and hides again the moment they
    // tap to hide the bars. Dismissing or jumping clears the pending
    // message so it doesn't reappear next time the bars are shown.
    function updateSyncPromptVisibility() {
        if (root._pendingSyncMessage !== "" && root.controlsVisible) {
            syncPromptBar.showPrompt(root._pendingSyncMessage)
        } else {
            syncPromptBar.visible = false
        }
    }

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
    // point sits at (anchorX, anchorY) fixed under that same screen point
    // afterward — both vertically (which page, and how far scrolled into
    // it) and horizontally (how far panned into an over-wide zoomed page,
    // for every page currently visible, not just the one exactly under the
    // anchor — see the loop below). anchorX is in pageList's own
    // coordinates (viewport-relative — pageList itself never scrolls
    // horizontally, so that's also content-absolute); anchorY is content-
    // absolute (i.e. already includes contentY), matching what
    // PinchHandler's centroid.position reports for a handler declared
    // directly inside a Flickable/ListView (its parentItem for positioning
    // purposes turns out to be the Flickable's contentItem, not the outer
    // viewport Item — confirmed by on-device logging). Both default to the
    // viewport's center for the toolbar's +/- buttons. A live pinch passes
    // its current centroid every frame, so the anchor tracks the fingers
    // as they move rather than staying fixed at wherever the gesture
    // started.
    // This does NOT touch pageList.committedZoom (see its own doc
    // comment) — callers decide separately whether this zoom level is
    // "live" (still gesturing) or should also commit a fresh render.
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
    function setZoom(newZoomRaw, anchorX, anchorY) {
        const newZoom = Math.min(Math.max(newZoomRaw, root.minZoom), root.maxZoom)
        const oldZoom = root.documentModel.zoom
        if (oldZoom <= 0 || newZoom === oldZoom) {
            return
        }

        const ax = anchorX !== undefined ? anchorX : pageList.width / 2
        const anchorAbsY = anchorY !== undefined ? anchorY : (pageList.contentY + pageList.height / 2)
        // The anchor's on-screen (viewport-relative) Y, captured before
        // zoom changes contentY below — this is what has to still be
        // under the same finger position afterward.
        const viewportY = anchorAbsY - pageList.contentY
        const anchorItem = pageList.itemAt(ax, anchorAbsY)
        const verticalFraction = anchorItem ? (anchorAbsY - anchorItem.y) / anchorItem.height : 0

        // Horizontal panning is per-delegate (each page is its own
        // Flickable), so with several pages visible at once (common once
        // zoomed out enough that a page's height is less than the
        // viewport's), only compensating the one page the pinch happens to
        // be exactly over left every other visible page's content just
        // growing from its own untouched left edge instead of zooming in
        // place along with the rest. Capturing every visible delegate's
        // own horizontal fraction up front, before the zoom changes
        // anything, and reapplying it to each afterward makes the whole
        // visible stack zoom together as one continuous view, the way
        // Acrobat does.
        const firstVisibleIndex = pageList.indexAt(ax, pageList.contentY)
        const lastVisibleIndex = pageList.indexAt(ax, pageList.contentY + pageList.height - 1)
        const visiblePans = []
        if (firstVisibleIndex >= 0 && lastVisibleIndex >= firstVisibleIndex) {
            for (let i = firstVisibleIndex; i <= lastVisibleIndex; i++) {
                const item = pageList.itemAtIndex(i)
                if (item) {
                    visiblePans.push({
                        item: item,
                        fraction: item.contentWidth > 0 ? (item.contentX + ax) / item.contentWidth : 0
                    })
                }
            }
        }

        const anchorIndex = anchorItem ? anchorItem.index : -1

        root.documentModel.zoom = newZoom

        if (anchorItem) {
            // anchorItem.height already reflects the new zoom -- its
            // binding recomputes synchronously off documentModel.zoom
            // above. anchorItem.y, though, is the cumulative height of
            // every PRECEDING page — for a page deep into a long document,
            // most of those precede the cache buffer and are estimated
            // rather than individually measured, so trusting a raw read of
            // anchorItem.y here could drift by roughly a page's worth once
            // that estimate hasn't caught up to the new zoom yet (this is
            // what made a pinch appear to anchor on the previous page).
            // positionViewAtIndex is the one operation Qt Quick guarantees
            // to resolve correctly for any index regardless of estimates —
            // it's what page-jump and cross-device sync already rely on
            // elsewhere in this file — so snap this page's top to the
            // viewport's top first, then nudge from there.
            pageList.positionViewAtIndex(anchorIndex, ListView.Beginning)
            pageList.contentY = pageList.contentY + verticalFraction * anchorItem.height - viewportY
        }
        for (const pan of visiblePans) {
            const item = pan.item
            const maxContentX = Math.max(0, item.contentWidth - item.width)
            item.contentX = Math.max(0, Math.min(pan.fraction * item.contentWidth - ax, maxContentX))
            item.returnToBounds()
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
            root._pendingSyncMessage = ""
        }
        onDismissed: root._pendingSyncMessage = ""
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
        // Floats above pageList (which now fills the whole screen) rather
        // than sitting beside it.
        z: 10
        // Whole bar hides — including the back button — so reading is
        // fully immersive; the OS back gesture still works meanwhile, and
        // a tap brings the bar (and the button) right back.
        enabled: root.controlsVisible

        transform: Translate {
            y: root.controlsVisible ? 0 : -topBar.height
            Behavior on y { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
        }

        // Blocks a tap anywhere in the bar's empty space (the fillWidth
        // spacers below have no input handling of their own) from falling
        // through to the document's tap-to-toggle MouseArea underneath —
        // without this, tapping just slightly off one of the real buttons
        // closed the bar you were trying to use instead of doing nothing.
        MouseArea { anchors.fill: parent }

        RowLayout {
            anchors.fill: parent
            anchors.margins: 4
            spacing: 2

            Button {
                text: "‹"
                flat: true
                onClicked: root.backRequested()
            }

            Item { Layout.fillWidth: true }

            RowLayout {
                spacing: 2
                Layout.alignment: Qt.AlignVCenter

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
        // Fills the whole screen at all times — topBar and navBar float
        // above it as overlays (see their z: 10) rather than squeezing it
        // between two reserved strips, so hiding them actually gives the
        // document the full screen instead of leaving a blank gap where
        // the bars used to sit.
        anchors.fill: parent
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
            // frame to keep the pinch's anchor point fixed on screen (see
            // its own comment) -- that's an incidental side effect of the
            // zoom math, not the user navigating, so it shouldn't be
            // treated as a page change (the toolbar's "N / total"
            // indicator would otherwise flicker through page numbers while
            // the user is just trying to zoom, which reads as "pages are
            // changing").
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
            zoomGestureActive: pinchHandler.active
            onTapped: root.toggleControls()
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
                root.setZoom(baseZoom * pinchHandler.scale,
                             pinchHandler.centroid.position.x, pinchHandler.centroid.position.y)
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
        // Floats above pageList (which now fills the whole screen) rather
        // than sitting beside it.
        z: 10
        // Blocks taps on the page label/field while the bar is slid
        // off-screen.
        enabled: root.controlsVisible

        transform: Translate {
            y: root.controlsVisible ? 0 : navBar.height
            Behavior on y { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
        }

        // Blocks a tap anywhere in the bar's empty space (the fillWidth
        // spacers below have no input handling of their own) from falling
        // through to the document's tap-to-toggle MouseArea underneath —
        // without this, tapping just slightly off the page number closed
        // the bar instead of opening the page-jump field.
        MouseArea { anchors.fill: parent }

        RowLayout {
            anchors.fill: parent
            anchors.margins: 8

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
