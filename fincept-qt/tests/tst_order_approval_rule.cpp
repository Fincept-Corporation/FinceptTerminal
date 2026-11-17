// Unit tests for src/trading/OrderApprovalRule.h — the approval decision UnifiedTrading
// makes at its order entry points from UnifiedOrder::source (defence in depth behind
// the callers that consult ActionCenter::should_queue themselves).
//
// Header-only and pure: it includes only trading/TradingTypes.h (Qt Core types +
// plain structs, order_source_kind()). No .cpp, no settings, no ActionCenter.
//
// The table under test (source kind x approval mode x reduce-only verified x
// came-from-approval):
//   came from an approved queue entry ................................. place
//   empty / ticket / basket / split / smart / replication / kill_switch /
//     auto_square_off / session_close ................................ place (any mode)
//   workflow_exit / algo_exit, reduce-only verified (yes) ............. place (any mode)
//   agent / workflow / algo / script / unknown kind, and
//     workflow_exit / algo_exit not verified (no / unknown):
//       Semi-Auto ..................................................... queue
//       Auto .......................................................... place
//       no account (legacy session path) .............................. refuse
// Reduce-only is read for the exit kinds only. Plus order_source_kind() parsing and
// effective_source() (an agent step overrides an empty or screen-set source, never an
// automated / system one).

#include "trading/OrderApprovalRule.h"
#include "trading/TradingTypes.h"

#include <QString>
#include <QStringList>
#include <QTest>

namespace ar = fincept::trading::approval_rule;
using fincept::trading::order_source_kind;
using Route = ar::Route;
using RO = ar::ReduceOnly;

class TestOrderApprovalRule : public QObject {
    Q_OBJECT

  private:
    static QStringList attended_sources() {
        return {QString(),
                QStringLiteral("ticket"),
                QStringLiteral("basket"),
                QStringLiteral("split"),
                QStringLiteral("smart"),
                QStringLiteral("replication"),
                QStringLiteral("kill_switch"),
                QStringLiteral("auto_square_off"),
                QStringLiteral("session_close")};
    }
    static QStringList automated_sources() {
        return {QStringLiteral("agent:claude_code"), QStringLiteral("agent:unknown"),
                QStringLiteral("workflow:wf_42"),    QStringLiteral("algo:dep-7f3a"),
                QStringLiteral("script:notebook"),   QStringLiteral("mcp:something"), // unknown kind: fail closed
                QStringLiteral("AGENT:Codex"),       QStringLiteral(" algo : x ")};
    }
    static QStringList exit_sources() {
        return {QStringLiteral("workflow_exit:wf_42"), QStringLiteral("algo_exit:dep-7f3a"),
                QStringLiteral("WORKFLOW_EXIT:wf"),    QStringLiteral(" algo_exit : x "),
                QStringLiteral("workflow_exit")};
    }
    static QList<RO> reduce_only_states() { return {RO::Yes, RO::No, RO::Unknown}; }

  private slots:
    // ── order_source_kind ────────────────────────────────────────────────────
    void kind_is_the_part_before_the_colon() {
        QCOMPARE(order_source_kind(QStringLiteral("agent:claude_code")), QStringLiteral("agent"));
        QCOMPARE(order_source_kind(QStringLiteral("workflow:wf:with:colons")), QStringLiteral("workflow"));
        QCOMPARE(order_source_kind(QStringLiteral("ticket")), QStringLiteral("ticket"));
        QCOMPARE(order_source_kind(QStringLiteral("  Algo:Dep1 ")), QStringLiteral("algo"));
        QCOMPARE(order_source_kind(QString()), QString());
        QCOMPARE(order_source_kind(QStringLiteral(":orphan")), QString());
    }

    // ── which kinds need a human ─────────────────────────────────────────────
    void attended_kinds_never_need_approval() {
        for (const QString& s : attended_sources())
            QVERIFY2(!ar::needs_human_approval(s), qPrintable(s));
    }
    void automated_and_unknown_kinds_need_approval() {
        for (const QString& s : automated_sources())
            QVERIFY2(ar::needs_human_approval(s), qPrintable(s));
    }

