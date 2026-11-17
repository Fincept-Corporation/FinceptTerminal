#pragma once
// HarnessDetect.h — where a harness CLI is installed. Pure search logic over an
// injected DetectContext (QtCore only), so the rules can be tested against fake
// file systems for every OS layout. HarnessRegistry runs it on a worker thread
// and then validates each candidate with the adapter's `--version` probe.
//
// Search order: the user's override (when it exists), PATH (Windows: PATHEXT,
// .exe first), the adapter's own candidates, then the shared per-OS list.
// Native executables are preferred over npm shims, which are resolved to
// "node + script" by reading the shim (cmd.exe only as a last resort).

#include "services/harness/HarnessTypes.h"

#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

namespace fincept::harness {

class IHarnessAdapter;

/// The real file system and process environment.
DetectContext make_system_detect_context(const QString& override_path);

/// Normalised absolute-or-relative path: surrounding quotes stripped, Windows
/// backslashes turned into '/', "." / ".." / duplicate and trailing separators removed.
QString normalize_detect_path(const QString& path, bool windows);

/// PATH entries in order: empty, relative and duplicate entries dropped.
QStringList split_search_path(const QString& value, bool windows);

/// File names to try for one executable name in one directory, best first
/// (Windows: name.exe, then name.cmd / name.bat when PATHEXT allows them).
QStringList executable_file_names(const QString& name, const DetectContext& ctx);

/// The shared per-OS directory list (after PATH and the adapter's candidates).
QStringList shared_search_dirs(const DetectContext& ctx);

/// True for a Windows batch launcher (.cmd / .bat), whatever the host OS.
bool is_cmd_shim_path(const QString& path);

/// Resolve an npm .cmd shim to the node executable and the JS file it runs
/// (Via::NodeScript) — or, when the package's bin is a native .exe, to that
/// executable (Via::Native). Empty when the shim is not an npm shim, its
/// target is missing or no node executable can be found.
std::optional<Installation> resolve_npm_shim(const QString& shim_path, const DetectContext& ctx);

/// Ordered, de-duplicated candidates for one harness. An override that exists
/// is the only candidate (path_overridden = true); an override that does not
/// exist falls back to auto-detection.
QVector<Installation> find_candidates(const IHarnessAdapter& adapter, const DetectContext& ctx);

enum class VersionVerdict {
    Accepted,   // the output identifies this harness; version + version_line filled in
    Rejected,   // it answered, and the answer names another program
    Unverified, // no usable answer (did not start, timed out, crashed, failed, said nothing)
};

/// Judge one `--version` result. `ran` = started and exited normally (no
/// timeout, no crash). On Accepted, inst.version / version_line are set; on
/// Rejected, inst.version_line holds the first line it printed.
VersionVerdict check_version(const IHarnessAdapter& adapter, Installation& inst, int exit_code, bool ran,
                             const QString& output);

} // namespace fincept::harness
