// ProviderToolFormatSelfTest.cpp — see ProviderToolFormatSelfTest.h.

#include "mcp/ProviderToolFormatSelfTest.h"

#include "mcp/GeminiSchema.h"
#include "mcp/McpProvider.h"
#include "mcp/McpService.h"
#include "services/llm/ProviderCatalog.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QString>
#include <QStringList>

#include <vector>

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

// ── Per-declaration checks, shared by the default-turn sections and the
// catalogue-wide sweep ──────────────────────────────────────────────────────
//
// Factored out rather than written twice on purpose. The default-turn sections
// and the sweep have to agree on what "valid" means: if they drift, the sweep stops
// being a superset and the coverage it exists to provide is imaginary.

// One OpenAI `tools[]` entry: {"type":"function","function":{name,description,parameters}}.
void ptf_check_openai_entry(const QJsonObject& entry, QSet<QString>* seen, QStringList* errs) {
    if (entry.value(QStringLiteral("type")).toString() != QLatin1String("function"))
        *errs << QStringLiteral("entry missing type=function");
    const QJsonObject fn = entry.value(QStringLiteral("function")).toObject();
    const QString name = fn.value(QStringLiteral("name")).toString();
    if (!ptf_openai_name_rx().match(name).hasMatch())
        *errs << QStringLiteral("'%1' violates ^[a-zA-Z0-9_-]{1,64}$").arg(name);
    if (seen) {
        if (seen->contains(name))
            *errs << QStringLiteral("duplicate function name '%1'").arg(name);
        seen->insert(name);
    }
    if (fn.value(QStringLiteral("description")).toString().trimmed().isEmpty())
        *errs << QStringLiteral("'%1' has no description").arg(name);
    if (!fn.contains(QStringLiteral("parameters"))) {
        *errs << QStringLiteral("'%1' has no parameters object").arg(name);
        return;
    }
    // `parameters` is a JSON Schema object. OpenAI itself tolerates a bare
    // {"type":"object"}, but the OpenAI-compatible fleet (Kimi, Groq, MiniMax,
    // Ollama, …) is less forgiving, and every one of them is reached through
    // this same payload.
    const QJsonObject params = fn.value(QStringLiteral("parameters")).toObject();
    if (params.value(QStringLiteral("type")).toString() != QLatin1String("object"))
        *errs << QStringLiteral("'%1' parameters.type != object").arg(name);
    if (!params.contains(QStringLiteral("properties")))
        *errs << QStringLiteral("'%1' parameters has no properties map").arg(name);
}

// One Anthropic `tools[]` entry: bare {name, description, input_schema}.
void ptf_check_anthropic_entry(const QJsonObject& t, QSet<QString>* seen, QStringList* errs) {
    const QString name = t.value(QStringLiteral("name")).toString();
    if (!ptf_openai_name_rx().match(name).hasMatch())
        *errs << QStringLiteral("'%1' violates ^[a-zA-Z0-9_-]{1,64}$").arg(name);
    if (seen) {
        if (seen->contains(name))
            *errs << QStringLiteral("duplicate tool name '%1'").arg(name);
        seen->insert(name);
    }
    if (t.value(QStringLiteral("description")).toString().trimmed().isEmpty())
        *errs << QStringLiteral("'%1' has no description").arg(name);
    // Anthropic takes a bare tool object — an OpenAI-style
    // {"type":"function","function":{…}} wrapper is a 400.
    if (t.contains(QStringLiteral("type")) || t.contains(QStringLiteral("function")))
        *errs << QStringLiteral("'%1' carries an OpenAI-style wrapper").arg(name);
    const QJsonObject schema = t.value(QStringLiteral("input_schema")).toObject();
    if (schema.isEmpty())
        *errs << QStringLiteral("'%1' has no input_schema").arg(name);
    else if (schema.value(QStringLiteral("type")).toString() != QLatin1String("object"))
        *errs << QStringLiteral("'%1' input_schema.type != object").arg(name);
    else if (!schema.contains(QStringLiteral("properties")))
        *errs << QStringLiteral("'%1' input_schema has no properties map").arg(name);
}

