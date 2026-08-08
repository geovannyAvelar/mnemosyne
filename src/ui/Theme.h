#pragma once

#include <QPalette>
#include <QString>

// A warm, rounded visual theme (cream/charcoal with a terracotta accent),
// applied via QPalette (for native chrome and unstyled widgets) plus a QSS
// stylesheet (for rounded corners, pill tabs, hover/selection states).
namespace Theme {

QPalette lightPalette();
QPalette darkPalette();

QString styleSheet(bool dark);

} // namespace Theme
