// Unit tests for src/services/algo_trading/AlgoRuleLogic.{h,cpp}
//
// The Algo Trading Builder's plain-language sentences, its inline rule validation (the blank
// form's "Close > 0" placeholder must never run), the blank-draft detection that makes the
// default run open a template, the rule-derived indicator warm-up of a backtest, and that every
// built-in template is a complete, valid strategy.
// AlgoRuleLogic.cpp includes AlgoTradingTypes.h (indicator catalogue), which includes the theme
// header for a colour helper — Qt Gui headers only, no app TU besides the unit itself.

#include "services/algo_trading/AlgoRuleLogic.h"
#include "services/algo_trading/AlgoTradingTypes.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QTest>

namespace rules = fincept::services::algo::rules;
using fincept::services::algo::algo_strategy_templates;

namespace {

QJsonObject leaf_value(const QString& ind, const QJsonObject& params, const QString& op, double v,
                       const QString& field = QStringLiteral("value")) {
    return QJsonObject{{"indicator", ind}, {"params", params}, {"field", field}, {"offset", 0},
                       {"operator", op},   {"compare_mode", "value"},           {"value", v}};
}

QJsonObject leaf_ind(const QString& ind, const QJsonObject& params, const QString& op, const QString& cind,
                     const QJsonObject& cparams, const QString& cfield = QStringLiteral("value")) {
    return QJsonObject{{"indicator", ind},
                       {"params", params},
                       {"field", "value"},
                       {"offset", 0},
                       {"operator", op},
                       {"compare_mode", "indicator"},
                       {"compare_indicator", cind},
                       {"compare_params", cparams},
                       {"compare_field", cfield},
                       {"compare_offset", 0}};
}

} // namespace

class TstAlgoRuleLogic : public QObject {
    Q_OBJECT

  private slots:
    void operand_text_names_params_fields_and_offsets() {
        QCOMPARE(rules::operand_text("SMA", {{"period", 50}}, "value", 0), QStringLiteral("SMA(50)"));
        QCOMPARE(rules::operand_text("CLOSE", {}, "value", 0), QStringLiteral("Close"));
        QCOMPARE(rules::operand_text("MACD", {{"fast", 12}, {"slow", 26}, {"signal", 9}}, "signal_line", 0),
                 QStringLiteral("MACD(12, 26, 9) signal line"));
        QCOMPARE(rules::operand_text("MACD", {}, "line", 0), QStringLiteral("MACD(12, 26, 9)")); // catalogue defaults
        QCOMPARE(rules::operand_text("BOLLINGER", {{"period", 20}, {"std_dev", 2}}, "upper", 0),
                 QStringLiteral("Bollinger Bands(20, 2) upper"));
        QCOMPARE(rules::operand_text("RSI", {{"period", 14}}, "value", 1), QStringLiteral("RSI(14) 1 bar(s) ago"));
    }

    void sentences_read_like_the_rule() {
        const QJsonArray golden = {leaf_ind("SMA", {{"period", 50}}, "crosses_above", "SMA", {{"period", 200}})};
        QCOMPARE(rules::rules_sentence(golden, "AND", rules::Side::Entry),
                 QStringLiteral("Enter when SMA(50) crosses above SMA(200)"));
        const QJsonArray two = {leaf_value("RSI", {{"period", 14}}, "<", 30),
                                leaf_ind("CLOSE", {}, ">", "SMA", {{"period", 200}})};
        QCOMPARE(rules::rules_sentence(two, "OR", rules::Side::Exit),
                 QStringLiteral("Exit when RSI(14) < 30 or Close > SMA(200)"));
        QJsonObject group{{"children", QJsonArray{leaf_value("VOLUME", {}, ">", 1e6), leaf_value("RSI", {{"period", 14}}, ">", 50)}},
                          {"logic", "OR"}};
        const QJsonArray nested = {leaf_ind("CLOSE", {}, "crosses_above", "EMA", {{"period", 20}}), group};
        QCOMPARE(rules::rules_sentence(nested, "AND", rules::Side::Scan),
                 QStringLiteral("Match when Close crosses above EMA(20) and (Volume > 1000000 or RSI(14) > 50)"));
        QJsonObject between = leaf_value("RSI", {{"period", 14}}, "between", 40);
        between["value2"] = 60;
        QCOMPARE(rules::condition_text(between), QStringLiteral("RSI(14) is between 40 and 60"));
        QCOMPARE(rules::condition_text(leaf_value("SMA", {{"period", 20}}, "rising", 0)),
                 QStringLiteral("SMA(20) is rising"));
        QVERIFY(rules::rules_sentence({}, "AND", rules::Side::Entry).contains(QStringLiteral("No entry rule")));
    }

    void placeholder_close_above_zero_is_blocked_with_a_reason() {
        const QJsonObject ph = leaf_value("CLOSE", {}, ">", 0.0);
        QVERIFY(rules::is_placeholder_condition(ph));
        const auto issues = rules::check_conditions({ph});
        QCOMPARE(issues.size(), 1);
        QCOMPARE(issues.first().index, 0);
        QVERIFY(issues.first().blocking);
        QVERIFY(issues.first().reason.contains(QStringLiteral("always true")));
        const QString why = rules::validate_strategy({ph}, {ph}, 2, 5, 0);
        QVERIFY(why.startsWith(QStringLiteral("ENTRY rule 1:")));
        // The blank form (placeholder both sides, or nothing) is a draft nobody wrote.
        QVERIFY(rules::is_blank_rules({ph}, {ph}));
        QVERIFY(rules::is_blank_rules({}, {}));
        QVERIFY(!rules::is_blank_rules({leaf_value("CLOSE", {}, ">", 400.0)}, {ph}));
    }

