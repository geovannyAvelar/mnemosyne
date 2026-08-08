#pragma once

#include <QString>

// Content hash (SHA-256, hex) for filePath, used to recognize "the same
// book" across devices where it may live at a different local path. Cached
// locally (keyed by path + size + modification time) so repeat opens of an
// unchanged file are instant; only a changed or first-seen file is re-hashed.
namespace FileIdentity {

QString contentHash(const QString &filePath);

} // namespace FileIdentity
