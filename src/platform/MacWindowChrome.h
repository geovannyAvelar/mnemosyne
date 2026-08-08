#pragma once

class QWindow;

// Hides the native traffic-light buttons and makes the title bar
// transparent/full-size so nothing native is drawn there — the app supplies
// its own close/minimize/fullscreen buttons instead (see TrafficLightButton
// in ui/), matching how apps like Claude Desktop embed that chrome directly
// in their own GUI rather than in native window decoration.
namespace MacWindowChrome {

void integrateTitleBar(QWindow *window);

} // namespace MacWindowChrome
