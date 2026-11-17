// tst_harness_runs_repo — migration v061 (src/storage/sqlite/migrations/
// v061_harness_runs.cpp) and CodingAgentRepository on top of it:
//
//   * a database written before v061 (v052 tables, rows with only the old
//     columns) is migrated by the real Database::open() path, and its rows
//     still load — the permission derived from the old `mode` column, the new
//     columns at their defaults;
//   * a run / schedule whose harness id has no adapter in this build keeps that
//     id verbatim through load and save (never relabelled to "claude");
//   * the new fields round-trip;
//   * v061 is idempotent (applied twice on an in-memory database, and again on
//     the migrated file) and skips a table that does not exist;
//   * MigrationRunner::inspect() calls the migrated file up to date.
//
// Temp-file database for the Database::open() path (it backs the file up before
// migrating, which an in-memory database cannot do); in-memory databases for the
// direct idempotency checks.

#include "services/coding_agents/CodingAgentRepository.h"
#include "services/coding_agents/CodingAgentTypes.h"
#include "storage/sqlite/Database.h"
#include "storage/sqlite/migrations/MigrationRunner.h"

#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>

using namespace fincept;
using namespace fincept::services::coding_agents;

namespace {

const Migration* thr_migration(int version) {
    for (const Migration& m : MigrationRunner::all_migrations()) {
        if (m.version == version)
            return &m;
    }
    return nullptr;
}

QStringList thr_columns(QSqlDatabase& db, const QString& table) {
    QStringList out;
    QSqlQuery q(db);
    q.exec(QStringLiteral("PRAGMA table_info(%1)").arg(table));
    while (q.next())
        out << q.value(1).toString();
    return out;
}

bool thr_exec(QSqlDatabase& db, const QString& sql) {
    QSqlQuery q(db);
    if (!q.exec(sql)) {
        qWarning("SQL failed: %s — %s", qPrintable(sql), qPrintable(q.lastError().text()));
        return false;
    }
    return true;
}

const CodingAgentRun* thr_find(const QVector<CodingAgentRun>& runs, const QString& id) {
    for (const CodingAgentRun& r : runs) {
        if (r.id == id)
            return &r;
    }
    return nullptr;
}

const CodingAgentSchedule* thr_find_schedule(const QVector<CodingAgentSchedule>& list, const QString& id) {
    for (const CodingAgentSchedule& s : list) {
        if (s.id == id)
            return &s;
    }
    return nullptr;
}

// A row exactly as a v052-era build wrote it (only the old columns).
QString thr_legacy_run_sql(const QString& id, const QString& agent, const QString& mode, qint64 created) {
    return QStringLiteral(
               "INSERT INTO coding_agent_runs (id, title, agent, prompt, model, mode, workdir, workdir_auto, "
               "use_fincept_tools, max_budget_usd, status, created_at, started_at, finished_at, session_id, exit_code, "
               "cost_usd, input_tokens, output_tokens, final_text, error, parent_run_id, schedule_id, origin) VALUES "
               "('%1', 'Legacy %2', '%2', 'Summarise AAPL', 'sonnet', '%3', '/work/run', 1, 1, 2.5, 'succeeded', %4, "
               "%4, %5, 'sess-%1', 0, 0.0123, 1200, 340, 'done', '', NULL, NULL, 'user')")
        .arg(id, agent, mode)
        .arg(created)
        .arg(created + 60000);
}

} // namespace

class TstHarnessRunsRepo : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase();
    void legacy_rows_load();
    void unknown_harness_preserved();
    void new_fields_round_trip();
    void schedules_old_and_new();
    void migration_idempotent_in_memory();
    void migration_reapplied_on_file();
    void inspect_up_to_date();
    void cleanupTestCase();

  private:
    QTemporaryDir dir_;
};

