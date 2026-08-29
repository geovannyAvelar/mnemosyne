#pragma once

// Poppler's iOS build uses the "generic" FONT_CONFIGURATION (see
// scripts/build-ios-deps.sh -- there's no fontconfig on iOS and no "ios"
// backend the way there's an "android" one). Without this call, every PDF
// that references one of the 14 standard PostScript fonts by name instead
// of embedding it (extremely common) fails to substitute anything: Poppler
// re-scans a nonexistent font directory and logs 14 failures on *every*
// substitution lookup, not just once, which is slow enough on a real
// multi-hundred-page book to trip iOS's watchdog and kill the app.
//
// Call once at process startup, before any document is opened -- see
// main_ios.mm. It points Poppler's GlobalParams at the bundled Ghostscript/
// URW Base35 fonts (resources/pdf-base14-fonts), which populates its font
// cache up front so every later internal lookup short-circuits instead of
// re-scanning.
void setupPopplerBase14Fonts();
