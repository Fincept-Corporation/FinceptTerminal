// McpClientProfile.cpp — see McpClientProfile.h.
//
// Profile values, and where they come from (checked 2026-11-17; "assumed" =
// not stated by the vendor's documentation and kept conservative):
//
//   structured_content — off for every profile. The tools declare no
//     outputSchema, so the spec makes structuredContent optional, and the text
//     content already carries the same JSON. OpenAI Codex builds the model's
//     function output from structuredContent INSTEAD of content when it is
//     present (codex-rs FunctionCallOutputPayload from CallToolResult), which
//     silently dropped the tool's message text; Qwen Code sends BOTH to the
//     model (the result was paid twice); Gemini CLI reads content only.
//   dialect — Standard everywhere: every harness listed here sends MCP input
//     schemas to its model as JSON Schema (Gemini CLI and Qwen Code pass them as
//     parametersJsonSchema / OpenAI-compatible parameters and sanitise on their
//     side). The Gemini and OpenAiStrict dialects stay available for a client
//     that turns out to need them, and are golden-tested.
//   tool_name_budget — 64 minus the prefix the harness adds
//     (Claude Code "mcp__<server>__" ≤ 128 total; Codex "mcp__<server>__" ≤ 128,
//     older builds 64; Gemini CLI "mcp_<server>_" truncated past 63; Qwen Code
//     "mcp__<server>__" ≤ 63 and no dots; Copilot "<server>-" ≤ 64); the
//     terminal's names are ≤ 13 characters, so every budget leaves headroom.
//   result_budget_cap_bytes — Claude Code saves text over 50,000 characters to
//     a file (and warns past MAX_MCP_OUTPUT_TOKENS, default 25,000 tokens);
//     Copilot CLI returns output over COPILOT_LARGE_OUTPUT_THRESHOLD_BYTES
//     (20,480) indirectly. Results are kept under those thresholds; the rest
//     stays readable with ui_read(res_…).
// Sources: code.claude.com/docs/en/mcp; learn.chatgpt.com/docs/extend/mcp and
// codex-rs/protocol/src/models.rs; gemini-cli docs/tools/mcp-server.md and
// packages/core/src/tools/mcp-tool.ts; qwen-code packages/core mcp-tool.ts;
// docs.github.com copilot-cli add-mcp-servers; cursor.com/docs/context/mcp;
// docs.x.ai/build/features/mcp-servers.

#include "mcp/server/McpClientProfile.h"

#include "mcp/GeminiSchema.h"

#include <QJsonValue>
#include <QStringList>

#include <array>

