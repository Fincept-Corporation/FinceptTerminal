// test_validator.cpp — Phase 3 validator unit tests
//
// Covers the dual-shape contract (legacy `properties` + structured `params`),
// default injection, required check, type / enum / range / pattern enforcement,
// and unknown-key tolerance. Uses Qt Test.

#include "mcp/McpTypes.h"
#include "mcp/SchemaValidator.h"
#include "mcp/ToolSchemaBuilder.h"

#include <QJsonArray>
#include <QObject>
#include <QTest>

using fincept::mcp::ToolSchema;
using fincept::mcp::ToolSchemaBuilder;
using fincept::mcp::validate_args;

class TestValidator : public QObject {
    Q_OBJECT

  private slots:

    // ── Required ────────────────────────────────────────────────────────

    void missingRequired_fails() {
        ToolSchema s = ToolSchemaBuilder()
                           .string("symbol", "Ticker").required()
                           .build();
        QJsonObject args{};
        auto r = validate_args(s, args);
        QVERIFY(r.is_err());
        QVERIFY(QString::fromStdString(r.error()).contains("symbol"));
    }

    void requiredPresent_succeeds() {
        ToolSchema s = ToolSchemaBuilder()
                           .string("symbol", "Ticker").required()
                           .build();
        QJsonObject args{{"symbol", "AAPL"}};
        QVERIFY(validate_args(s, args).is_ok());
    }

    // ── Type checks ─────────────────────────────────────────────────────

    void wrongType_fails() {
        ToolSchema s = ToolSchemaBuilder()
                           .integer("limit", "Max items")
                           .build();
        QJsonObject args{{"limit", "twenty"}}; // string instead of integer
        auto r = validate_args(s, args);
        QVERIFY(r.is_err());
        QVERIFY(QString::fromStdString(r.error()).contains("limit"));
        QVERIFY(QString::fromStdString(r.error()).contains("integer"));
    }

    void integerRejectsFractional() {
        ToolSchema s = ToolSchemaBuilder()
                           .integer("limit", "Max items")
                           .build();
        QJsonObject args{{"limit", 3.5}};
        QVERIFY(validate_args(s, args).is_err());
    }

    void numberAcceptsFractional() {
        ToolSchema s = ToolSchemaBuilder()
                           .number("price", "Price")
                           .build();
        QJsonObject args{{"price", 1.23}};
        QVERIFY(validate_args(s, args).is_ok());
    }

    // ── Enum ────────────────────────────────────────────────────────────

    void enumViolation_fails() {
        ToolSchema s = ToolSchemaBuilder()
                           .string("side", "buy or sell").required()
                               .enums({"buy", "sell"})
                           .build();
        QJsonObject args{{"side", "long"}};
        auto r = validate_args(s, args);
        QVERIFY(r.is_err());
        QVERIFY(QString::fromStdString(r.error()).contains("side"));
    }

    void enumMatch_succeeds() {
        ToolSchema s = ToolSchemaBuilder()
                           .string("side", "buy or sell").required()
                               .enums({"buy", "sell"})
                           .build();
        QJsonObject args{{"side", "buy"}};
        QVERIFY(validate_args(s, args).is_ok());
    }

    // ── Range / length / pattern ────────────────────────────────────────

    void numberOutOfRange_fails() {
        ToolSchema s = ToolSchemaBuilder()
                           .integer("limit", "Limit").between(1, 100)
                           .build();
        QJsonObject args1{{"limit", 0}};
        QVERIFY(validate_args(s, args1).is_err());

        QJsonObject args2{{"limit", 999}};
        QVERIFY(validate_args(s, args2).is_err());

        QJsonObject args3{{"limit", 50}};
        QVERIFY(validate_args(s, args3).is_ok());
    }

    void stringLength_enforced() {
        ToolSchema s = ToolSchemaBuilder()
                           .string("title", "Title").length(1, 5)
                           .build();
        QJsonObject too_short{{"title", ""}};
        QVERIFY(validate_args(s, too_short).is_err());

        QJsonObject too_long{{"title", "abcdef"}};
        QVERIFY(validate_args(s, too_long).is_err());

        QJsonObject ok{{"title", "abc"}};
        QVERIFY(validate_args(s, ok).is_ok());
    }

    void patternMismatch_fails() {
        ToolSchema s = ToolSchemaBuilder()
                           .string("symbol", "Ticker").pattern("^[A-Z]{1,5}$")
                           .build();
        QJsonObject bad{{"symbol", "lowercase"}};
        QVERIFY(validate_args(s, bad).is_err());

        QJsonObject good{{"symbol", "AAPL"}};
        QVERIFY(validate_args(s, good).is_ok());
    }

