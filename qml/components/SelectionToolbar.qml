import QtQuick
import QtQuick.Controls.Basic

// Floating action shown above the current text selection, replacing
// desktop's right-click "Highlight" context-menu entry. Copy-to-clipboard
// is deliberately out of scope for this pass — highlighting is the actual
// Stage 7 deliverable; clipboard access would need a small new C++ bridge
// for marginal value here.
Rectangle {
    id: root

    implicitWidth: highlightButton.implicitWidth + 16
    implicitHeight: highlightButton.implicitHeight + 8
    radius: height / 2
    color: Theme.base
    border.color: Theme.border
    border.width: 1

    signal highlightRequested()

    Button {
        id: highlightButton
        anchors.centerIn: parent
        text: qsTr("Highlight")
        flat: true
        onClicked: root.highlightRequested()
    }
}