namespace fincept::mcp::profiles {

namespace {

constexpr int kNameBudgetPrefixed = 64 - 32; // a 32-char server+prefix allowance

const std::array<ClientProfile, static_cast<int>(ClientKind::Count_)>& table() {
    // structuredContent is off for EVERY profile, Generic included: no tool
    // declares an outputSchema (so the spec does not require it), the text
    // content carries the same JSON, and an unknown client may be Codex-like
    // (structured only → the message is lost) or Qwen-like (both → paid twice).
    // A tool that declares an outputSchema keeps it whatever the profile says.
    static const std::array<ClientProfile, static_cast<int>(ClientKind::Count_)> k = {{
        {ClientKind::Generic, "generic", "MCP client", "", "external", SchemaDialect::Standard, false, 64, 0, 0},
        {ClientKind::ClaudeCode, "claude-code", "Claude Code", "claude", "claude_code", SchemaDialect::Standard, false,
         kNameBudgetPrefixed, 0, 48000},
        {ClientKind::ClaudeDesktop, "claude-desktop", "Claude Desktop", "", "claude_desktop", SchemaDialect::Standard,
         false, kNameBudgetPrefixed, 0, 48000},
        {ClientKind::Codex, "codex", "OpenAI Codex", "codex", "codex", SchemaDialect::Standard, false,
         kNameBudgetPrefixed, 0, 0},
        {ClientKind::GeminiCli, "gemini-cli", "Gemini CLI", "gemini", "gemini", SchemaDialect::Standard, false,
         kNameBudgetPrefixed, 0, 0},
        {ClientKind::QwenCode, "qwen-code", "Qwen Code", "qwen", "qwen", SchemaDialect::Standard, false,
         kNameBudgetPrefixed, 0, 0},
        {ClientKind::Cursor, "cursor", "Cursor", "cursor", "cursor", SchemaDialect::Standard, false,
         kNameBudgetPrefixed, 0, 0},
        {ClientKind::Copilot, "copilot", "GitHub Copilot", "copilot", "copilot", SchemaDialect::Standard, false,
         kNameBudgetPrefixed, 0, 20000},
        {ClientKind::Grok, "grok", "Grok", "grok", "grok", SchemaDialect::Standard, false, kNameBudgetPrefixed, 0, 0},
        {ClientKind::VsCode, "vscode", "VS Code", "", "vscode", SchemaDialect::Standard, false, kNameBudgetPrefixed, 0,
         0},
    }};
    return k;
}

bool contains_ci(const QString& hay, const char* needle) {
    return hay.contains(QLatin1String(needle), Qt::CaseInsensitive);
}

// ── OpenAI strict ────────────────────────────────────────────────────────────

// Keywords strict mode rejects or ignores; dropping them only costs the model a
// hint, keeping them costs the whole request.
const QStringList& strict_dropped_keywords() {
    static const QStringList k = {
        QStringLiteral("default"),          QStringLiteral("minLength"),        QStringLiteral("maxLength"),
        QStringLiteral("pattern"),          QStringLiteral("format"),           QStringLiteral("minimum"),
        QStringLiteral("maximum"),          QStringLiteral("exclusiveMinimum"), QStringLiteral("exclusiveMaximum"),
        QStringLiteral("multipleOf"),       QStringLiteral("minItems"),         QStringLiteral("maxItems"),
        QStringLiteral("uniqueItems"),      QStringLiteral("minProperties"),    QStringLiteral("maxProperties"),
        QStringLiteral("patternProperties"), QStringLiteral("examples"),        QStringLiteral("$schema"),
        QStringLiteral("propertyNames"),    QStringLiteral("dependentRequired"),
    };
    return k;
}

QJsonObject strict_schema(const QJsonObject& in, int depth);

/// `schema` made able to hold null as well.
QJsonObject strict_nullable(QJsonObject schema) {
    const QJsonValue type = schema.value(QStringLiteral("type"));
    if (type.isString()) {
        if (type.toString() != QLatin1String("null"))
            schema.insert(QStringLiteral("type"), QJsonArray{type.toString(), QStringLiteral("null")});
    } else if (type.isArray()) {
        QJsonArray t = type.toArray();
        if (!t.contains(QStringLiteral("null")))
            t.append(QStringLiteral("null"));
        schema.insert(QStringLiteral("type"), t);
    } else if (schema.contains(QStringLiteral("anyOf"))) {
        QJsonArray variants = schema.value(QStringLiteral("anyOf")).toArray();
        variants.append(QJsonObject{{QStringLiteral("type"), QStringLiteral("null")}});
        schema.insert(QStringLiteral("anyOf"), variants);
    }
    if (schema.contains(QStringLiteral("enum"))) {
        QJsonArray e = schema.value(QStringLiteral("enum")).toArray();
        if (!e.contains(QJsonValue(QJsonValue::Null)))
            e.append(QJsonValue(QJsonValue::Null));
        schema.insert(QStringLiteral("enum"), e);
    }
    return schema;
}

QJsonObject strict_schema(const QJsonObject& in, int depth) {
    QJsonObject out = in;
    for (const QString& k : strict_dropped_keywords())
        out.remove(k);
    if (depth > 16)
        return QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}};

    // Untyped ("any JSON value"): strict mode needs a type — the scalar kinds.
    if (!out.contains(QStringLiteral("type")) && !out.contains(QStringLiteral("anyOf")) &&
        !out.contains(QStringLiteral("enum")) && !out.contains(QStringLiteral("const")) &&
        !out.contains(QStringLiteral("$ref"))) {
        out.insert(QStringLiteral("type"), QJsonArray{QStringLiteral("string"), QStringLiteral("number"),
                                                      QStringLiteral("boolean")});
    }

    if (out.contains(QStringLiteral("anyOf"))) {
        QJsonArray variants;
        for (const QJsonValue& v : out.value(QStringLiteral("anyOf")).toArray())
            variants.append(v.isObject() ? strict_schema(v.toObject(), depth + 1) : v);
        out.insert(QStringLiteral("anyOf"), variants);
    }