void TstHarnessRunsRepo::initTestCase() {
    QCoreApplication::setOrganizationName(QStringLiteral("FinceptTests"));
    QCoreApplication::setApplicationName(QStringLiteral("tst_harness_runs_repo"));
    QVERIFY(dir_.isValid());
    const QString path = dir_.filePath(QStringLiteral("fincept.db"));
    auto& database = Database::instance();

    // 1. A database as a build without v061 left it: v052's tables, old rows.
    register_migration_v052();
    {
        const auto opened = database.open(path);
        QVERIFY2(opened.is_ok(), opened.is_ok() ? "" : opened.error().c_str());
        QSqlDatabase& db = database.raw_db();
        QVERIFY(!thr_columns(db, QStringLiteral("coding_agent_runs")).contains(QStringLiteral("effort")));
        QVERIFY(thr_exec(db, thr_legacy_run_sql(QStringLiteral("run-claude-ro"), QStringLiteral("claude"),
                                                QStringLiteral("analysis"), 1000)));
        QVERIFY(thr_exec(db, thr_legacy_run_sql(QStringLiteral("run-codex-rw"), QStringLiteral("codex"),
                                                QStringLiteral("workspace_edits"), 2000)));
        // A harness this build has no adapter for (another edition / a later build).
        QVERIFY(thr_exec(db, thr_legacy_run_sql(QStringLiteral("run-unknown"), QStringLiteral("gemini-x"),
                                                QStringLiteral("analysis"), 3000)));
        QVERIFY(thr_exec(db, QStringLiteral(
                                 "INSERT INTO coding_agent_schedules (id, name, agent, prompt, model, mode, "
                                 "use_fincept_tools, max_budget_usd, kind, interval_minutes, daily_time, weekdays_mask, "
                                 "enabled, last_run_at, next_run_at, created_at, updated_at) VALUES ('sched-legacy', "
                                 "'Morning brief', 'codex', 'Brief me', '', 'workspace_edits', 1, NULL, 'daily', 60, "
                                 "'08:00', 31, 1, NULL, NULL, 100, 100)")));
        database.close();
    }

    // 2. This build: v061 registered, the same file opened again — the real
    //    open → inspect → back up → migrate path.
    register_migration_v061();
    QCOMPARE(MigrationRunner::highest_registered_version(), 61);
    const auto reopened = database.open(path);
    QVERIFY2(reopened.is_ok(), reopened.is_ok() ? "" : reopened.error().c_str());
    QSqlDatabase& db = database.raw_db();
    const QStringList run_cols = thr_columns(db, QStringLiteral("coding_agent_runs"));
    for (const char* c : {"effort", "permission_mode", "harness_version", "time_limit_s", "max_turns", "cached_tokens"})
        QVERIFY2(run_cols.contains(QString::fromLatin1(c)), c);
    const QStringList sched_cols = thr_columns(db, QStringLiteral("coding_agent_schedules"));
    for (const char* c : {"effort", "permission_mode", "time_limit_s", "max_turns", "workdir"})
        QVERIFY2(sched_cols.contains(QString::fromLatin1(c)), c);
}

void TstHarnessRunsRepo::legacy_rows_load() {
    const auto loaded = CodingAgentRepository::instance().load_runs(100);
    QVERIFY(loaded.is_ok());
    const QVector<CodingAgentRun> runs = loaded.value();

    const CodingAgentRun* ro = thr_find(runs, QStringLiteral("run-claude-ro"));
    QVERIFY(ro);
    QCOMPARE(ro->harness, QStringLiteral("claude"));
    QCOMPARE(ro->permission, PermissionMode::ReadOnly); // empty permission_mode → old mode "analysis"
    QCOMPARE(ro->model, QStringLiteral("sonnet"));
    QVERIFY(ro->effort.isEmpty());
    QVERIFY(ro->harness_version.isEmpty());
    QCOMPARE(ro->time_limit_s, 0);
    QCOMPARE(ro->max_turns, 0);
    QCOMPARE(ro->cached_tokens, qint64(0));
    QCOMPARE(ro->input_tokens, qint64(1200));
    QCOMPARE(ro->status, RunStatus::Succeeded);
    QVERIFY(ro->cost_usd.has_value());
    QCOMPARE(ro->session_id, QStringLiteral("sess-run-claude-ro"));

    const CodingAgentRun* rw = thr_find(runs, QStringLiteral("run-codex-rw"));
    QVERIFY(rw);
    QCOMPARE(rw->harness, QStringLiteral("codex"));
    QCOMPARE(rw->permission, PermissionMode::WorkspaceWrite); // old mode "workspace_edits"
}

