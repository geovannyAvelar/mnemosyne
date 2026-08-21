pragma Singleton
import QtQuick

// Color tokens ported from src/ui/Theme.cpp's Colors struct (light palette
// only, for now — dark mode is a later mobile-port pass, same as it was a
// separate pass on desktop). Delivery mechanism differs (QML singleton
// instead of QPalette + QSS) but the actual values are the same warm
// cream/charcoal + terracotta palette.
QtObject {
    readonly property color window: "#FAF9F5"
    readonly property color panel: "#F5F3EC"
    readonly property color base: "#FFFFFF"
    readonly property color raisedHover: "#EEECE3"
    readonly property color border: "#E5E2D9"
    readonly property color text: "#2B2A27"
    readonly property color mutedText: "#8A877C"
    readonly property color accent: "#D97756"
    readonly property color accentHover: "#C2603F"
    readonly property color accentPressed: "#AD5435"
    readonly property color accentText: "#FFFFFF"
}
