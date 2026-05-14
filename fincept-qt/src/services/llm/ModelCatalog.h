#pragma once
// ModelCatalog — per-model max output token budgets, sourced from each
// provider's public docs (verified 2026-04-28).
//
// Design notes
// ------------
// • The number returned is the **maximum output tokens** the API will accept
//   in the `max_tokens` / `max_completion_tokens` / `maxOutputTokens` field.
//   It is NOT the context window — most providers cap output below context.
// • For models whose docs publish no separate output cap (xAI, Moonshot,
//   OpenRouter pass-through), we pick a sane default below the context
//   window — sending the full context as `max_tokens` works on some APIs
//   and silently overruns on others, and burning 200k+ on a chat reply is
//   wasteful.
// • Pattern matching is glob-style on the model id (case-insensitive),
//   first-match wins. Order entries from most-specific to least-specific
//   inside each provider.
// • `output_cap()` returns 0 if no entry matched — caller should fall back
//   to a conservative default (we use 8192 in LlmService::resolved_max_tokens).
//
// Updating the catalog
// --------------------
// When a provider ships a new model, append a new entry to the relevant
// provider section in ModelCatalog.cpp. Cite the docs URL in a comment so
// the next person to touch it can verify.

#include <QString>

namespace fincept::ai_chat {

class ModelCatalog {
  public:
    /// Look up the published max output token cap for a (provider, model)
    /// pair. provider should be lowercase (e.g. "openai", "anthropic",
    /// "kimi", "gemini"). model is matched case-insensitively against
    /// the catalog's glob patterns.
    ///
    /// Returns 0 if no match — caller should use a conservative default.
    static int output_cap(const QString& provider, const QString& model);
};

} // namespace fincept::ai_chat
