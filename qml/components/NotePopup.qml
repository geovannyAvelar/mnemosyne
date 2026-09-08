import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// Lightweight "here's the note" popup shown when the reader taps a
// highlight that has a note attached -- the QML/mobile counterpart of
// desktop's NotePopup (src/ui/NotePopup.h). Positioned by the caller (see
// PdfContinuousPageItem.qml) near the tapped highlight; dismissed either by
// its own Close button or by the caller's page-wide tap handler noticing a
// popup is open and closing it on the next outside tap, rather than this
// component owning any outside-tap detection itself.
//
// Desktop's version also offers an Edit action, opening NoteDialog to
// rewrite the note's text/color. There's no mobile equivalent of that
// dialog yet -- SelectionToolbar.qml only offers Highlight, not Add/Edit
// Note, so mobile can't author a note in the first place (only view one
// synced in from desktop) -- so Edit is left out here rather than wiring a
// button to a dialog that doesn't exist. Remove is kept: it needs no new UI,
// just the removeHighlightAt() call the caller already has for highlights.
Rectangle {
    id: root

    property alias noteText: noteLabel.text

    signal removeRequested()
    signal closeRequested()

    implicitWidth: Math.min(240, contentColumn.implicitWidth + 24)
    implicitHeight: contentColumn.implicitHeight + 16
    radius: 8
    color: Theme.base
    border.color: Theme.border
    border.width: 1

    // Consumes its own taps so a tap on the note text or its buttons
    // doesn't fall through to the page's own tap-to-dismiss MouseArea
    // underneath (see PdfContinuousPageItem.qml).
    MouseArea { anchors.fill: parent }

    ColumnLayout {
        id: contentColumn
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        Text {
            id: noteLabel
            Layout.maximumWidth: 216
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: Theme.text
            font.pixelSize: 13
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Item { Layout.fillWidth: true }

            Button {
                text: qsTr("Remove")
                flat: true
                onClicked: root.removeRequested()
            }

            Button {
                text: qsTr("Close")
                flat: true
                onClicked: root.closeRequested()
            }
        }
    }
}
