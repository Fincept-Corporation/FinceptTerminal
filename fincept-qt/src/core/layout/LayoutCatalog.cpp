#include "core/layout/LayoutCatalog.h"

#include "core/layout/WorkspaceShell.h"
#include "core/logging/Logger.h"
#include "core/profile/ProfilePaths.h"
#include "storage/sqlite/SqliteSupport.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>

#include <functional>

namespace fincept {

namespace {
constexpr const char* kCatalogTag = "LayoutCatalog";
constexpr const char* kConnectionName = "fincept_layouts_index";

/// PRAGMA user_version of layouts/_index.db. Bump together with an
/// `if (from_version < N)` step in create_index_schema() and the expected
/// columns in expected_index_tables().
constexpr int kIndexSchemaVersion = 1;

// UI-thread only and tiny (one row per saved layout). Same lock wait as the
// other stores; the -wal is truncated back to 4 MB after a checkpoint.
const db::PragmaProfile kIndexPragmas{/*cache_kb=*/1000, /*busy_timeout_ms=*/5000, /*foreign_keys=*/false,
                                      /*journal_size_limit=*/4LL * 1024 * 1024};

/// The index connection, looked up by name each time: no QSqlDatabase handle
/// is cached (a static one outlived Qt's SQL registry at exit and pinned the
/// connection so it could never be removed or reopened).
QSqlDatabase index_db() {
    return QSqlDatabase::database(QString::fromLatin1(kConnectionName), /*open=*/false);
}

// ── Open gate ────────────────────────────────────────────────────────────────
// LOCAL COPY: the same gate (GateVerdict, GateResult, TableSpec, verify_tables,
// run_open_gate) is in CacheDatabase.cpp and WorkspaceDb.cpp, and
// quarantine_db_files() is also in WorkspaceDb.cpp. They belong in
// storage/sqlite/SqliteSupport; fold the copies into one there.
// Its own named namespace because the unity build merges this file's anonymous
// namespace with the other copies; pull it in with a FUNCTION-scope
// `using namespace layout_index_gate;` only (a namespace-scope one would leak).
namespace layout_index_gate {

enum class GateVerdict {
    Ok,    ///< schema current and verified
    Reset, ///< damaged, or written by another build: replace the file
    Fail,  ///< environmental (busy, read-only, disk full, permissions): leave the file alone
};

struct GateResult {
    GateVerdict verdict = GateVerdict::Fail;
    QString label;  ///< "corrupt" or "incompatible" when verdict == Reset
    QString reason; ///< for the log only (may carry driver text)
    db::SqlErrorKind kind = db::SqlErrorKind::None;
    int found_version = 0;  ///< PRAGMA user_version the file had
    bool was_empty = false; ///< the file had no tables (new file)
};

struct TableSpec {
    QString table;
    QString pk;          ///< column that must be part of the primary key
    QStringList columns; ///< columns that must exist; extra columns are allowed
};

bool is_environmental(db::SqlErrorKind k) {
    switch (k) {
        case db::SqlErrorKind::Busy:
        case db::SqlErrorKind::Locked:
        case db::SqlErrorKind::ReadOnly:
        case db::SqlErrorKind::Full:
        case db::SqlErrorKind::CantOpen:
        case db::SqlErrorKind::IoErr:
            return true;
        default:
            return false;
    }
}

GateResult gate_reset(const QString& label, const QString& reason, db::SqlErrorKind kind = db::SqlErrorKind::None) {
    GateResult g;
    g.verdict = GateVerdict::Reset;
    g.label = label;
    g.reason = reason;
    g.kind = kind;
    return g;
}

/// A driver error while reading the file: damage resets it, anything else
/// (busy, read-only, I/O, permissions) leaves it alone.
GateResult gate_error(db::SqlErrorKind kind, const QString& reason) {
    if (kind == db::SqlErrorKind::Corrupt || kind == db::SqlErrorKind::NotADb)
        return gate_reset(QStringLiteral("corrupt"), reason, kind);
    GateResult g;
    g.verdict = GateVerdict::Fail;
    g.reason = reason;
    g.kind = kind;
    return g;
}

/// Every table in `specs` exists, has at least the listed columns, and has `pk`
/// in its primary key (read through pragma_table_info).
bool verify_tables(QSqlDatabase& db, const QList<TableSpec>& specs, QString* problem) {
    for (const TableSpec& spec : specs) {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT name, pk FROM pragma_table_info(?)"));
        q.addBindValue(spec.table);
        if (!q.exec()) {
            *problem = QStringLiteral("cannot read the columns of %1: %2").arg(spec.table, q.lastError().text());
            return false;
        }
        QSet<QString> have;
        bool pk_ok = spec.pk.isEmpty();
        while (q.next()) {
            const QString name = q.value(0).toString().toLower();
            have.insert(name);
            if (name == spec.pk.toLower() && q.value(1).toInt() > 0)
                pk_ok = true;
        }
        if (have.isEmpty()) {
            *problem = QStringLiteral("table %1 is missing").arg(spec.table);
            return false;
        }
        for (const QString& col : spec.columns) {
            if (!have.contains(col.toLower())) {
                *problem = QStringLiteral("table %1 has no column %2").arg(spec.table, col);
                return false;
            }
        }
        if (!pk_ok) {
            *problem = QStringLiteral("%1.%2 is not the primary key").arg(spec.table, spec.pk);
            return false;
        }
    }
    return true;
}

/// Read-only checks first (is it SQLite, is it intact, which schema version),
/// then `build(from_version)` plus the column check in ONE `BEGIN IMMEDIATE`
/// transaction that also stamps PRAGMA user_version = `version`. A file that
/// fails any check is rolled back and never written to. Journal mode and the
/// other PRAGMAs are the caller's job AFTER an Ok verdict, so a file that is
/// about to be replaced is not even switched to WAL. `legacy_check` runs for
/// files with tables but user_version 0 and returns a reason to reject, or "".
GateResult run_open_gate(QSqlDatabase& db, int version, const QList<TableSpec>& specs,
                         const std::function<Result<void>(int from_version)>& build,
                         const std::function<QString()>& legacy_check = {}) {
    auto run = [&db](const QString& sql, QSqlError* error) {
        QSqlQuery q(db);
        if (q.exec(sql))
            return true;
        if (error)
            *error = q.lastError();
        return false;
    };

    // 1. SQLite opens lazily: this is the first read of the file, and where
    //    "file is not a database" shows up.
    int tables = 0;
    {
        QSqlQuery q(db);
        if (!q.exec(QStringLiteral("SELECT count(*) FROM sqlite_master")) || !q.next())
            return gate_error(db::classify(q.lastError()), "cannot read the schema: " + q.lastError().text());
        tables = q.value(0).toInt();
    }

    // 2. Damaged pages.
    if (tables > 0) {
        db::SqlErrorKind kind = db::SqlErrorKind::None;
        auto qc = db::quick_check(db, &kind);
        if (qc.is_err())
            return gate_error(kind, QString::fromStdString(qc.error()));
    }

    // 3. Schema version. 0 = new file or one from before versioning.
    int found = 0;
    {
        QSqlQuery q(db);
        if (!q.exec(QStringLiteral("PRAGMA user_version")) || !q.next())
            return gate_error(db::classify(q.lastError()), "cannot read user_version: " + q.lastError().text());
        found = q.value(0).toInt();
    }
    if (found < 0 || found > version) {
        return gate_reset(QStringLiteral("incompatible"),
                          QStringLiteral("schema version %1 is newer than or unknown to this build (it supports up to %2)")
                              .arg(found)
                              .arg(version));
    }
    if (found == 0 && tables > 0 && legacy_check) {
        const QString why = legacy_check();
        if (!why.isEmpty())
            return gate_reset(QStringLiteral("incompatible"), why);
    }

    // 4. Create, upgrade, verify, stamp: all or nothing.
    QSqlError error;
    if (!run(QStringLiteral("BEGIN IMMEDIATE"), &error))
        return gate_error(db::classify(error), "cannot start the schema transaction: " + error.text());
    auto rolled_back = [&run](GateResult g) {
        run(QStringLiteral("ROLLBACK"), nullptr);
        return g;
    };

    auto built = build(found);
    if (built.is_err()) {
        const QString text = QString::fromStdString(built.error());
        const db::SqlErrorKind kind = db::classify_text(text);
        if (is_environmental(kind))
            return rolled_back(gate_error(kind, "schema setup failed: " + text));
        const bool damaged = (kind == db::SqlErrorKind::Corrupt || kind == db::SqlErrorKind::NotADb);
        return rolled_back(gate_reset(damaged ? QStringLiteral("corrupt") : QStringLiteral("incompatible"),
                                      "schema setup failed: " + text, kind));
    }
    QString problem;
    if (!verify_tables(db, specs, &problem))
        return rolled_back(
            gate_reset(QStringLiteral("incompatible"), "column check failed: " + problem, db::SqlErrorKind::Schema));
    if (found != version && !run(QStringLiteral("PRAGMA user_version = %1").arg(version), &error))
        return rolled_back(gate_error(db::classify(error), "cannot stamp user_version: " + error.text()));
    if (!run(QStringLiteral("COMMIT"), &error))
        return rolled_back(gate_error(db::classify(error), "cannot commit the schema: " + error.text()));

    GateResult ok;
    ok.verdict = GateVerdict::Ok;
    ok.found_version = found;
    ok.was_empty = (tables == 0);
    return ok;
}

/// Move a database that must not be used out of the way WITHOUT deleting it:
/// `<path>` becomes `<path>.<label>-<yyyyMMdd-hhmmss>`, and its -wal/-journal
/// get the same name plus their suffix so the moved copy stays openable. -shm
/// is a rebuildable index and is deleted. Side files move first: a -wal left
/// next to the new, empty file would be replayed into it. Returns the new path,
/// or "" with `problem` set (everything put back). The connection must already
/// be closed — Windows cannot rename an open file.
QString quarantine_db_files(const QString& path, const QString& label, QString* problem) {
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-hhmmss"));
    QString target = QStringLiteral("%1.%2-%3").arg(path, label, stamp);
    for (int n = 2; QFile::exists(target) && n < 100; ++n)
        target = QStringLiteral("%1.%2-%3-%4").arg(path, label, stamp).arg(n);

    const QStringList side_suffixes = {QStringLiteral("-wal"), QStringLiteral("-journal")};
    QStringList moved;
    auto put_back = [&]() {
        for (const QString& suffix : moved)
            QFile::rename(target + suffix, path + suffix);
    };
    for (const QString& suffix : side_suffixes) {
        const QString side = path + suffix;
        if (!QFile::exists(side))
            continue;
        if (!QFile::rename(side, target + suffix)) {
            *problem = QStringLiteral("cannot move %1 aside").arg(side);
            put_back();
            return {};
        }
        moved << suffix;
    }
    QFile::remove(path + QStringLiteral("-shm"));
    if (QFile::exists(path) && !QFile::rename(path, target)) {
        *problem = QStringLiteral("cannot rename %1 to %2").arg(path, target);
        put_back();
        return {};
    }
    return target;
}
} // namespace layout_index_gate
// ── end of open gate ─────────────────────────────────────────────────────────

QList<layout_index_gate::TableSpec> expected_index_tables() {
    return {
        {QStringLiteral("layouts"),
         QStringLiteral("uuid"),
         {"uuid", "name", "kind", "created_at_unix", "updated_at_unix", "description", "thumbnail_path"}},
        {QStringLiteral("_meta"), QStringLiteral("key"), {"key", "value"}},
    };
}

/// CREATE … IF NOT EXISTS for the index, inside the open gate's transaction.
Result<void> create_index_schema(QSqlDatabase& db, int from_version) {
    QSqlQuery q(db);
    if (!q.exec("CREATE TABLE IF NOT EXISTS layouts ("
                "  uuid TEXT PRIMARY KEY,"
                "  name TEXT NOT NULL,"
                "  kind TEXT NOT NULL,"
                "  created_at_unix INTEGER NOT NULL,"
                "  updated_at_unix INTEGER NOT NULL,"
                "  description TEXT,"
                "  thumbnail_path TEXT"
                ")")) {
        return Result<void>::err(q.lastError().text().toStdString());
    }
    // Inside the gate a failure here means the existing `layouts` table has
    // another shape (no kind/updated_at_unix): reject the file, don't limp on.
    if (!q.exec("CREATE INDEX IF NOT EXISTS idx_layouts_kind_updated "
                "ON layouts(kind, updated_at_unix DESC)")) {
        return Result<void>::err(q.lastError().text().toStdString());
    }
    // `_meta` is a generic key/value store. Used today by:
    //   last_loaded_uuid    — Phase 3 cold-boot restore pointer.
    //   fwsp_import_done    — Phase 7 one-shot legacy import flag.
    if (!q.exec("CREATE TABLE IF NOT EXISTS _meta ("
                "  key TEXT PRIMARY KEY,"
                "  value TEXT NOT NULL"
                ")")) {
        return Result<void>::err(q.lastError().text().toStdString());
    }
    // In-file upgrade steps go here as `if (from_version < N) { ... }` (idempotent,
    // same transaction). v1 is the first versioned schema; unversioned indexes
    // from earlier builds already match it.
    Q_UNUSED(from_version);
    return Result<void>::ok();
}

/// Close and unregister the index connection. `orderly` also runs
/// PRAGMA optimize and folds the -wal back (quit / profile switch).
void close_index_connection(bool orderly) {
    const QString name = QString::fromLatin1(kConnectionName);
    if (!QSqlDatabase::contains(name))
        return;
    {
        QSqlDatabase db = QSqlDatabase::database(name, /*open=*/false);
        if (db.isOpen()) {
            if (orderly) {
                {
                    QSqlQuery q(db);
                    if (!q.exec(QStringLiteral("PRAGMA optimize")))
                        LOG_WARN(kCatalogTag, "PRAGMA optimize failed: " + q.lastError().text());
                }
                db::checkpoint_truncate(db);
            }
            db.close();
        }
    } // the last handle must be gone before removeDatabase, or Qt warns "still in use"
    QSqlDatabase::removeDatabase(name);
}

/// Open the index at `path` and run the gate. On Ok the PRAGMAs (WAL with
/// read-back, mmap off, busy timeout) are applied and `journal_mode` is set.
layout_index_gate::GateResult open_index_connection(const QString& path, QString* journal_mode) {
    using namespace layout_index_gate; // function scope only — see the gate's comment
    close_index_connection(/*orderly=*/false);
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QString::fromLatin1(kConnectionName));
    db.setDatabaseName(path);
    if (!db.open())
        return gate_error(db::classify(db.lastError()), "cannot open: " + db.lastError().text());
    GateResult g = run_open_gate(db, kIndexSchemaVersion, expected_index_tables(),
                                 [&db](int from) { return create_index_schema(db, from); });
    if (g.verdict == GateVerdict::Ok)
        *journal_mode = db::apply_pragmas(db, kIndexPragmas, /*set_journal_mode=*/true);
    return g;
}
} // namespace

