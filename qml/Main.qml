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
                } else if (format === "epub") {
                    if (epubReaderModel.open(filePath, format)) {
                        stackView.push(epubReaderScreenComponent)
                    } else {
                        console.log("Failed to open EPUB:", epubReaderModel.errorMessage)
                    }
                } else {
                    console.log("Unrecognized format:", title, "(" + format + ")", filePath)
                }
            }
            onSettingsRequested: stackView.push(settingsScreenComponent)
        }
    }

    Component {
        id: settingsScreenComponent
        SettingsScreen {
            onBackRequested: stackView.pop()
        }
    }

    Component {
        id: pdfReaderScreenComponent
        PdfReaderScreen {
            documentModel: pdfDocumentModel
        }
    }

    Component {
        id: epubReaderScreenComponent
        EpubReaderScreen {
            documentModel: epubReaderModel
        }
    }

    onClosing: (close) => {
        if (stackView.depth > 1) {
            close.accepted = false
            stackView.pop()
        }
    }
}
