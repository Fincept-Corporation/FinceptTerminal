#include "app/LegacyRootAdoption.h"

#include "app/EditionRootDecision.h"
#include "app/InstanceLock.h"
#include "core/config/AppPaths.h"
#include "core/config/AppSettings.h"
#include "core/config/EditionIdentity.h"
#include "core/config/ProfileManager.h"
#include "core/logging/Logger.h"
#include "core/privacy/LogRedact.h"
#include "python/PythonEnvRelocation.h"
#include "storage/sqlite/SqliteSupport.h"
#include "storage/sqlite/migrations/MigrationRunner.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QLockFile>
#include <QSaveFile>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlError>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QUuid>

#include <algorithm>
#include <cstdio>
#include <iterator>

namespace fincept::legacy_root {
namespace {

constexpr const char* kTag = "EditionRoot";
constexpr const char* kMarkerName = ".edition-root.json";
constexpr int kMaxProfilesChecked = 64;

// Whose the legacy folder is (the files only the private build writes, the
// order of the rules) is decided in EditionRootDecision.{h,cpp}, read-only.
namespace decision = fincept::legacy_root::decision;

// Root entries the copy fallback leaves behind: caches, logs, crash dumps. The
// Python runtime (python::relocation::kRuntimeEntries) is linked or copied and
// then relocated, the uv download cache hard-linked when possible — an adopted
// install must start with a working Python, never in first-time setup.
const char* const kNotCopied[] = {"cache", "logs", "crashdumps", "crashes"};

QStringList g_lines; // for the log file, which main() opens later

void say(const QString& line, bool warn = false) {
    std::fprintf(stderr, "[EditionRoot] %s\n", qUtf8Printable(line));
    std::fflush(stderr);
    g_lines << QString(warn ? QLatin1Char('W') : QLatin1Char('I')) + line;
}

/// A step detail (or a routine outcome): DEBUG in the log; the adoption's outcome
/// line is the INFO summary.
void say_detail(const QString& line) {
    std::fprintf(stderr, "[EditionRoot] %s\n", qUtf8Printable(line)); // stderr as before
    std::fflush(stderr);
    g_lines << QLatin1Char('D') + line;
}

void flush_log_later() {
    // Logger::set_file() runs later in main(); post the lines to the first
    // event-loop turn, when the log file is open.
    const QStringList lines = g_lines;
    g_lines.clear();
    if (lines.isEmpty() || !QCoreApplication::instance())
        return;
    QTimer::singleShot(0, QCoreApplication::instance(), [lines]() {
        for (const QString& l : lines) {
            // The lines name legacy and new data folders: home directory as "~".
            if (l.startsWith(QLatin1Char('W')))
                LOG_WARN(kTag, privacy::redact_text(l.mid(1)));
            else if (l.startsWith(QLatin1Char('D')))
                LOG_DEBUG(kTag, privacy::redact_text(l.mid(1)));
            else
                LOG_INFO(kTag, privacy::redact_text(l.mid(1)));
        }
    });
}

template <std::size_t N>
bool listed(const char* const (&names)[N], const QString& name) {
    return std::any_of(std::begin(names), std::end(names), [&name](const char* n) { return name == QLatin1String(n); });
}

/// Remove a root that holds no data (empty folders and the top-level manifest
/// only — decision::holds_no_data). Refuses — returns false — as soon as it
/// holds anything else.
bool remove_empty_root(const QString& dir) {
    if (!QFileInfo::exists(dir))
        return true;
    if (!decision::holds_no_data(dir))
        return false;
    QFile::remove(QDir(dir).absoluteFilePath(QStringLiteral("profiles.json")));
    QStringList dirs;
    QDirIterator it(dir, QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext())
        dirs << it.next();
    std::sort(dirs.begin(), dirs.end(), [](const QString& a, const QString& b) { return a.size() > b.size(); });
    for (const QString& d : std::as_const(dirs))
        QDir().rmdir(d); // empty by construction; a non-empty one simply stays
    QDir().rmdir(dir);
    return !QFileInfo::exists(dir);
}

// Highest migration register_known_migrations() registers. Checked against the
// registry once main() has registered everything (see adopt_legacy_root()).
constexpr int kKnownHighestMigration = 66;

void register_known_migrations() {
    // MigrationRunner::inspect() compares a file with the schema this build's
    // migrations create, so the registry must be filled before main() does it.
    // Every register function is idempotent (main() calls them again later).
    // Keep in step with main()'s list: a legacy root written by a build with a
    // migration missing here classifies as "newer" and is left untouched (safe,
    // but not adopted). Releases end at v058; v059 came from development builds
    // run before the edition split.
    using Fn = void (*)();
    static constexpr Fn kAll[] = {
        &register_migration_v001, &register_migration_v002, &register_migration_v003, &register_migration_v004,
        &register_migration_v005, &register_migration_v006, &register_migration_v007, &register_migration_v008,
        &register_migration_v009, &register_migration_v010, &register_migration_v011, &register_migration_v012,
        &register_migration_v013, &register_migration_v014, &register_migration_v015, &register_migration_v016,
        &register_migration_v017, &register_migration_v018, &register_migration_v019, &register_migration_v020,
        &register_migration_v021, &register_migration_v022, &register_migration_v023, &register_migration_v024,
        &register_migration_v025, &register_migration_v026, &register_migration_v027, &register_migration_v028,
        &register_migration_v029, &register_migration_v030, &register_migration_v031, &register_migration_v032,
        &register_migration_v033, &register_migration_v034, &register_migration_v035, &register_migration_v036,
        &register_migration_v037, &register_migration_v038, &register_migration_v039, &register_migration_v040,
        &register_migration_v041, &register_migration_v042, &register_migration_v043, &register_migration_v044,
        &register_migration_v045, &register_migration_v046, &register_migration_v047, &register_migration_v048,
        &register_migration_v049, &register_migration_v050, &register_migration_v051, &register_migration_v052,
        &register_migration_v053, &register_migration_v054, &register_migration_v055, &register_migration_v056,
        &register_migration_v057, &register_migration_v058, &register_migration_v059, &register_migration_v060,
        &register_migration_v061, &register_migration_v062, &register_migration_v063, &register_migration_v064,
        &register_migration_v065, // Derivatives position book + saved strategies
        &register_migration_v066, // Notes board (v065: Derivatives)
    };
    for (Fn f : kAll)
        f();
}

struct DbVerdict {
    decision::DbKind kind = decision::DbKind::Missing;
    QString detail;
};

/// Classify a database WITHOUT writing anything next to it: opened read-only
/// and immutable (no lock, no -wal/-shm created or read).
DbVerdict classify_db(const QString& path) {
    DbVerdict v;
    if (!QFileInfo(path).isFile())
        return v;
    v.kind = decision::DbKind::Unreadable;
    const QString conn = QStringLiteral("fincept_edition_probe_%1").arg(QUuid::createUuid().toString(QUuid::Id128));
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), conn);
        db.setDatabaseName(QUrl::fromLocalFile(QFileInfo(path).absoluteFilePath()).toString(QUrl::FullyEncoded) +
                           QStringLiteral("?immutable=1"));
        db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY;QSQLITE_OPEN_URI"));
        if (!db.open()) {
            v.detail = db.lastError().text();
        } else {
            MigrationRunner runner(db);
            const auto r = runner.inspect();
            if (r.is_err()) {
                v.detail = QString::fromStdString(r.error());
            } else {
                const SchemaInspection si = r.value();
                v.detail = QStringLiteral("schema v%1, this build v%2").arg(si.current).arg(si.target);
                switch (si.state) {
                    case SchemaState::UpToDate:
                    case SchemaState::NeedsUpgrade:
                        v.kind = decision::DbKind::Ours;
                        break;
                    case SchemaState::Fresh:
                        v.kind = decision::DbKind::Empty;
                        break;
                    case SchemaState::Newer:
                        v.kind = decision::DbKind::Foreign;
                        v.detail += QStringLiteral(", newer than this build");
                        break;
                    case SchemaState::Incompatible:
                        v.kind = decision::DbKind::Foreign;
                        v.detail += QStringLiteral(", lacks %1%2")
                                        .arg(si.missing.mid(0, 4).join(QLatin1String(", ")))
                                        .arg(si.missing.size() > 4 ? QStringLiteral(", ...") : QString());
                        break;
                }
            }
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(conn);
    return v;
}

QStringList legacy_profile_names(const QString& legacy_root) {
    QStringList names{QStringLiteral("default")};
    QFile f(legacy_root + QStringLiteral("/profiles.json"));
    if (!f.exists())
        return names; // single-profile install: nothing to read
    if (!f.open(QIODevice::ReadOnly)) {
        say(QStringLiteral("Legacy profiles manifest could not be read, adopting the default profile only path=%1 "
                           "error=\"%2\"")
                .arg(f.fileName(), f.errorString()),
            /*warn=*/true);
        return names;
    }
    QJsonParseError parse_error{};
    const QJsonDocument manifest = QJsonDocument::fromJson(f.readAll(), &parse_error);
    if (parse_error.error != QJsonParseError::NoError) {
        say(QStringLiteral("Legacy profiles manifest could not be parsed, adopting the default profile only path=%1 "
                           "error=\"%2\"")
                .arg(f.fileName(), parse_error.errorString()),
            /*warn=*/true);
        return names;
    }
    const QJsonArray arr = manifest.object().value(QStringLiteral("profiles")).toArray();
    for (const QJsonValue& v : arr) {
        const QString n = v.isObject() ? v.toObject().value(QStringLiteral("name")).toString() : v.toString();
        // A name is a folder under profiles/: never one that leaves it.
        if (n.contains(QLatin1Char('/')) || n.contains(QLatin1Char('\\')) || n.startsWith(QLatin1Char('.')))
            continue;
        if (!n.isEmpty() && !names.contains(n))
            names << n;
        if (names.size() >= kMaxProfilesChecked)
            break;
    }
    return names;
}

/// A process that still uses the legacy identity — the private build, or a
/// release before the edition split — answers on its single-instance socket
/// ("FinceptTerminal-<profile>", InstanceLock::legacy_socket_name_for).
/// Connecting and closing without sending is ignored by InstanceLock.
bool legacy_instance_running(const QStringList& profiles) {
    for (const QString& p : profiles) {
        QLocalSocket sock;
        sock.connectToServer(InstanceLock::legacy_socket_name_for(
            QStringLiteral("%1-%2").arg(QLatin1String(edition::kLegacyInstanceKeyPrefix), p)));
        if (sock.waitForConnected(150)) {
            sock.abort();
            return true;
        }
    }
    return false;
}

/// Copy every value of the legacy QSettings store that the edition's store does
/// not have yet. The legacy store is only read (it is the private build's too).
int copy_legacy_settings() {
    QSettings legacy(QString::fromLatin1(edition::kLegacySettingsOrganization),
                     QString::fromLatin1(edition::kLegacySettingsApplication));
    legacy.setFallbacksEnabled(false); // macOS: not the global preference domain
    QSettings mine = app_settings::open();
    int copied = 0;
    for (const QString& key : legacy.allKeys()) {
        if (mine.contains(key))
            continue;
        mine.setValue(key, legacy.value(key));
        ++copied;
    }
    mine.sync();
    return copied;
}

bool is_sqlite_file(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    return f.read(16) == QByteArray("SQLite format 3\0", 16);
}

/// Consistent copy of a database another process may be writing (VACUUM INTO on
/// a read-only connection; committed -wal content included). With no -wal or
/// -journal beside it nobody has the file open for writing, and it is read
/// immutable, so not even an empty -wal / -shm is created next to it.
bool snapshot_db(const QString& path, const QString& dest) {
    const QString conn = QStringLiteral("fincept_edition_copy_%1").arg(QUuid::createUuid().toString(QUuid::Id128));
    const bool quiescent =
        !QFileInfo::exists(path + QStringLiteral("-wal")) && !QFileInfo::exists(path + QStringLiteral("-journal"));
    bool ok = false;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), conn);
        if (quiescent) {
            db.setDatabaseName(QUrl::fromLocalFile(QFileInfo(path).absoluteFilePath()).toString(QUrl::FullyEncoded) +
                               QStringLiteral("?immutable=1"));
            db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY;QSQLITE_OPEN_URI"));
        } else {
            db.setDatabaseName(path);
            db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY;QSQLITE_BUSY_TIMEOUT=5000"));
        }
        if (db.open()) {
            ok = db::backup_to(db, dest).is_ok();
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(conn);
    return ok;
}

