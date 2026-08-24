import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item {
    id: root

    signal fileActivated(string filePath, string title, string format)
    signal settingsRequested()

    function guessFormat(name) {
        const lower = name.toLowerCase()
        if (lower.endsWith(".epub")) return "epub"
        if (lower.endsWith(".pdf")) return "pdf"
        return "unknown"
    }

    Connections {
        target: androidStorageAccess
        function onDocumentPicked(uri, displayName) {
            libraryModel.recordOpened(uri, displayName, root.guessFormat(displayName))
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.window
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 16

        RowLayout {
            Layout.fillWidth: true

            Text {
                text: qsTr("Library")
                color: Theme.text
                font.pixelSize: 32
                font.bold: true
            }

            Item { Layout.fillWidth: true }

            Button {
                text: "⚙"
                flat: true
                onClicked: root.settingsRequested()
            }
        }

        Button {
            id: openButton
            text: qsTr("Open Document…")
            onClicked: androidStorageAccess.pickDocument()

            background: Rectangle {
                implicitHeight: 44
                radius: 10
                color: openButton.pressed ? Theme.accentPressed
                     : openButton.hovered ? Theme.accentHover
                                           : Theme.accent
            }
            contentItem: Text {
                text: openButton.text
                color: Theme.accentText
                font.pixelSize: 16
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }

        Text {
            text: qsTr("RECENT DOCUMENTS")
            color: Theme.mutedText
            font.pixelSize: 12
            font.letterSpacing: 1
        }

        Text {
            visible: libraryGrid.count === 0
            text: qsTr("No recent documents yet.")
            color: Theme.mutedText
            font.pixelSize: 14
        }

        GridView {
            id: libraryGrid
            Layout.fillWidth: true
            Layout.fillHeight: true
            cellWidth: 168
            cellHeight: 220
            model: libraryModel
            clip: true

            delegate: Item {
                width: libraryGrid.cellWidth
                height: libraryGrid.cellHeight

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 8
                    radius: 12
                    color: Theme.base
                    border.color: Theme.border
                    border.width: 1

                    MouseArea {
                        anchors.fill: parent
                        onClicked: root.fileActivated(model.filePath, model.title, model.format)
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 6

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 120
                            radius: 8
                            color: Theme.panel

                            Text {
                                anchors.centerIn: parent
                                text: model.format.toUpperCase()
                                color: Theme.mutedText
                                font.pixelSize: 14
                                font.bold: true
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: model.title
                            color: Theme.text
                            font.pixelSize: 14
                            elide: Text.ElideRight
                            maximumLineCount: 2
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }
        }
    }

    Component.onCompleted: libraryModel.refresh()
}
