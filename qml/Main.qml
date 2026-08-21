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
                if (format === "pdf") {
                    if (pdfDocumentModel.open(filePath, format)) {
                        stackView.push(pdfReaderScreenComponent)
                    } else {
                        console.log("Failed to open PDF:", pdfDocumentModel.errorMessage)
                    }
                } else {
                    // EPUB reading lands in a later mobile-port stage.
                    console.log("Activated:", title, "(" + format + ")", filePath)
                }
            }
        }
    }

    Component {
        id: pdfReaderScreenComponent
        PdfReaderScreen {
            documentModel: pdfDocumentModel
        }
    }

    onClosing: (close) => {
        if (stackView.depth > 1) {
            close.accepted = false
            stackView.pop()
        }
    }
}
