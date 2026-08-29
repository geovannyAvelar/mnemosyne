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

    readonly property size pointSize: documentModel.pageSizePoints(index)
    // The resolution Poppler last rasterized this page at — kept in sync
    // with documentModel.zoom by PdfReaderScreen's PinchHandler once a
    // pinch settles (not during the live gesture itself, which only needs
    // this Item's height/pageImage's width-height to update, both cheap
    // layout, not a fresh render). See PdfPageImageProvider — the "id"
    // string it parses is "<index>-<scale>".
    property real renderScale: 2.0

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
    interactive: contentWidth > width
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
        source: "image://pdfpage/" + root.index + "-" + root.renderScale.toFixed(2)
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
        // delegate (only one page can have an active selection at a time),
        // so each delegate only renders it when the selection belongs to
        // this page (see PdfSelectionController::selectionPageIndex).
        Repeater {
            model: pdfSelectionController.selectionPageIndex === root.index ? pdfSelectionController.selectionRects : []
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
                if (point.pressed && pdfSelectionController.selectionPageIndex === root.index) {
                    pdfSelectionController.updateSelection(point.position.x / root.renderScale, point.position.y / root.renderScale)
                }
            }
        }
    }

    SelectionToolbar {
        visible: pdfSelectionController.selectionPageIndex === root.index && pdfSelectionController.selectedText.length > 0
        parent: pageImage
        x: {
            const rects = pdfSelectionController.selectionRects
            return rects.length > 0 ? rects[0].x * root.renderScale : 0
        }
        y: {
            const rects = pdfSelectionController.selectionRects
            return rects.length > 0 ? Math.max(0, rects[0].y * root.renderScale - height - 8) : 0
        }
        onHighlightRequested: {
            const rects = pdfSelectionController.selectionRects
            if (rects.length === 0) {
                return
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
            highlightsModel.addHighlight(root.index, r, pdfSelectionController.selectedText)
            pdfSelectionController.clearSelection()
        }
    }

    // Recycled/destroyed while its page still had the active selection
    // (e.g. scrolled far enough off-screen for ListView to reuse this
    // delegate for a different index) — clear it rather than leave a
    // selection "stuck" pointing at a page index no longer backed by this
    // Item's own gesture state.
    Component.onDestruction: {
        if (pdfSelectionController.selectionPageIndex === root.index) {
            pdfSelectionController.clearSelection()
        }
    }
}
