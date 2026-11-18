#pragma once
// InstanceKey — the single-instance identity of a terminal process: which
// running terminal a new launch hands its arguments to (InstanceLock).
//
// Today's key is "<edition prefix>-<profile>" (InstanceLock adds the OS user).
// A launch with another data folder (FINCEPT_DATA_DIR) but the same profile name
// used to get the same key, so a test or scratch run forwarded its arguments to
// the user's running terminal and opened a window inside the real session. The
// data folder is part of the identity now: an overridden root adds a stable
// hash of its canonical path, so each data folder + profile is its own primary.
// The default root keeps today's key exactly — existing installs, their
// running instance and the legacy socket probe are unaffected.
//
// Header-only, Qt Core only; unit-tested in tests/tst_edition_identity.cpp.

#include <QByteArray>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QString>

namespace fincept::instance_key {

/// Whether data-folder paths compare without regard to case on this platform
/// (Windows: NTFS is case-insensitive; macOS: APFS usually is too).
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
inline constexpr bool kCaseInsensitivePaths = true;
#else
inline constexpr bool kCaseInsensitivePaths = false;
#endif

/// One spelling per folder: '/' separators (a '\' is taken as one on every
/// platform), "." / ".." resolved, no trailing '/', lower-case where paths are
/// case-insensitive. Pure string work — see canonical_root() for resolving
/// links. Empty in, empty out.
inline QString normalise_root(const QString& path, bool case_insensitive = kCaseInsensitivePaths) {
    QString p = path.trimmed();
    p.replace(QLatin1Char('\\'), QLatin1Char('/'));
    p = QDir::cleanPath(p);
    if (p.isEmpty() || p == QLatin1String("."))
        return {};
    // A bare drive ("C:\" → "C:/") keeps its slash on every platform: QDir::cleanPath()
    // preserves it only where drive letters exist, and the key must not depend on the OS
    // the path was normalised on.
    if (p.size() == 2 && p.at(1) == QLatin1Char(':') && p.at(0).isLetter())
        p += QLatin1Char('/');
    while (p.size() > 1 && p.endsWith(QLatin1Char('/')) && !p.endsWith(QLatin1String(":/")))
        p.chop(1);
    return case_insensitive ? p.toLower() : p;
}

/// The folder as the file system names it: links, junctions and 8.3 short
/// names resolved when it exists (main() creates the root before asking),
/// else its absolute path; then normalise_root().
inline QString canonical_root(const QString& path) {
    if (path.trimmed().isEmpty())
        return {};
    const QFileInfo fi(QDir::fromNativeSeparators(path.trimmed()));
    const QString canonical = fi.canonicalFilePath();
    return normalise_root(canonical.isEmpty() ? fi.absoluteFilePath() : canonical);
}

/// 16 hex digits identifying a normalised data root.
inline QString root_tag(const QString& normalised_root) {
    return QString::fromLatin1(
        QCryptographicHash::hash(normalised_root.toUtf8(), QCryptographicHash::Sha256).toHex().left(16));
}

/// The key InstanceLock is acquired with.
///   prefix          — edition::kInstanceKeyPrefix
///   profile         — the active profile name
///   override_root   — canonical_root(AppPaths::root_override()): empty when
///                     FINCEPT_DATA_DIR is unset or was refused
///   default_root    — canonical_root() of the edition's default data folder
/// "<prefix>-<profile>" when the root is the default one (also when
/// FINCEPT_DATA_DIR merely names the default folder: same data, same terminal);
/// "<prefix>-<profile>@<root_tag>" for any other folder.
inline QString make(const QString& prefix, const QString& profile, const QString& override_root,
                    const QString& default_root) {
    const QString base = prefix + QLatin1Char('-') + profile;
    if (override_root.isEmpty() || override_root == default_root)
        return base;
    return base + QLatin1Char('@') + root_tag(override_root);
}

} // namespace fincept::instance_key
