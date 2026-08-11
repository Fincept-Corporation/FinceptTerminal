#pragma once
// LlmContentExtractors.h — pure helpers for extracting user-visible text from
// the response shapes of the OpenAI / Anthropic / Gemini APIs. No state, no
// I/O — just JSON walkers used by LlmService and its tool-loop follow-ups.

#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <vector>

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

/// One tool invocation recovered from text/XML markup.
struct TextToolCall {
    QString name; // may be "<server>__<tool>" or the bare tool name
    QJsonObject args;
};

/// True when `content` carries any recognised text/XML tool-call markup.
/// Cheap pre-check — use before paying for full extraction.
bool has_text_tool_markup(const QString& content);

/// Recover tool invocations emitted as text instead of structured `tool_calls`.
/// Models without native function calling (MiniMax, some OpenRouter routes) and
/// every prompt-injected catalog path (Fincept /research/llm/async) land here.
///
/// Patterns, in precedence order — the first that yields any call wins:
///   1. <tool_call>{"name":…, "arguments":{…}}</tool_call>
///   2. <invoke name="…"> with <parameter> children or a JSON body
///   3. [ns:]tool_call … /[ns:]tool_call  (minimax style)
///   4. ```tool_call fenced JSON
///
/// A pattern-3 block whose body is not JSON is not a tool call we can name; its
/// raw text is appended to *raw_blocks (when non-null) so a caller with access
/// to the tool registry can map it onto a query/sql-shaped tool. Keeps this
/// translation unit free of MCP dependencies.
std::vector<TextToolCall> extract_text_tool_calls(const QString& content, QStringList* raw_blocks = nullptr);

} // namespace fincept::ai_chat
