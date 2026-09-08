#pragma once

// Persisted comic-reading preferences (CBZ only), global across the app --
// like Dark Mode and TextReaderTypography, not saved per book: a reader who
// prefers two-page spreads or right-to-left (manga) order almost always
// wants that for every comic they open, not just one.
namespace ComicReadingSettings {

// Two pages shown side by side instead of one at a time. Pages pair up
// starting from index 0 (0+1, 2+3, ...) -- no special-cased single cover
// page, so ComicView::goToPage() snaps any odd target down to the even
// index starting its pair.
bool doublePageMode();
void setDoublePageMode(bool enabled);

// Reading order within a spread: the lower-index page on the right and the
// higher-index one on the left (manga convention) instead of the reverse.
// No effect in single-page mode.
bool rightToLeft();
void setRightToLeft(bool enabled);

} // namespace ComicReadingSettings