bool copy_file(const QString& from, const QString& to) {
    QDir().mkpath(QFileInfo(to).absolutePath());
    if (is_sqlite_file(from) && snapshot_db(from, to))
        return true;
    QFile::remove(to);
    if (!QFile::copy(from, to))
        return false;
    // A raw copy of a database must take its uncheckpointed -wal along.
    if (QFileInfo::exists(from + QStringLiteral("-wal"))) {
        QFile::remove(to + QStringLiteral("-wal"));
        QFile::copy(from + QStringLiteral("-wal"), to + QStringLiteral("-wal"));
    }
    return true;
}

/// The Python runtime, file by file: hard links where the volume allows (instant,
/// no extra space; relocation later replaces, never edits, the files it changes,
/// so the old folder's copies stay as they are), else copies; symlinks re-created
/// with their own target. `allow_copy` false (the uv cache): what cannot be
/// linked is skipped — a cache is only an optimisation.
void link_runtime(const QString& from, const QString& to, bool allow_copy, int* failures) {
    QDir().mkpath(to);
    const QFileInfoList entries =
        QDir(from).entryInfoList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
    for (const QFileInfo& fi : entries) {
        const QString dest = to + QLatin1Char('/') + fi.fileName();
        if (fi.isDir() && !fi.isSymLink()) {
            link_runtime(fi.absoluteFilePath(), dest, allow_copy, failures);
            continue;
        }
        if (!python::relocation::link_or_copy(fi.absoluteFilePath(), dest, allow_copy) && allow_copy && failures)
            ++*failures;
    }
}