void TstHarnessRunsRepo::unknown_harness_preserved() {
    auto& repo = CodingAgentRepository::instance();
    auto loaded = repo.load_runs(100);
    QVERIFY(loaded.is_ok());
    const CodingAgentRun* unknown = thr_find(loaded.value(), QStringLiteral("run-unknown"));
    QVERIFY(unknown);
    QCOMPARE(unknown->harness, QStringLiteral("gemini-x")); // verbatim — never "claude"

    // Saving the row back (e.g. "interrupted" at start-up) keeps the id too.
    CodingAgentRun copy = *unknown;
    copy.error = QStringLiteral("touched");
    QVERIFY(repo.upsert_run(copy).is_ok());
    loaded = repo.load_runs(100);
    const CodingAgentRun* again = thr_find(loaded.value(), QStringLiteral("run-unknown"));
    QVERIFY(again);
    QCOMPARE(again->harness, QStringLiteral("gemini-x"));
    QCOMPARE(again->error, QStringLiteral("touched"));
    QCOMPARE(again->permission, PermissionMode::ReadOnly);
}

void TstHarnessRunsRepo::new_fields_round_trip() {
    auto& repo = CodingAgentRepository::instance();
    CodingAgentRun r;
    r.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    r.title = QStringLiteral("Grok run");
    r.harness = QStringLiteral("grok");
    r.prompt = QStringLiteral("Earnings preview");
    r.model = QStringLiteral("grok-4.6");
    r.effort = QStringLiteral("high");
    r.permission = PermissionMode::FullAccess;
    r.workdir = QStringLiteral("/work/x");
    r.time_limit_s = 1800;
    r.max_turns = 25;
    r.harness_version = QStringLiteral("1.0.41");
    r.input_tokens = 5000;
    r.output_tokens = 700;
    r.cached_tokens = 4200;
    r.status = RunStatus::Failed;
    r.created_at_ms = 9000;
    r.failure = QStringLiteral("auth"); // not persisted
    QVERIFY(repo.upsert_run(r).is_ok());

    const auto loaded = repo.load_runs(100);
    QVERIFY(loaded.is_ok());
    const CodingAgentRun* got = thr_find(loaded.value(), r.id);
    QVERIFY(got);
    QCOMPARE(got->harness, QStringLiteral("grok"));
    QCOMPARE(got->effort, QStringLiteral("high"));
    QCOMPARE(got->permission, PermissionMode::FullAccess);
    QCOMPARE(got->time_limit_s, 1800);
    QCOMPARE(got->max_turns, 25);
    QCOMPARE(got->harness_version, QStringLiteral("1.0.41"));
    QCOMPARE(got->cached_tokens, qint64(4200));
    QVERIFY(got->failure.isEmpty());

    // The legacy column stays meaningful for an older build reading this file.
    QSqlQuery q(Database::instance().raw_db());
    QVERIFY(q.exec(QStringLiteral("SELECT mode, permission_mode FROM coding_agent_runs WHERE id = '%1'").arg(r.id)));
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toString(), QStringLiteral("workspace_edits"));
    QCOMPARE(q.value(1).toString(), QStringLiteral("full_access"));
}