LayoutCatalog& LayoutCatalog::instance() {
    static LayoutCatalog s;
    return s;
}

Result<void> LayoutCatalog::open() {
    if (opened_)
        return Result<void>::ok();

    // Ensure layouts dir + index file path exist before SQLite opens.
    QDir().mkpath(ProfilePaths::layouts_dir());
    const QString path = ProfilePaths::layouts_index_db();

    using namespace layout_index_gate; // function scope only — see the gate's comment
    QString journal_mode;
    GateResult g = open_index_connection(path, &journal_mode);
    if (g.verdict == GateVerdict::Reset) {
        // Never delete: move it aside (another build may own it) and rebuild
        // from the layout files below.
        close_index_connection(/*orderly=*/false);
        QString problem;
        const QString moved = quarantine_db_files(path, g.label, &problem);
        if (moved.isEmpty()) {
            g.verdict = GateVerdict::Fail;
            g.reason = QStringLiteral("file is %1 (%2) and could not be moved aside: %3").arg(g.label, g.reason, problem);
        } else {
            LOG_WARN(kCatalogTag, QString("Layouts index is %1 (%2) — moved to %3; rebuilding it from the layout files")
                                      .arg(g.label, g.reason, log_path(moved)));
            g = open_index_connection(path, &journal_mode);
            if (g.verdict == GateVerdict::Reset) // quarantine once, never loop
                g.verdict = GateVerdict::Fail;
        }
    }
    if (g.verdict != GateVerdict::Ok) {
        close_index_connection(/*orderly=*/false);
        const QString msg =
            QString("layouts index unavailable [%1]: %2").arg(QString::fromLatin1(db::kind_name(g.kind)), g.reason);
        LOG_ERROR(kCatalogTag, logf("Layouts index unavailable", {{"kind", QString::fromLatin1(db::kind_name(g.kind))},
                                                                  {"path", log_path(path)},
                                                                  {"error", g.reason}}));
        return Result<void>::err(msg.toStdString());
    }
    opened_ = true;
    LOG_DEBUG(kCatalogTag, QString("Layouts catalogue opened: %1 (schema v%2%3, journal_mode=%4)")
                               .arg(log_path(path))
                               .arg(kIndexSchemaVersion)
                               .arg(g.was_empty ? QStringLiteral(", new file")
                                    : g.found_version != kIndexSchemaVersion
                                        ? QStringLiteral(", stamped from v%1").arg(g.found_version)
                                        : QString())
                               .arg(journal_mode.isEmpty() ? QStringLiteral("?") : journal_mode));

    // The JSON files are the source of truth; the index only lists them. An
    // empty index next to existing layout files (quarantined above, deleted by
    // hand, or never written) is rebuilt so the layouts reappear in Launchpad.
    rebuild_index_from_files_();
    return Result<void>::ok();
}

