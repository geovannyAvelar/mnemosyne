import QtQuick
import QtQuick.Controls.Basic

// One PDF page: a fixed-resolution raster from PdfPageImageProvider,
// pinch-zoomed as a smooth QML transform rather than by re-rendering on
// every gesture frame (that's what the fixed renderScale is for — Poppler
// only re-rasters when a page is first shown, never during the pinch
// itself). The gesture's final scale is written back to documentModel.zoom
// so it persists via ReadingProgressStore and applies to newly-shown pages.
//
// Text selection: a long-press starts a selection at the nearest word (see
// PdfSelectionController), and dragging while still pressed extends it —
// deliberately not using preventStealing on the MouseArea below, so a
// normal drag-to-pan gesture (fast movement from the first touch) still
// reaches the Flickable/PinchHandler above unimpeded; only a held-still
// press (which Flickable's own drag-threshold detection ignores) lets
// pressAndHold fire and hand the gesture to selection instead.
Flickable {
    id: flick

    required property int pageIndex
    required property var documentModel

    readonly property real renderScale: 2.0 // ~144 DPI raster, independent of zoom
    readonly property size pointSize: documentModel.pageSizePoints(pageIndex)
    readonly property bool isCurrentPage: pageIndex === documentModel.currentPage

    property var pageHighlights: []
    function refreshHighlights() {
        pageHighlights = highlightsModel.highlightsForTarget(flick.pageIndex)
    }

    function unionOfRects(rects) {
        if (rects.length === 0) {
            return Qt.rect(0, 0, 0, 0)
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
        return r
    }

    Component.onCompleted: refreshHighlights()
    Connections {
        target: highlightsModel
        function onModelReset() { flick.refreshHighlights() }
    }

    contentWidth: Math.max(width, pageImage.width * pageImage.scale)
    contentHeight: Math.max(height, pageImage.height * pageImage.scale)
    boundsBehavior: Flickable.StopAtBounds
    clip: true

    PinchHandler {
        target: pageImage
        minimumScale: 0.25 / flick.renderScale
        maximumScale: 4.0 / flick.renderScale
        onActiveChanged: {
            if (!active) {
                // The gesture just broke pageImage.scale's declarative
                // binding (PinchHandler drives it imperatively while
                // active) — sync the model, then re-bind so external zoom
                // changes (e.g. restoring progress on next open) still flow
                // through normally.
                flick.documentModel.zoom = pageImage.scale * flick.renderScale
                pageImage.scale = Qt.binding(function () { return flick.documentModel.zoom / flick.renderScale })
            }
        }
    }

    Image {
        id: pageImage
        asynchronous: true
        cache: true
        source: "image://pdfpage/" + flick.pageIndex + "-" + flick.renderScale.toFixed(2)
        width: flick.pointSize.width * flick.renderScale
        height: flick.pointSize.height * flick.renderScale
        scale: flick.documentModel.zoom / flick.renderScale
        // Default transformOrigin is Center, which would render (and hit-test)
        // the image shifted away from (0,0) by a quarter of its full
        // pre-scale size — TopLeft keeps it anchored where contentWidth/
        // contentHeight above assume it is.
        transformOrigin: Item.TopLeft
        smooth: true
        antialiasing: true

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
            model: flick.pageHighlights
            delegate: Rectangle {
                required property var modelData
                x: modelData.pageRect.x * flick.renderScale
                y: modelData.pageRect.y * flick.renderScale
                width: modelData.pageRect.width * flick.renderScale
                height: modelData.pageRect.height * flick.renderScale
                color: Theme.accent
                opacity: 0.35
            }
        }

        // Live selection, only on the page currently being interacted with
        // — pdfSelectionController is shared across all page delegates
        // (only one is ever actively touched at a time in the SwipeView).
        Repeater {
            model: flick.isCurrentPage ? pdfSelectionController.selectionRects : []
            delegate: Rectangle {
                required property var modelData
                x: modelData.x * flick.renderScale
                y: modelData.y * flick.renderScale
                width: modelData.width * flick.renderScale
                height: modelData.height * flick.renderScale
                color: Theme.accent
                opacity: 0.45
            }
        }

        MouseArea {
            anchors.fill: parent
            enabled: flick.isCurrentPage
            onPressAndHold: (mouse) => {
                pdfSelectionController.beginSelection(mouse.x / flick.renderScale, mouse.y / flick.renderScale)
            }
            onPositionChanged: (mouse) => {
                if (pdfSelectionController.selectedText.length > 0) {
                    pdfSelectionController.updateSelection(mouse.x / flick.renderScale, mouse.y / flick.renderScale)
                }
            }
        }
    }

    SelectionToolbar {
        visible: flick.isCurrentPage && pdfSelectionController.selectedText.length > 0
        parent: pageImage
        x: {
            const rects = pdfSelectionController.selectionRects
            return rects.length > 0 ? rects[0].x * flick.renderScale : 0
        }
        y: {
            const rects = pdfSelectionController.selectionRects
            return rects.length > 0 ? Math.max(0, rects[0].y * flick.renderScale - height - 8) : 0
        }
        onHighlightRequested: {
            const rects = pdfSelectionController.selectionRects
            const unionRect = flick.unionOfRects(rects)
            highlightsModel.addHighlight(flick.pageIndex, unionRect, pdfSelectionController.selectedText)
            pdfSelectionController.clearSelection()
            flick.refreshHighlights()
        }
    }
}