/// Copy `from` into `to`: the Python runtime through link_runtime(), caches and
/// logs not at all, SQLite side files folded in by the snapshot, other symlinks
/// never (they are never followed out of the root).
void copy_essential(const QString& from, const QString& to, bool top_level, QStringList* problems) {
    QDir().mkpath(to);
    const QFileInfoList entries =
        QDir(from).entryInfoList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
    for (const QFileInfo& fi : entries) {
        const QString name = fi.fileName();
        const QString dest = to + QLatin1Char('/') + name;
        if (top_level && listed(kNotCopied, name))
            continue;
        if (top_level && (listed(python::relocation::kRuntimeEntries, name) ||
                          name == QLatin1String(python::relocation::kCacheEntry))) {
            const bool cache = name == QLatin1String(python::relocation::kCacheEntry);
            int failures = 0;
            if (fi.isDir() && !fi.isSymLink())
                link_runtime(fi.absoluteFilePath(), dest, !cache, &failures);
            else if (!python::relocation::link_or_copy(fi.absoluteFilePath(), dest, true))
                ++failures;
            if (failures && problems)
                *problems << QStringLiteral("%1 (%2 file(s))").arg(name).arg(failures);
            continue;
        }
        if (fi.isSymLink())
            continue;
        if (fi.isDir()) {
            copy_essential(fi.absoluteFilePath(), dest, false, problems);
            continue;
        }
        if (name.endsWith(QLatin1String("-wal")) || name.endsWith(QLatin1String("-shm")) ||
            name.endsWith(QLatin1String("-journal")))
            continue;
        if (!copy_file(fi.absoluteFilePath(), dest) && problems)
            *problems << QDir(from).relativeFilePath(fi.absoluteFilePath());
    }
}