    const QJsonValue type = out.value(QStringLiteral("type"));
    const bool is_object = type.toString() == QLatin1String("object") ||
                           (type.isArray() && type.toArray().contains(QStringLiteral("object"))) ||
                           out.contains(QStringLiteral("properties"));
    const bool is_array = type.toString() == QLatin1String("array") ||
                          (type.isArray() && type.toArray().contains(QStringLiteral("array")));
    if (is_array) {
        const QJsonValue items = out.value(QStringLiteral("items"));
        out.insert(QStringLiteral("items"), items.isObject() ? strict_schema(items.toObject(), depth + 1)
                                                             : QJsonObject{{QStringLiteral("type"),
                                                                            QStringLiteral("string")}});
    }
    if (is_object) {
        const QJsonObject props = out.value(QStringLiteral("properties")).toObject();
        QStringList required;
        for (const QJsonValue& r : out.value(QStringLiteral("required")).toArray())
            required << r.toString();
        QJsonObject strict_props;
        QJsonArray all_required;
        for (auto it = props.constBegin(); it != props.constEnd(); ++it) {
            QJsonObject child = strict_schema(it.value().toObject(), depth + 1);
            if (!required.contains(it.key()))
                child = strict_nullable(child);
            strict_props.insert(it.key(), child);
            all_required.append(it.key());
        }
        out.insert(QStringLiteral("properties"), strict_props);
        out.insert(QStringLiteral("required"), all_required);
        out.insert(QStringLiteral("additionalProperties"), false);
    }
    return out;
}

QJsonObject strict_restore_object(const QJsonObject& schema, const QJsonObject& args, int depth);

QJsonValue strict_restore_value(const QJsonObject& schema, const QJsonValue& value, int depth) {
    if (depth > 16)
        return value;
    if (value.isObject() && schema.value(QStringLiteral("properties")).isObject())
        return strict_restore_object(schema, value.toObject(), depth + 1);
    if (value.isArray() && schema.value(QStringLiteral("items")).isObject()) {
        const QJsonObject items = schema.value(QStringLiteral("items")).toObject();
        QJsonArray out;
        for (const QJsonValue& v : value.toArray())
            out.append(strict_restore_value(items, v, depth + 1));
        return out;
    }
    return value;
}

QJsonObject strict_restore_object(const QJsonObject& schema, const QJsonObject& args, int depth) {
    const QJsonObject props = schema.value(QStringLiteral("properties")).toObject();
    QStringList required;
    for (const QJsonValue& r : schema.value(QStringLiteral("required")).toArray())
        required << r.toString();
    QJsonObject out;
    for (auto it = args.constBegin(); it != args.constEnd(); ++it) {
        // A null the strict schema forced the model to send for an OPTIONAL
        // property means "not given". A null for a required one is passed on so
        // the tool's own validator reports it.
        if (it.value().isNull() && props.contains(it.key()) && !required.contains(it.key()))
            continue;
        const QJsonValue decl = props.value(it.key());
        out.insert(it.key(), decl.isObject() ? strict_restore_value(decl.toObject(), it.value(), depth) : it.value());
    }
    return out;
}

bool strict_check(const QJsonObject& s, const QString& path, QString* why, int depth) {
    auto fail = [why, &path](const QString& reason) {
        if (why)
            *why = path + QStringLiteral(": ") + reason;
        return false;
    };
    if (depth > 32)
        return fail(QStringLiteral("too deep"));
    const QJsonValue type = s.value(QStringLiteral("type"));
    if (type.isUndefined() && !s.contains(QStringLiteral("anyOf")) && !s.contains(QStringLiteral("enum")) &&
        !s.contains(QStringLiteral("const")) && !s.contains(QStringLiteral("$ref")))
        return fail(QStringLiteral("no type"));
    for (const QString& k : strict_dropped_keywords()) {
        if (s.contains(k))
            return fail(QStringLiteral("keyword %1").arg(k));
    }
    if (s.contains(QStringLiteral("anyOf"))) {
        int i = 0;
        for (const QJsonValue& v : s.value(QStringLiteral("anyOf")).toArray()) {
            if (!strict_check(v.toObject(), path + QStringLiteral("/anyOf/%1").arg(i++), why, depth + 1))
                return false;
        }
    }
    const bool is_object = type.toString() == QLatin1String("object") ||
                           (type.isArray() && type.toArray().contains(QStringLiteral("object")));
    if (is_object) {
        if (s.value(QStringLiteral("additionalProperties")) != QJsonValue(false))
            return fail(QStringLiteral("additionalProperties is not false"));
        const QJsonObject props = s.value(QStringLiteral("properties")).toObject();
        const QJsonArray required = s.value(QStringLiteral("required")).toArray();
        for (auto it = props.constBegin(); it != props.constEnd(); ++it) {
            if (!required.contains(it.key()))
                return fail(QStringLiteral("property %1 not required").arg(it.key()));
            if (!strict_check(it.value().toObject(), path + QLatin1Char('/') + it.key(), why, depth + 1))
                return false;
        }
    }
    if (s.value(QStringLiteral("items")).isObject() &&
        !strict_check(s.value(QStringLiteral("items")).toObject(), path + QStringLiteral("/items"), why, depth + 1))
        return false;
    return true;
}

