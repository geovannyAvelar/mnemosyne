#pragma once

#include <QtGlobal>

// A persisted, per-device logical clock (see SyncOrdering.h for how it's
// used to order sync entries) -- fixes wall-clock skew between devices
// mattering for "which edit happened first" decisions, since this counter
// only ever moves forward relative to what this device has itself done or
// observed, never relative to any device's system clock.
namespace LamportClock {

// Advances and returns the local counter for a new local event (a
// highlight edit, a progress save, a sync-log delete). Persisted
// (QSettings "Device/lamportClock", alongside DeviceIdentity's own key),
// monotonic, never returns 0 -- so 0 is always safe to use elsewhere as
// "no Lamport value present" (an entry or record written before this
// field existed). Gaps are fine; Lamport clocks only need monotonicity,
// not contiguity.
//
// Must be called from the thread sync operations run on -- today that's
// always the UI thread (nothing in this subsystem uses a worker thread);
// this isn't locked, so a future change moving sync work to a background
// thread would need to add that.
quint64 tick();

// Folds an observed remote value into the local counter: local =
// max(local, remoteValue). Call for every remote entry a pull/merge
// examines, including ones that lose the merge -- the local clock must
// catch up to everything observed, not just what was accepted.
void observe(quint64 remoteValue);

} // namespace LamportClock
