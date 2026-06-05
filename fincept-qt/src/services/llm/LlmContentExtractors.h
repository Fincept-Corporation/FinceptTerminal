#pragma once
// LlmContentExtractors.h — pure helpers for extracting user-visible text from
// the response shapes of the OpenAI / Anthropic / Gemini APIs. No state, no
// I/O — just JSON walkers used by LlmService and its tool-loop follow-ups.

#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

namespace fincept::ai_chat {

/// Pull a human-readable error message out of an LLM provider's non-2xx HTTP
/// body so callers can surface the *real* reason (bad model id, unsupported
/// parameter, quota, …) instead of Qt's generic "server replied: Bad Request".
/// Handles the shapes seen across OpenAI / AIHubMix / OpenRouter / DeepSeek /
/// Groq / Anthropic: top-level {"message":...}, nested {"error":{"message":...,
/// "metadata":{"raw":...}}}, and {"error":"<string>"}. Returns empty when the
/// body is not a recognised error object (caller then falls back to the Qt
/// transport error string).
QString parse_server_error_message(const QByteArray& body);

/// Extract user-visible text from an OpenAI-compatible `message` object.
///
/// Handles: plain `content` string, content-as-parts arrays (some providers
/// echo OpenAI Responses-style parts), `reasoning_content` for reasoning
/// models that exhaust max_tokens mid-reasoning, and `refusal` messages.
/// Never returns the empty string when the API actually replied — the caller
/// can rely on `isEmpty()` meaning "no reply".
QString extract_openai_message_text(const QJsonObject& msg);

/// Remove any <think>…</think> chain-of-thought blocks from a non-streamed
/// response. Used by tool-loop final synthesis and Gemini/Fincept fallbacks
/// where the entire reply arrives as one body — the streaming path has its
/// own incremental filter (do_streaming_request::filter_think) because tags
/// can straddle SSE chunk boundaries.
QString strip_think_blocks(QString content);

/// Extract user-visible text from an Anthropic /v1/messages content-blocks
/// array. Concatenates multiple `text` blocks (Claude can emit several when a
/// tool_use block sits between them) and falls back to `thinking` blocks only
/// when no text block exists.
QString extract_anthropic_content_text(const QJsonArray& content);

/// Extract user-visible text from a Gemini candidate's parts array. Handles
/// multiple `text` parts (Gemini often splits long replies), `thought:true`
/// parts (extended thinking — only fallback if no normal text), and silently
/// skips `functionCall` parts which the caller handles separately.
QString extract_gemini_parts_text(const QJsonArray& parts);

} // namespace fincept::ai_chat