    // ── the full table ───────────────────────────────────────────────────────
    void came_from_approval_always_places() {
        for (const QStringList& set : {attended_sources(), automated_sources()})
            for (const QString& s : set)
                for (bool semi : {false, true})
                    for (bool known : {false, true})
                        QCOMPARE(ar::decide(s, semi, /*came_from_approval=*/true, known), Route::Place);
    }
    void attended_sources_place_in_every_mode() {
        for (const QString& s : attended_sources())
            for (bool semi : {false, true})
                for (bool known : {false, true})
                    QCOMPARE(ar::decide(s, semi, false, known), Route::Place);
    }
    void automated_sources_queue_in_semi_auto() {
        for (const QString& s : automated_sources())
            QCOMPARE(ar::decide(s, /*semi_auto=*/true, false, true), Route::Queue);
    }
    void automated_sources_place_in_auto() {
        for (const QString& s : automated_sources())
            QCOMPARE(ar::decide(s, /*semi_auto=*/false, false, true), Route::Place);
    }
    void automated_sources_refused_without_an_account() {
        for (const QString& s : automated_sources())
            for (bool semi : {false, true})
                QCOMPARE(ar::decide(s, semi, false, /*mode_known=*/false), Route::Refuse);
    }
    void empty_source_keeps_todays_behaviour() {
        // An unstated source is a person's order: never queued, never refused by this rule.
        QCOMPARE(ar::decide(QString(), true, false, true), Route::Place);
        QCOMPARE(ar::decide(QString(), true, false, false), Route::Place);
        QCOMPARE(ar::decide(QString(), false, false, true), Route::Place);
    }
    void route_names() {
        QCOMPARE(QString::fromLatin1(ar::route_name(Route::Place)), QStringLiteral("place"));
        QCOMPARE(QString::fromLatin1(ar::route_name(Route::Queue)), QStringLiteral("queue"));
        QCOMPARE(QString::fromLatin1(ar::route_name(Route::Refuse)), QStringLiteral("refuse"));
    }

