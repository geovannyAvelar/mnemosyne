import QtQuick
import QtQuick.Controls.Basic

// One PDF page: a fixed-resolution raster from PdfPageImageProvider,
// pinch-zoomed as a smooth QML transform rather than by re-rendering on
// every gesture frame (that's what the fixed renderScale is for — Poppler
// only re-rasters when a page is first shown, never during the pinch
// itself). The gesture's final scale is written back to documentModel.zoom
// so it persists via ReadingProgressStore and applies to newly-shown pages.
Flickable {
    id: flick

    required property int pageIndex
    required property var documentModel

    readonly property real renderScale: 2.0 // ~144 DPI raster, independent of zoom
    readonly property size pointSize: documentModel.pageSizePoints(pageIndex)

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
    }
}
