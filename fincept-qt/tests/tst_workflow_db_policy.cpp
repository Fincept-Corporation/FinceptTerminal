// Unit tests for src/services/workflow/adapters/WorkflowDbPolicy.h (and the SQL
// console guard it applies, src/screens/settings/StorageSqlConsoleGuard.h) —
// header-only, zero app sources.
//
// The workflow "Database" node used to accept any statement whose first word was
// SELECT / WITH / PRAGMA / EXPLAIN and ran it on the application's own read-write
// connection: "WITH x AS (SELECT 1) DELETE FROM settings" deleted, "PRAGMA
// foreign_keys=OFF" changed the connection every repository on that thread uses,
// and secure_credentials came back unmasked. These are the decisions taken before
// any connection is opened (the read-only connection behind them is the second
// line of defence).

#include "services/workflow/adapters/WorkflowDbPolicy.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

namespace dbn = fincept::workflow::db_node;
using Source = dbn::Source;

namespace {

bool allowed(const QString& sql, Source source = Source::AppDatabase, const QString& op = QStringLiteral("select")) {
    return dbn::refusal(sql, op, source).isEmpty();
}

} // namespace

class TstWorkflowDbPolicy : public QObject {
    Q_OBJECT

  private slots:
    void plain_reads_are_allowed() {
        QVERIFY(allowed("SELECT symbol, price FROM watchlist_items"));
        QVERIFY(allowed("  select * from portfolios where id = 'x'  "));
        QVERIFY(allowed("SELECT 1;")); // a trailing semicolon is harmless
        QVERIFY(allowed("VALUES (1), (2)"));
        QVERIFY(allowed("WITH recent AS (SELECT * FROM news_articles LIMIT 5) SELECT title FROM recent"));
        // replace() as a function is not REPLACE INTO
        QVERIFY(allowed("WITH x AS (SELECT replace(name, 'a', 'b') AS n FROM portfolios) SELECT n FROM x"));
        QVERIFY(allowed("SELECT count(*) FROM llm_profiles"));
        QVERIFY(allowed("SELECT name, provider FROM llm_profiles"));
        QVERIFY(allowed("SELECT * FROM pragma_table_info('settings')"));
    }

    void cte_fronting_a_write_is_refused() {
        QVERIFY(!allowed("WITH x AS (SELECT 1) DELETE FROM settings"));
        QVERIFY(!allowed("with x as (select 1) delete from settings where 1"));
        QVERIFY(!allowed("WITH x AS (SELECT 1) INSERT INTO settings(key, value) VALUES ('a', 'b')"));
        QVERIFY(!allowed("WITH x AS (SELECT 1) UPDATE settings SET value = '' "));
        QVERIFY(!allowed("WITH x AS (SELECT 1) REPLACE INTO settings(key, value) VALUES ('a', 'b')"));
        QVERIFY(!allowed("WITH x AS (SELECT 1) DELETE FROM settings", Source::CustomFile));
        QVERIFY(dbn::refusal("WITH x AS (SELECT 1) DELETE FROM settings", "select", Source::AppDatabase)
                    .contains("read-only"));
    }

    void state_changing_pragmas_are_refused() {
        const char* setters[] = {
            "PRAGMA foreign_keys=OFF",          "PRAGMA foreign_keys = 0",        "PRAGMA journal_mode=DELETE",
            "PRAGMA journal_mode(DELETE)",      "PRAGMA main.journal_mode = WAL", "PRAGMA writable_schema=1",
            "PRAGMA writable_schema = ON",      "PRAGMA query_only=0",            "PRAGMA user_version = 5",
            "PRAGMA wal_checkpoint(TRUNCATE)",  "PRAGMA optimize",                "PRAGMA synchronous=OFF",
            "PRAGMA locking_mode = EXCLUSIVE",  "PRAGMA cache_size = 1",          "PRAGMA secure_delete=1",
            "pragma   FOREIGN_KEYS = off",      "PRAGMA \"foreign_keys\" = 0",    "PRAGMA incremental_vacuum",
        };
        for (const char* sql : setters) {
            QVERIFY2(!allowed(sql), sql);
            QVERIFY2(!allowed(sql, Source::CustomFile), sql);
        }
    }

    void schema_pragmas_are_allowed() {
        QVERIFY(allowed("PRAGMA table_info(settings)"));
        QVERIFY(allowed("PRAGMA main.table_info(settings)"));
        QVERIFY(allowed("PRAGMA index_list('instruments')"));
        QVERIFY(allowed("PRAGMA user_version"));
        QVERIFY(allowed("PRAGMA journal_mode")); // reading the mode is fine
        QVERIFY(allowed("PRAGMA quick_check"));
    }

