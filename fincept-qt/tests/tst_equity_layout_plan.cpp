// tst_equity_layout_plan — the Equity Trading screen's responsive rules
// (screens/equity_trading/EquityLayoutPlan.h): width classes and their breakpoints,
// font scaling, the per-class zone plan, splitter fitting and restoring.
// QtCore only.

#include "screens/equity_trading/EquityLayoutPlan.h"

#include <QTest>

using namespace fincept::screens::equity;

namespace {
int sum(const QList<int>& v) {
    int s = 0;
    for (int x : v)
        s += x;
    return s;
}
} // namespace

class TstEquityLayoutPlan : public QObject {
    Q_OBJECT
  private slots:
    void breakpoints_at_reference_font() {
        QCOMPARE(eq_width_class(1920, 14), EqWidthClass::Wide);
        QCOMPARE(eq_width_class(1600, 14), EqWidthClass::Wide);
        QCOMPARE(eq_width_class(1599, 14), EqWidthClass::Medium);
        QCOMPARE(eq_width_class(1280, 14), EqWidthClass::Medium);
        QCOMPARE(eq_width_class(1279, 14), EqWidthClass::Narrow);
        QCOMPARE(eq_width_class(1000, 14), EqWidthClass::Narrow);
        QCOMPARE(eq_width_class(999, 14), EqWidthClass::Compact);
        QCOMPARE(eq_width_class(0, 14), EqWidthClass::Compact);
        QCOMPARE(eq_width_class(-5, 14), EqWidthClass::Compact);
    }

    void larger_font_behaves_like_a_narrower_window() {
        // 24 px base: 1920 px is 1120 px at the reference font -> Narrow.
        QCOMPARE(eq_effective_width(1920, 24), 1120);
        QCOMPARE(eq_width_class(1920, 24), EqWidthClass::Narrow);
        QCOMPARE(eq_width_class(2800, 24), EqWidthClass::Wide);
        QCOMPARE(eq_width_class(1600, 24), EqWidthClass::Compact);
        // A smaller font never widens the class (dense screens stay as designed).
        QCOMPARE(eq_effective_width(1280, 9), 1280);
        QCOMPARE(eq_width_class(1280, 9), EqWidthClass::Medium);
        QCOMPARE(eq_width_class(999, 9), EqWidthClass::Compact);
    }

    void plan_per_class() {
        const auto wide = eq_layout_plan(EqWidthClass::Wide);
        QVERIFY(!wide.single_column && !wide.watchlist_rail && !wide.right_drawer);
        QCOMPARE(wide.depth_levels, 5);
        QVERIFY(wide.watchlist_chars > 0 && wide.right_chars > 0);

        const auto medium = eq_layout_plan(EqWidthClass::Medium);
        QVERIFY(!medium.single_column && !medium.watchlist_rail && !medium.right_drawer);
        QVERIFY(medium.depth_levels < wide.depth_levels);      // depth collapses
        QVERIFY(medium.watchlist_chars < wide.watchlist_chars); // watchlist narrows

        const auto narrow = eq_layout_plan(EqWidthClass::Narrow);
        QVERIFY(narrow.watchlist_rail && narrow.right_drawer && !narrow.single_column);
        QCOMPARE(narrow.watchlist_chars, 0);

        const auto compact = eq_layout_plan(EqWidthClass::Compact);
        QVERIFY(compact.single_column);
        QCOMPARE(compact.cls, EqWidthClass::Compact);
    }

    void class_keys_are_stable_and_distinct() {
        QCOMPARE(eq_width_class_key(EqWidthClass::Wide), QStringLiteral("wide"));
        QCOMPARE(eq_width_class_key(EqWidthClass::Medium), QStringLiteral("medium"));
        QCOMPARE(eq_width_class_key(EqWidthClass::Narrow), QStringLiteral("narrow"));
        QCOMPARE(eq_width_class_key(EqWidthClass::Compact), QStringLiteral("compact"));
    }

    void fit_gives_the_flex_pane_the_rest() {
        const auto s = eq_fit_sizes({300, 0, 320}, {200, 500, 280}, 1, 1920);
        QCOMPARE(s, (QList<int>{300, 1300, 320}));
        QCOMPARE(sum(s), 1920);
    }

    void fit_hidden_pane_gets_nothing() {
        const auto s = eq_fit_sizes({0, 0, 320}, {200, 500, 280}, 1, 1200);
        QCOMPARE(s, (QList<int>{0, 880, 320}));
    }

    void fit_shrinks_fixed_panes_before_the_flex_minimum() {
        // 1000 px: 300 + 320 wanted leaves 380 < 500 for the chart -> the fixed panes give 120.
        const auto s = eq_fit_sizes({300, 0, 320}, {200, 500, 280}, 1, 1000);
        QCOMPARE(sum(s), 1000);
        QCOMPARE(s[1], 500);
        QVERIFY(s[0] >= 200 && s[2] >= 280);
        QVERIFY(s[0] < 300 && s[2] < 320);
    }

    void fit_never_below_fixed_minimums_until_impossible() {
        // Minimums alone (200 + 500 + 280 = 980) still fit 990: everything at its floor + 10.
        const auto s = eq_fit_sizes({300, 0, 320}, {200, 500, 280}, 1, 990);
        QCOMPARE(sum(s), 990);
        QVERIFY(s[0] >= 200 && s[2] >= 280);
        // Impossible: the result still sums to the total and nothing is negative.
        const auto t = eq_fit_sizes({300, 0, 320}, {200, 500, 280}, 1, 400);
        QCOMPARE(sum(t), 400);
        for (int v : t)
            QVERIFY(v >= 0);
    }

    void fit_guards() {
        QCOMPARE(eq_fit_sizes({}, {}, 0, 100), QList<int>{});
        QCOMPARE(eq_fit_sizes({1, 2}, {}, 5, 100), (QList<int>{0, 0}));
        QCOMPARE(eq_fit_sizes({1, 2}, {}, 0, 0), (QList<int>{0, 0}));
    }

    void restore_scales_to_the_new_total() {
        const auto s = eq_restore_sizes({300, 900, 300}, {200, 400, 250}, {true, true, true}, 1800);
        QCOMPARE(sum(s), 1800);
        QCOMPARE(s[0], 360);
        QCOMPARE(s[2], 360);
    }

    void restore_ignores_hidden_panes() {
        const auto s = eq_restore_sizes({300, 900, 300}, {200, 400, 250}, {true, true, false}, 1200);
        QCOMPARE(s[2], 0);
        QCOMPARE(sum(s), 1200);
        QCOMPARE(s[0], 300);
    }

    void restore_refuses_bad_or_squeezing_state() {
        QVERIFY(eq_restore_sizes({300, 900}, {}, {true, true, true}, 1200).isEmpty()); // count
        QVERIFY(eq_restore_sizes({0, 0, 0}, {}, {true, true, true}, 1200).isEmpty());  // all zero
        QVERIFY(eq_restore_sizes({-1, 9, 9}, {}, {true, true, true}, 1200).isEmpty()); // negative
        QVERIFY(eq_restore_sizes({1, 1, 1}, {}, {true, true, true}, 0).isEmpty());     // no room
        // A pane that would land under its minimum: defaults instead (no squeezed state).
        QVERIFY(eq_restore_sizes({50, 900, 300}, {200, 400, 250}, {true, true, true}, 1250).isEmpty());
    }
};

QTEST_MAIN(TstEquityLayoutPlan)
#include "tst_equity_layout_plan.moc"
