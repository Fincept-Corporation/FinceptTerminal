#pragma once
// ModelListParse — a provider's "list models" answer turned into the list the
// model picker shows. Pure (Qt Core only, no logging, no network), so it runs on
// a worker thread for LlmService::fetch_models() — OpenRouter's list alone is over
// 1 MB — and tests/tst_model_list_parse.cpp covers each provider's payload shape.

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace fincept::ai_chat {

struct ParsedModelList {
    /// Model ids, sorted case-insensitively. Gemini: only models that support
    /// generateContent, "models/" prefix removed. Entries without an id are skipped.
    QStringList models;
    /// False when the body is not a JSON object (models is then empty).
    bool parsed = false;
};

/// Parse and shape the answer of the models endpoint of `provider`
/// (case-insensitive): Gemini / Google {"models":[{name, supportedGenerationMethods}]},
/// Ollama {"models":[{name}]}, Fincept {"data":{"models":[…]}} or {"data":[…]}
/// (falls back to the known MiniMax models), everything else OpenAI-compatible
/// {"data":[{id}]} (OpenAI, Anthropic, OpenRouter, Groq, DeepSeek, xAI, Kimi, …).
/// Pure; any thread.
ParsedModelList parse_model_list(const QString& provider, const QByteArray& body);

} // namespace fincept::ai_chat
