import QtQuick
import QtQuick.Controls.Basic

ApplicationWindow {
    id: window
    width: 411
    height: 891
    visible: true
    title: "Mnemosyne"

    background: Rectangle {
        color: Theme.window
    }

    StackView {
        id: stackView
        anchors.fill: parent
        initialItem: libraryScreenComponent
    }

    Component {
        id: libraryScreenComponent
        LibraryScreen {
            onFileActivated: (filePath, title, format) => {
                // Reader screens (PDF/EPUB) land in a later mobile-port
                // stage; for now, opening a document just confirms which
                // one was tapped, proving the library → activation wiring.
                console.log("Activated:", title, "(" + format + ")", filePath)
            }
        }
    }
}
