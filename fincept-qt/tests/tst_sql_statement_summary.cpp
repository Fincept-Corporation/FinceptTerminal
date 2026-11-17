// db::statement_summary (src/storage/sqlite/SqliteSupport.cpp): the statement
// name Database writes into a failure / slow-query log line. It must name the
// keyword and table and must never carry a literal or a bound value.
#include "storage/sqlite/SqliteSupport.h"

#include <QtTest>

using fincept::db::statement_summary;

class TestSqlStatementSummary : public QObject {
    Q_OBJECT
  private slots:
    void keyword_and_table_data() {
        QTest::addColumn<QString>("sql");
        QTest::addColumn<QString>("expected");
        QTest::newRow("insert") << "INSERT INTO settings (key, value) VALUES (?, ?)" << "INSERT settings";
        QTest::newRow("insert-or") << "insert or replace into  \"watchlist_items\"(a) values (1)"
                                   << "INSERT watchlist_items";
        QTest::newRow("replace") << "REPLACE INTO kv VALUES ('secret', 'x')" << "REPLACE kv";
        QTest::newRow("update") << "UPDATE portfolios SET name = 'Ann' WHERE id = ?" << "UPDATE portfolios";
        QTest::newRow("update-or") << "UPDATE OR IGNORE portfolios SET x = 1" << "UPDATE portfolios";
        QTest::newRow("delete") << "DELETE FROM notes WHERE title = 'private'" << "DELETE notes";
        QTest::newRow("select") << "SELECT a, b FROM orders o JOIN fills f ON f.o = o.id WHERE a = 'x'"
                                << "SELECT orders";
        QTest::newRow("select-subquery") << "SELECT * FROM (SELECT 1)" << "SELECT";
        QTest::newRow("select-from-in-string") << "SELECT 'from secret' AS x FROM real_table" << "SELECT real_table";
        QTest::newRow("with") << "WITH t AS (SELECT 1) SELECT * FROM t" << "WITH t";
        QTest::newRow("create") << "CREATE TABLE IF NOT EXISTS screen_state (id TEXT)" << "CREATE screen_state";
        QTest::newRow("create-index") << "CREATE INDEX IF NOT EXISTS idx_a ON t(a)" << "CREATE idx_a";
        QTest::newRow("drop") << "DROP TABLE old_rows" << "DROP old_rows";
        QTest::newRow("alter") << "ALTER TABLE t ADD COLUMN c TEXT" << "ALTER t";
        QTest::newRow("pragma") << "PRAGMA journal_mode = WAL" << "PRAGMA journal_mode";
        QTest::newRow("commit") << "COMMIT" << "COMMIT";
        QTest::newRow("comment") << "-- note\n /* c */ SELECT x FROM y" << "SELECT y";
        QTest::newRow("empty") << "   " << "?";
        QTest::newRow("qualified") << "SELECT * FROM main.settings" << "SELECT main.settings";
    }
    void keyword_and_table() {
        QFETCH(QString, sql);
        QFETCH(QString, expected);
        QCOMPARE(statement_summary(sql), expected);
    }

    void never_contains_values() {
        const QString s = statement_summary(QStringLiteral("INSERT INTO t VALUES ('hunter2', 42)"));
        QVERIFY(!s.contains(QStringLiteral("hunter2")));
        QVERIFY(!s.contains(QStringLiteral("42")));
        QVERIFY(statement_summary(QString(500, QLatin1Char('a'))).size() <= 64);
    }
};

QTEST_GUILESS_MAIN(TestSqlStatementSummary)
#include "tst_sql_statement_summary.moc"
