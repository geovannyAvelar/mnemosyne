import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// Whole-library reading stats (see app/ReadingStatsStore.h) -- time spent
// and pages read today, the reader's current/longest day-streak, their
// average pages/day pace, and all-time totals. The mobile counterpart of
// desktop's ReadingStatsDock; unlike that always-visible sidebar tab,
// mobile has no permanent sidebar to put this in, so it's its own screen,
// reached the same way SettingsScreen is (see LibraryScreen.qml's "📊"
// button).
Item {
    id: root

    signal backRequested()

    // readingStatsModel's own Q_INVOKABLEs have no change notification (see
    // its own header comment) -- refreshed explicitly on show rather than
    // bound to directly, the same reason LibraryScreen.qml caches
    // allCollections()/allTags() into plain properties instead of binding
    // to them live.
    function refresh() {
        root._todaySeconds = readingStatsModel.todaySeconds()
        root._todayPages = readingStatsModel.todayPages()
        root._currentStreak = readingStatsModel.currentStreakDays()
        root._longestStreak = readingStatsModel.longestStreakDays()
        root._pace = readingStatsModel.averagePagesPerDay()
        root._allTimeSeconds = readingStatsModel.allTimeSeconds()
        root._allTimePages = readingStatsModel.allTimePages()
    }

    property int _todaySeconds: 0
    property int _todayPages: 0
    property int _currentStreak: 0
    property int _longestStreak: 0
    property real _pace: 0
    property int _allTimeSeconds: 0
    property int _allTimePages: 0

    // "2h 5m", "45m", or "0m" -- mirrors ReadingStatsDock::formatDuration()
    // on desktop; kept in sync by hand since one's C++ and the other's QML.
    function formatDuration(totalSeconds) {
        const minutes = Math.floor(totalSeconds / 60)
        const hours = Math.floor(minutes / 60)
        const remainingMinutes = minutes % 60
        return hours > 0 ? qsTr("%1h %2m").arg(hours).arg(remainingMinutes) : qsTr("%1m").arg(remainingMinutes)
    }

    function formatDays(days) {
        return days === 1 ? qsTr("1 day") : qsTr("%1 days").arg(days)
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
            spacing: 8

            Button {
                text: "‹"
                flat: true
                onClicked: root.backRequested()
            }

            Text {
                text: qsTr("Stats")
                color: Theme.text
                font.pixelSize: 24
                font.bold: true
            }
        }

        Text {
            text: qsTr("TODAY")
            color: Theme.mutedText
            font.pixelSize: 12
            font.letterSpacing: 1
        }
        Text {
            text: qsTr("%1 · %2 pages").arg(root.formatDuration(root._todaySeconds)).arg(root._todayPages)
            color: Theme.text
            font.pixelSize: 16
        }

        Text {
            text: qsTr("STREAK")
            color: Theme.mutedText
            font.pixelSize: 12
            font.letterSpacing: 1
        }
        Text {
            text: root._currentStreak > 0
                  ? qsTr("%1 current · %2 best").arg(root.formatDays(root._currentStreak)).arg(root.formatDays(root._longestStreak))
                  : qsTr("No current streak · %1 best").arg(root.formatDays(root._longestStreak))
            color: Theme.text
            font.pixelSize: 16
        }

        Text {
            text: qsTr("PACE")
            color: Theme.mutedText
            font.pixelSize: 12
            font.letterSpacing: 1
        }
        Text {
            text: qsTr("%1 pages/day, last 30 days").arg(root._pace.toFixed(1))
            color: Theme.text
            font.pixelSize: 16
        }

        Text {
            text: qsTr("ALL TIME")
            color: Theme.mutedText
            font.pixelSize: 12
            font.letterSpacing: 1
        }
        Text {
            text: qsTr("%1 · %2 pages").arg(root.formatDuration(root._allTimeSeconds)).arg(root._allTimePages)
            color: Theme.text
            font.pixelSize: 16
        }

        Item { Layout.fillHeight: true }
    }

    Component.onCompleted: root.refresh()
}
