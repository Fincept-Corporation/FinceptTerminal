// src/app/DockInstanceGuard.h — header-only, zero app sources. The decisions
// that keep one screen per dock id: build / reuse / join, navigate / join /
// defer, which duplicate survives, and the nav.switch_screen payload rule.
// Also the saved-layout side (DockStateFilter.h): a repeated widget name keeps
// its first entry only.
#include "app/DockInstanceGuard.h"
#include "app/DockStateFilter.h"

#include <QSet>
#include <QtTest>

#include <functional>

using namespace fincept::dock_instance;

namespace {

// A layout written by an older build with one widget named twice.
const char* const kRepeatedState =
    "<?xml version=\"1.0\"?><QtAdvancedDockingSystem Version=\"3\"><Container Floating=\"0\">"
    "<Splitter Orientation=\"-\" Count=\"2\">"
    "<Area Tabs=\"1\" Current=\"equity_trading\"><Widget Name=\"equity_trading\" Closed=\"0\"/></Area>"
    "<Area Tabs=\"2\" Current=\"news\"><Widget Name=\"news\" Closed=\"0\"/>"
    "<Widget Name=\"equity_trading\" Closed=\"0\"/></Area>"
    "<Sizes>400 400 </Sizes></Splitter></Container></QtAdvancedDockingSystem>";
const char* const kKeptArea = "<Area Tabs=\"1\" Current=\"equity_trading\">";

/// The router's build path reduced to its bookkeeping: a factory that re-enters
/// the build of the same id while it runs (what a re-show of the dock widget
/// during a screen's first show did) must not run a second time.
struct FakeRouter {
    QSet<QString> registered;
    QSet<QString> building;
    QHash<QString, int> factory_calls;
    std::function<void(const QString&)> during_factory; // re-entry hook

    bool materialise(const QString& id) {
        switch (materialise_decision(registered.contains(id), building.contains(id))) {
            case Build::Existing:
                return true;
            case Build::Join:
                return false;
            case Build::Create:
                break;
        }
        building.insert(id);
        ++factory_calls[id];
        if (during_factory)
            during_factory(id);
        registered.insert(id);
        building.remove(id);
        return true;
    }
};

} // namespace

class TestDockInstanceGuard : public QObject {
    Q_OBJECT
  private slots:
    void base_type_strips_the_dup_suffix() {
        QCOMPARE(base_type(QStringLiteral("watchlist#dup3")), QStringLiteral("watchlist"));
        QCOMPARE(base_type(QStringLiteral("equity_trading")), QStringLiteral("equity_trading"));
        QCOMPARE(base_type(QString()), QString());
    }

    void single_instance_is_the_default() {
        for (const char* id : {"equity_trading", "crypto_trading", "fno", "algo_trading", "settings", "profile",
                               "notes", "report_builder", "dashboard", "ai_chat", "file_manager", "about",
                               "equity_trading#dup2", "a_screen_added_later"}) {
            QVERIFY2(is_single_instance_type(QString::fromLatin1(id)), id);
            QVERIFY2(!is_multi_instance_type(QString::fromLatin1(id)), id);
        }
        for (const char* id : {"watchlist", "watchlist#dup2", "markets", "news", "equity_research", "screener",
                               "derivatives", "code_editor", "backtesting"}) {
            QVERIFY2(is_multi_instance_type(QString::fromLatin1(id)), id);
            QVERIFY2(!is_single_instance_type(QString::fromLatin1(id)), id);
        }
        QVERIFY(!is_single_instance_type(QString())); // no id, no rule
        // The layout filter uses the same rule.
        QVERIFY(fincept::dock_state::is_single_instance_screen(QStringLiteral("settings")));
        QVERIFY(fincept::dock_state::is_single_instance_duplicate(QStringLiteral("notes#dup2")));
        QVERIFY(!fincept::dock_state::is_single_instance_duplicate(QStringLiteral("news#dup2")));
    }

    void order_ticket_screens() {
        QVERIFY(holds_order_ticket(QStringLiteral("equity_trading")));
        QVERIFY(holds_order_ticket(QStringLiteral("crypto_trading#dup2")));
        QVERIFY(holds_order_ticket(QStringLiteral("fno")));
        QVERIFY(!holds_order_ticket(QStringLiteral("settings")));
        for (const QString& t : multi_instance_types())
            QVERIFY2(!holds_order_ticket(t), qPrintable(t)); // never duplicable
    }

    void build_decision() {
        QCOMPARE(materialise_decision(false, false), Build::Create);
        QCOMPARE(materialise_decision(false, true), Build::Join);
        QCOMPARE(materialise_decision(true, false), Build::Existing);
        QCOMPARE(materialise_decision(true, true), Build::Existing);
    }

    void navigate_decision_joins_and_defers() {
        QCOMPARE(navigate_decision(false, false), Nav::Proceed);
        QCOMPARE(navigate_decision(true, false), Nav::Join);
        QCOMPARE(navigate_decision(true, true), Nav::Join);   // navigate in flight builds it
        QCOMPARE(navigate_decision(false, true), Nav::Defer); // hidden build in flight
    }