    // ── Defaults ────────────────────────────────────────────────────────

    void defaultsInjectedWhenAbsent() {
        ToolSchema s = ToolSchemaBuilder()
                           .string("category", "Filter")
                               .default_str("ALL").enums({"ALL", "MARKETS"})
                           .integer("limit", "Max").default_int(20)
                           .boolean("force", "Force").default_bool(false)
                           .build();
        QJsonObject args{};
        QVERIFY(validate_args(s, args).is_ok());
        QCOMPARE(args["category"].toString(), QString("ALL"));
        QCOMPARE(args["limit"].toInt(), 20);
        QCOMPARE(args["force"].toBool(), false);
    }

    void defaultsDontOverrideCallerValues() {
        ToolSchema s = ToolSchemaBuilder()
                           .integer("limit", "Max").default_int(20)
                           .build();
        QJsonObject args{{"limit", 50}};
        QVERIFY(validate_args(s, args).is_ok());
        QCOMPARE(args["limit"].toInt(), 50);
    }

    // ── Optional + no default ───────────────────────────────────────────

    void optionalAbsentNoDefault_succeeds() {
        ToolSchema s = ToolSchemaBuilder()
                           .string("query", "Optional search")
                           .build();
        QJsonObject args{};
        QVERIFY(validate_args(s, args).is_ok());
        QVERIFY(!args.contains("query"));
    }

    // ── Unknown keys ────────────────────────────────────────────────────

    void unknownKey_isAllowed() {
        ToolSchema s = ToolSchemaBuilder()
                           .string("symbol", "Ticker").required()
                           .build();
        QJsonObject args{{"symbol", "AAPL"}, {"_meta", "extra"}};
        QVERIFY(validate_args(s, args).is_ok());
    }

    // ── Legacy `properties` shape ───────────────────────────────────────

    void legacyShape_validatesType() {
        ToolSchema s;
        s.properties = QJsonObject{
            {"limit", QJsonObject{{"type", "integer"}, {"description", "Max"}}}};
        QJsonObject bad{{"limit", "not a number"}};
        QVERIFY(validate_args(s, bad).is_err());

        QJsonObject good{{"limit", 10}};
        QVERIFY(validate_args(s, good).is_ok());
    }

    void legacyShape_validatesRequired() {
        ToolSchema s;
        s.properties = QJsonObject{
            {"symbol", QJsonObject{{"type", "string"}}}};
        s.required = {"symbol"};
        QJsonObject empty{};
        QVERIFY(validate_args(s, empty).is_err());

        QJsonObject good{{"symbol", "AAPL"}};
        QVERIFY(validate_args(s, good).is_ok());
    }

    void legacyShape_picksUpEnum() {
        ToolSchema s;
        QJsonArray allowed{"buy", "sell"};
        s.properties = QJsonObject{
            {"side", QJsonObject{{"type", "string"}, {"enum", allowed}}}};
        QJsonObject bad{{"side", "long"}};
        QVERIFY(validate_args(s, bad).is_err());
    }

    // ── to_json output ──────────────────────────────────────────────────

    void toJson_includesEnumAndDefault() {
        ToolSchema s = ToolSchemaBuilder()
                           .string("category", "Filter")
                               .default_str("ALL").enums({"ALL", "MARKETS"})
                           .build();
        QJsonObject j = s.to_json();
        QJsonObject category = j["properties"].toObject()["category"].toObject();
        QCOMPARE(category["default"].toString(), QString("ALL"));
        QJsonArray e = category["enum"].toArray();
        QCOMPARE(e.size(), 2);
    }

    void toJson_mergesLegacyAndStructured() {
        ToolSchema s;
        s.properties = QJsonObject{
            {"old_field", QJsonObject{{"type", "string"}}}};
        s.params.insert("new_field", []() {
            fincept::mcp::ToolParam p;
            p.type = "integer";
            p.required = true;
            return p;
        }());

        QJsonObject j = s.to_json();
        QJsonObject props = j["properties"].toObject();
        QVERIFY(props.contains("old_field"));
        QVERIFY(props.contains("new_field"));

        QJsonArray req = j["required"].toArray();
        QCOMPARE(req.size(), 1);
        QCOMPARE(req[0].toString(), QString("new_field"));
    }
};

QTEST_MAIN(TestValidator)
#include "test_validator.moc"