void LayoutCatalog::close() {
    close_index_connection(/*orderly=*/true);
    if (opened_)
        LOG_DEBUG(kCatalogTag, "Layouts catalogue closed");
    opened_ = false;
}

int LayoutCatalog::rebuild_index_from_files_() {
    QSqlDatabase db = index_db();
    if (!db.isOpen())
        return 0;
    {
        QSqlQuery q(db);
        if (!q.exec(QStringLiteral("SELECT count(*) FROM layouts")) || !q.next() || q.value(0).toLongLong() > 0)
            return 0;
    }
    const QFileInfoList files =
        QDir(ProfilePaths::layouts_dir()).entryInfoList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
    if (files.isEmpty())
        return 0;

    // One transaction: one commit for the lot instead of one fsync per row.
    const bool in_tx = QSqlQuery(db).exec(QStringLiteral("BEGIN IMMEDIATE"));
    int added = 0;
    int skipped = 0;
    for (const QFileInfo& fi : files) {
        // load_workspace() finds a layout by its FILE name, so the file name is
        // the id the index must carry; anything not named <uuid>.json is not ours.
        const LayoutId file_id = LayoutId::from_string(fi.completeBaseName());
        if (file_id.is_null()) {
            ++skipped;
            continue;
        }
        QFile f(fi.absoluteFilePath());
        if (!f.open(QIODevice::ReadOnly)) {
            LOG_WARN(kCatalogTag, logf("Layout file could not be read while rebuilding the index",
                                       {{"path", log_path(fi.absoluteFilePath())}, {"error", f.errorString()}}));
            ++skipped;
            continue;
        }
        QJsonParseError perr{};
        const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &perr);
        if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
            LOG_WARN(kCatalogTag, QString("Index rebuild: skipping unreadable layout file %1").arg(fi.fileName()));
            ++skipped;
            continue;
        }
        layout::Workspace w = layout::Workspace::from_json(doc.object());
        w.id = file_id;
        if (w.name.trimmed().isEmpty())
            w.name = fi.completeBaseName();
        if (w.updated_at_unix <= 0)
            w.updated_at_unix = fi.lastModified().toSecsSinceEpoch();
        if (w.created_at_unix <= 0)
            w.created_at_unix = w.updated_at_unix;
        auto r = upsert_index_row(w);
        if (r.is_ok()) {
            ++added;
        } else {
            LOG_WARN(kCatalogTag, QString("Index rebuild: %1 not indexed: %2")
                                      .arg(fi.fileName(), QString::fromStdString(r.error())));
            ++skipped;
        }
    }
    // WorkspaceFwspImporter runs right after the first successful open and
    // records fwsp_import_done in this index. Layout files on disk mean that
    // import already ran for this profile; running it again against a rebuilt
    // index would add every legacy name a second time.
    if (added > 0)
        set_meta(QStringLiteral("fwsp_import_done"), QStringLiteral("1"));
    QSqlQuery commit(db);
    if (in_tx && !commit.exec(QStringLiteral("COMMIT"))) {
        QSqlQuery(db).exec(QStringLiteral("ROLLBACK"));
        LOG_WARN(kCatalogTag, logf("Index rebuild could not be committed, the layouts list stays empty this session",
                                   {{"count", added}, {"error", commit.lastError().text()}}));
        return 0;
    }
    LOG_WARN(kCatalogTag, QString("Layouts index was empty — rebuilt %1 entr%2 from layout files (%3 skipped)")
                              .arg(added)
                              .arg(added == 1 ? QStringLiteral("y") : QStringLiteral("ies"))
                              .arg(skipped));
    return added;
}

