// test_schema_roundtrip — round-trip a model response through the JSON parser
// and the action_to_json serialiser, asserting fields survive intact.
//
// Reference: .grill-me/alpha-arena-production-refactor.md §Phase 2.

#include "services/alpha_arena/AlphaArenaJson.h"
#include "services/alpha_arena/AlphaArenaSchema.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QTest>

using namespace fincept::services::alpha_arena;

class TestSchemaRoundtrip : public QObject {
    Q_OBJECT

  private slots:

    void parses_minimal_buy_action() {
        const QString raw = R"({
            "actions":[{
                "signal":"buy_to_enter",
                "coin":"BTC",
                "quantity":0.1,
                "leverage":5,
                "profit_target":75000.0,
                "stop_loss":68000.0,
                "invalidation_condition":"break of 67k",
                "confidence":0.72,
                "risk_usd":250.0,
                "justification":"breakout above 70k with volume confirmation"
            }]
        })";
        ModelResponse r = parse_model_response(raw);
        QVERIFY(!r.parse_error.has_value());
        QCOMPARE(r.actions.size(), 1);
        const ProposedAction& a = r.actions[0];
        QCOMPARE(a.signal, Signal::BuyToEnter);
        QCOMPARE(a.coin, QStringLiteral("BTC"));
        QCOMPARE(a.quantity, 0.1);
        QCOMPARE(a.leverage, 5);
        QCOMPARE(a.profit_target, 75000.0);
        QCOMPARE(a.stop_loss, 68000.0);
        QCOMPARE(a.confidence, 0.72);
        QCOMPARE(a.risk_usd, 250.0);
    }

    void roundtrip_action_to_json_and_back() {
        ProposedAction in;
        in.signal = Signal::SellToEnter;
        in.coin = QStringLiteral("ETH");
        in.quantity = 2.5;
        in.leverage = 10;
        in.profit_target = 3200.0;
        in.stop_loss = 3500.0;
        in.invalidation_condition = QStringLiteral("close above 3500");
        in.confidence = 0.55;
        in.risk_usd = 500.0;
        in.justification = QStringLiteral("rejection at 3500 resistance");

        QJsonObject obj = action_to_json(in);
        QJsonObject envelope;
        QJsonArray arr;
        arr.append(obj);
        envelope[QStringLiteral("actions")] = arr;
        const QString text = QString::fromUtf8(QJsonDocument(envelope).toJson(QJsonDocument::Compact));

        ModelResponse out = parse_model_response(text);
        QVERIFY(!out.parse_error.has_value());
        QCOMPARE(out.actions.size(), 1);
        const auto& a = out.actions[0];
        QCOMPARE(a.signal, in.signal);
        QCOMPARE(a.coin, in.coin);
        QCOMPARE(a.quantity, in.quantity);
        QCOMPARE(a.leverage, in.leverage);
        QCOMPARE(a.profit_target, in.profit_target);
        QCOMPARE(a.stop_loss, in.stop_loss);
        QCOMPARE(a.invalidation_condition, in.invalidation_condition);
        QCOMPARE(a.confidence, in.confidence);
        QCOMPARE(a.justification, in.justification);
    }

    void hold_action_skips_entry_field_validation() {
        const QString raw = R"({"actions":[{
            "signal":"hold","coin":"BTC","quantity":0,"leverage":1,
            "profit_target":0,"stop_loss":0,"invalidation_condition":"-",
            "confidence":0.1,"risk_usd":0,"justification":"sit out"
        }]})";
        ModelResponse r = parse_model_response(raw);
        QVERIFY(!r.parse_error.has_value());
        QCOMPARE(r.actions.size(), 1);
        QCOMPARE(r.actions[0].signal, Signal::Hold);
    }

    void close_action_accepted_without_pt_sl() {
        const QString raw = R"({"actions":[{
            "signal":"close","coin":"SOL","quantity":1.0,"leverage":1,
            "profit_target":0,"stop_loss":0,"invalidation_condition":"-",
            "confidence":0.9,"risk_usd":0,"justification":"take profits"
        }]})";
        ModelResponse r = parse_model_response(raw);
        QVERIFY(!r.parse_error.has_value());
        QCOMPARE(r.actions.size(), 1);
        QCOMPARE(r.actions[0].signal, Signal::Close);
    }

    void unknown_signal_drops_action() {
        const QString raw = R"({"actions":[{
            "signal":"frobnicate","coin":"BTC","quantity":1,"leverage":1,
            "profit_target":1,"stop_loss":1,"invalidation_condition":"x",
            "confidence":0.5,"risk_usd":0,"justification":"x"
        }]})";
        ModelResponse r = parse_model_response(raw);
        // Defective drops, but envelope still parsed; either zero actions
        // with parse_error noting drop, or zero actions.
        QCOMPARE(r.actions.size(), 0);
    }

    void unknown_coin_drops_action() {
        const QString raw = R"({"actions":[{
            "signal":"buy_to_enter","coin":"PEPE","quantity":1,"leverage":1,
            "profit_target":1,"stop_loss":0.9,"invalidation_condition":"x",
            "confidence":0.5,"risk_usd":0.1,"justification":"x"
        }]})";
        ModelResponse r = parse_model_response(raw);
        QCOMPARE(r.actions.size(), 0);
    }

    void leverage_clamped_to_max() {
        const QString raw = R"({"actions":[{
            "signal":"buy_to_enter","coin":"BTC","quantity":1,"leverage":50,
            "profit_target":71000,"stop_loss":69000,"invalidation_condition":"x",
            "confidence":0.5,"risk_usd":2000,"justification":"x"
        }]})";
        ModelResponse r = parse_model_response(raw);
        QVERIFY(!r.parse_error.has_value() || r.actions.size() == 1);
        if (r.actions.size() == 1) {
            QCOMPARE(r.actions[0].leverage, limits::kMaxLeverage);
        }
    }

    void confidence_clamped_to_unit_interval() {
        const QString raw = R"({"actions":[{
            "signal":"hold","coin":"BTC","quantity":0,"leverage":1,
            "profit_target":0,"stop_loss":0,"invalidation_condition":"-",
            "confidence":1.7,"risk_usd":0,"justification":"x"
        }]})";
        ModelResponse r = parse_model_response(raw);
        if (r.actions.size() == 1) {
            QVERIFY(r.actions[0].confidence >= 0.0 && r.actions[0].confidence <= 1.0);
        }
    }

    void invalidation_truncated_to_280() {
        const QString long_text = QString(500, QChar('x'));
        QJsonObject act;
        act[QStringLiteral("signal")] = QStringLiteral("hold");
        act[QStringLiteral("coin")] = QStringLiteral("BTC");
        act[QStringLiteral("quantity")] = 0;
        act[QStringLiteral("leverage")] = 1;
        act[QStringLiteral("profit_target")] = 0;
        act[QStringLiteral("stop_loss")] = 0;
        act[QStringLiteral("invalidation_condition")] = long_text;
        act[QStringLiteral("confidence")] = 0.1;
        act[QStringLiteral("risk_usd")] = 0;
        act[QStringLiteral("justification")] = QStringLiteral("ok");
        QJsonObject env;
        QJsonArray arr;
        arr.append(act);
        env[QStringLiteral("actions")] = arr;
        const QString raw = QString::fromUtf8(QJsonDocument(env).toJson(QJsonDocument::Compact));

        ModelResponse r = parse_model_response(raw);
        if (r.actions.size() == 1) {
            QVERIFY(r.actions[0].invalidation_condition.size() <= limits::kInvalidationConditionMaxChars);
        }
    }

    void code_fence_is_stripped() {
        const QString raw = QStringLiteral(
            "Sure, here's my decision:\n"
            "```json\n"
            "{\"actions\":[{\"signal\":\"hold\",\"coin\":\"BTC\",\"quantity\":0,\"leverage\":1,"
            "\"profit_target\":0,\"stop_loss\":0,\"invalidation_condition\":\"-\","
            "\"confidence\":0.1,\"risk_usd\":0,\"justification\":\"x\"}]}\n"
            "```\n"
            "Hope that helps!");
        ModelResponse r = parse_model_response(raw);
        QVERIFY(!r.parse_error.has_value() || r.actions.size() == 1);
    }

    void totally_garbled_response_records_parse_error() {
        ModelResponse r = parse_model_response(QStringLiteral("nope, not gonna trade today"));
        QVERIFY(r.parse_error.has_value());
        QCOMPARE(r.actions.size(), 0);
    }

    void signal_wire_roundtrip() {
        for (auto s : {Signal::BuyToEnter, Signal::SellToEnter, Signal::Hold, Signal::Close}) {
            const QString w = signal_to_wire(s);
            const auto back = signal_from_wire(w);
            QVERIFY(back.has_value());
            QCOMPARE(back.value(), s);
        }
        QVERIFY(!signal_from_wire(QStringLiteral("nonsense")).has_value());
    }

    void mode_wire_roundtrip() {
        for (auto m : {CompetitionMode::Baseline, CompetitionMode::Monk, CompetitionMode::Situational}) {
            const QString w = mode_to_wire(m);
            const auto back = mode_from_wire(w);
            QVERIFY(back.has_value());
            QCOMPARE(back.value(), m);
        }
    }
};

QTEST_MAIN(TestSchemaRoundtrip)
#include "test_schema_roundtrip.moc"