/// After the runtime landed at `root` (renamed, or copied to `dir_on_disk` that
/// becomes `root`): rewrite what still names an old root, and — once it is at
/// `root` (`verify`) — run every venv and import the essentials. Returns the
/// sentence for the log; `repair` is set when a background repair was scheduled.
/// `relocated` (copy path): the summary of the relocation already done in the
/// staging folder — then the tree is not scanned a second time.
QString settle_python(const QString& dir_on_disk, const QString& legacy_root, const QString& root, bool verify,
                      const QString& relocated = {}, bool* repair = nullptr) {
    namespace rel = python::relocation;
    QString relocation_summary = relocated;
    if (relocation_summary.isEmpty())
        relocation_summary = rel::relocate_tree(dir_on_disk, legacy_root, root).summary();
    if (!verify)
        return relocation_summary;
    const QList<rel::VenvVerdict> verdicts = rel::verify_and_prepare(root);
    bool any = false;
    bool needs_repair = false;
    for (const rel::VenvVerdict& v : verdicts) {
        any = any || v.state != rel::VenvVerdict::Absent;
        needs_repair = needs_repair || v.state == rel::VenvVerdict::Broken || v.state == rel::VenvVerdict::PackagesMissing;
    }
    if (repair)
        *repair = needs_repair;
    if (!any)
        return QStringLiteral("no Python environment was installed");
    QString s = QStringLiteral("Python environment: %1; checked by importing the essentials: %2")
                    .arg(relocation_summary, rel::describe(verdicts));
    if (needs_repair)
        s += QStringLiteral(" — repaired in the background with the terminal open (no setup screen)");
    return s;
}