QString LayoutCatalog::file_path_for_(const LayoutId& id) const {
    return ProfilePaths::layouts_dir() + "/" + id.to_string() + ".json";
}

Result<void> LayoutCatalog::upsert_index_row(const layout::Workspace& w) {
    QSqlDatabase db = index_db();
    if (!db.isOpen())
        return Result<void>::err("index db not open");

    QSqlQuery q(db);
    q.prepare("INSERT INTO layouts(uuid, name, kind, created_at_unix, updated_at_unix, "
              "                     description, thumbnail_path) "
              "VALUES (?, ?, ?, ?, ?, ?, ?) "
              "ON CONFLICT(uuid) DO UPDATE SET "
              "  name=excluded.name, kind=excluded.kind, "
              "  updated_at_unix=excluded.updated_at_unix, "
              "  description=excluded.description, "
              "  thumbnail_path=excluded.thumbnail_path");
    q.addBindValue(w.id.to_string());
    q.addBindValue(w.name);
    q.addBindValue(w.kind);
    q.addBindValue(w.created_at_unix);
    q.addBindValue(w.updated_at_unix);
    q.addBindValue(w.description);
    q.addBindValue(w.thumbnail_path);
    if (!q.exec())
        return Result<void>::err(q.lastError().text().toStdString());
    return Result<void>::ok();
}