QJsonObject normalised_object_schema(QJsonObject schema) {
    if (schema.value(QStringLiteral("type")).toString() != QLatin1String("object"))
        schema.insert(QStringLiteral("type"), QStringLiteral("object"));
    if (!schema.value(QStringLiteral("properties")).isObject())
        schema.insert(QStringLiteral("properties"), QJsonObject{});
    return schema;
}

} // namespace

const ClientProfile& profile(ClientKind kind) {
    const int i = static_cast<int>(kind);
    if (i < 0 || i >= static_cast<int>(ClientKind::Count_))
        return table()[0];
    return table()[static_cast<size_t>(i)];
}

QList<ClientKind> all_kinds() {
    QList<ClientKind> out;
    for (int i = 0; i < static_cast<int>(ClientKind::Count_); ++i)
        out.append(static_cast<ClientKind>(i));
    return out;
}

ClientKind detect_client(const QString& client_name, const QString& user_agent) {
    // Most specific first: "claude-code" before any "claude", Qwen (a Gemini CLI
    // fork) before Gemini.
    for (const QString& s : {client_name, user_agent}) {
        if (s.isEmpty())
            continue;
        if (contains_ci(s, "claude-code") || contains_ci(s, "claude code") || contains_ci(s, "claude-cli"))
            return ClientKind::ClaudeCode;
        if (contains_ci(s, "claude-ai") || contains_ci(s, "claude desktop") || contains_ci(s, "claude-desktop"))
            return ClientKind::ClaudeDesktop;
        if (contains_ci(s, "codex"))
            return ClientKind::Codex;
        if (contains_ci(s, "qwen"))
            return ClientKind::QwenCode;
        if (contains_ci(s, "gemini"))
            return ClientKind::GeminiCli;
        if (contains_ci(s, "cursor"))
            return ClientKind::Cursor;
        if (contains_ci(s, "copilot"))
            return ClientKind::Copilot;
        if (contains_ci(s, "grok"))
            return ClientKind::Grok;
        if (contains_ci(s, "visual studio code") || contains_ci(s, "vscode"))
            return ClientKind::VsCode;
    }
    return ClientKind::Generic;
}

ClientKind kind_for_harness(const QString& harness_id) {
    for (const ClientProfile& p : table()) {
        if (p.harness_id[0] != '\0' && harness_id.compare(QLatin1String(p.harness_id), Qt::CaseInsensitive) == 0)
            return p.kind;
    }
    return ClientKind::Generic;
}

QJsonObject shape_input_schema(const QJsonObject& schema, SchemaDialect dialect) {
    switch (dialect) {
        case SchemaDialect::Gemini: {
            const auto g = sanitize_schema_for_gemini(normalised_object_schema(schema));
            // Parameterless: an OBJECT with an empty properties map is rejected by
            // the Gemini API, so the schema says "object" and nothing else.
            return g ? *g : QJsonObject{{QStringLiteral("type"), QStringLiteral("object")}};
        }
        case SchemaDialect::OpenAiStrict:
            return strict_schema(normalised_object_schema(schema), 0);
        case SchemaDialect::Standard:
            break;
    }
    return normalised_object_schema(schema);
}

QJsonArray shape_catalogue(const QJsonArray& entries, const ClientProfile& p, QStringList* warnings) {
    QJsonArray out;
    for (const QJsonValue& v : entries) {
        QJsonObject e = v.toObject();
        const QString name = e.value(QStringLiteral("name")).toString();
        if (name.size() > p.tool_name_budget && warnings)
            warnings->append(QStringLiteral("%1: tool name '%2' is longer than %3 characters")
                                 .arg(QLatin1String(p.id), name)
                                 .arg(p.tool_name_budget));
        if (p.dialect != SchemaDialect::Standard)
            e.insert(QStringLiteral("inputSchema"),
                     shape_input_schema(e.value(QStringLiteral("inputSchema")).toObject(), p.dialect));
        if (p.description_max_chars > 0) {
            const QString d = e.value(QStringLiteral("description")).toString();
            if (d.size() > p.description_max_chars)
                e.insert(QStringLiteral("description"), d.left(p.description_max_chars - 1) + QChar(0x2026));
        }
        out.append(e);
    }
    return out;
}

QJsonObject restore_call_args(const QJsonObject& original_schema, const QJsonObject& args, SchemaDialect dialect) {
    switch (dialect) {
        case SchemaDialect::Gemini:
            return restore_gemini_call_args(original_schema, args);
        case SchemaDialect::OpenAiStrict:
            return strict_restore_object(original_schema, args, 0);
        case SchemaDialect::Standard:
            break;
    }
    return args;
}

bool is_strict_schema(const QJsonObject& schema, QString* why) {
    return strict_check(schema, QStringLiteral("$"), why, 0);
}

} // namespace fincept::mcp::profiles