    void impossible_and_self_comparisons_are_flagged() {
        QVERIFY(!rules::check_conditions({leaf_value("RSI", {{"period", 14}}, ">", 120)}).isEmpty());   // never
        QVERIFY(!rules::check_conditions({leaf_value("RSI", {{"period", 14}}, "<", 0)}).isEmpty());     // never
        QVERIFY(!rules::check_conditions({leaf_value("WILLIAMS_R", {{"period", 14}}, ">", 5)}).isEmpty());
        QVERIFY(!rules::check_conditions({leaf_value("CLOSE", {}, "<", -1)}).isEmpty());
        QVERIFY(!rules::check_conditions({leaf_ind("SMA", {{"period", 50}}, "crosses_above", "SMA", {{"period", 50}})})
                     .isEmpty());
        QJsonObject bad_between = leaf_value("RSI", {{"period", 14}}, "between", 70);
        bad_between["value2"] = 30;
        QVERIFY(!rules::check_conditions({bad_between}).isEmpty());
        QJsonObject no_rhs = leaf_ind("CLOSE", {}, ">", QString(), {});
        QVERIFY(rules::check_conditions({no_rhs}).first().reason.contains(QStringLiteral("compare")));
        QJsonObject empty_group{{"children", QJsonArray()}, {"logic", "AND"}};
        QVERIFY(!rules::check_conditions({empty_group}).isEmpty());
        // Rows inside a group are located by (index, child).
        QJsonObject group{{"children", QJsonArray{leaf_value("RSI", {{"period", 14}}, "<", 30), leaf_value("CLOSE", {}, ">", 0)}},
                          {"logic", "OR"}};
        const auto in_group = rules::check_conditions({leaf_value("RSI", {{"period", 14}}, "<", 30), group});
        QCOMPARE(in_group.size(), 1);
        QCOMPARE(in_group.first().index, 1);
        QCOMPARE(in_group.first().child, 1);
        // Fine rules pass.
        QVERIFY(rules::check_conditions({leaf_value("RSI", {{"period", 14}}, "<", 30)}).isEmpty());
        QVERIFY(rules::check_conditions({leaf_value("CLOSE", {}, ">", 450.5)}).isEmpty());
    }

    void a_strategy_needs_an_entry_and_a_way_out() {
        const QJsonArray entry = {leaf_value("RSI", {{"period", 14}}, "<", 30)};
        QVERIFY(rules::validate_strategy({}, {}, 2, 5, 0).contains(QStringLiteral("entry")));
        QVERIFY(rules::validate_strategy(entry, {}, 0, 0, 0).contains(QStringLiteral("never exits")));
        QVERIFY(rules::validate_strategy(entry, {}, 2, 0, 0).isEmpty()); // a stop is a way out
        QVERIFY(rules::validate_strategy(entry, {}, 0, 0, 1.5).isEmpty());
        QVERIFY(rules::validate_strategy(entry, {leaf_value("RSI", {{"period", 14}}, ">", 60)}, 0, 0, 0).isEmpty());
    }

    void warmup_follows_the_slowest_indicator() {
        const QJsonArray golden = {leaf_ind("SMA", {{"period", 50}}, "crosses_above", "SMA", {{"period", 200}})};
        QCOMPARE(rules::warmup_bars(golden, {}), 201); // SMA 200 + the previous bar of a cross
        QCOMPARE(rules::warmup_bars({leaf_value("CLOSE", {}, ">", 1)}, {}), 50); // the floor
        QCOMPARE(rules::warmup_bars({leaf_ind("EMA", {{"period", 200}}, ">", "CLOSE", {})}, {}), 499); // capped
        QJsonObject offset = leaf_ind("CLOSE", {}, ">", "DONCHIAN", {{"period", 120}}, "upper");
        offset["compare_offset"] = 1;
        QCOMPARE(rules::warmup_bars({offset}, {}), 122);
    }

    void every_template_is_a_complete_valid_strategy() {
        const auto tpls = algo_strategy_templates();
        QVERIFY(tpls.size() >= 7);
        QCOMPARE(tpls.first().name, QStringLiteral("Golden Cross")); // the default run's template
        for (const auto& t : tpls) {
            QVERIFY2(rules::validate_strategy(t.entry, t.exit, t.stop_loss, t.take_profit, 0).isEmpty(),
                     qPrintable(t.name));
            QVERIFY2(!rules::is_blank_rules(t.entry, t.exit), qPrintable(t.name));
            QVERIFY(rules::rules_sentence(t.entry, t.entry_logic, rules::Side::Entry).startsWith(QStringLiteral("Enter when")));
        }
    }
};

QTEST_GUILESS_MAIN(TstAlgoRuleLogic)
#include "tst_algo_rule_logic.moc"