    void other_writes_and_side_effects_are_refused() {
        const char* writes[] = {
            "DELETE FROM settings",
            "UPDATE settings SET value = ''",
            "INSERT INTO settings(key, value) VALUES ('a', 'b')",
            "REPLACE INTO settings(key, value) VALUES ('a', 'b')",
            "DROP TABLE settings",
            "CREATE TABLE t(x)",
            "ALTER TABLE settings ADD COLUMN x",
            "ATTACH DATABASE '/tmp/x.db' AS other",
            "DETACH other",
            "VACUUM",
            "VACUUM INTO '/tmp/copy.db'",
            "REINDEX",
            "ANALYZE",
            "BEGIN",
            "/* leading comment */ DELETE FROM settings",
            "-- comment\nDELETE FROM settings",
        };
        for (const char* sql : writes) {
            QVERIFY2(!allowed(sql), sql);
            QVERIFY2(!allowed(sql, Source::CustomFile), sql);
        }
    }

    void one_statement_only() {
        QVERIFY(!allowed("SELECT 1; DELETE FROM settings"));
        QVERIFY(!allowed("SELECT 1; SELECT 2"));
        QVERIFY(allowed("SELECT ';' AS semicolon")); // inside a string it is data
        QVERIFY(allowed("SELECT 1 /* ; DELETE FROM settings */"));
        QVERIFY(!allowed("SELECT 'unterminated"));
        QVERIFY(!allowed("   "));
    }

    void credential_tables_on_the_app_database() {
        QVERIFY(!allowed("SELECT * FROM secure_credentials"));
        QVERIFY(!allowed("SELECT key FROM credentials"));
        QVERIFY(!allowed("SELECT * FROM ws_provider_configs"));
        QVERIFY(!allowed("SELECT * FROM 'secure_credentials'"));  // string literal as a table name
        QVERIFY(!allowed("SELECT * FROM \"SECURE_CREDENTIALS\"")); // quoted identifier, any case
        QVERIFY(!allowed("SELECT s.* FROM settings s JOIN secure_credentials c ON 1"));
        // A user's own file may well have a table with that name.
        QVERIFY(allowed("SELECT * FROM credentials", Source::CustomFile));
    }

    void secret_columns_on_the_app_database() {
        QVERIFY(!allowed("SELECT * FROM llm_profiles"));
        QVERIFY(!allowed("SELECT api_key FROM llm_profiles"));
        QVERIFY(!allowed("SELECT name, env FROM mcp_servers"));
        QVERIFY(allowed("SELECT * FROM llm_profiles", Source::CustomFile));
    }

    void blocked_functions() {
        QVERIFY(!allowed("SELECT load_extension('/tmp/evil.so')"));
        QVERIFY(!allowed("SELECT writefile('/tmp/x', 'y')"));
        QVERIFY(!allowed("SELECT * FROM sqlite_dbpage"));
        QVERIFY(!allowed("SELECT readfile('/etc/passwd')", Source::CustomFile));
    }

    void write_operations() {
        // Never against the application's database, whatever the statement.
        QVERIFY(!allowed("DELETE FROM settings", Source::AppDatabase, "delete"));
        QVERIFY(!allowed("SELECT 1", Source::AppDatabase, "raw"));
        QVERIFY(!allowed("INSERT INTO t VALUES (1)", Source::AppDatabase, "insert"));
        QVERIFY(!allowed("UPDATE t SET x = 1", Source::AppDatabase, "update"));
        // A custom file is the user's own.
        QVERIFY(allowed("DELETE FROM t", Source::CustomFile, "delete"));
        QVERIFY(allowed("CREATE TABLE t(x)", Source::CustomFile, "raw"));
        // An unknown operation is the read-only select, as before.
        QVERIFY(!allowed("DELETE FROM t", Source::CustomFile, "frobnicate"));
        QVERIFY(dbn::is_write_operation("raw"));
        QVERIFY(!dbn::is_write_operation("select"));
        QVERIFY(!dbn::is_write_operation("SELECT"));
    }

    void same_file_detection() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString a = dir.filePath("fincept.db");
        const QString b = dir.filePath("other.db");
        for (const QString& p : {a, b}) {
            QFile f(p);
            QVERIFY(f.open(QIODevice::WriteOnly));
        }
        QVERIFY(dbn::same_file(a, a));
        QVERIFY(dbn::same_file(a, dir.path() + "/./fincept.db"));
        QVERIFY(QDir(dir.path()).mkdir("sub"));
        QVERIFY(dbn::same_file(a, dir.path() + "/sub/../fincept.db"));
        QVERIFY(!dbn::same_file(a, b));
        QVERIFY(!dbn::same_file(a, dir.filePath("missing.db")));
        QVERIFY(!dbn::same_file(a, QString()));
    }
};

QTEST_GUILESS_MAIN(TstWorkflowDbPolicy)
#include "tst_workflow_db_policy.moc"
