// tst_mcp_ui_catalog.cpp — the ui_* tool catalogue (src/mcp/core/CoreToolCatalog.cpp)
// and the argument validator every call goes through (src/mcp/SchemaValidator.cpp).
//
// The catalogue rides on every request of every harness turn, so its size is a
// budget: a token regression fails here. Its schemas must also be accepted by
// strict clients (plain JSON Schema: type object, properties, required, typed
// properties, string enums, no oneOf / anyOf / allOf / $ref, names
// ^[A-Za-z0-9_]+$), and each tool's inputSchema must stay under Codex's per-tool
// limit (5,000 bytes; we hold 4,500).

#include "mcp/SchemaValidator.h"
#include "mcp/core/CoreToolCatalog.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTest>

using namespace fincept::mcp;
using namespace fincept::mcp::core;

namespace {

// Measured 2026-11-17 for the nine ui_* tools: 8,535 bytes (9,544 before this
// work) (name + description + compact
// schema, as ToolSelfTest counts the whole catalogue). Trim the catalogue when
// a change needs more; do not raise these.
constexpr int kUiCatalogueBudgetBytes = 8700;
constexpr int kPerToolSchemaBudgetBytes = 4500;

int compact_size(const QJsonObject& o) {
    return static_cast<int>(QJsonDocument(o).toJson(QJsonDocument::Compact).size());
}

/// Every problem a strict client would have with a schema fragment.
void strict_problems(const QJsonValue& v, const QString& where, QStringList* out) {
    if (!v.isObject()) {
        if (v.isArray()) {
            const QJsonArray a = v.toArray();
            for (int i = 0; i < a.size(); ++i)
                strict_problems(a.at(i), where + QStringLiteral("[%1]").arg(i), out);
        }
        return;
    }
    const QJsonObject o = v.toObject();
    for (const char* banned : {"oneOf", "anyOf", "allOf", "$ref", "not"}) {
        if (o.contains(QString::fromLatin1(banned)))
            *out << where + QStringLiteral(": uses ") + QString::fromLatin1(banned);
    }
    if (o.contains(QStringLiteral("enum"))) {
        for (const QJsonValue& e : o.value(QStringLiteral("enum")).toArray()) {
            if (!e.isString())
                *out << where + QStringLiteral(": non-string enum value");
        }
    }
    if (o.value(QStringLiteral("type")).toString() == QLatin1String("object") && o.contains(QStringLiteral("properties"))) {
        static const QRegularExpression name_rx(QStringLiteral("^[A-Za-z0-9_]{1,64}$"));
        const QJsonObject props = o.value(QStringLiteral("properties")).toObject();
        for (auto it = props.constBegin(); it != props.constEnd(); ++it) {
            if (!name_rx.match(it.key()).hasMatch())
                *out << where + QStringLiteral(": bad property name ") + it.key();
            const QJsonObject p = it.value().toObject();
            if (!p.contains(QStringLiteral("type")))
                *out << where + QStringLiteral(".") + it.key() + QStringLiteral(": untyped");
            if (p.value(QStringLiteral("type")).toString() == QLatin1String("array") &&
                !p.value(QStringLiteral("items")).toObject().contains(QStringLiteral("type")))
                *out << where + QStringLiteral(".") + it.key() + QStringLiteral(": array items untyped");
            strict_problems(it.value(), where + QStringLiteral(".") + it.key(), out);
        }
        for (const QJsonValue& r : o.value(QStringLiteral("required")).toArray()) {
            if (!props.contains(r.toString()))
                *out << where + QStringLiteral(": required '") + r.toString() + QStringLiteral("' not declared");
        }
    }
    if (o.contains(QStringLiteral("items")))
        strict_problems(o.value(QStringLiteral("items")), where + QStringLiteral(".items"), out);
}

const UiToolSpec* find(const std::vector<UiToolSpec>& specs, const QString& name) {
    for (const UiToolSpec& s : specs) {
        if (s.name == name)
            return &s;
    }
    return nullptr;
}

} // namespace

class TestMcpUiCatalog : public QObject {
    Q_OBJECT

  private slots:
    void the_nine_ui_tools_in_order() {
        const auto specs = ui_tool_specs();
        QStringList names;
        for (const UiToolSpec& s : specs)
            names << s.name;
        QCOMPARE(names, (QStringList{QStringLiteral("ui_overview"), QStringLiteral("ui_open"),
                                     QStringLiteral("ui_snapshot"), QStringLiteral("ui_act"),
                                     QStringLiteral("ui_read"), QStringLiteral("ui_wait"),
                                     QStringLiteral("ui_screenshot"), QStringLiteral("ui_workspace"),
                                     QStringLiteral("ui_invoke")}));
        for (const UiToolSpec& s : specs) {
            QVERIFY2(s.category, qPrintable(s.name));
            QVERIFY2(s.timeout_ms > 0 && s.timeout_ms <= 180000, qPrintable(s.name));
            const int len = static_cast<int>(s.description.size());
            QVERIFY2(len >= 40 && len <= 700, qPrintable(QStringLiteral("%1: %2 chars").arg(s.name).arg(len)));
        }
    }

