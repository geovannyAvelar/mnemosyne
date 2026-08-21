import QtQuick
import QtQuick.Window

Window {
    width: 640
    height: 480
    visible: true
    title: "Mnemosyne Android Smoke Test"

    Rectangle {
        anchors.fill: parent
        color: "#2b2320"

        Text {
            anchors.centerIn: parent
            anchors.margins: 24
            width: parent.width - 48
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            text: smokeTestBridge.openSamplePdfSummary()
            color: "#f5ead9"
            font.pixelSize: 22
        }
    }
}