// One Gemini `functionDeclarations[]` entry: {name, description, parameters?}.
// `parameters` is legitimately absent for a parameterless tool.
void ptf_check_gemini_decl(const QJsonObject& d, QSet<QString>* seen, QStringList* errs) {
    const QString name = d.value(QStringLiteral("name")).toString();
    if (!is_valid_gemini_function_name(name))
        *errs << QStringLiteral("'%1' violates ^[a-zA-Z_][a-zA-Z0-9_.:-]{0,63}$").arg(name);
    if (seen) {
        if (seen->contains(name))
            *errs << QStringLiteral("duplicate function name '%1'").arg(name);
        seen->insert(name);
    }
    if (d.value(QStringLiteral("description")).toString().trimmed().isEmpty())
        *errs << QStringLiteral("'%1' has no description").arg(name);
    if (!d.contains(QStringLiteral("parameters")))
        return;
    const QJsonObject params = d.value(QStringLiteral("parameters")).toObject();
    if (params.value(QStringLiteral("type")).toString() != QLatin1String("object"))
        *errs << QStringLiteral("%1.parameters: top-level type must be OBJECT").arg(name);
    ptf_check_gemini_schema(params, name + QStringLiteral(".parameters"), errs);
}

// Every parameter a tool declares must still be reachable in the dialect's
// rendering of it. A dropped parameter is not a 400 — it is worse: the request
// succeeds and the model is simply never told the argument exists.
void ptf_check_params_survived(const QString& dialect, const QString& name, const QJsonObject& source_schema,
                               const QJsonObject& rendered_params, bool params_present, QStringList* errs) {
    const QJsonObject src = source_schema.value(QStringLiteral("properties")).toObject();
    if (src.isEmpty())
        return; // genuinely parameterless
    if (!params_present) {
        *errs << QStringLiteral("%1/%2: declares %3 parameter(s) but the %1 payload carries none — the model "
                                "would be told the tool takes no arguments")
                     .arg(dialect, name, QString::number(src.size()));
        return;
    }
    const QJsonObject kept = rendered_params.value(QStringLiteral("properties")).toObject();
    for (auto it = src.constBegin(); it != src.constEnd(); ++it) {
        if (!kept.contains(it.key()))
            *errs << QStringLiteral("%1/%2: parameter '%3' was dropped in translation").arg(dialect, name, it.key());
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

    // The default filter is what every chat turn sends: the whole internal
    // catalogue (the ten core tools) plus any external servers' tools.

    // ── OpenAI ──────────────────────────────────────────────────────────
    {
        const QJsonArray tools = svc.format_tools_for_openai(default_filter);
        ptf_out(QStringLiteral("\n[1] OPENAI — %1 function declarations").arg(tools.size()));

        QStringList errs;
        if (tools.isEmpty())
            errs << QStringLiteral("empty tool array — the model would have no tools at all");
        QSet<QString> seen;
        for (const auto& v : tools)
            ptf_check_openai_entry(v.toObject(), &seen, &errs);
        ptf_report(QStringLiteral("openai payload"), errs, &failed);
    }

    // ── Anthropic ───────────────────────────────────────────────────────
    {
        const QJsonArray tools = svc.format_tools_for_anthropic(default_filter);
        ptf_out(QStringLiteral("\n[2] ANTHROPIC — %1 tools").arg(tools.size()));

        QStringList errs;
        if (tools.isEmpty())
            errs << QStringLiteral("empty tool array — the model would have no tools at all");
        QSet<QString> seen;
        for (const auto& v : tools)
            ptf_check_anthropic_entry(v.toObject(), &seen, &errs);
        ptf_report(QStringLiteral("anthropic payload"), errs, &failed);

        // The whole point of routing Anthropic through the shared selector: it
        // must see the same tools the OpenAI path does, not a different slice.
        const QJsonArray oai = svc.format_tools_for_openai(default_filter);
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
        const QJsonArray tools = svc.format_tools_for_gemini(default_filter);
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
        QSet<QString> seen;
        for (const auto& v : decls) {
            const QJsonObject d = v.toObject();
            if (d.contains(QStringLiteral("parameters")))
                ++with_params;
            ptf_check_gemini_decl(d, &seen, &errs);
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
        // TOP level only: `parameters` must be an OBJECT, so there is nothing to
        // substitute into and the caller has to omit the field. (A NESTED empty
        // object is a free-form bag and becomes a STRING -- checked below.)
        if (sanitize_schema_for_gemini(QJsonObject{{"type", "object"}, {"properties", QJsonObject{}}}).has_value())
            errs << QStringLiteral("top-level empty-properties object should sanitise to nullopt");

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
            // A nested object with no declared properties is a FREE-FORM bag,
            // not a broken schema. Gemini cannot express one, so it must be
            // carried as a JSON-bearing STRING rather than dropped -- dropping
            // it removes the parameter from the model's reach entirely.
            if (props.value(QStringLiteral("empty")).toObject().value(QStringLiteral("type")).toString() !=
                QLatin1String("string")) {
                errs << QStringLiteral("free-form object property should become a STRING surrogate, not vanish");
            }
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
            // `ghost` was never a property; `empty` still is one (as a STRING).
            if (req.contains(QStringLiteral("ghost")))
                errs << QStringLiteral("'required' still names a property that is not declared: ") + req.join(',');
            if (!req.contains(QStringLiteral("empty")))
                errs << QStringLiteral("'required' dropped a free-form object parameter that is still declared");
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

    // -- Catalogue-wide sweep, all three dialects ------------------------
    //
    // Sections [1]-[3] see what a default chat turn sends. The sweep also runs
    // every registered tool through each dialect under explicit name filters,
    // so a declaration that only an agent's filtered catalogue would carry is
    // checked as well (a Gemini parameter that does not survive translation
    // makes the tool inert there without any error).
    //
    // The sweep drives the REAL formatters rather than re-deriving what they
    // ought to emit -- a test that reimplements the thing it checks agrees with
    // its own bugs. Each dialect caps its array (Gemini at 128, the others at
    // 256), so the catalogue goes through in chunks of anchored name filters.
    {
        const std::vector<UnifiedTool> all = svc.get_all_tools();
        ptf_out(QStringLiteral("\n[5] CATALOGUE SWEEP, ALL DIALECTS - %1 tools").arg(all.size()));

        // Wire name -> source schema, for the parameter-survival check.
        QHash<QString, QJsonObject> source_schema;
        QStringList every_name;
        for (const auto& t : all) {
            source_schema.insert(t.server_id + QStringLiteral("__") +
                                     McpProvider::encode_tool_name_for_wire(t.name),
                                 t.input_schema);
            every_name << QRegularExpression::escape(t.name);
        }

        constexpr int kChunk = 100; // comfortably under the tightest dialect cap (128)
        QStringList oai_errs, ant_errs, gem_errs;
        QSet<QString> oai_seen, ant_seen, gem_seen;
        int oai_n = 0, ant_n = 0, gem_n = 0, gem_with_params = 0;

        for (int off = 0; off < every_name.size(); off += kChunk) {
            const QStringList names = every_name.mid(off, kChunk);
            ToolFilter f;
            f.name_patterns = QStringList{QStringLiteral("^(?:") + names.join(QLatin1Char('|')) +
                                          QStringLiteral(")$")};
            f.max_tools = kChunk * 4; // above the chunk size; the dialect cap still applies

            const QJsonArray oai = svc.format_tools_for_openai(f);
            if (oai.size() != names.size()) {
                oai_errs << QStringLiteral("chunk at %1: asked for %2 tools, payload carried %3")
                                .arg(off)
                                .arg(names.size())
                                .arg(oai.size());
            }
            for (const auto& v : oai) {
                const QJsonObject e = v.toObject();
                ptf_check_openai_entry(e, &oai_seen, &oai_errs);
                const QJsonObject fn = e.value(QStringLiteral("function")).toObject();
                const QString name = fn.value(QStringLiteral("name")).toString();
                ptf_check_params_survived(QStringLiteral("openai"), name, source_schema.value(name),
                                          fn.value(QStringLiteral("parameters")).toObject(),
                                          fn.contains(QStringLiteral("parameters")), &oai_errs);
                ++oai_n;
            }

            const QJsonArray ant = svc.format_tools_for_anthropic(f);
            if (ant.size() != oai.size())
                ant_errs << QStringLiteral("chunk at %1: anthropic %2 vs openai %3 -- selection diverged")
                                .arg(off)
                                .arg(ant.size())
                                .arg(oai.size());
            for (const auto& v : ant) {
                const QJsonObject t = v.toObject();
                ptf_check_anthropic_entry(t, &ant_seen, &ant_errs);
                const QString name = t.value(QStringLiteral("name")).toString();
                ptf_check_params_survived(QStringLiteral("anthropic"), name, source_schema.value(name),
                                          t.value(QStringLiteral("input_schema")).toObject(), true, &ant_errs);
                ++ant_n;
            }

            const QJsonArray gem_tools = svc.format_tools_for_gemini(f);
            const QJsonArray decls =
                gem_tools.isEmpty()
                    ? QJsonArray{}
                    : gem_tools[0].toObject().value(QStringLiteral("functionDeclarations")).toArray();
            if (decls.size() != oai.size()) {
                gem_errs << QStringLiteral("chunk at %1: gemini %2 vs openai %3 -- declarations were dropped")
                                .arg(off)
                                .arg(decls.size())
                                .arg(oai.size());
            }
            for (const auto& v : decls) {
                const QJsonObject d = v.toObject();
                ptf_check_gemini_decl(d, &gem_seen, &gem_errs);
                const QString name = d.value(QStringLiteral("name")).toString();
                const bool has_params = d.contains(QStringLiteral("parameters"));
                if (has_params)
                    ++gem_with_params;
                ptf_check_params_survived(QStringLiteral("gemini"), name, source_schema.value(name),
                                          d.value(QStringLiteral("parameters")).toObject(), has_params, &gem_errs);
                ++gem_n;
            }
        }

        ptf_out(QStringLiteral("    openai %1 | anthropic %2 | gemini %3 (%4 with parameters)")
                    .arg(oai_n)
                    .arg(ant_n)
                    .arg(gem_n)
                    .arg(gem_with_params));
        ptf_report(QStringLiteral("openai catalogue sweep"), oai_errs, &failed);
        ptf_report(QStringLiteral("anthropic catalogue sweep"), ant_errs, &failed);
        ptf_report(QStringLiteral("gemini catalogue sweep"), gem_errs, &failed);
    }

    // -- Free-form object round-trip -------------------------------------
    // Gemini has no `additionalProperties` and rejects an OBJECT with an empty
    // `properties` map, so "an object with arbitrary keys" is inexpressible.
    // Such a parameter goes out as a JSON-carrying STRING and must come back an
    // object before dispatch - otherwise `args["params"].toObject()` is `{}` and
    // the tool runs on nothing.
    {
        ptf_out(QStringLiteral("\n[6] GEMINI FREE-FORM OBJECT ROUND-TRIP"));
        QStringList errs;

        // This is the exact shape of the ~115 catalog-driven quant tools.
        const QJsonObject schema{
            {"type", "object"},
            {"properties",
             QJsonObject{
                 {"module_id", QJsonObject{{"type", "string"}}},
                 {"params", QJsonObject{{"type", "object"}, {"description", "Module-specific arguments object"}}},
                 {"rows", QJsonObject{{"type", "array"}, {"items", QJsonObject{{"type", "object"}}}}},
             }},
            {"required", QJsonArray{"module_id"}}};

        const auto out = sanitize_schema_for_gemini(schema);
        if (!out) {
            errs << QStringLiteral("schema sanitised away entirely");
        } else {
            const QJsonObject props = out->value(QStringLiteral("properties")).toObject();
            if (!props.contains(QStringLiteral("params")))
                errs << QStringLiteral("free-form object parameter was dropped instead of substituted");
            else if (props.value(QStringLiteral("params")).toObject().value(QStringLiteral("type")).toString() !=
                     QLatin1String("string"))
                errs << QStringLiteral("free-form object parameter must be declared as STRING");
            if (!props.contains(QStringLiteral("rows")))
                errs << QStringLiteral("array-of-free-form-objects parameter was dropped");
            ptf_check_gemini_schema(*out, QStringLiteral("freeform"), &errs);
        }

        // What the model sends back, and what the handler must receive.
        const QJsonObject restored = restore_gemini_call_args(
            schema, QJsonObject{{"module_id", "backtesting"},
                                {"params", "{\"symbol\":\"AAPL\",\"lookback\":30}"},
                                {"rows", "[{\"a\":1}]"}});
        const QJsonObject rp = restored.value(QStringLiteral("params")).toObject();
        if (rp.value(QStringLiteral("symbol")).toString() != QLatin1String("AAPL") ||
            rp.value(QStringLiteral("lookback")).toInt() != 30) {
            errs << QStringLiteral("serialised object argument did not parse back into an object");
        }
        const QJsonArray rr = restored.value(QStringLiteral("rows")).toArray();
        if (rr.size() != 1 || rr[0].toObject().value(QStringLiteral("a")).toInt() != 1)
            errs << QStringLiteral("serialised array argument did not parse back into an array");
        if (restored.value(QStringLiteral("module_id")).toString() != QLatin1String("backtesting"))
            errs << QStringLiteral("a plain string argument must pass through untouched");

        // A model that gets it right anyway, and a model that sends junk: the
        // first must be left alone, the second must reach the argument
        // validator unchanged rather than being swallowed here.
        const QJsonObject already_ok =
            restore_gemini_call_args(schema, QJsonObject{{"params", QJsonObject{{"symbol", "MSFT"}}}});
        if (already_ok.value(QStringLiteral("params")).toObject().value(QStringLiteral("symbol")).toString() !=
            QLatin1String("MSFT"))
            errs << QStringLiteral("a correctly-typed object argument must pass through untouched");
        const QJsonObject garbage = restore_gemini_call_args(schema, QJsonObject{{"params", "not json"}});
        if (!garbage.value(QStringLiteral("params")).isString())
            errs << QStringLiteral("an unparseable argument must be left for the validator to reject");

        ptf_report(QStringLiteral("free-form object handling"), errs, &failed);
    }

    // ── Endpoint composition ────────────────────────────────────────────
    // A tool payload is only half the contract; it still has to reach the
    // route that speaks that dialect. Gemini is the trap: its body is the only
    // non-OpenAI shape, so a user-supplied base_url that gets "/chat/completions"
    // appended sends a native `contents` body to an OpenAI-compat route and
    // 400s — tools included. ProviderCatalog::chat_endpoint mirrors
    // LlmService::get_endpoint_url; both must agree with these cases.
    {
        ptf_out(QStringLiteral("\n[7] ENDPOINT COMPOSITION"));
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

        // Every provider the LLM Config screen can select must resolve to an
        // endpoint from its own defaults, with no base_url typed in. Two of
        // them (AstraFlow, AstraFlow CN) carry their host in
        // `default_base_url` rather than in the switch above, and were reachable
        // only because the screen prefills that value -- clearing the field
        // dropped them to "No endpoint URL for provider". Enumerating
        // known_providers() rather than a hand-listed set is the point: a
        // provider added later is covered the day it is added.
        QStringList coverage;
        for (const QString& prov : ai_chat::ProviderCatalog::known_providers()) {
            if (prov == QLatin1String("fincept"))
                continue; // route is AppConfig::api_base_url() + "/research/chat", composed by the caller
            const QString url =
                ai_chat::ProviderCatalog::chat_endpoint(prov, QString(), QStringLiteral("test-model"));
            if (url.isEmpty()) {
                coverage << QStringLiteral("%1: no endpoint with default settings").arg(prov);
                continue;
            }
            if (!url.startsWith(QLatin1String("http")))
                coverage << QStringLiteral("%1: endpoint is not an http(s) URL: %2").arg(prov, url);
            // The dialect the body will be built in has to match the route it is
            // posted to. Gemini is the only non-OpenAI-shaped body.
            const bool gemini_route = url.contains(QLatin1String(":generateContent"));
            const bool gemini_provider = (prov == QLatin1String("gemini") || prov == QLatin1String("google"));
            if (gemini_route != gemini_provider) {
                coverage << QStringLiteral("%1: dialect/route mismatch -- %2")
                                .arg(prov, gemini_route ? QStringLiteral("native Gemini route for a non-Gemini "
                                                                         "provider")
                                                        : QStringLiteral("OpenAI-compat route for Gemini"));
            }
            if (!gemini_provider) {
                const QString want = (prov == QLatin1String("anthropic")) ? QStringLiteral("/messages")
                                                                         : QStringLiteral("/chat/completions");
                if (!url.endsWith(want))
                    coverage << QStringLiteral("%1: endpoint does not end in %2: %3").arg(prov, want, url);
            }
        }
        ptf_report(QStringLiteral("every known provider resolves an endpoint"), coverage, &failed);
    }

    ptf_out(QStringLiteral("\n--------------------------------------------------------------"));
    ptf_out(failed ? QStringLiteral("  RESULT: FAIL") : QStringLiteral("  RESULT: PASS"));
    ptf_out(QStringLiteral("--------------------------------------------------------------\n"));
    return failed ? 1 : 0;
}

} // namespace fincept::mcp
