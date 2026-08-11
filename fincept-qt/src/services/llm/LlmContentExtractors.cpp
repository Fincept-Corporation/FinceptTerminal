// LlmContentExtractors.cpp — pure JSON walkers for provider response shapes.

#include "services/llm/LlmContentExtractors.h"

#include <QJsonDocument>
#include <QJsonValue>
#include <QRegularExpression>
#include <QVector>

namespace fincept::ai_chat {

QString extract_openai_message_text(const QJsonObject& msg) {
    // Plain string content — the standard shape
    QJsonValue cv = msg["content"];
    if (cv.isString()) {
        QString s = cv.toString();
        if (!s.isEmpty())
            return s;
    } else if (cv.isArray()) {
        // Some providers (GPT-4o vision, OpenRouter multimodal echoes) return
        // content as an array of {type, text} parts. Concatenate all text parts.
        QString joined;
        for (const auto& pv : cv.toArray()) {
            QJsonObject p = pv.toObject();
            QString type = p["type"].toString();
            if (type == "text" || type == "output_text")
                joined += p["text"].toString();
        }
        if (!joined.isEmpty())
            return joined;
    }

    // Reasoning models (kimi-k2.5/k2.6/k2-thinking, deepseek-reasoner, some
    // xAI grok-4 reasoning variants) put the final answer in `reasoning_content`
    // when max_tokens is exhausted mid-reasoning, or when `content` is deliberately
    // empty. Fall back so the user sees the chain-of-thought instead of nothing.
    QString rc = msg["reasoning_content"].toString();
    if (!rc.isEmpty())
        return rc;

    // Some OpenAI-compatible providers (newer OpenAI, Groq) emit a `refusal`
    // field instead of content when the model declines. Surfacing it is better
    // than returning blank.
    QString refusal = msg["refusal"].toString();
    if (!refusal.isEmpty())
        return refusal;

    return {};
}

QString extract_anthropic_content_text(const QJsonArray& content) {
    QString text;
    QString thinking_fallback;
    for (const auto& bv : content) {
        QJsonObject b = bv.toObject();
        const QString type = b["type"].toString();
        if (type == "text") {
            text += b["text"].toString();
        } else if (type == "thinking" && thinking_fallback.isEmpty()) {
            thinking_fallback = b["thinking"].toString();
        }
    }
    if (!text.isEmpty())
        return text;
    return thinking_fallback;
}

QString extract_gemini_parts_text(const QJsonArray& parts) {
    QString text;
    QString thought_fallback;
    for (const auto& pv : parts) {
        QJsonObject p = pv.toObject();
        if (p.contains("functionCall"))
            continue;
        QString t = p["text"].toString();
        if (t.isEmpty())
            continue;
        if (p["thought"].toBool()) {
            if (thought_fallback.isEmpty())
                thought_fallback = t;
        } else {
            text += t;
        }
    }
    if (!text.isEmpty())
        return text;
    return thought_fallback;
}

// ── Text/XML tool-call recovery ────────────────────────────────────────────

namespace {

// Every brace-balanced top-level JSON object inside `s`, in order.
//
// A single <tool_call> block is NOT reliably one JSON object. MiniMax batches
// several calls into one block, newline-separated:
//
//   <tool_call>{"name":"tool_list","arguments":{...}}
//   {"name":"tool_list","arguments":{...}} </tool_call>
//
// Handing that whole span to QJsonDocument::fromJson fails (two concatenated
// objects aren't a document), which used to yield ZERO extracted calls and let
// the raw markup fall through into the chat bubble as if it were the answer.
// Scanning for balanced spans instead also covers a JSON-array body — the outer
// `[` is simply skipped and each element is picked up on its own.
//
// String-aware: braces inside quoted values (and escaped quotes) don't count.
QVector<QJsonObject> scan_json_objects(const QString& s) {
    QVector<QJsonObject> out;
    int depth = 0;
    qsizetype start = -1;
    bool in_str = false;
    bool esc = false;
    for (qsizetype i = 0; i < s.size(); ++i) {
        const QChar c = s[i];
        if (in_str) {
            if (esc)
                esc = false;
            else if (c == QLatin1Char('\\'))
                esc = true;
            else if (c == QLatin1Char('"'))
                in_str = false;
            continue;
        }
        if (c == QLatin1Char('"')) {
            in_str = true;
        } else if (c == QLatin1Char('{')) {
            if (depth == 0)
                start = i;
            ++depth;
        } else if (c == QLatin1Char('}') && depth > 0) {
            if (--depth == 0 && start >= 0) {
                const auto doc = QJsonDocument::fromJson(s.mid(start, i - start + 1).toUtf8());
                if (doc.isObject())
                    out.append(doc.object());
                start = -1;
            }
        }
    }
    return out;
}

// Pull {name, arguments} out of a JSON object emitted inside tool markup.
// `arguments` is accepted both as an object and as a JSON-encoded string —
// models disagree on which, and both appear in the wild from the same model.
bool parse_tool_call_object(const QJsonObject& obj, TextToolCall& out) {
    QString name = obj["name"].toString();
    if (name.isEmpty())
        name = obj["function"].toString();
    if (name.isEmpty())
        return false;
    QJsonObject args = obj["arguments"].toObject();
    if (args.isEmpty() && obj["arguments"].isString())
        args = QJsonDocument::fromJson(obj["arguments"].toString().toUtf8()).object();
    out = {name, args};
    return true;
}

} // namespace

bool has_text_tool_markup(const QString& content) {
    static const QRegularExpression rx(QStringLiteral("<\\s*/?\\s*(?:\\w+\\s*:\\s*)?tool_call\\b"
                                                      "|<\\s*invoke\\s+name="
                                                      "|```\\s*tool_call\\b"),
                                       QRegularExpression::CaseInsensitiveOption);
    return rx.match(content).hasMatch();
}

std::vector<TextToolCall> extract_text_tool_calls(const QString& content, QStringList* raw_blocks) {
    std::vector<TextToolCall> calls;

    // --- Pattern 1: <tool_call> … </tool_call>, optionally namespaced ---
    // The body is scanned for balanced JSON objects rather than parsed whole, so
    // a block carrying several calls yields several calls.
    {
        static const QRegularExpression rx("<\\s*(?:\\w+\\s*:\\s*)?tool_call\\s*>([\\s\\S]*?)<\\s*/\\s*"
                                           "(?:\\w+\\s*:\\s*)?tool_call\\s*>",
                                           QRegularExpression::CaseInsensitiveOption |
                                               QRegularExpression::DotMatchesEverythingOption);
        auto it = rx.globalMatch(content);
        bool any_block = false;
        while (it.hasNext()) {
            auto m = it.next();
            any_block = true;
            for (const QJsonObject& obj : scan_json_objects(m.captured(1))) {
                TextToolCall tc;
                if (parse_tool_call_object(obj, tc))
                    calls.push_back(tc);
            }
        }

        // Unterminated block — the model opened <tool_call> and ran out of
        // tokens (or just omitted the close). Scan the tail rather than lose
        // the call and print the markup at the user.
        if (!any_block) {
            static const QRegularExpression rx_open("<\\s*(?:\\w+\\s*:\\s*)?tool_call\\s*>",
                                                    QRegularExpression::CaseInsensitiveOption);
            const auto om = rx_open.match(content);
            if (om.hasMatch()) {
                for (const QJsonObject& obj : scan_json_objects(content.mid(om.capturedEnd()))) {
                    TextToolCall tc;
                    if (parse_tool_call_object(obj, tc))
                        calls.push_back(tc);
                }
            }
        }
    }

    // --- Pattern 2: <invoke name="…"> with <parameter> children or JSON body ---
    if (calls.empty()) {
        static const QRegularExpression rx_invoke("<invoke\\s+name=\"([^\"]+)\"[^>]*>([\\s\\S]*?)</invoke>",
                                                  QRegularExpression::MultilineOption |
                                                      QRegularExpression::DotMatchesEverythingOption);
        static const QRegularExpression rx_param("<parameter\\s+name=\"([^\"]+)\"[^>]*>([\\s\\S]*?)</parameter>",
                                                 QRegularExpression::MultilineOption |
                                                     QRegularExpression::DotMatchesEverythingOption);
        auto it = rx_invoke.globalMatch(content);
        while (it.hasNext()) {
            auto m = it.next();
            QString name = m.captured(1);
            QString body = m.captured(2).trimmed();
            QJsonObject args;

            auto pit = rx_param.globalMatch(body);
            bool has_params = false;
            while (pit.hasNext()) {
                auto pm = pit.next();
                const QString pname = pm.captured(1);
                const QString pval = pm.captured(2).trimmed();
                has_params = true;
                // Parse as JSON so arrays/objects survive; fall back to the literal.
                auto pdoc = QJsonDocument::fromJson(pval.toUtf8());
                if (pdoc.isArray())
                    args[pname] = pdoc.array();
                else if (pdoc.isObject())
                    args[pname] = pdoc.object();
                else
                    args[pname] = pval;
            }

            if (!has_params && !body.isEmpty()) {
                auto jdoc = QJsonDocument::fromJson(body.toUtf8());
                if (jdoc.isObject())
                    args = jdoc.object();
            }

            if (!name.isEmpty())
                calls.push_back({name, args});
        }
    }

    // --- Pattern 3: [ns:]tool_call … /[ns:]tool_call ---
    if (calls.empty()) {
        static const QRegularExpression rx("(?:\\w+:)?tool_call\\s+([\\s\\S]*?)\\s*/(?:\\w+:)?tool_call",
                                           QRegularExpression::MultilineOption);
        auto it = rx.globalMatch(content);
        while (it.hasNext()) {
            auto m = it.next();
            const QString body = m.captured(1).trimmed();
            bool got = false;
            for (const QJsonObject& obj : scan_json_objects(body)) {
                TextToolCall tc;
                if (parse_tool_call_object(obj, tc)) {
                    calls.push_back(tc);
                    got = true;
                }
            }
            if (got)
                continue;
            // Not JSON — an unnamed raw command (SQL, …). Hand it back for the
            // caller to map; we can't name a tool without the registry.
            if (raw_blocks && !body.isEmpty())
                raw_blocks->append(body);
        }
    }

    // --- Pattern 4: ```tool_call fenced JSON ---
    if (calls.empty()) {
        static const QRegularExpression rx("```tool_call\\s*\\n([\\s\\S]*?)\\n\\s*```",
                                           QRegularExpression::MultilineOption);
        auto it = rx.globalMatch(content);
        while (it.hasNext()) {
            auto m = it.next();
            for (const QJsonObject& obj : scan_json_objects(m.captured(1))) {
                TextToolCall tc;
                if (parse_tool_call_object(obj, tc))
                    calls.push_back(tc);
            }
        }
    }

    return calls;
}

} // namespace fincept::ai_chat
