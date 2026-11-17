// tst_markets_logic — the pure rules of the Markets screen (src/screens/markets/MarketsLogic.h)
// and its column registry (MarketColumns): saved-symbol canonicalisation, row states,
// retry wording classes, grid re-flow, panel drag-to-reorder slots, first-run heights,
// ribbon tile count, column auto-hide, sort order of missing / n/a values, legacy column
// mapping, "n/a" rules.

#include "screens/markets/MarketColumns.h"
#include "screens/markets/MarketsLogic.h"

#include <QtTest>

#include <cmath>
#include <limits>

using namespace fincept::screens;
namespace ml = fincept::screens::markets_logic;
using fincept::core::instrument::AssetClass;

class TstMarketsLogic : public QObject {
    Q_OBJECT
  private slots:
    void canonical_panel_symbols_rewrites_only_known_forms() {
        bool changed = false;
        const QStringList in{"USDJPY=X", "EURUSD=X", "usdjpy=x", "LBR=F", "", "FOO BAR"};
        const QStringList out = ml::canonical_panel_symbols(in, &changed);
        QVERIFY(changed);
        QCOMPARE(out, (QStringList{"JPY=X", "EURUSD=X", "LBR=F", "FOO BAR"}));
        // Idempotent: a second pass changes nothing.
        bool again = true;
        QCOMPARE(ml::canonical_panel_symbols(out, &again), out);
        QVERIFY(!again);
    }

    void canonical_or_kept_upper_cases_unknown_text() {
        QCOMPARE(ml::canonical_or_kept(QStringLiteral(" usdinr=x ")), QStringLiteral("INR=X"));
        QCOMPARE(ml::canonical_or_kept(QString()), QString());
    }

