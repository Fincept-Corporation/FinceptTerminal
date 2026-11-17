#pragma once
// HarnessArenaLogic.h — the pure parts of an Alpha Arena decision answered by
// an agent harness (HarnessArenaClient): the decision's JSON schema, the
// harness request built from the arena's request fields, the JSON object
// pulled out of the model's reply, and the result mapped onto the arena's
// result fields. Header-only, QtCore only (+ HarnessChatTypes.h) so a unit
// test needs neither the arena engine nor QtNetwork.
//
// The schema mirrors what ArenaDecisionParser::parse_decision() reads:
//   {"actions":[{"action":"open|close|hold","coin","side":"long|short",
//                "size_usd","leverage","reason"}, …]}
// written strict-mode compatible (every property required, nullable where the
// prompt lets the model leave a field out, no additional properties), so both
// Claude Code (--json-schema) and Codex (--output-schema) accept it; the
// parser reads a null field exactly like a missing one.

#include "services/harness/HarnessChatTypes.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QString>

namespace fincept::arena::harness_arena {

/// What a harness call came to, in the arena's result terms (ArenaLlmResult
/// without the QtNetwork header).
struct ArenaHarnessOutcome {
    QString content; // the decision JSON object (compact), or the raw reply when none was found
    QString error;   // "timeout" exactly for a timeout (ArenaEngine::finish_agent compares it)
    int prompt_tokens = 0;
    int completion_tokens = 0;
    qint64 latency_ms = 0;
    bool success = false;
};

/// The arena decision schema (compact JSON).
inline QString decision_schema() {
    return QStringLiteral(
        R"({"type":"object","additionalProperties":false,"required":["actions"],"properties":{"actions":{"type":"array","minItems":1,"items":{"type":"object","additionalProperties":false,"required":["action","coin","side","size_usd","leverage","reason"],"properties":{"action":{"type":"string","enum":["open","close","hold"]},"coin":{"type":["string","null"]},"side":{"type":["string","null"],"enum":["long","short",null]},"size_usd":{"type":["number","null"]},"leverage":{"type":["number","null"]},"reason":{"type":"string"}}}}}})");
}

/// One harness call for one arena decision: no terminal tools, the arena's
/// prompts, JSON by schema where the harness supports it, `timeout_ms` for the
/// whole call (the arena's 60 s — also the time to the first event).
inline harness::HarnessChatRequest build_request(const QString& harness_id, const QString& model_id,
                                                 const QString& effort, const QString& system_prompt,
                                                 const QString& user_prompt, int timeout_ms, bool json_schema) {
    harness::HarnessChatRequest r;
    r.harness_id = harness_id;
    r.model = model_id;
    r.effort = effort;
    r.system_prompt = system_prompt;
    r.user_message = user_prompt;
    r.feature = QStringLiteral("arena");
    r.tools = harness::ChatTools::None;
    r.paper_only = true; // no tools at all; belt and braces should one ever be given
    if (json_schema)
        r.json_schema = decision_schema();
    const int t = timeout_ms > 0 ? timeout_ms : 60000;
    r.first_event_timeout_ms = t;
    r.timeout_ms = t;
    return r;
}

/// The first complete JSON object in a model reply: the whole text, a ```json
/// fence, or the first balanced {...} (braces inside strings do not count).
/// Empty when there is none.
inline QString extract_json_object(const QString& raw) {
    const auto as_object = [](const QString& text) -> QString {
        QJsonParseError pe{};
        const QJsonDocument doc = QJsonDocument::fromJson(text.trimmed().toUtf8(), &pe);
        return doc.isObject() ? QString::fromUtf8(doc.toJson(QJsonDocument::Compact)) : QString();
    };
    if (const QString whole = as_object(raw); !whole.isEmpty())
        return whole;
    static const QRegularExpression fence(QStringLiteral("```(?:json)?\\s*(\\{.*?\\})\\s*```"),
                                          QRegularExpression::DotMatchesEverythingOption);
    if (const auto m = fence.match(raw); m.hasMatch()) {
        if (const QString fenced = as_object(m.captured(1)); !fenced.isEmpty())
            return fenced;
    }
    // Every '{' may start the object (prose can contain a stray brace first).
    for (qsizetype start = raw.indexOf(QLatin1Char('{')); start >= 0; start = raw.indexOf(QLatin1Char('{'), start + 1)) {
        int depth = 0;
        bool in_str = false;
        bool esc = false;
        for (qsizetype i = start; i < raw.size(); ++i) {
            const QChar ch = raw.at(i);
            if (in_str) {
                if (esc)
                    esc = false;
                else if (ch == QLatin1Char('\\'))
                    esc = true;
                else if (ch == QLatin1Char('"'))
                    in_str = false;
                continue;
            }
            if (ch == QLatin1Char('"')) {
                in_str = true;
            } else if (ch == QLatin1Char('{')) {
                ++depth;
            } else if (ch == QLatin1Char('}') && --depth == 0) {
                if (const QString found = as_object(raw.mid(start, i - start + 1)); !found.isEmpty())
                    return found;
                break;
            }
        }
    }
    return {};
}

/// Claude Code's `result` event can carry the schema-conforming object as
/// `structured_output` (kept in the Usage event's raw_json, which is the
/// result object minus its text). Empty when absent.
inline QString structured_output_from_result_raw(const QString& raw_json) {
    if (!raw_json.contains(QLatin1String("structured_output")))
        return {};
    const QJsonObject o = QJsonDocument::fromJson(raw_json.toUtf8()).object();
    const QJsonValue so = o.value(QStringLiteral("structured_output"));
    if (so.isObject())
        return QString::fromUtf8(QJsonDocument(so.toObject()).toJson(QJsonDocument::Compact));
    if (so.isString())
        return extract_json_object(so.toString());
    return {};
}

/// The harness result in arena terms. A reply without any JSON object is still
/// handed on as the raw text (success): the engine's parser reports
/// "no JSON object found" and runs its one repair retry.
inline ArenaHarnessOutcome map_result(const harness::HarnessChatResult& r, const QString& structured_output = {}) {
    ArenaHarnessOutcome out;
    out.latency_ms = r.duration_ms;
    constexpr qint64 kIntMax = 2147483647;
    out.prompt_tokens = static_cast<int>(qBound(qint64(0), r.input_tokens, kIntMax));
    out.completion_tokens = static_cast<int>(qBound(qint64(0), r.output_tokens, kIntMax));
    if (!r.success) {
        if (r.failure == harness::FailureKind::Timeout)
            out.error = QStringLiteral("timeout");
        else if (r.cancelled)
            out.error = QStringLiteral("cancelled");
        else
            out.error = r.error.isEmpty() ? QStringLiteral("the harness could not answer") : r.error.left(1000);
        return out;
    }
    QString json = structured_output.isEmpty() ? extract_json_object(r.content) : structured_output;
    out.content = json.isEmpty() ? r.content : json;
    if (out.content.trimmed().isEmpty()) {
        out.error = QStringLiteral("empty completion");
        return out;
    }
    out.success = true;
    return out;
}

} // namespace fincept::arena::harness_arena