void TstHarnessRunsRepo::schedules_old_and_new() {
    auto& repo = CodingAgentRepository::instance();
    auto loaded = repo.load_schedules();
    QVERIFY(loaded.is_ok());
    const CodingAgentSchedule* legacy = thr_find_schedule(loaded.value(), QStringLiteral("sched-legacy"));
    QVERIFY(legacy);
    QCOMPARE(legacy->harness, QStringLiteral("codex"));
    QCOMPARE(legacy->permission, PermissionMode::WorkspaceWrite);
    QVERIFY(legacy->workdir.isEmpty());
    QVERIFY(legacy->effort.isEmpty());
    QCOMPARE(legacy->time_limit_s, 0);

    CodingAgentSchedule s;
    s.id = QStringLiteral("sched-new");
    s.name = QStringLiteral("Close recap");
    s.harness = QStringLiteral("some-future-harness");
    s.prompt = QStringLiteral("Recap the close");
    s.effort = QStringLiteral("low");
    s.permission = PermissionMode::ReadOnly;
    s.time_limit_s = 600;
    s.max_turns = 5;
    s.workdir = QStringLiteral("/data/briefs");
    s.created_at_ms = 200;
    s.updated_at_ms = 200;
    QVERIFY(repo.upsert_schedule(s).is_ok());
    loaded = repo.load_schedules();
    QVERIFY(loaded.is_ok());
    const CodingAgentSchedule* got = thr_find_schedule(loaded.value(), QStringLiteral("sched-new"));
    QVERIFY(got);
    QCOMPARE(got->harness, QStringLiteral("some-future-harness"));
    QCOMPARE(got->effort, QStringLiteral("low"));
    QCOMPARE(got->time_limit_s, 600);
    QCOMPARE(got->max_turns, 5);
    QCOMPARE(got->workdir, QStringLiteral("/data/briefs"));
    QCOMPARE(got->permission, PermissionMode::ReadOnly);
}

void TstHarnessRunsRepo::migration_idempotent_in_memory() {
    const Migration* v052 = thr_migration(52);
    const Migration* v061 = thr_migration(61);
    QVERIFY(v052 && v061);
    const QString name = QStringLiteral("thr_mem_%1").arg(QUuid::createUuid().toString(QUuid::Id128));
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
        db.setDatabaseName(QStringLiteral(":memory:"));
        QVERIFY(db.open());
        // Without the tables at all: nothing to extend, no error.
        QVERIFY(v061->apply(db).is_ok());
        QVERIFY(v052->apply(db).is_ok());
        QVERIFY(thr_exec(db, QStringLiteral("CREATE TABLE llm_profiles (id TEXT PRIMARY KEY, name TEXT)")));
        QVERIFY(thr_exec(db, QStringLiteral("INSERT INTO llm_profiles (id, name) VALUES ('p1', 'x')")));
        QVERIFY(v061->apply(db).is_ok());
        const QStringList once = thr_columns(db, QStringLiteral("coding_agent_runs"));
        QVERIFY(v061->apply(db).is_ok()); // second run: guarded ADD COLUMNs do nothing
        QCOMPARE(thr_columns(db, QStringLiteral("coding_agent_runs")), once);
        QVERIFY(thr_columns(db, QStringLiteral("llm_profiles")).contains(QStringLiteral("reasoning_effort")));
        QSqlQuery q(db);
        QVERIFY(q.exec(QStringLiteral("SELECT reasoning_effort FROM llm_profiles WHERE id = 'p1'")));
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toString(), QString()); // existing row: the constant default ''
        q.finish();
        db.close();
    }
    QSqlDatabase::removeDatabase(name);
}

void TstHarnessRunsRepo::migration_reapplied_on_file() {
    const Migration* v061 = thr_migration(61);
    QVERIFY(v061);
    QSqlDatabase& db = Database::instance().raw_db();
    const QStringList before = thr_columns(db, QStringLiteral("coding_agent_schedules"));
    QVERIFY(v061->apply(db).is_ok());
    QCOMPARE(thr_columns(db, QStringLiteral("coding_agent_schedules")), before);
}

void TstHarnessRunsRepo::inspect_up_to_date() {
    QSqlDatabase& db = Database::instance().raw_db();
    MigrationRunner runner(db);
    const auto inspected = runner.inspect();
    QVERIFY2(inspected.is_ok(), inspected.is_ok() ? "" : inspected.error().c_str());
    QCOMPARE(inspected.value().current, 61);
    QCOMPARE(inspected.value().target, 61);
    QCOMPARE(static_cast<int>(inspected.value().state), static_cast<int>(SchemaState::UpToDate));
    QVERIFY2(inspected.value().missing.isEmpty(), qPrintable(inspected.value().missing.join(QStringLiteral(", "))));
}

void TstHarnessRunsRepo::cleanupTestCase() {
    Database::instance().close();
}

QTEST_GUILESS_MAIN(TstHarnessRunsRepo)
#include "tst_harness_runs_repo.moc"
