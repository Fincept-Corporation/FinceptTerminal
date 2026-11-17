// tst_mcp_run_tokens.cpp — the run-scoped bearer tokens of the terminal's MCP
// server (McpHttpServer_RunTokens.h): lifecycle, expiry, policy, ceiling.
//
// Header-only unit (QtCore). The end-to-end checks against the live server —
// a NoNavigation token never sees or calls ui_open, an ended / expired token
// gets 401, the long-lived token keeps working — are in the server's own
// self-test (McpServerSelfTest.cpp, "run-scoped tokens"), which needs the app.

#include "mcp/server/McpHttpServer_RunTokens.h"

#include <QSet>
#include <QtTest>

using namespace fincept::mcp;

class TestMcpRunTokens : public QObject {
    Q_OBJECT
  private slots:
    void begin_check_end();
    void expired_token_is_rejected();
    void unknown_and_empty_are_rejected();
    void policy_travels_with_the_token();
    void navigation_is_hidden_only_for_no_navigation();
    void tokens_are_unique_and_long();
    void ceiling_evicts_the_oldest();
    void ttl_is_clamped();
};

void TestMcpRunTokens::begin_check_end() {
    McpRunTokenTable t;
    const qint64 now = 1'000'000;
    const QString tok = t.begin(RunTokenPolicy{}, 60'000, now);
    QVERIFY(tok.startsWith(QStringLiteral("run_")));
    QVERIFY(t.check(tok, now + 1).has_value());
    QCOMPARE(t.live(now + 1), 1);
    t.end(tok);
    QVERIFY(!t.check(tok, now + 2).has_value());
    QCOMPARE(t.live(now + 2), 0);
    t.end(tok); // again: no-op
    t.end(QString());
}

void TestMcpRunTokens::expired_token_is_rejected() {
    McpRunTokenTable t;
    const qint64 now = 5'000'000;
    const QString tok = t.begin(RunTokenPolicy{}, 2'000, now);
    QVERIFY(t.check(tok, now + 1'999).has_value());
    QVERIFY(!t.check(tok, now + 2'000).has_value());
    QVERIFY(!t.check(tok, now + 60'000).has_value());
    QCOMPARE(t.live(now + 2'000), 0);
    // An expired entry is swept on the next begin().
    t.begin(RunTokenPolicy{}, 2'000, now + 10'000);
    QCOMPARE(t.live(now + 10'001), 1);
}

void TestMcpRunTokens::unknown_and_empty_are_rejected() {
    McpRunTokenTable t;
    const QString tok = t.begin(RunTokenPolicy{}, 60'000, 0);
    QVERIFY(!t.check(QString(), 1).has_value());
    QVERIFY(!t.check(QStringLiteral("run_nope"), 1).has_value());
    QVERIFY(!t.check(tok.left(tok.size() - 1), 1).has_value());
    QVERIFY(!t.check(tok + QLatin1Char('x'), 1).has_value());
}

void TestMcpRunTokens::policy_travels_with_the_token() {
    McpRunTokenTable t;
    RunTokenPolicy p;
    p.scope = RunToolScope::NoNavigation;
    p.paper_only = true;
    p.label = QStringLiteral("quick_chat · claude");
    const QString tok = t.begin(p, 60'000, 0);
    const auto got = t.check(tok, 1);
    QVERIFY(got.has_value());
    QCOMPARE(got->scope, RunToolScope::NoNavigation);
    QVERIFY(got->paper_only);
    QCOMPARE(got->label, QStringLiteral("quick_chat · claude"));
    const QString all = t.begin(RunTokenPolicy{}, 60'000, 0);
    QCOMPARE(t.check(all, 1)->scope, RunToolScope::All);
    QVERIFY(!t.check(all, 1)->paper_only);
}

void TestMcpRunTokens::navigation_is_hidden_only_for_no_navigation() {
    QVERIFY(run_scope_hides_category(RunToolScope::NoNavigation, QStringLiteral("navigation")));
    QVERIFY(!run_scope_hides_category(RunToolScope::NoNavigation, QStringLiteral("ui-read")));
    QVERIFY(!run_scope_hides_category(RunToolScope::NoNavigation, QStringLiteral("ui-control")));
    QVERIFY(!run_scope_hides_category(RunToolScope::All, QStringLiteral("navigation")));
    QVERIFY(!run_scope_hides_category(RunToolScope::NoNavigation, QString()));
}

void TestMcpRunTokens::tokens_are_unique_and_long() {
    McpRunTokenTable t;
    QSet<QString> seen;
    for (int i = 0; i < 50; ++i) {
        const QString tok = t.begin(RunTokenPolicy{}, 60'000, 0);
        QVERIFY(tok.size() >= 4 + 43); // "run_" + 32 random bytes, base64url
        QVERIFY(!seen.contains(tok));
        seen.insert(tok);
    }
}

void TestMcpRunTokens::ceiling_evicts_the_oldest() {
    McpRunTokenTable t;
    QStringList toks;
    for (int i = 0; i < McpRunTokenTable::kMaxLiveTokens; ++i)
        toks << t.begin(RunTokenPolicy{}, 60'000, 100 + i);
    QCOMPARE(t.live(1'000), McpRunTokenTable::kMaxLiveTokens);
    const QString extra = t.begin(RunTokenPolicy{}, 60'000, 10'000);
    QCOMPARE(t.live(10'001), McpRunTokenTable::kMaxLiveTokens);
    QVERIFY(!t.check(toks.constFirst(), 10'001).has_value()); // the oldest went
    QVERIFY(t.check(toks.at(1), 10'001).has_value());
    QVERIFY(t.check(extra, 10'001).has_value());
}

void TestMcpRunTokens::ttl_is_clamped() {
    McpRunTokenTable t;
    const QString zero = t.begin(RunTokenPolicy{}, 0, 0);
    QVERIFY(t.check(zero, McpRunTokenTable::kMinTtlMs - 1).has_value());
    QVERIFY(!t.check(zero, McpRunTokenTable::kMinTtlMs).has_value());
    const QString huge = t.begin(RunTokenPolicy{}, 1LL << 50, 0);
    QVERIFY(t.check(huge, McpRunTokenTable::kMaxTtlMs - 1).has_value());
    QVERIFY(!t.check(huge, McpRunTokenTable::kMaxTtlMs).has_value());
}

QTEST_GUILESS_MAIN(TestMcpRunTokens)
#include "tst_mcp_run_tokens.moc"
