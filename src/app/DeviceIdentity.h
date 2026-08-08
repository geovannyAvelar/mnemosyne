#pragma once

#include <QString>

// A stable identifier for this installation, used to tell "my own last
// write" apart from another device's when merging sync logs.
namespace DeviceIdentity {

// Persisted UUID, generated once on first run.
QString id();

// Human-readable label for prompts, e.g. "Alice's MacBook".
QString name();

} // namespace DeviceIdentity
