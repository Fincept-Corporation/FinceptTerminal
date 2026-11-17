#pragma once
// AdapterSupport.h — validation and argv helpers shared by the harness adapters
// that put the prompt on the command line (Grok, Gemini, Cursor, Copilot, Qwen).
// QtCore only; pure functions.

#include "services/coding_agents/AgentStreamParsers.h"
#include "services/harness/HarnessTypes.h"

#include <QRegularExpression>
#include <QString>
#include <QStringList>

namespace fincept::harness::adapter_support {

/// A prompt or system prompt passed as one argv entry must stay well inside the
/// Windows command-line limit (32,767 UTF-16 units for the whole line).
inline constexpr int kArgvTextBudgetChars = 24000;

/// A model id that is safe as a separate argument: starts with a letter or digit
/// (never a flag), no spaces or quotes.
bool valid_model_id(const QString& model);
/// A session id for a resume flag: [A-Za-z0-9_-], 1..128, not starting with '-'.
bool valid_session_id(const QString& id);
/// 8-4-4-4-12 hexadecimal.
bool valid_uuid(const QString& id);
/// Plain server name, http(s) URL without quotes / backslashes / spaces, and a
/// valid environment-variable name — usable on a command line and in a config entry.
bool valid_mcp_endpoint(const McpEndpoint& endpoint);

/// Free text as ONE argv value: a leading '-' would be read as a flag by the
/// CLI's option parser, so such text gets one leading space (meaning unchanged).
QString argv_text(const QString& text);

/// "Authorization: Bearer <FINCEPT_MCP_TOKEN>" — the copy-paste header with the placeholder.
QString placeholder_bearer_header();

/// "X.Y.Z" when `line` names `tool` (case-insensitive) or is a bare version
/// ("1.2.3", "v1.2.3", "1.2.3-beta.1") — what yargs-based CLIs print for
/// --version. Empty otherwise, and always empty for a line naming one of
/// `foreign` (another harness answering under this name).
QString named_or_bare_version(const QString& line, const QString& tool, const QStringList& foreign);

/// Shared failure classification for argv-prompt harnesses: success when the
/// process exited 0 without an error result and produced a result or text;
/// otherwise Auth / RateLimit / Model by the given patterns (lower-case text of
/// result_error, last_error and stderr), else Other.
FailureKind classify(int exit_code, const services::coding_agents::StreamFacts& facts, const QStringList& stderr_tail,
                     const QRegularExpression& auth_rx, bool require_result);

} // namespace fincept::harness::adapter_support