Result<LayoutId> LayoutCatalog::save_workspace(const layout::Workspace& w_in) {
    if (!opened_) {
        auto r = open();
        if (r.is_err())
            return Result<LayoutId>::err(r.error());
    }

    layout::Workspace w = w_in;
    if (w.id.is_null())
        w.id = LayoutId::generate();
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    if (w.created_at_unix == 0)
        w.created_at_unix = now;
    w.updated_at_unix = now;

    // Write the JSON file first; if that fails, we don't want a stale
    // index row pointing at nothing.
    // QSaveFile writes to a temp file and renames on commit(), so a crash or a
    // full disk can no longer leave a truncated layout under the real name (the
    // old Truncate-then-write also ignored a short write and reported success).
    const QString path = file_path_for_(w.id);
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        LOG_WARN(kCatalogTag, logf("Layout file could not be written", {{"layout", w.id.to_string()},
                                                                         {"path", log_path(path)},
                                                                         {"error", f.errorString()}}));
        return Result<LayoutId>::err(("Failed to open " + path + ": " + f.errorString()).toStdString());
    }
    f.write(QJsonDocument(w.to_json()).toJson(QJsonDocument::Indented));
    // commit() also fails when the write above was short (QSaveFile keeps the error).
    if (!f.commit()) {
        LOG_WARN(kCatalogTag, logf("Layout file could not be written", {{"layout", w.id.to_string()},
                                                                         {"path", log_path(path)},
                                                                         {"error", f.errorString()}}));
        return Result<LayoutId>::err(("Failed to write " + path + ": " + f.errorString()).toStdString());
    }

    auto idx = upsert_index_row(w);
    if (idx.is_err()) {
        LOG_WARN(kCatalogTag, QString("File written but index update failed for %1: %2")
                                  .arg(w.id.to_string(), QString::fromStdString(idx.error())));
        return Result<LayoutId>::err(idx.error());
    }
    LOG_INFO(kCatalogTag, QString("Saved layout '%1' (uuid=%2)").arg(w.name, w.id.to_string()));
    emit layout_saved(w.id);
    return Result<LayoutId>::ok(w.id);
}

