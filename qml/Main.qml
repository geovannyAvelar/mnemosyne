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
                        // A recent entry whose file no longer exists (e.g.
                        // moved/deleted, or on iOS an imported copy from
                        // before this app's own container path rotated)
                        // used to fail completely silently here — clicking
                        // it just did nothing, and the broken entry stuck
                        // around in Recents forever since nothing ever
                        // removed it. Surface the failure and drop the
                        // entry so re-picking the document is the obvious
                        // next step instead of a dead end.
                        console.log("Failed to open PDF:", pdfDocumentModel.errorMessage)
                        libraryModel.removeEntry(filePath)
                        stackView.currentItem.showError(qsTr("Couldn't open \"%1\" — it may have been moved or deleted. Removed from Recents.").arg(title))
                    }
                } else if (format === "epub") {
                    if (epubReaderModel.open(filePath, format)) {
                        stackView.push(epubReaderScreenComponent)
                    } else {
                        console.log("Failed to open EPUB:", epubReaderModel.errorMessage)
                        libraryModel.removeEntry(filePath)
                        stackView.currentItem.showError(qsTr("Couldn't open \"%1\" — it may have been moved or deleted. Removed from Recents.").arg(title))
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
            onBackRequested: stackView.pop()
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
