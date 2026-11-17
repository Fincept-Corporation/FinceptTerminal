#pragma once
// HarnessChatBackend.h — terminal AI answered by an agent harness (Claude Code,
// Codex): what LlmService calls for a "harness:<id>" route, what the Alpha
// Arena harness client and Chat Mode's harness engine call.
//
//   HarnessChatResult r = HarnessChatBackend::complete(request, sink);   // worker thread
//
// Per call, on the GUI thread first (bounded wait): the harness is re-checked
// (installed, eligible — API-key authentication only — and consented), the
// terminal's MCP server is started when needed and a RUN-SCOPED token is minted
// with the call's tool policy (NoNavigation / paper-only enforced by the server;
// the token dies with the call). Then, on the calling worker thread, the
// process runs (HarnessChatRun: max 2 at once, fresh empty folder, read-only,
// no built-in tools, timeouts, cancel = process-tree kill). Afterwards the
// registry learns the outcome (sign-in re-check, rate-limit cooldown,
// re-detection) and the run token is retired.
//
// Credentials: the child gets the user's own environment (API-key variables
// pass unless the harness's env policy strips them; a subscription token such
// as CLAUDE_CODE_OAUTH_TOKEN is removed for terminal AI). Nothing here reads,
// stores, copies or logs a credential; the MCP run token travels only in the
// child's environment. Logs carry the feature, harness, outcome and timing —
// never prompt or answer text.

#include "services/harness/HarnessChatTypes.h"

namespace fincept::harness {

class HarnessChatBackend {
  public:
    /// Blocking. Worker threads only — refused (an error result) on the GUI thread.
    static HarnessChatResult complete(const HarnessChatRequest& req, const HarnessChatSink& sink);

    /// How long a harness that answered without its API key (a subscription
    /// session) stays out of terminal AI.
    static constexpr qint64 kSubscriptionRefusalMs = 10LL * 60 * 1000;
    /// MCP tool calls of a terminal-AI run may wait this long (approval dialogs).
    static constexpr int kMcpToolTimeoutS = 300;
};

} // namespace fincept::harness