    void row_state_rules() {
        const qint64 now = 10'000'000;
        QCOMPARE(ml::row_state(false, false, ml::Session::Unknown, now, 0, 60000), ml::RowState::Loading);
        QCOMPARE(ml::row_state(false, true, ml::Session::Unknown, now, 0, 60000), ml::RowState::FailedNoData);
        QCOMPARE(ml::row_state(true, true, ml::Session::Regular, now, now, 60000), ml::RowState::FailedOld);
        QCOMPARE(ml::row_state(true, false, ml::Session::Regular, now, now - 1000, 60000), ml::RowState::Ok);
        QCOMPARE(ml::row_state(true, false, ml::Session::Closed, now, now - 1000, 60000), ml::RowState::Closed);
        QCOMPARE(ml::row_state(true, false, ml::Session::Extended, now, now - 1000, 60000), ml::RowState::Closed);
        QCOMPARE(ml::row_state(true, false, ml::Session::Regular, now, now - 200'000, 60000), ml::RowState::Stale);
        // 5M interval: 2.5 x 5 min before a quote is stale.
        QCOMPARE(ml::row_state(true, false, ml::Session::Regular, now, now - 600'000, 300000), ml::RowState::Ok);
        QCOMPARE(ml::stale_after_ms(0), qint64(150'000));
        QCOMPARE(ml::stale_after_ms(300000), qint64(750'000));
    }

    void session_from_state() {
        QCOMPARE(ml::session_from_state(QStringLiteral("REGULAR")), ml::Session::Regular);
        QCOMPARE(ml::session_from_state(QStringLiteral("PRE")), ml::Session::Extended);
        QCOMPARE(ml::session_from_state(QStringLiteral("POST")), ml::Session::Extended);
        QCOMPARE(ml::session_from_state(QStringLiteral("CLOSED")), ml::Session::Closed);
        QCOMPARE(ml::session_from_state(QString()), ml::Session::Unknown);
    }

    void retry_wording_classes() {
        QCOMPARE(ml::retry_kind_for(QStringLiteral(
                     "Yahoo Finance is rate limiting requests (too many requests) - retrying automatically in a minute")),
                 ml::RetryKind::RateLimited);
        QCOMPARE(ml::retry_kind_for(QStringLiteral("Symbol 'ZZZZNOTREAL' not found on Yahoo Finance (unknown or delisted)")),
                 ml::RetryKind::NotFound);
        QCOMPARE(ml::retry_kind_for(QStringLiteral("Could not reach Yahoo Finance for X (network error)")),
                 ml::RetryKind::Ladder);
        QCOMPARE(ml::retry_minutes(ml::RetryKind::NotFound), 15);
        QCOMPARE(ml::retry_minutes(ml::RetryKind::RateLimited), 1);
        QCOMPARE(ml::retry_minutes(ml::RetryKind::Ladder), 0);
    }

    void reading_order_and_reflow() {
        const QVector<QStringList> cols{{"a", "b", "c"}, {"d", "e"}, {"f"}};
        const QStringList reading = ml::reading_order(cols);
        QCOMPARE(reading, (QStringList{"a", "d", "f", "b", "e", "c"}));
        const QVector<QStringList> two = ml::reflow(reading, 2);
        QCOMPARE(two.size(), 2);
        QCOMPARE(two[0], (QStringList{"a", "f", "e"}));
        QCOMPARE(two[1], (QStringList{"d", "b", "c"}));
        QCOMPARE(ml::reflow(reading, 1).first(), reading);
        QCOMPARE(ml::grid_columns_for_width(1200), 3);
        QCOMPARE(ml::grid_columns_for_width(960), 3);
        QCOMPARE(ml::grid_columns_for_width(959), 2);
        QCOMPARE(ml::grid_columns_for_width(640), 2);
        QCOMPARE(ml::grid_columns_for_width(639), 1);
    }

    void move_panel_to_slot_rules() {
        const QVector<QStringList> cols{{"a", "b", "c"}, {"d", "e"}, {}};
        bool changed = false;
        // Same column, down: the slot is counted with "a" still in place (below "b" = 2).
        QCOMPARE(ml::move_panel_to_slot(cols, "a", 0, 2, &changed), (QVector<QStringList>{{"b", "a", "c"}, {"d", "e"}, {}}));
        QVERIFY(changed);
        // Same column, to the bottom (slot = size).
        QCOMPARE(ml::move_panel_to_slot(cols, "a", 0, 3), (QVector<QStringList>{{"b", "c", "a"}, {"d", "e"}, {}}));
        // Same column, up to the top.
        QCOMPARE(ml::move_panel_to_slot(cols, "c", 0, 0), (QVector<QStringList>{{"c", "a", "b"}, {"d", "e"}, {}}));
        // Above or below itself: nothing changes.
        QCOMPARE(ml::move_panel_to_slot(cols, "b", 0, 1, &changed), cols);
        QVERIFY(!changed);
        QCOMPARE(ml::move_panel_to_slot(cols, "b", 0, 2, &changed), cols);
        QVERIFY(!changed);
        // Across columns: between "d" and "e".
        QCOMPARE(ml::move_panel_to_slot(cols, "b", 1, 1), (QVector<QStringList>{{"a", "c"}, {"d", "b", "e"}, {}}));
        // Into the empty third column.
        QCOMPARE(ml::move_panel_to_slot(cols, "e", 2, 0), (QVector<QStringList>{{"a", "b", "c"}, {"d"}, {"e"}}));
        // The last panel of a column leaves it empty (the column keeps its place).
        QCOMPARE(ml::move_panel_to_slot({{"a"}, {"b"}, {"c"}}, "a", 2, 1),
                 (QVector<QStringList>{{}, {"b"}, {"c", "a"}}));
        // Out-of-range slots are clamped; an unknown id changes nothing.
        QCOMPARE(ml::move_panel_to_slot(cols, "a", 9, 99), (QVector<QStringList>{{"b", "c"}, {"d", "e"}, {"a"}}));
        QCOMPARE(ml::move_panel_to_slot(cols, "a", -1, -5), cols);
        QCOMPARE(ml::move_panel_to_slot(cols, "zz", 1, 0, &changed), cols);
        QVERIFY(!changed);
    }

    void panel_drop_target_from_geometry() {
        // Three shown columns 0-99, 103-202, 206-305 (3 px handles); the third is an empty
        // saved column. Panels: a 0-99, b 103-202 in column 0; d 0-150 in column 1.
        using Col = ml::PanelDropColumn;
        QVector<Col> shown(3);
        shown[0].left = 0;
        shown[0].right = 99;
        shown[0].panels = {{"a", 0, 99}, {"b", 103, 202}};
        shown[1].left = 103;
        shown[1].right = 202;
        shown[1].panels = {{"d", 0, 150}};
        shown[2].left = 206;
        shown[2].right = 305;
        shown[2].saved_column = 2;

        ml::PanelDropTarget t = ml::panel_drop_target_at(shown, 50, 10); // upper half of "a"
        QVERIFY(t.valid);
        QCOMPARE(t.column, 0);
        QCOMPARE(t.anchor, QStringLiteral("a"));
        QVERIFY(!t.after);
        t = ml::panel_drop_target_at(shown, 50, 80); // lower half of "a" = above "b"
        QCOMPARE(t.anchor, QStringLiteral("b"));
        QVERIFY(!t.after);
        t = ml::panel_drop_target_at(shown, 50, 400); // below everything: after the last
        QCOMPARE(t.anchor, QStringLiteral("b"));
        QVERIFY(t.after);
        t = ml::panel_drop_target_at(shown, 101, 140); // on the handle: nearest column (0)
        QCOMPARE(t.column, 0);
        t = ml::panel_drop_target_at(shown, 250, 40); // the empty saved column
        QCOMPARE(t.column, 2);
        QVERIFY(t.anchor.isEmpty());
        QCOMPARE(t.empty_column, 2);
        t = ml::panel_drop_target_at(shown, 900, 40); // right of the grid: nearest column
        QCOMPARE(t.column, 2);

        // An empty re-flowed column is not a target; no column at all: invalid.
        QVector<Col> reflowed(2);
        reflowed[0].left = 0;
        reflowed[0].right = 99;
        reflowed[0].panels = {{"a", 0, 99}};
        reflowed[1].left = 103;
        reflowed[1].right = 202;
        t = ml::panel_drop_target_at(reflowed, 150, 10);
        QCOMPARE(t.column, 0);
        QCOMPARE(t.anchor, QStringLiteral("a"));
        QVERIFY(!ml::panel_drop_target_at({}, 10, 10).valid);
    }

    void saved_drop_slot_and_reflow_mapping() {
        const QVector<QStringList> saved{{"a", "b", "c"}, {"d", "e"}, {"f"}};
        ml::PanelDropTarget t;
        int col = -1;
        int idx = -1;
        QVERIFY(!ml::saved_drop_slot(saved, t, &col, &idx)); // invalid target
        t.valid = true;
        t.anchor = QStringLiteral("e");
        QVERIFY(ml::saved_drop_slot(saved, t, &col, &idx));
        QCOMPARE(col, 1);
        QCOMPARE(idx, 1);
        t.after = true;
        QVERIFY(ml::saved_drop_slot(saved, t, &col, &idx));
        QCOMPARE(idx, 2);
        t.anchor = QStringLiteral("nope");
        QVERIFY(!ml::saved_drop_slot(saved, t, &col, &idx));
        // An empty saved column: its end.
        const QVector<QStringList> with_empty{{"a"}, {}, {"b"}};
        ml::PanelDropTarget e;
        e.valid = true;
        e.empty_column = 1;
        QVERIFY(ml::saved_drop_slot(with_empty, e, &col, &idx));
        QCOMPARE(col, 1);
        QCOMPARE(idx, 0);
        QCOMPARE(ml::move_panel_to_slot(with_empty, "a", col, idx), (QVector<QStringList>{{}, {"a"}, {"b"}}));

        // 2-column re-flow of the saved layout shows {a, f, e} | {d, b, c}. Dropping "a"
        // below "f" (shown column 0) lands below "f" in the saved layout (column 2).
        const QVector<QStringList> two = ml::reflow(ml::reading_order(saved), 2);
        QCOMPARE(two[0], (QStringList{"a", "f", "e"}));
        ml::PanelDropTarget r;
        r.valid = true;
        r.anchor = QStringLiteral("f");
        r.after = true;
        QVERIFY(ml::saved_drop_slot(saved, r, &col, &idx));
        QCOMPARE(ml::move_panel_to_slot(saved, "a", col, idx), (QVector<QStringList>{{"b", "c"}, {"d", "e"}, {"f", "a"}}));
        // Dropping "c" above "d" (shown column 1, top) lands above "d" in saved column 1.
        r.anchor = QStringLiteral("d");
        r.after = false;
        QVERIFY(ml::saved_drop_slot(saved, r, &col, &idx));
        QCOMPARE(ml::move_panel_to_slot(saved, "c", col, idx), (QVector<QStringList>{{"a", "b"}, {"c", "d", "e"}, {"f"}}));
    }

    void proportional_heights_and_ribbon() {
        QCOMPARE(ml::proportional_sizes({18, 12, 0}), (QList<int>{1600, 1400, 200}));
        QCOMPARE(ml::ribbon_tile_count(1440, 124, 6, 10), 10);
        QCOMPARE(ml::ribbon_tile_count(800, 124, 6, 10), 6);
        QCOMPARE(ml::ribbon_tile_count(200, 124, 6, 10), 3);
        QCOMPARE(ml::ribbon_tile_count(200, 124, 6, 2), 2);
    }

    void auto_hide_drops_lowest_rank_first() {
        // (column, width): change 90, change_pct 70, spark 96, day_range 150
        const QVector<QPair<int, int>> visible{{3, 90}, {4, 70}, {5, 96}, {6, 150}};
        const QHash<int, int> rank{{3, 9}, {4, -1}, {5, 10}, {6, 1}};
        QVERIFY(ml::auto_hidden_columns(visible, rank, 1000, 96).isEmpty());
        // 96 + 406 = 502 needed; at 400 day_range (rank 1) goes first.
        QCOMPARE(ml::auto_hidden_columns(visible, rank, 400, 96), (QSet<int>{6}));
        // At 250: day_range, then change (9), then spark (10); CHG % never.
        QCOMPARE(ml::auto_hidden_columns(visible, rank, 170, 96), (QSet<int>{6, 3, 5}));
    }

    void missing_and_na_sort_last_both_ways() {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        // Ascending: lessThan(a, b) = compare(a, b, false) < 0.
        QVERIFY(ml::compare_keys(1.0, false, nan, false, false) < 0);
        QVERIFY(ml::compare_keys(nan, false, 1.0, true, false) < 0); // missing before n/a
        QVERIFY(ml::compare_keys(1.0, false, 2.0, false, false) < 0);
        // Descending: the proxy calls lessThan(right, left); a real value must still come first.
        QVERIFY(ml::compare_keys(nan, false, 1.0, false, true) < 0);
        QVERIFY(ml::compare_keys(5.0, true, 1.0, false, true) < 0);
        QCOMPARE(ml::direction_of(0.0), 0);
        QCOMPARE(ml::direction_of(nan), 0);
        QCOMPARE(ml::direction_of(-0.1), -1);
    }

    void legacy_column_mapping() {
        QCOMPARE(mcols::ids_from_legacy({"SYMBOL", "LAST", "CHG", "CHG%", "HIGH", "LOW"}), mcols::default_ids());
        QCOMPARE(mcols::ids_from_legacy({"SYMBOL", "LAST", "VOL", "BID", "ASK", "OPEN", "NAME"}),
                 (QStringList{"name", "last", "volume", "open"}));
        QCOMPARE(mcols::ids_from_legacy({"CHG%", "LAST"}), (QStringList{"name", "change_pct", "last"}));
        QCOMPARE(mcols::legacy_from_ids({"name", "last", "spark", "volume"}), (QStringList{"SYMBOL", "LAST", "VOL"}));
        QCOMPARE(mcols::sanitize_ids({"panel", "bogus", "last", "last", "change"}),
                 (QStringList{"name", "last", "change"}));
        QCOMPARE(mcols::default_ids(), (QStringList{"name", "last", "change", "change_pct", "spark"}));
        QCOMPARE(mcols::index_of(u"range_52w"), static_cast<int>(mcols::Range52w));
        QVERIFY(mcols::locked(mcols::Name));
        QVERIFY(mcols::locked(mcols::Last));
        QVERIFY(!mcols::locked(mcols::ChangePct));
    }

    void not_applicable_rules() {
        fincept::services::QuoteData q;
        q.volume = 0;
        QVERIFY(!mcols::applies(mcols::Volume, AssetClass::Fx, q));
        QVERIFY(!mcols::applies(mcols::Volume, AssetClass::Index, q));
        q.volume = 1000;
        QVERIFY(mcols::applies(mcols::Volume, AssetClass::Index, q));
        QVERIFY(mcols::applies(mcols::Volume, AssetClass::Equity, q));
        QVERIFY(!mcols::applies(mcols::RelVolume, AssetClass::Equity, q)); // no average volume
        q.average_volume = 2000;
        QVERIFY(mcols::applies(mcols::RelVolume, AssetClass::Equity, q));
        QVERIFY(mcols::applies(mcols::Last, AssetClass::Fx, q));
    }
};

QTEST_GUILESS_MAIN(TstMarketsLogic)
#include "tst_markets_logic.moc"
