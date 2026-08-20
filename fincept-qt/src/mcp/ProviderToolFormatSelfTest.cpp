// ProviderToolFormatSelfTest.cpp — see ProviderToolFormatSelfTest.h.

#include "mcp/ProviderToolFormatSelfTest.h"

#include "mcp/GeminiSchema.h"
#include "mcp/McpService.h"
#include "services/llm/ProviderCatalog.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QString>
#include <QStringList>

#include <cstdio>

namespace fincept::mcp {
namespace {

void ptf_out(const QString& line) {
    std::printf("%s\n", line.toUtf8().constData());
    std::fflush(stdout);
}

// Shared by OpenAI and Anthropic: both document the function/tool name as
// ^[a-zA-Z0-9_-]{1,64}$.
const QRegularExpression& ptf_openai_name_rx() {
    static const QRegularExpression rx(QStringLiteral("^[a-zA-Z0-9_-]{1,64}$"));
    return rx;
}

// Keys Gemini's Schema message defines. Kept independent of the whitelist in
// GeminiSchema.cpp on purpose: if the two disagree, the test should fail
// rather than agree with the implementation it is checking.
const QSet<QString>& ptf_gemini_schema_keys() {
    static const QSet<QString> k = {
        QStringLiteral("type"),          QStringLiteral("format"),        QStringLiteral("title"),
        QStringLiteral("description"),   QStringLiteral("nullable"),      QStringLiteral("enum"),
        QStringLiteral("items"),         QStringLiteral("properties"),    QStringLiteral("required"),
        QStringLiteral("minItems"),      QStringLiteral("maxItems"),      QStringLiteral("minProperties"),
        QStringLiteral("maxProperties"), QStringLiteral("minLength"),     QStringLiteral("maxLength"),
        QStringLiteral("pattern"),       QStringLiteral("minimum"),       QStringLiteral("maximum"),
        QStringLiteral("default"),       QStringLiteral("anyOf"),         QStringLiteral("example"),
        QStringLiteral("propertyOrdering"),
    };
    return k;
}

const QSet<QString>& ptf_gemini_types() {
    static const QSet<QString> k = {
        QStringLiteral("string"), QStringLiteral("number"), QStringLiteral("integer"),
        QStringLiteral("boolean"), QStringLiteral("array"), QStringLiteral("object"),
    };
    return k;
}

// Walk a sanitised Gemini schema and record every way the API would reject it.
void ptf_check_gemini_schema(const QJsonObject& schema, const QString& path, QStringList* errs) {
    for (auto it = schema.constBegin(); it != schema.constEnd(); ++it) {
        if (!ptf_gemini_schema_keys().contains(it.key()))
            *errs << QStringLiteral("%1: unsupported key '%2'").arg(path, it.key());
    }

    const QString type = schema.value(QStringLiteral("type")).toString();
    if (type.isEmpty())
        *errs << QStringLiteral("%1: missing 'type'").arg(path);
    else if (!ptf_gemini_types().contains(type))
        *errs << QStringLiteral("%1: type '%2' is not a Gemini Type").arg(path, type);

    if (schema.contains(QStringLiteral("enum")) && type != QLatin1String("string"))
        *errs << QStringLiteral("%1: 'enum' is only valid on a STRING schema (got '%2')").arg(path, type);

    if (type == QLatin1String("object")) {
        const QJsonObject props = schema.value(QStringLiteral("properties")).toObject();
        // The failure that broke Gemini tool calling outright: an OBJECT with
        // an empty properties map is rejected with "should be non-empty for
        // OBJECT type", and it takes the whole request with it.
        if (props.isEmpty()) {
            *errs << QStringLiteral("%1: OBJECT with empty 'properties' — Gemini rejects the entire request").arg(path);
        }
        for (const auto& r : schema.value(QStringLiteral("required")).toArray()) {
            if (!props.contains(r.toString()))
                *errs << QStringLiteral("%1: required '%2' is not a declared property").arg(path, r.toString());
        }
        for (auto pit = props.constBegin(); pit != props.constEnd(); ++pit) {
            ptf_check_gemini_schema(pit.value().toObject(), path + QStringLiteral(".") + pit.key(), errs);
        }
    }

    if (type == QLatin1String("array")) {
        if (!schema.contains(QStringLiteral("items")))
            *errs << QStringLiteral("%1: ARRAY without 'items'").arg(path);
        else
            ptf_check_gemini_schema(schema.value(QStringLiteral("items")).toObject(),
                                    path + QStringLiteral("[]"), errs);
    }
}

void ptf_report(const QString& label, const QStringList& errs, bool* failed) {
    if (errs.isEmpty()) {
        ptf_out(QStringLiteral("    OK   ") + label);
        return;
    }
    *failed = true;
    ptf_out(QStringLiteral("    FAIL ") + label + QStringLiteral(": ") + QString::number(errs.size()));
    int shown = 0;
    for (const auto& e : errs) {
        ptf_out(QStringLiteral("           - ") + e);
        if (++shown >= 20) {
            ptf_out(QStringLiteral("           … and %1 more").arg(errs.size() - shown));
            break;
        }
    }
}

} // namespace

int run_provider_tool_format_selftest() {
    ptf_out(QStringLiteral("\n=============================================================="));
    ptf_out(QStringLiteral("  LLM PROVIDER TOOL-FORMAT SELF-TEST"));
    ptf_out(QStringLiteral("=============================================================="));

    bool failed = false;
    auto& svc = McpService::instance();
    const ToolFilter default_filter;

    // Every dialect is exercised twice: once as the opening turn (no activated
    // tools) and once mid-turn with a discovered tool fed back, because the
    // activation path has its own cache key and its own history of drift.
    const QSet<QString> activated = {QStringLiteral("create_portfolio"), QStringLiteral("get_quote")};

    // ── OpenAI ──────────────────────────────────────────────────────────
    {
        const QJsonArray tools = svc.format_tools_for_openai(default_filter, activated);
        ptf_out(QStringLiteral("\n[1] OPENAI — %1 function declarations").arg(tools.size()));

        QStringList errs;
        if (tools.isEmpty())
            errs << QStringLiteral("empty tool array — the model would have no tools at all");
        QSet<QString> seen;
        for (const auto& v : tools) {
            const QJsonObject e = v.toObject();
            if (e.value(QStringLiteral("type")).toString() != QLatin1String("function"))
                errs << QStringLiteral("entry missing type=function");
            const QJsonObject fn = e.value(QStringLiteral("function")).toObject();
            const QString name = fn.value(QStringLiteral("name")).toString();
            if (!ptf_openai_name_rx().match(name).hasMatch())
                errs << QStringLiteral("'%1' violates ^[a-zA-Z0-9_-]{1,64}$").arg(name);
            if (seen.contains(name))
                errs << QStringLiteral("duplicate function name '%1'").arg(name);
            seen.insert(name);
            if (fn.value(QStringLiteral("description")).toString().trimmed().isEmpty())
                errs << QStringLiteral("'%1' has no description").arg(name);
            if (!fn.contains(QStringLiteral("parameters")))
                errs << QStringLiteral("'%1' has no parameters object").arg(name);
        }
        ptf_report(QStringLiteral("openai payload"), errs, &failed);
    }

    // ── Anthropic ───────────────────────────────────────────────────────
    {
        const QJsonArray tools = svc.format_tools_for_anthropic(default_filter, activated);
        ptf_out(QStringLiteral("\n[2] ANTHROPIC — %1 tools").arg(tools.size()));

        QStringList errs;
        if (tools.isEmpty())
            errs << QStringLiteral("empty tool array — the model would have no tools at all");
        for (const auto& v : tools) {
            const QJsonObject t = v.toObject();
            const QString name = t.value(QStringLiteral("name")).toString();
            if (!ptf_openai_name_rx().match(name).hasMatch())
                errs << QStringLiteral("'%1' violates ^[a-zA-Z0-9_-]{1,64}$").arg(name);
            // Anthropic takes a bare tool object — an OpenAI-style
            // {"type":"function","function":{…}} wrapper is a 400.
            if (t.contains(QStringLiteral("type")) || t.contains(QStringLiteral("function")))
                errs << QStringLiteral("'%1' carries an OpenAI-style wrapper").arg(name);
            const QJsonObject schema = t.value(QStringLiteral("input_schema")).toObject();
            if (schema.isEmpty())
                errs << QStringLiteral("'%1' has no input_schema").arg(name);
            else if (schema.value(QStringLiteral("type")).toString() != QLatin1String("object"))
                errs << QStringLiteral("'%1' input_schema.type != object").arg(name);
            else if (!schema.contains(QStringLiteral("properties")))
                errs << QStringLiteral("'%1' input_schema has no properties map").arg(name);
        }
        ptf_report(QStringLiteral("anthropic payload"), errs, &failed);

        // The whole point of routing Anthropic through the shared selector: it
        // must see the same tools the OpenAI path does, not a different slice.
        const QJsonArray oai = svc.format_tools_for_openai(default_filter, activated);
        QStringList parity;
        if (tools.size() != oai.size()) {
            parity << QStringLiteral("anthropic advertises %1 tools, openai %2 — the two dialects "
                                     "are not selecting from the same set")
                          .arg(tools.size())
                          .arg(oai.size());
        }
        ptf_report(QStringLiteral("anthropic/openai selection parity"), parity, &failed);
    }

    // ── Gemini ──────────────────────────────────────────────────────────
    {
        const QJsonArray tools = svc.format_tools_for_gemini(default_filter, activated);
        const QJsonArray decls =
            tools.isEmpty() ? QJsonArray{}
                            : tools[0].toObject().value(QStringLiteral("functionDeclarations")).toArray();
        ptf_out(QStringLiteral("\n[3] GEMINI — %1 function declarations").arg(decls.size()));

        QStringList errs;
        if (decls.isEmpty())
            errs << QStringLiteral("empty functionDeclarations — the model would have no tools at all");
        // Gemini rejects a request carrying more than 128 declarations.
        if (decls.size() > 128)
            errs << QStringLiteral("%1 declarations exceeds Gemini's limit of 128").arg(decls.size());

        int with_params = 0;
        for (const auto& v : decls) {
            const QJsonObject d = v.toObject();
            const QString name = d.value(QStringLiteral("name")).toString();
            if (!is_valid_gemini_function_name(name))
                errs << QStringLiteral("'%1' violates ^[a-zA-Z_][a-zA-Z0-9_.:-]{0,63}$").arg(name);
            if (d.value(QStringLiteral("description")).toString().trimmed().isEmpty())
                errs << QStringLiteral("'%1' has no description").arg(name);
            if (d.contains(QStringLiteral("parameters"))) {
                ++with_params;
                ptf_check_gemini_schema(d.value(QStringLiteral("parameters")).toObject(),
                                        name + QStringLiteral(".parameters"), &errs);
            }
        }
        ptf_out(QStringLiteral("    %1 with parameters, %2 parameterless (parameters omitted, as Gemini requires)")
                    .arg(with_params)
                    .arg(decls.size() - with_params));
        ptf_report(QStringLiteral("gemini payload"), errs, &failed);
    }

    // ── Sanitiser unit checks ───────────────────────────────────────────
    // Concrete regressions rather than catalogue-wide sweeps: each of these is
    // a shape a real MCP server can register and every one of them used to be
    // forwarded to Gemini verbatim.
    {
        ptf_out(QStringLiteral("\n[4] GEMINI SCHEMA TRANSLATION"));
        QStringList errs;

        // Parameterless tool → no usable schema, caller must omit `parameters`.
        if (sanitize_schema_for_gemini(QJsonObject{{"type", "object"}, {"properties", QJsonObject{}}}).has_value())
            errs << QStringLiteral("empty-properties object should sanitise to nullopt");

        // JSON Schema keywords with no Gemini equivalent must be dropped, not forwarded.
        const QJsonObject noisy{
            {"type", "object"},
            {"$schema", "https://json-schema.org/draft/2020-12/schema"},
            {"additionalProperties", false},
            {"properties",
             QJsonObject{
                 {"sym", QJsonObject{{"type", "string"}, {"format", "uri"}, {"pattern", "^[A-Z]+$"}}},
                 {"n", QJsonObject{{"type", "integer"}, {"exclusiveMinimum", 0}, {"enum", QJsonArray{1, 2}}}},
                 {"tags", QJsonObject{{"type", "array"}}},
                 {"empty", QJsonObject{{"type", "object"}, {"properties", QJsonObject{}}}},
             }},
            {"required", QJsonArray{"sym", "empty", "ghost"}}};
        const auto cleaned = sanitize_schema_for_gemini(noisy);
        if (!cleaned) {
            errs << QStringLiteral("a schema with real properties should not sanitise away");
        } else {
            ptf_check_gemini_schema(*cleaned, QStringLiteral("noisy"), &errs);
            const QJsonObject props = cleaned->value(QStringLiteral("properties")).toObject();
            if (props.contains(QStringLiteral("empty")))
                errs << QStringLiteral("property with an unusable object schema should have been dropped");
            if (!props.value(QStringLiteral("tags")).toObject().contains(QStringLiteral("items")))
                errs << QStringLiteral("ARRAY property should have been given a default 'items'");
            if (props.value(QStringLiteral("sym")).toObject().contains(QStringLiteral("format")))
                errs << QStringLiteral("string format 'uri' is not in Gemini's vocabulary and should be dropped");
            const QStringList req = [&] {
                QStringList r;
                for (const auto& v : cleaned->value(QStringLiteral("required")).toArray())
                    r << v.toString();
                return r;
            }();
            if (req.contains(QStringLiteral("ghost")) || req.contains(QStringLiteral("empty")))
                errs << QStringLiteral("'required' still names a property that is not declared: ") + req.join(',');
        }

        // A ["string","null"] union is legal JSON Schema and illegal here.
        const auto nullable = sanitize_schema_for_gemini(
            QJsonObject{{"type", "object"},
                        {"properties", QJsonObject{{"x", QJsonObject{{"type", QJsonArray{"string", "null"}}}}}}});
        if (!nullable) {
            errs << QStringLiteral("nullable-union schema should survive sanitisation");
        } else {
            const QJsonObject x = nullable->value(QStringLiteral("properties")).toObject()
                                      .value(QStringLiteral("x")).toObject();
            if (x.value(QStringLiteral("type")).toString() != QLatin1String("string"))
                errs << QStringLiteral("type union should collapse to its non-null member");
            if (!x.value(QStringLiteral("nullable")).toBool())
                errs << QStringLiteral("type union should set nullable=true");
        }

        ptf_report(QStringLiteral("sanitiser behaviour"), errs, &failed);
    }

    // ── Endpoint composition ────────────────────────────────────────────
    // A tool payload is only half the contract; it still has to reach the
    // route that speaks that dialect. Gemini is the trap: its body is the only
    // non-OpenAI shape, so a user-supplied base_url that gets "/chat/completions"
    // appended sends a native `contents` body to an OpenAI-compat route and
    // 400s — tools included. ProviderCatalog::chat_endpoint mirrors
    // LlmService::get_endpoint_url; both must agree with these cases.
    {
        ptf_out(QStringLiteral("\n[5] ENDPOINT COMPOSITION"));
        QStringList errs;
        struct Case {
            const char* provider;
            const char* base_url;
            const char* model;
            const char* expected;
        };
        static const Case kCases[] = {
            // Defaults.
            {"openai", "", "gpt-5", "https://api.openai.com/v1/chat/completions"},
            {"anthropic", "", "claude-sonnet-5", "https://api.anthropic.com/v1/messages"},
            {"gemini", "", "gemini-2.5-pro",
             "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-pro:generateContent"},
            // Custom OpenAI-compatible endpoints — with and without a version segment.
            {"openrouter", "https://proxy.local", "x", "https://proxy.local/v1/chat/completions"},
            {"openrouter", "https://proxy.local/v1", "x", "https://proxy.local/v1/chat/completions"},
            {"openrouter", "https://proxy.local/v1/chat/completions", "x",
             "https://proxy.local/v1/chat/completions"},
            {"anthropic", "https://proxy.local", "x", "https://proxy.local/v1/messages"},
            // Gemini behind a proxy / regional host must stay on the native path.
            {"gemini", "https://gw.example.com", "gemini-2.5-flash",
             "https://gw.example.com/v1beta/models/gemini-2.5-flash:generateContent"},
            {"gemini", "https://gw.example.com/v1beta", "gemini-2.5-flash",
             "https://gw.example.com/v1beta/models/gemini-2.5-flash:generateContent"},
            {"gemini", "https://gw.example.com/v1beta/", "gemini-2.5-flash",
             "https://gw.example.com/v1beta/models/gemini-2.5-flash:generateContent"},
        };
        for (const auto& c : kCases) {
            const QString got = ai_chat::ProviderCatalog::chat_endpoint(QString::fromUtf8(c.provider),
                                                                       QString::fromUtf8(c.base_url),
                                                                       QString::fromUtf8(c.model));
            const QString want = QString::fromUtf8(c.expected);
            if (got != want) {
                errs << QStringLiteral("%1 base='%2' -> '%3' (expected '%4')")
                            .arg(QString::fromUtf8(c.provider), QString::fromUtf8(c.base_url), got, want);
            }
        }
        ptf_report(QStringLiteral("chat endpoint per provider"), errs, &failed);
    }

    ptf_out(QStringLiteral("\n--------------------------------------------------------------"));
    ptf_out(failed ? QStringLiteral("  RESULT: FAIL") : QStringLiteral("  RESULT: PASS"));
    ptf_out(QStringLiteral("--------------------------------------------------------------\n"));
    return failed ? 1 : 0;
}

} // namespace fincept::mcp
