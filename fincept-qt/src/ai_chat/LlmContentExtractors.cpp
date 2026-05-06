// LlmContentExtractors.cpp — pure JSON walkers for provider response shapes.

#include "ai_chat/LlmContentExtractors.h"

#include <QJsonValue>

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

} // namespace fincept::ai_chat
