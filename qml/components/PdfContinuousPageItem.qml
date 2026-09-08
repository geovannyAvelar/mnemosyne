import QtQuick
import QtQuick.Controls.Basic

// One page inside PdfReaderScreen's continuous-scroll ListView (Adobe
// Reader-style: every page flows one after another in a single vertical
// scroll, rather than the old SwipeView's one-page-at-a-time paging).
// Pinch-zoom is owned by one PinchHandler at the PdfReaderScreen/ListView
// level (see there for why), so this delegate only needs to scroll
// HORIZONTALLY, for whatever a page zoomed in past the viewport's width
// overflows by — vertical page-to-page scrolling is entirely the outer
// ListView's job. A horizontal-only Flickable nested inside a vertical one
// doesn't fight it for gestures the way two same-axis Flickables would:
// QtQuick's own drag-direction disambiguation lets a single-finger
// horizontal drag settle here while a vertical drag (or a two-finger
// pinch, owned by the ListView's PinchHandler) passes through untouched —
// the same reasoning that already makes nested horizontal ListViews
// inside a vertical one a common, working pattern elsewhere.
//
// height is a real layout property (pointSize.height * documentModel.zoom),
// not a transform — ListView computes total scroll extent by summing
// delegate heights, so the displayed size has to actually match layout
// size or pages would visually overlap/gap as zoom changes.
Flickable {
    id: root

    required property int index
    required property var documentModel

    // Tap-anywhere-on-the-page-to-toggle-controls, bubbled up to
    // PdfReaderScreen. Handled here (nested inside this per-page Flickable,
    // via the sibling MouseArea below) rather than at the outer ListView
    // level — a tap detector placed *outside* this Flickable has no
    // cooperative relationship with its horizontal drag-to-pan, so it just
    // wins every touch outright and pan never gets a chance to start. Only
    // being a genuine child of the exact Flickable whose gesture it must
    // coexist with lets Qt Quick's childMouseEventFilter arbitrate between
    // the two, the same mechanism that already lets the outer ListView's
    // vertical scroll coexist with a tap.
    signal tapped()

    readonly property size pointSize: documentModel.pageSizePoints(index)
    // The resolution Poppler last rasterized this page at — kept in sync
    // with documentModel.zoom by PdfReaderScreen's PinchHandler once a
    // pinch settles (not during the live gesture itself, which only needs
    // this Item's height/pageImage's width-height to update, both cheap
    // layout, not a fresh render). See PdfPageImageProvider — the "id"
    // string it parses is "<index>-<scale>".
    property real renderScale: 2.0

    // Set by PdfReaderScreen while its outer PinchHandler is active, so a
    // two-finger touch drives zoom only — without this, incidental hand
    // movement mid-pinch could also register as a single-finger drag here,
    // panning this page horizontally at the same time as it zooms.
    property bool zoomGestureActive: false

    // Per-document page color inversion, set by PdfReaderScreen from its own
    // pageDarkMode toggle -- deliberately NOT themeSettings.dark (the
    // app-wide Dark Mode setting), which only restyles UI chrome elsewhere
    // and has no effect on rendered PDF pages. See that property's doc
    // comment for why.
    property bool pageDarkMode: false

    width: ListView.view ? ListView.view.width : 0
    height: pointSize.height * documentModel.zoom
    flickableDirection: Flickable.HorizontalFlick
    contentWidth: pageImage.width
    contentHeight: height
    // pointSize.width * documentModel.zoom can only ever be >= width,
    // never smaller — PdfReaderScreen's minZoom floor is defined as
    // exactly "the page's full width fills the viewport", so there's
    // nothing to pan (and no point enabling drag recognition, which would
    // otherwise still eat the first bit of every touch even though it can
    // never actually move) whenever a page happens to sit right at that
    // floor.
    interactive: contentWidth > width && !zoomGestureActive
    boundsBehavior: Flickable.StopAtBounds
    clip: true

    // highlightsForTarget() is a plain Q_INVOKABLE, not a NOTIFYing
    // property, so a Repeater bound directly to its return value would
    // never refresh after the first evaluation — this cached property +
    // explicit refresh on the model's own reset signal is what makes a
    // just-added highlight (see onHighlightRequested below) actually show
    // up without needing to scroll this delegate off-screen and back.
    property var pageHighlights: []
    function refreshHighlights() {
        pageHighlights = highlightsModel.highlightsForTarget(root.index)
    }
    Component.onCompleted: refreshHighlights()
    Connections {
        target: highlightsModel
        function onModelReset() { root.refreshHighlights() }
    }

    Image {
        id: pageImage
        // No horizontal centering: this sits in Flickable content space
        // now, left-edge at content x=0, panned via root's own contentX —
        // fine since pointSize.width * zoom is never smaller than root's
        // own width (see the minZoom floor note above), so there's never
        // a gap to center within.
        width: root.pointSize.width * root.documentModel.zoom
        height: root.pointSize.height * root.documentModel.zoom
        // "-dark" suffix asks PdfPageImageProvider to invert the rendered
        // page's colors (see there) -- part of the URL so toggling dark
        // mode naturally busts this Image's own cache: true cache instead
        // of needing an explicit reload.
        source: "image://pdfpage/" + root.index + "-" + root.renderScale.toFixed(2) + (root.pageDarkMode ? "-dark" : "")
        asynchronous: true
        cache: true
        smooth: true
        antialiasing: true
        // A fresh render (renderScale bump after a pinch settles, or the
        // very first time this page scrolls into view) briefly shows the
        // placeholder before the new pixmap is ready — fading in instead
        // of popping avoids the "abrupt" feel flagged for the old
        // page-to-page paging.
        opacity: status === Image.Ready ? 1 : 0
        Behavior on opacity {
            NumberAnimation { duration: 120 }
        }

        Rectangle {
            anchors.fill: parent
            color: Theme.panel
            visible: pageImage.status !== Image.Ready

            BusyIndicator {
                anchors.centerIn: parent
                running: pageImage.status === Image.Loading
            }
        }

        // Persisted highlights for this page.
        Repeater {
            model: root.pageHighlights
            delegate: Rectangle {
                required property var modelData
                x: modelData.pageRect.x * root.renderScale
                y: modelData.pageRect.y * root.renderScale
                width: modelData.pageRect.width * root.renderScale
                height: modelData.pageRect.height * root.renderScale
                color: Theme.accent
                opacity: 0.35
            }
        }

        // Live selection — pdfSelectionController is shared across every
        // delegate, and a selection can span more than one page (see
        // PdfSelectionController's own doc comment), so each delegate asks
        // for just its own page's rects rather than assuming there's a
        // single active page. selectionPageIndices (a real NOTIFYing
        // property) is read here purely so this binding re-evaluates on
        // every selectionChanged -- selectionRectsForPage() itself is a
        // plain Q_INVOKABLE with no change notification of its own.
        Repeater {
            model: pdfSelectionController.selectionPageIndices.indexOf(root.index) >= 0
                   ? pdfSelectionController.selectionRectsForPage(root.index) : []
            delegate: Rectangle {
                required property var modelData
                x: modelData.x * root.renderScale
                y: modelData.y * root.renderScale
                width: modelData.width * root.renderScale
                height: modelData.height * root.renderScale
                color: Theme.accent
                opacity: 0.45
            }
        }

        TapHandler {
            // Not a MouseArea deliberately — see PdfReaderScreen.qml's
            // PinchHandler docs for why a MouseArea's exclusive touch grab
            // would starve it of two-finger gestures.
            onLongPressed: {
                pdfSelectionController.beginSelection(root.index, point.position.x / root.renderScale, point.position.y / root.renderScale)
            }
            onPointChanged: {
                if (!point.pressed || pdfSelectionController.selectionPageIndices.length === 0) {
                    return
                }
                // The drag may have moved past this delegate's own page
                // (down into the next page, or up into the previous one) --
                // TapHandler keeps delivering pointChanged to whichever
                // delegate's handler actually grabbed the gesture (this
                // one, the page the long-press started on) even once the
                // finger is no longer over it, the same way a desktop mouse
                // grab does (see PdfPageStackView::mouseMoveEvent()), just
                // reporting a `position` that's since run outside this
                // Item's own bounds. Map that through the shared ListView's
                // content coordinate space to find which page the finger is
                // *actually* over now, then re-express the point in that
                // page's own local (page-space) coordinates -- mapToItem/
                // mapFromItem resolve this correctly even across another
                // delegate's own Flickable pan, since they walk the real
                // scene graph transforms rather than doing flat geometry.
                const list = root.ListView.view
                const contentPoint = pageImage.mapToItem(list.contentItem, point.position.x, point.position.y)
                const targetIndex = list.indexAt(contentPoint.x, contentPoint.y)
                if (targetIndex < 0) {
                    return
                }
                const targetItem = list.itemAtIndex(targetIndex)
                if (!targetItem) {
                    return
                }
                const localPoint = list.contentItem.mapToItem(targetItem, contentPoint.x, contentPoint.y)
                pdfSelectionController.updateSelection(targetIndex, localPoint.x / targetItem.renderScale,
                                                        localPoint.y / targetItem.renderScale)
            }
        }
    }

    MouseArea {
        // Direct child of root (this page's own horizontal Flickable), NOT
        // nested inside pageImage with the selection TapHandler above —
        // this only needs root's OWN childMouseEventFilter arbitration
        // against root's own drag, not any relationship with selection or
        // the outer ListView's PinchHandler. A MouseArea (not TapHandler)
        // because on-device testing found TapHandler never reliably fires
        // when a Flickable ancestor is interactive, no matter where it's
        // nested — only a MouseArea gets the childMouseEventFilter
        // cooperation that lets a plain tap coexist with dragging.
        // Declared after pageImage (rather than before) so it actually
        // gets input priority — a hit-test candidate declared earlier in
        // the same parent lost out to later siblings in earlier testing.
        anchors.fill: parent
        onClicked: root.tapped()
    }

    // Only the FIRST page a (possibly multi-page) selection spans shows the
    // toolbar -- one floating Copy/Highlight affordance for the whole
    // selection, anchored near where it starts, not one per spanned page.
    readonly property bool isFirstSelectedPage: pdfSelectionController.selectionPageIndices.length > 0
                                                 && pdfSelectionController.selectionPageIndices[0] === root.index

    SelectionToolbar {
        visible: root.isFirstSelectedPage && pdfSelectionController.selectedText.length > 0
        parent: pageImage
        x: {
            const rects = pdfSelectionController.selectionRectsForPage(root.index)
            return rects.length > 0 ? rects[0].x * root.renderScale : 0
        }
        y: {
            const rects = pdfSelectionController.selectionRectsForPage(root.index)
            return rects.length > 0 ? Math.max(0, rects[0].y * root.renderScale - height - 8) : 0
        }
        onHighlightRequested: {
            // One Highlight per page the selection spans, each with that
            // page's own bounding rect and text slice -- mirrors desktop's
            // PdfView::addHighlightForSelection() (see there for why a
            // single rect/text pair can't represent a cross-page span).
            for (const pageIndex of pdfSelectionController.selectionPageIndices) {
                const rects = pdfSelectionController.selectionRectsForPage(pageIndex)
                if (rects.length === 0) {
                    continue
                }
                var r = rects[0]
                for (var i = 1; i < rects.length; i++) {
                    const b = rects[i]
                    const x1 = Math.min(r.x, b.x)
                    const y1 = Math.min(r.y, b.y)
                    const x2 = Math.max(r.x + r.width, b.x + b.width)
                    const y2 = Math.max(r.y + r.height, b.y + b.height)
                    r = Qt.rect(x1, y1, x2 - x1, y2 - y1)
                }
                highlightsModel.addHighlight(pageIndex, r, pdfSelectionController.selectionTextForPage(pageIndex))
            }
            pdfSelectionController.clearSelection()
        }
    }

    // Recycled/destroyed while its page was still part of the active
    // selection (e.g. scrolled far enough off-screen for ListView to reuse
    // this delegate for a different index) — clear it rather than leave a
    // selection "stuck" referencing a page index no longer backed by this
    // Item's own gesture state.
    Component.onDestruction: {
        if (pdfSelectionController.selectionPageIndices.indexOf(root.index) >= 0) {
            pdfSelectionController.clearSelection()
        }
    }
}
