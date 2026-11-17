#pragma once
// PythonEnvRelocation.h — make the managed Python runtime work after its data
// root moved (src/app/LegacyRootAdoption: com.fincept.terminal → the edition's
// folder).
//
// uv records absolute paths when it builds the runtime, so a moved folder is
// not a working one:
//   - <venv>/pyvenv.cfg  `home = <root>/python/cpython-…[/bin]` — the base
//     interpreter; PythonSetupManager::venv_valid() and the interpreter itself
//     read it (on Windows Scripts\python.exe is a launcher that follows it)
//   - <venv>/bin/python  absolute symlink to the base interpreter (Linux/macOS)
//   - <venv>/bin/*       `#!<root>/<venv>/bin/python3` shebangs, VIRTUAL_ENV=… in
//                        the activate scripts
//   - <venv>\Scripts\*.exe  uv trampolines with the python.exe path in a trailer
//   - <venv>/…/site-packages/*.pth, *.egg-link  absolute paths, when present
//   - <root>/python/…/_sysconfigdata*.py  uv writes the install prefix there
//     (used when packages build native code)
// relocate_tree() rewrites every such reference in place — text files through
// an atomic replace (a new file, so a hard-linked source is never changed),
// symlinks re-created, trampoline trailers rebuilt — and verify_and_prepare()
// then runs each environment's interpreter and imports the essentials. What
// cannot be made to work is handed to PythonSetupManager as a background repair
// (repair_flag_path()): never the blocking first-time setup screen.

#include <QList>
#include <QString>
#include <QStringList>

namespace fincept::python::relocation {

/// Venv folders under the data root (PythonSetupManager's kPsmVenv*).
inline constexpr const char* kVenvNames[] = {"venv-numpy1", "venv-numpy2"};

/// Top-level entries of the managed runtime (uv binary, CPython, venvs, setup
/// flags). The uv download cache is separate: re-creatable, but it is what
/// makes a repair possible offline.
inline constexpr const char* kRuntimeEntries[] = {"uv",          "python",          "venv-numpy1", "venv-numpy2",
                                                  ".setup_complete", ".full_libraries_requested"};
inline constexpr const char* kCacheEntry = "uv-cache";

struct RelocationReport {
    QStringList old_prefixes; ///< roots the runtime referred to (from pyvenv.cfg, plus the known old root)
    int files_rewritten = 0;
    int links_repointed = 0;
    int launchers_patched = 0;
    QStringList problems; ///< for the log; nothing here is fatal on its own
    QString summary() const;
};

/// Rewrite the absolute references to the folder the runtime was installed in
/// so it works at `new_root`. `dir` is where the runtime is on disk now: the new
/// root itself, or a staging folder that is renamed to `new_root` afterwards.
/// `known_old_root` is used besides the prefix each pyvenv.cfg names (they differ
/// when the old folder was itself reached through a moved or linked path).
/// Idempotent: a reference that already points at `new_root` is left alone.
RelocationReport relocate_tree(const QString& dir, const QString& known_old_root, const QString& new_root);

/// The roots other than `root` that the venvs under `root` were built in (from
/// their pyvenv.cfg). Empty when the runtime is where it was built. Cheap: reads
/// two small files.
QStringList foreign_roots(const QString& root);

struct VenvVerdict {
    enum State { Absent, Ok, PackagesMissing, Broken };
    QString venv;
    State state = Absent;
    QString detail;
};

/// Run every venv's interpreter under `root` (isolated, no bytecode written)
/// and import the essentials. A venv whose interpreter does not run is moved
/// aside (and deleted in the background); for anything not Ok the repair flag is
/// written, so the next check_status() takes the non-blocking background path.
QList<VenvVerdict> verify_and_prepare(const QString& root);

/// One line per venv ("venv-numpy2 ok", "venv-numpy1 broken (…)").
QString describe(const QList<VenvVerdict>& verdicts);

/// Present while an adopted runtime still needs repairing. PythonSetupManager
/// then never asks for first-time setup: it repairs in the background (status
/// bar chip) and removes the flag when a run succeeds.
QString repair_flag_path(const QString& root);

/// Hard link `from` at `to` (same volume: instant, no extra space), else copy.
/// Symlinks are re-created with their own target, never followed.
bool link_or_copy(const QString& from, const QString& to, bool allow_copy = true);

} // namespace fincept::python::relocation
