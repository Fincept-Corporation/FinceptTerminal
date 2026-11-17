#pragma once
// EditionRootDecision.h — the pure half of LegacyRootAdoption: whose is the
// com.fincept.terminal folder that releases shared with the private build, and
// what may this edition do with it.
//
// Qt Core only. Everything here READS the file system (QFileInfo / QDirIterator)
// and never writes, renames or locks anything, so the rules can be unit-tested
// on fake directory trees (tests/tst_edition_root_decision.cpp) and can never
// disturb a folder the private build is using.
//
// Order of the rules (LegacyRootAdoption.cpp applies them with its lock held and
// after its decision marker was found missing):
//   1. the edition's own root already holds data   -> Existing (nothing moved)
//   2. no legacy folder                            -> Fresh
//   3. the legacy folder is a link / junction / file -> Undetermined (untouched)
//   4. it holds a file only the private build writes -> LeftForOtherBuild
//   5. its databases decide (by_databases):
//        main data/fincept.db missing / empty / unreadable -> Undetermined
//        main or a named profile's database not this lineage's -> LeftForOtherBuild
//        otherwise -> adopt (this edition's data from before the split)

#include "app/LegacyRootAdoption.h"

#include <QList>
#include <QString>
#include <QStringList>

namespace fincept::legacy_root::decision {

/// Paths, relative to a data root, that only the private build writes (checked
/// against its source: graphics.json at the root on every start, test-settings
/// of its test runs, its own stores under data/). None occurs in this tree.
QStringList private_fingerprints();

/// The private_fingerprints() present under `legacy_root` (empty: none).
QStringList private_fingerprints_in(const QString& legacy_root);

/// True when `dir` holds no data: it does not exist, or holds only folders and
/// a top-level profiles.json (what main() and the crash handler create before
/// adoption runs).
bool holds_no_data(const QString& dir);

struct FileFacts {
    bool root_holds_data = false;  ///< the edition's own root is in use already
    bool legacy_exists = false;    ///< the legacy path exists (or is a dangling link)
    bool legacy_plain_dir = false; ///< ... and is a real directory, not a link / junction
    QStringList private_files;     ///< private_fingerprints_in(legacy root)
};

/// Read-only look at the edition's root and the legacy root.
FileFacts gather(const QString& root, const QString& legacy_root);

enum class DbKind { Missing, Ours, Foreign, Empty, Unreadable };

struct Verdict {
    /// True: `outcome` is the decision. False: go on — after by_files() the
    /// databases decide; after by_databases() the folder is this lineage's and
    /// is adopted.
    bool final = true;
    Outcome outcome = Outcome::Undetermined;
    QString reason;
};

/// Rules 1-4.
Verdict by_files(const FileFacts& facts);

struct ProfileDb {
    QString name;
    DbKind kind = DbKind::Missing;
    QString detail;
};

/// Rule 5. `profiles` are the named (non-default) profiles of the legacy root.
Verdict by_databases(DbKind main_db, const QString& main_detail, const QList<ProfileDb>& profiles);

} // namespace fincept::legacy_root::decision