/// finagent_core/repositories.py kept agents.db outside the data root
/// (%APPDATA%/fincept-terminal, ~/fincept-terminal elsewhere), where the
/// private build also reads it. It now lives in <root>/finagent; an adopted
/// install takes a copy. The original stays where it is.
void copy_legacy_agents_db(const QString& root) {
    const QString legacy = QDir::cleanPath(qEnvironmentVariable("APPDATA", QDir::homePath()) +
                                           QStringLiteral("/fincept-terminal/agents.db"));
    const QString dest = root + QStringLiteral("/finagent/agents.db");
    if (!QFileInfo(legacy).isFile() || QFileInfo::exists(dest))
        return;
    if (copy_file(legacy, dest))
        say_detail(QStringLiteral("copied the agent repository %1 to %2").arg(legacy, dest));
    else
        say(QStringLiteral("could not copy the agent repository %1").arg(legacy), true);
}

void write_marker(const Report& rep) {
    QDir().mkpath(rep.root);
    QJsonObject o{
        {QStringLiteral("edition"), QLatin1String(edition::kId)},
        {QStringLiteral("decision"), QLatin1String(outcome_name(rep.outcome))},
        {QStringLiteral("reason"), rep.reason},
        {QStringLiteral("legacy_root"), QDir::toNativeSeparators(rep.legacy_root)},
        {QStringLiteral("decided_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {QStringLiteral("app_version"), QCoreApplication::applicationVersion()},
    };
    QSaveFile f(rep.root + QLatin1Char('/') + QLatin1String(kMarkerName));
    if (!f.open(QIODevice::WriteOnly) || f.write(QJsonDocument(o).toJson(QJsonDocument::Indented)) < 0 || !f.commit())
        say(QStringLiteral("could not record the decision in %1").arg(f.fileName()), true);
}

} // namespace

const char* outcome_name(Outcome o) {
    switch (o) {
        case Outcome::Skipped:
            return "skipped";
        case Outcome::AlreadyDecided:
            return "already-decided";
        case Outcome::Existing:
            return "existing";
        case Outcome::Fresh:
            return "fresh";
        case Outcome::Adopted:
            return "adopted";
        case Outcome::Copied:
            return "copied";
        case Outcome::LeftForOtherBuild:
            return "left-for-other-build";
        case Outcome::Undetermined:
            return "undetermined";
        case Outcome::Deferred:
            return "deferred";
    }
    return "unknown";
}

Report adopt_legacy_root() {
    Report rep;
    rep.root = AppPaths::root();
    rep.legacy_root = AppPaths::legacy_shared_root();

    const auto finish = [&rep](Outcome o, const QString& reason, bool record) {
        rep.outcome = o;
        rep.reason = reason;
        if (record)
            write_marker(rep);
        const bool untouched = o == Outcome::LeftForOtherBuild || o == Outcome::Undetermined;
        const QString line =
            QStringLiteral("%1 — %2. Data root: %3%4")
                .arg(QLatin1String(outcome_name(o)), reason, QDir::toNativeSeparators(rep.root),
                     untouched ? QStringLiteral("; %1 left untouched").arg(QDir::toNativeSeparators(rep.legacy_root))
                               : QString());
        // Skipped (FINCEPT_DATA_DIR set) repeats at every start of such a profile: DEBUG.
        if (o == Outcome::Skipped)
            say_detail(line);
        else
            say(line, o == Outcome::Deferred);
        flush_log_later();
        return rep;
    };

    // Drift guard for register_known_migrations(): by the first event-loop turn
    // main() has registered every migration of this build.
    if (QCoreApplication::instance()) {
        QTimer::singleShot(0, QCoreApplication::instance(), []() {
            const int registered = MigrationRunner::highest_registered_version();
            if (registered > kKnownHighestMigration) {
                LOG_WARN(kTag, QStringLiteral("LegacyRootAdoption knows migrations up to v%1 but this build registers "
                                              "v%2: add the new register_migration_vNNN to register_known_migrations()")
                                   .arg(kKnownHighestMigration)
                                   .arg(registered));
            }
        });
    }

    if (AppPaths::root_override_refused()) {
        say(QStringLiteral("FINCEPT_DATA_DIR is ignored: it names the private build's data folder (or one holding its "
                           "files); this edition uses its own default data folder"),
            /*warn=*/true);
        flush_log_later();
    }
    if (!AppPaths::root_override().isEmpty())
        return finish(Outcome::Skipped, QStringLiteral("FINCEPT_DATA_DIR chooses the data root"), false);

    // One decision at a time across processes (two profiles start together).
    // The lock sits beside the roots, never inside the legacy one.
    const QString lock_path = AppPaths::platform_data_base() + QLatin1Char('/') +
                              QLatin1String(edition::kDataDirName) + QStringLiteral(".adoption.lock");

    const QString marker = rep.root + QLatin1Char('/') + QLatin1String(kMarkerName);
    if (QFileInfo::exists(marker)) {
        rep.outcome = Outcome::AlreadyDecided;
        // Safety net, two small reads per start: a runtime that still names
        // another root (an adoption interrupted between the rename and the
        // relocation, a root adopted by a build without relocation, a folder
        // moved by hand) is relocated before PythonSetupManager judges it.
        if (!python::relocation::foreign_roots(rep.root).isEmpty()) {
            QLockFile lock(lock_path);
            lock.setStaleLockTime(10 * 60 * 1000);
            if (lock.tryLock(15000)) {
                say(QStringLiteral("decided earlier; %1").arg(settle_python(rep.root, rep.legacy_root, rep.root, true)));
                flush_log_later();
            }
        }
        return rep;
    }

    QDir().mkpath(AppPaths::platform_data_base());
    QLockFile lock(lock_path);
    lock.setStaleLockTime(10 * 60 * 1000);
    if (!lock.tryLock(15000))
        return finish(Outcome::Deferred, QStringLiteral("another start of this terminal holds the adoption lock"),
                      false);
    if (QFileInfo::exists(marker)) {
        rep.outcome = Outcome::AlreadyDecided;
        return rep;
    }

    // ── Whose is it? Read-only from here to the rename (EditionRootDecision.h).
    const decision::FileFacts facts = decision::gather(rep.root, rep.legacy_root);
    const decision::Verdict by_files = decision::by_files(facts);
    if (by_files.final) {
        QString reason = by_files.reason;
        if (by_files.outcome == Outcome::Existing && !python::relocation::foreign_roots(rep.root).isEmpty())
            reason += QStringLiteral("; ") + settle_python(rep.root, rep.legacy_root, rep.root, true);
        return finish(by_files.outcome, reason, true);
    }

    register_known_migrations();
    const DbVerdict main_db = classify_db(rep.legacy_root + QStringLiteral("/data/fincept.db"));
    const QStringList profiles = legacy_profile_names(rep.legacy_root);
    QList<decision::ProfileDb> profile_dbs;
    if (main_db.kind == decision::DbKind::Ours) {
        for (const QString& p : profiles) {
            if (p == QLatin1String("default"))
                continue;
            const DbVerdict v =
                classify_db(rep.legacy_root + QStringLiteral("/profiles/") + p + QStringLiteral("/data/fincept.db"));
            profile_dbs.append({p, v.kind, v.detail});
        }
    }
    const decision::Verdict by_db = decision::by_databases(main_db.kind, main_db.detail, profile_dbs);
    if (by_db.final)
        return finish(by_db.outcome, by_db.reason, true);

    // ── This lineage's: adopt. ───────────────────────────────────────────────
    const bool held = legacy_instance_running(profiles);
    const int settings_copied = copy_legacy_settings();
    const QString settings_note = QStringLiteral("%1 setting(s) copied from %2/%3")
                                      .arg(settings_copied)
                                      .arg(QLatin1String(edition::kLegacySettingsOrganization),
                                           QLatin1String(edition::kLegacySettingsApplication));

    bool moved = false;
    QString rename_note;
    if (held) {
        rename_note = QStringLiteral("a program using the old identity is running");
    } else {
        for (int attempt = 0; attempt < 3 && !moved; ++attempt) {
            if (!remove_empty_root(rep.root))
                break;
            moved = QDir().rename(rep.legacy_root, rep.root);
        }
        if (!moved)
            rename_note = QStringLiteral("the folder could not be renamed (in use, or on another volume)");
    }
    if (moved) {
        ProfileManager::instance().reload_after_root_change();
        copy_legacy_agents_db(rep.root);
        const QString python_note = settle_python(rep.root, rep.legacy_root, rep.root, true);
        return finish(Outcome::Adopted,
                      QStringLiteral("this build's data (%1) was moved from %2; %3. %4")
                          .arg(main_db.detail, QDir::toNativeSeparators(rep.legacy_root), settings_note, python_note),
                      true);
    }

    // Copy fallback. Staging is this edition's own folder; a leftover from an
    // interrupted attempt is discarded.
    const QString staging = rep.root + QStringLiteral(".adopting");
    QDir(staging).removeRecursively();
    QStringList problems;
    copy_essential(rep.legacy_root, staging, true, &problems);
    // Rewritten in the staging folder for the root it is about to become.
    const QString staged_relocation = settle_python(staging, rep.legacy_root, rep.root, false);
    if (!remove_empty_root(rep.root) || !QDir().rename(staging, rep.root)) {
        return finish(Outcome::Deferred,
                      QStringLiteral("the copy is ready in %1 but the data root could not be replaced; retried at the "
                                     "next start")
                          .arg(QDir::toNativeSeparators(staging)),
                      false);
    }
    ProfileManager::instance().reload_after_root_change();
    copy_legacy_agents_db(rep.root);
    const QString python_note = settle_python(rep.root, rep.legacy_root, rep.root, true, staged_relocation);
    QString reason = QStringLiteral("this build's data (%1) was copied — %2 — and %3 was kept as it was; %4. Caches "
                                    "and logs were not copied. %5")
                         .arg(main_db.detail, rename_note, QDir::toNativeSeparators(rep.legacy_root), settings_note,
                              python_note);
    if (!problems.isEmpty())
        reason += QStringLiteral(". Not copied: %1").arg(problems.mid(0, 10).join(QLatin1String(", ")));
    return finish(Outcome::Copied, reason, true);
}

} // namespace fincept::legacy_root
