import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// Minimal settings surface for the one thing that needs one on mobile so
// far: Google Drive sign-in (SyncController). Desktop's equivalent is
// spread across MainWindow's Sync menu + a one-time "Set Up Google
// Sign-In" QDialog (ui/MainWindow.cpp); this folds both into a single
// screen since there's no menu bar to hang a submenu off on Android.
Item {
    id: root

    signal backRequested()

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
            spacing: 8

            Button {
                text: "‹"
                flat: true
                onClicked: root.backRequested()
            }

            Text {
                text: qsTr("Settings")
                color: Theme.text
                font.pixelSize: 24
                font.bold: true
            }
        }

        Text {
            text: qsTr("GOOGLE DRIVE SYNC")
            color: Theme.mutedText
            font.pixelSize: 12
            font.letterSpacing: 1
        }

        // Mnemosyne ships its own bundled OAuth client (GoogleAuth.cpp's
        // kBundledClientId) — no credential-entry step here, matching
        // desktop's Sync menu. hasClientCredentials is only false in an
        // unconfigured from-source build (see docs/google-drive-setup.md's
        // Android section for maintainer notes on provisioning one), same
        // as desktop's own "Sign in with Google Drive..." menu item being
        // disabled in that case.
        Text {
            Layout.fillWidth: true
            visible: !syncController.hasClientCredentials
            text: qsTr("This build wasn't compiled with Google sign-in support.")
            color: Theme.mutedText
            font.pixelSize: 14
            wrapMode: Text.WordWrap
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8
            visible: syncController.hasClientCredentials

            Text {
                Layout.fillWidth: true
                visible: syncController.isSignedIn
                text: qsTr("Signed in as %1").arg(syncController.accountEmail)
                color: Theme.text
                font.pixelSize: 15
            }

            Text {
                Layout.fillWidth: true
                visible: !syncController.isSignedIn && !syncController.signInInProgress
                text: qsTr("Not signed in — reading progress won't sync across devices.")
                color: Theme.mutedText
                font.pixelSize: 14
                wrapMode: Text.WordWrap
            }

            Text {
                Layout.fillWidth: true
                visible: syncController.lastError.length > 0
                text: syncController.lastError
                color: "#D97757"
                font.pixelSize: 13
                wrapMode: Text.WordWrap
            }

            RowLayout {
                spacing: 8

                Button {
                    text: syncController.signInInProgress ? qsTr("Signing in…") : qsTr("Sign in with Google")
                    enabled: !syncController.isSignedIn && !syncController.signInInProgress
                    onClicked: syncController.startSignIn()
                }

                Button {
                    text: qsTr("Sign out")
                    visible: syncController.isSignedIn
                    onClicked: syncController.signOut()
                }

                BusyIndicator {
                    running: syncController.signInInProgress
                    implicitWidth: 24
                    implicitHeight: 24
                }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