Result<layout::Workspace> LayoutCatalog::load_workspace(const LayoutId& id) {
    if (!opened_) {
        auto r = open();
        if (r.is_err())
            return Result<layout::Workspace>::err(r.error());
    }
    const QString path = file_path_for_(id);
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return Result<layout::Workspace>::err(("Failed to open " + path + ": " + f.errorString()).toStdString());
    const QByteArray bytes = f.readAll();
    f.close();

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &err);
    if (err.error != QJsonParseError::NoError)
        return Result<layout::Workspace>::err(("Parse error in " + path + ": " + err.errorString()).toStdString());
    if (!doc.isObject())
        return Result<layout::Workspace>::err(("Layout file is not a JSON object: " + path).toStdString());

    return Result<layout::Workspace>::ok(layout::Workspace::from_json(doc.object()));
}

Result<void> LayoutCatalog::remove_layout(const LayoutId& id) {
    if (!opened_) {
        auto r = open();
        if (r.is_err())
            return r;
    }
    QFile::remove(file_path_for_(id));
    // Best-effort thumbnail cleanup; not fatal if it's missing.
    QFile::remove(ProfilePaths::layouts_dir() + "/thumbs/" + id.to_string() + ".png");
    QSqlDatabase db = index_db();
    if (db.isOpen()) {
        QSqlQuery q(db);
        q.prepare("DELETE FROM layouts WHERE uuid = ?");
        q.addBindValue(id.to_string());
        // The layout would keep appearing in the list after its file is removed.
        if (!q.exec())
            LOG_WARN(kCatalogTag, logf("Layout index row could not be deleted",
                                       {{"layout", id.to_string()}, {"error", q.lastError().text()}}));
    }
    // If the removed layout was pinned as last_loaded, clear the pointer so
    // cold-boot doesn't try to restore a dead UUID.
    auto last = meta(QStringLiteral("last_loaded_uuid"));
    if (last.is_ok() && last.value() == id.to_string())
        set_meta(QStringLiteral("last_loaded_uuid"), QString());

    // If the removed layout is also the one this session has loaded, drop
    // the in-memory current-name/id pointer so the title bar suffix and
    // the toolbar's Save-vs-Save-As decision stop pointing at a dead UUID.
    if (layout::WorkspaceShell::current_id() == id)
        layout::WorkspaceShell::clear_current();

    emit layout_removed(id);
    return Result<void>::ok();
}