    void reentrant_build_runs_the_factory_once() {
        FakeRouter r;
        int nested = 0;
        // The first show of the screen re-shows its panel twice (window re-created,
        // then a queued navigate) — both ask for the same id again.
        r.during_factory = [&](const QString& id) {
            for (int i = 0; i < 2; ++i) {
                ++nested;
                QVERIFY(!r.materialise(id)); // joined, built nothing
            }
        };
        QVERIFY(r.materialise(QStringLiteral("equity_trading")));
        QCOMPARE(nested, 2);
        QCOMPARE(r.factory_calls.value(QStringLiteral("equity_trading")), 1);
        // Afterwards every request reuses it.
        QVERIFY(r.materialise(QStringLiteral("equity_trading")));
        QCOMPARE(r.factory_calls.value(QStringLiteral("equity_trading")), 1);
        // Another id built from inside the first build is its own build.
        r.during_factory = [&](const QString& id) {
            if (id == QLatin1String("markets"))
                QVERIFY(r.materialise(QStringLiteral("news")));
        };
        QVERIFY(r.materialise(QStringLiteral("markets")));
        QCOMPARE(r.factory_calls.value(QStringLiteral("markets")), 1);
        QCOMPARE(r.factory_calls.value(QStringLiteral("news")), 1);
    }

    void keeper_prefers_registered_then_plain_then_open() {
        QCOMPARE(keeper({}), -1);
        QCOMPARE(keeper({{QStringLiteral("x"), false, true}, {QStringLiteral("x"), true, false}}), 1);
        QCOMPARE(keeper({{QStringLiteral("fno#dup2"), false, true}, {QStringLiteral("fno"), false, false}}), 1);
        QCOMPARE(keeper({{QStringLiteral("fno#dup2"), false, false}, {QStringLiteral("fno#dup3"), false, true}}), 1);
        QCOMPARE(keeper({{QStringLiteral("a"), false, false}, {QStringLiteral("a"), false, false}}), 0);
    }

    void surplus_panels_only_for_single_instance_types() {
        const QList<Instance> panels = {
            {QStringLiteral("equity_trading"), false, false},
            {QStringLiteral("watchlist"), false, true},
            {QStringLiteral("equity_trading#dup2"), false, true},
            {QStringLiteral("watchlist#dup2"), false, true},
            {QStringLiteral("settings"), false, true},
            {QStringLiteral("fno#dup3"), false, true}, // alone: nothing to collapse here
            {QStringLiteral("crypto_trading#dup2"), false, false},
            {QStringLiteral("crypto_trading#dup4"), false, true},
        };
        QCOMPARE(surplus_panels(panels),
                 (QStringList{QStringLiteral("equity_trading#dup2"), QStringLiteral("crypto_trading#dup2")}));
        QVERIFY(surplus_panels({}).isEmpty());
    }

    void switch_payload_to_navigate_arguments() {
        QCOMPARE(switch_is_exclusive({}), false); // default: beside the current view
        QCOMPARE(switch_is_exclusive({{QStringLiteral("exclusive"), true}}), true);
        QCOMPARE(switch_is_exclusive({{QStringLiteral("exclusive"), false}}), false);
        QCOMPARE(switch_is_exclusive({{QStringLiteral("new_panel"), true}}), false);  // Ctrl+Enter
        QCOMPARE(switch_is_exclusive({{QStringLiteral("new_panel"), false}}), true);  // Enter
        QCOMPARE(switch_is_exclusive({{QStringLiteral("exclusive"), true}, {QStringLiteral("new_panel"), false}}),
                 true);
        // Disagreeing keys: the new-panel request wins, nothing is closed.
        QCOMPARE(switch_is_exclusive({{QStringLiteral("exclusive"), true}, {QStringLiteral("new_panel"), true}}),
                 false);
        // Strings as a JSON payload from an MCP tool may carry them.
        QCOMPARE(switch_is_exclusive({{QStringLiteral("exclusive"), QStringLiteral("true")}}), true);
    }

    void layout_with_a_repeated_widget_keeps_the_first() {
        const QByteArray in(kRepeatedState);
        QStringList dropped;
        const QByteArray out = fincept::dock_state::without_single_instance_duplicates(in, &dropped);
        QCOMPARE(dropped, QStringList{QStringLiteral("equity_trading")});
        QCOMPARE(fincept::dock_state::widget_names(out),
                 (QStringList{QStringLiteral("equity_trading"), QStringLiteral("news")}));
        // The kept entry's area still names it as current.
        const QByteArray kept_area(kKeptArea);
        QVERIFY(out.contains(kept_area));
        // Compressed blobs the same way, and stay compressed.
        QStringList dropped_z;
        const QByteArray out_z = fincept::dock_state::without_single_instance_duplicates(qCompress(in), &dropped_z);
        QCOMPARE(dropped_z, dropped);
        QVERIFY(qUncompress(out_z) == out);
    }
};

QTEST_GUILESS_MAIN(TestDockInstanceGuard)
#include "tst_dock_instance_guard.moc"
