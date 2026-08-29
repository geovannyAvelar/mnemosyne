pragma Singleton
import QtQuick

// Color tokens ported from src/ui/Theme.cpp's Colors struct (both the
// light and dark palettes — values copied verbatim from lightColors()/
// darkColors() there, so mobile and desktop stay visually consistent).
// themeSettings is a QML context property (see ThemeSettings.h) backed by
// the same QSettings "darkMode" key desktop's menu action uses.
QtObject {
    readonly property bool dark: themeSettings.dark

    readonly property color window: dark ? "#262624" : "#FAF9F5"
    readonly property color panel: dark ? "#2A2A28" : "#F5F3EC"
    readonly property color base: dark ? "#30302E" : "#FFFFFF"
    readonly property color raisedHover: dark ? "#383835" : "#EEECE3"
    readonly property color border: dark ? "#3D3D3A" : "#E5E2D9"
    readonly property color text: dark ? "#F5F4EF" : "#2B2A27"
    readonly property color mutedText: dark ? "#9B9993" : "#8A877C"
    readonly property color accent: "#D97756"
    readonly property color accentHover: dark ? "#E28A6C" : "#C2603F"
    readonly property color accentPressed: dark ? "#C2603F" : "#AD5435"
    readonly property color accentText: "#FFFFFF"
}