Result<QList<LayoutCatalog::Entry>> LayoutCatalog::list_layouts() {
    if (!opened_) {
        auto r = open();
        if (r.is_err())
            return Result<QList<Entry>>::err(r.error());
    }
    QSqlDatabase db = index_db();
    if (!db.isOpen())
        return Result<QList<Entry>>::err("index db not open");

    QSqlQuery q(db);
    if (!q.exec("SELECT uuid, name, kind, created_at_unix, updated_at_unix, "
                "       description, thumbnail_path "
                "FROM layouts ORDER BY updated_at_unix DESC"))
        return Result<QList<Entry>>::err(q.lastError().text().toStdString());

    QList<Entry> out;
    while (q.next()) {
        Entry e;
        e.id = LayoutId::from_string(q.value(0).toString());
        e.name = q.value(1).toString();
        e.kind = q.value(2).toString();
        e.created_at_unix = q.value(3).toLongLong();
        e.updated_at_unix = q.value(4).toLongLong();
        e.description = q.value(5).toString();
        e.thumbnail_path = q.value(6).toString();
        out.append(e);
    }
    return Result<QList<Entry>>::ok(out);
}

Result<QList<LayoutCatalog::Entry>> LayoutCatalog::recent_layouts(int limit, bool include_auto) {
    auto r = list_layouts();
    if (r.is_err())
        return r;
    QList<Entry> filtered;
    for (const auto& e : r.value()) {
        if (!include_auto && e.kind == QStringLiteral("auto"))
            continue;
        filtered.append(e);
        if (filtered.size() >= limit)
            break;
    }
    return Result<QList<Entry>>::ok(filtered);
}