    // ── exit kinds (workflow_exit / algo_exit) ───────────────────────────────
    void exit_kinds_are_recognised() {
        for (const QString& s : exit_sources())
            QVERIFY2(ar::is_exit_source(s), qPrintable(s));
        QVERIFY(ar::is_exit_kind(QStringLiteral("workflow_exit")));
        QVERIFY(ar::is_exit_kind(QStringLiteral("algo_exit")));
        for (const char* s : {"", "workflow:wf1", "algo:dep1", "agent:x", "script:nb", "ticket", "kill_switch",
                              "exit", "workflow-exit:wf1", "algoexit:d", "exit:algo"})
            QVERIFY2(!ar::is_exit_source(QString::fromLatin1(s)), s);
        QCOMPARE(order_source_kind(QStringLiteral("workflow_exit:wf:7")), QStringLiteral("workflow_exit"));
    }
    void exit_kinds_need_approval_unless_verified() {
        // Not an attended kind: without a verified reduce-only check they are automated.
        for (const QString& s : exit_sources())
            QVERIFY2(ar::needs_human_approval(s), qPrintable(s));
    }
    void exit_sources_full_table() {
        // kind x approval mode (Semi-Auto / Auto / no account) x reduce-only x came-from-approval.
        for (const QString& s : exit_sources())
            for (bool semi : {false, true})
                for (bool known : {false, true})
                    for (RO ro : reduce_only_states())
                        for (bool approved : {false, true}) {
                            Route want;
                            if (approved || ro == RO::Yes)
                                want = Route::Place;
                            else if (!known)
                                want = Route::Refuse;
                            else
                                want = semi ? Route::Queue : Route::Place;
                            const QString what = QStringLiteral("%1 semi=%2 known=%3 ro=%4 approved=%5")
                                                     .arg(s)
                                                     .arg(int(semi))
                                                     .arg(int(known))
                                                     .arg(QString::fromLatin1(ar::reduce_only_name(ro)))
                                                     .arg(int(approved));
                            QVERIFY2(ar::decide(s, semi, approved, known, ro) == want, qPrintable(what));
                        }
    }
    void verified_exit_places_without_approval() {
        for (const QString& s : exit_sources()) {
            QCOMPARE(ar::decide(s, /*semi_auto=*/true, false, true, RO::Yes), Route::Place);
            QCOMPARE(ar::decide(s, /*semi_auto=*/false, false, true, RO::Yes), Route::Place);
            QCOMPARE(ar::decide(s, true, false, /*mode_known=*/false, RO::Yes), Route::Place);
        }
    }
    void unverified_exit_is_treated_like_workflow_and_algo() {
        for (const QString& s : exit_sources())
            for (RO ro : {RO::No, RO::Unknown}) {
                QCOMPARE(ar::decide(s, true, false, true, ro),
                         ar::decide(QStringLiteral("workflow:wf1"), true, false, true));
                QCOMPARE(ar::decide(s, true, false, true, ro), Route::Queue);
                QCOMPARE(ar::decide(s, false, false, true, ro), Route::Place);
                QCOMPARE(ar::decide(s, true, false, false, ro), Route::Refuse);
                QCOMPARE(ar::decide(s, false, false, false, ro), Route::Refuse);
            }
    }
    void exit_without_a_reduce_only_answer_defaults_to_unknown() {
        // The 4-argument form (basket / split / any caller that does not check) = Unknown.
        for (const QString& s : exit_sources()) {
            QCOMPARE(ar::decide(s, true, false, true), Route::Queue);
            QCOMPARE(ar::decide(s, false, false, true), Route::Place);
            QCOMPARE(ar::decide(s, true, false, false), Route::Refuse);
        }
    }
    void reduce_only_is_ignored_for_every_other_kind() {
        // A verified reduce-only answer never lets an entry kind skip approval ...
        for (const QString& s : automated_sources())
            for (RO ro : reduce_only_states()) {
                QCOMPARE(ar::decide(s, true, false, true, ro), Route::Queue);
                QCOMPARE(ar::decide(s, false, false, true, ro), Route::Place);
                QCOMPARE(ar::decide(s, true, false, false, ro), Route::Refuse);
                QCOMPARE(ar::decide(s, true, true, false, ro), Route::Place);
            }
        // ... and a failed one never holds a person's / system order.
        for (const QString& s : attended_sources())
            for (RO ro : reduce_only_states())
                for (bool semi : {false, true})
                    for (bool known : {false, true})
                        QCOMPARE(ar::decide(s, semi, false, known, ro), Route::Place);
    }
    void reduce_only_names() {
        QCOMPARE(QString::fromLatin1(ar::reduce_only_name(RO::Yes)), QStringLiteral("yes"));
        QCOMPARE(QString::fromLatin1(ar::reduce_only_name(RO::No)), QStringLiteral("no"));
        QCOMPARE(QString::fromLatin1(ar::reduce_only_name(RO::Unknown)), QStringLiteral("unknown"));
    }
    void agent_never_overrides_an_exit_source() {
        for (const QString& s : exit_sources())
            QCOMPARE(ar::effective_source(s, QStringLiteral("agent:claude_code")), s);
    }

    // ── effective_source ─────────────────────────────────────────────────────
    void agent_overrides_empty_and_screen_sources() {
        const QString agent = QStringLiteral("agent:claude_code");
        for (const char* s : {"", "ticket", "basket", "split", "smart"})
            QCOMPARE(ar::effective_source(QString::fromLatin1(s), agent), agent);
    }
    void agent_never_downgrades_automated_or_system_sources() {
        const QString agent = QStringLiteral("agent:codex");
        for (const char* s : {"algo:dep1", "workflow:wf1", "agent:claude_code", "script:nb", "replication",
                              "kill_switch", "auto_square_off", "session_close"})
            QCOMPARE(ar::effective_source(QString::fromLatin1(s), agent), QString::fromLatin1(s));
    }
    void no_agent_keeps_the_stated_source() {
        for (const char* s : {"", "ticket", "algo:dep1"})
            QCOMPARE(ar::effective_source(QString::fromLatin1(s), QString()), QString::fromLatin1(s));
    }
    void agent_pressing_the_ticket_in_semi_auto_is_queued() {
        // The end-to-end consequence the entry points rely on.
        const QString src = ar::effective_source(QStringLiteral("ticket"), QStringLiteral("agent:claude_code"));
        QCOMPARE(ar::decide(src, /*semi_auto=*/true, false, true), Route::Queue);
        QCOMPARE(ar::decide(src, /*semi_auto=*/false, false, true), Route::Place);
        // ... and a person's ticket in the same account is not.
        QCOMPARE(ar::decide(ar::effective_source(QStringLiteral("ticket"), QString()), true, false, true),
                 Route::Place);
    }
};

QTEST_GUILESS_MAIN(TestOrderApprovalRule)
#include "tst_order_approval_rule.moc"
