// AdapterFactory.cpp — make_adapter() implementation.
//
// Dispatches a provider id string to the correct ProviderAdapter
// implementation. Phase 5 ships with the OpenAI adapter; subsequent sessions
// add Anthropic, Gemini, Fincept, and TextRegex adapters here as they
// migrate out of LlmService.cpp.

#include "mcp/dispatch/ProviderAdapter.h"

#include "mcp/dispatch/OpenAiAdapter.h"

namespace fincept::mcp::dispatch {

std::unique_ptr<ProviderAdapter> make_adapter(const QString& provider_id) {
    // OpenAI plus all OpenAI-compatible providers (Groq, OpenRouter,
    // DeepSeek, Together, Ollama-compat, etc.) share the same wire shape.
    // The adapter only cares about the format, not the host — auth/URL
    // details stay in LlmService where they're configured.
    if (provider_id == "openai" || provider_id == "groq" || provider_id == "openrouter" ||
        provider_id == "ollama" || provider_id == "deepseek" || provider_id == "together") {
        return std::make_unique<OpenAiAdapter>();
    }

    // Anthropic, Gemini, Fincept, and TextRegex adapters land in a
    // subsequent session — see Task 5.4–5.7. Until they exist, callers
    // (LlmService) keep their existing inline tool-call paths for those
    // providers and only the OpenAI path uses the dispatcher.
    return nullptr;
}

} // namespace fincept::mcp::dispatch