LayoutId LayoutCatalog::find_by_name(const QString& name) {
    auto r = list_layouts();
    if (r.is_err()) {
        LOG_WARN(kCatalogTag, logf("Layouts could not be listed", {{"error", QString::fromStdString(r.error())}}));
        return LayoutId();
    }
    for (const auto& e : r.value()) {
        if (e.name.compare(name, Qt::CaseInsensitive) == 0)
            return e.id;
    }
    return LayoutId();
}

Result<void> LayoutCatalog::export_to(const LayoutId& id, const QString& path) {
    auto wr = load_workspace(id);
    if (wr.is_err())
        return Result<void>::err(wr.error());
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        LOG_WARN(kCatalogTag, logf("Layout export failed", {{"layout", id.to_string()},
                                                            {"path", log_path(path)},
                                                            {"error", f.errorString()}}));
        return Result<void>::err(("Export open failed: " + f.errorString()).toStdString());
    }
    f.write(QJsonDocument(wr.value().to_json()).toJson(QJsonDocument::Indented));
    if (!f.commit()) {
        LOG_WARN(kCatalogTag, logf("Layout export failed", {{"layout", id.to_string()},
                                                            {"path", log_path(path)},
                                                            {"error", f.errorString()}}));
        return Result<void>::err(("Export write failed: " + f.errorString()).toStdString());
    }
    return Result<void>::ok();
}

Result<QString> LayoutCatalog::meta(const QString& key) const {
    QSqlDatabase db = index_db();
    if (!db.isOpen())
        return Result<QString>::err("index db not open");
    QSqlQuery q(db);
    q.prepare("SELECT value FROM _meta WHERE key = ?");
    q.addBindValue(key);
    if (!q.exec())
        return Result<QString>::err(q.lastError().text().toStdString());
    if (q.next())
        return Result<QString>::ok(q.value(0).toString());
    return Result<QString>::ok(QString());
}

Result<void> LayoutCatalog::set_meta(const QString& key, const QString& value) {
    QSqlDatabase db = index_db();
    if (!db.isOpen())
        return Result<void>::err("index db not open");
    QSqlQuery q(db);
    q.prepare("INSERT INTO _meta(key, value) VALUES(?, ?) "
              "ON CONFLICT(key) DO UPDATE SET value = excluded.value");
    q.addBindValue(key);
    q.addBindValue(value);
    if (!q.exec())
        return Result<void>::err(q.lastError().text().toStdString());
    return Result<void>::ok();
}

Result<LayoutId> LayoutCatalog::last_loaded_id() const {
    auto m = meta(QStringLiteral("last_loaded_uuid"));
    if (m.is_err())
        return Result<LayoutId>::err(m.error());
    if (m.value().isEmpty())
        return Result<LayoutId>::ok(LayoutId());
    return Result<LayoutId>::ok(LayoutId::from_string(m.value()));
}

Result<void> LayoutCatalog::set_last_loaded_id(const LayoutId& id) {
    return set_meta(QStringLiteral("last_loaded_uuid"), id.is_null() ? QString() : id.to_string());
}

Result<LayoutId> LayoutCatalog::import_from(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return Result<LayoutId>::err(("Import open failed: " + f.errorString()).toStdString());
    const QByteArray bytes = f.readAll();
    f.close();

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &err);
    if (err.error != QJsonParseError::NoError)
        return Result<LayoutId>::err(("Parse error: " + err.errorString()).toStdString());
    if (!doc.isObject())
        return Result<LayoutId>::err("Import failed: file is not a layout (JSON root is not an object)");

    layout::Workspace w = layout::Workspace::from_json(doc.object());
    // Mint a fresh id so two users importing the same shared file don't
    // collide on the same UUID. Caller can override via save_workspace
    // path-based copy if they really want to preserve cross-user identity.
    w.id = LayoutId::generate();
    return save_workspace(w);
}

} // namespace fincept