    void schemas_are_valid_for_strict_clients() {
        for (const UiToolSpec& s : ui_tool_specs()) {
            const QJsonObject schema = s.schema.to_json();
            QCOMPARE(schema.value(QStringLiteral("type")).toString(), QStringLiteral("object"));
            QVERIFY(schema.value(QStringLiteral("properties")).isObject());
            QStringList problems;
            strict_problems(schema, s.name, &problems);
            QVERIFY2(problems.isEmpty(), qPrintable(problems.join(QStringLiteral("; "))));
        }
    }

    void catalogue_stays_within_its_byte_budget() {
        int total = 0;
        QStringList sizes;
        for (const UiToolSpec& s : ui_tool_specs()) {
            const int schema_bytes = compact_size(s.schema.to_json());
            const int desc_bytes = static_cast<int>(s.description.toUtf8().size());
            total += schema_bytes + desc_bytes + static_cast<int>(s.name.size());
            sizes << QStringLiteral("%1 schema %2 + description %3").arg(s.name).arg(schema_bytes).arg(desc_bytes);
            QVERIFY2(schema_bytes < kPerToolSchemaBudgetBytes,
                     qPrintable(QStringLiteral("%1 inputSchema is %2 bytes").arg(s.name).arg(schema_bytes)));
        }
        for (const QString& line : sizes)
            qInfo("%s", qPrintable(line));
        qInfo("ui_* catalogue: %d bytes (budget %d)", total, kUiCatalogueBudgetBytes);
        QVERIFY2(total <= kUiCatalogueBudgetBytes, "the ui_* catalogue outgrew its budget — trim it");
    }

    void validator_is_forgiving_where_it_is_safe() {
        const auto specs = ui_tool_specs();
        const ToolSchema act = find(specs, QStringLiteral("ui_act"))->schema;
        const ToolSchema read = find(specs, QStringLiteral("ui_read"))->schema;
        const ToolSchema snapshot = find(specs, QStringLiteral("ui_snapshot"))->schema;

        // An enum value in another case → the declared spelling.
        QJsonObject a{{QStringLiteral("action"), QStringLiteral("Click")}, {QStringLiteral("ref"), QStringLiteral("e1")}};
        QVERIFY(validate_args(act, a).is_ok());
        QCOMPARE(a.value(QStringLiteral("action")).toString(), QStringLiteral("click"));
        // A number for the string `value` → its text.
        QJsonObject v{{QStringLiteral("action"), QStringLiteral("set_value")},
                      {QStringLiteral("ref"), QStringLiteral("e1")},
                      {QStringLiteral("value"), 5}};
        QVERIFY(validate_args(act, v).is_ok());
        QCOMPARE(v.value(QStringLiteral("value")).toString(), QStringLiteral("5"));
        // A lone step object → a list of one.
        QJsonObject st{{QStringLiteral("steps"),
                        QJsonObject{{QStringLiteral("action"), QStringLiteral("wait")}, {QStringLiteral("until"), QStringLiteral("idle")}}}};
        QVERIFY(validate_args(act, st).is_ok());
        QVERIFY(st.value(QStringLiteral("steps")).isArray());
        // format=JSON → json.
        QJsonObject r{{QStringLiteral("ref"), QStringLiteral("e1")}, {QStringLiteral("format"), QStringLiteral("JSON")}};
        QVERIFY(validate_args(read, r).is_ok());
        QCOMPARE(r.value(QStringLiteral("format")).toString(), QStringLiteral("json"));
        // A null optional parameter is "not given".
        QJsonObject n{{QStringLiteral("target"), QJsonValue::Null}, {QStringLiteral("since"), QStringLiteral("s3")}};
        QVERIFY(validate_args(snapshot, n).is_ok());
        QVERIFY(!n.contains(QStringLiteral("target")));
    }

    void validator_still_refuses_what_is_wrong() {
        const auto specs = ui_tool_specs();
        const ToolSchema act = find(specs, QStringLiteral("ui_act"))->schema;
        const ToolSchema read = find(specs, QStringLiteral("ui_read"))->schema;
        QJsonObject unknown{{QStringLiteral("action"), QStringLiteral("click")}, {QStringLiteral("bogus"), 1}};
        QVERIFY(validate_args(act, unknown).is_err());
        QJsonObject bad_enum{{QStringLiteral("action"), QStringLiteral("teleport")}};
        QVERIFY(validate_args(act, bad_enum).is_err());
        QJsonObject missing;
        QVERIFY(validate_args(read, missing).is_err());
        QJsonObject range{{QStringLiteral("ref"), QStringLiteral("e1")}, {QStringLiteral("limit"), 0}};
        QVERIFY(validate_args(read, range).is_err());
    }
};

QTEST_MAIN(TestMcpUiCatalog)
#include "tst_mcp_ui_catalog.moc"
