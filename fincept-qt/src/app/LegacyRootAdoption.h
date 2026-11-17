#pragma once
// LegacyRootAdoption.h — first start under the edition's own data root.
//
// Released versions of this terminal kept everything in com.fincept.terminal
// (%LOCALAPPDATA%, ~/Library/Application Support, ~/.local/share) and in the
// QSettings store Fincept/FinceptTerminal — the same names the separate private
// build uses. Both builds therefore opened, migrated and deleted each other's
// files. This build now uses its own names (core/config/EditionIdentity.h).
//
// adopt_legacy_root() carries an existing community install over, once:
//   - FINCEPT_DATA_DIR set ............... nothing to do (the caller chose the root)
//   - decision already recorded .......... nothing to do (<root>/.edition-root.json)
//   - the new root already holds data .... recorded as "existing", nothing moved
//   - no legacy root ..................... recorded as "fresh"
//   - legacy root is this lineage's ...... ADOPTED: renamed to the new root (one
//       atomic same-volume rename), or — when another process holds it or the
//       rename fails — its state is COPIED (databases through a consistent
//       SQLite snapshot; the Python runtime and uv cache hard-linked where the
//       volume allows, else copied; caches and logs left behind) and the legacy
//       root is left as it was. QSettings values are copied (never moved) into
//       the edition's store.
//   - legacy root is the private build's,
//     or ownership cannot be determined .. left untouched; this build starts
//       fresh in its own root.
//
// "This lineage's" is decided read-only: the main database is opened immutable
// and classified with MigrationRunner::inspect() — at or below this build's
// schema and every table/column this build's migrations create present — and
// the root must hold none of the files only the private build writes.
// Nothing under the legacy root is ever written, renamed into, or deleted
// except by the single rename that adopts it.
//
// The Python runtime moves too, and uv records absolute paths in it, so after
// either path it is relocated (python/PythonEnvRelocation.h: pyvenv.cfg,
// interpreter links, shebangs, Windows launchers, _sysconfigdata) and checked by
// importing the essentials. Anything that still does not work is repaired by
// PythonSetupManager in the background with the terminal open — offline first,
// from the uv cache that moved along — never through the first-time setup
// screen. A root whose runtime still names another root is relocated on any
// later start as well, so an interruption between the rename and the
// relocation heals itself.
//
// What an OLDER binary sees afterwards (decided, on purpose): nothing. After a
// rename the old folder is gone and nothing is put back — no marker file, no
// link. com.fincept.terminal is the private build's name: anything left there
// (a folder holding a note, a link to the new root) would hand that build this
// edition's data or a folder it treats as its own, and an older binary would
// not read a note anyway. An older community binary therefore starts like a
// first run (sign-in, Python setup) in a fresh com.fincept.terminal, and the
// data it shows is not the adopted data; going back to it means moving the
// edition's folder back by hand. The record of the move is
// <root>/.edition-root.json and the log.
//
// Safe to interrupt: until the rename the legacy root is untouched and the new
// root holds no data, so the next start repeats the decision; a half-made copy
// lives in a sibling staging folder that the next attempt discards. Never shows
// UI. Must run after QApplication exists and before anything opens a file under
// AppPaths::root() (TerminalShell::initialise()).

#include <QString>

namespace fincept::legacy_root {

enum class Outcome {
    Skipped,          ///< FINCEPT_DATA_DIR is set
    AlreadyDecided,   ///< an earlier start recorded the decision
    Existing,         ///< the new root already held data
    Fresh,            ///< no legacy root
    Adopted,          ///< legacy root renamed into place
    Copied,           ///< legacy root's essential state copied; legacy root kept
    LeftForOtherBuild,///< legacy root belongs to another build: untouched
    Undetermined,     ///< ownership could not be decided: untouched
    Deferred,         ///< could not decide now (lock busy); retried next start
};

struct Report {
    Outcome outcome = Outcome::Skipped;
    QString reason;      ///< one line, for the log
    QString root;        ///< AppPaths::root()
    QString legacy_root; ///< the com.fincept.terminal folder considered
};

/// See the file comment. Idempotent; logs the path taken.
Report adopt_legacy_root();

/// Stable lower-case name of an outcome ("adopted", "left-for-other-build", ...).
const char* outcome_name(Outcome o);

} // namespace fincept::legacy_root
