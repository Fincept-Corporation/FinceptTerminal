#pragma once
// McpTokenPolicy.h — what the external MCP server does with the stored bearer
// token it read from SecureStorage (key "mcp_server/token"). Header-only, Qt Core
// only; unit-tested by tests/tst_secure_read_callers.cpp.
//
// The rule that keeps every configured client working: a NEW token is minted
// only when the store was read and holds none. A token that could not be read
// may well exist — minting one then would replace it on the next store() and
// silently break every external client (Claude Code, Codex, Gemini, VS Code…)
// that has the old one configured. On a read error the server stays stopped,
// says why, and tries again with back-off.

#include "storage/secure/SecretRead.h"

#include <QString>

#include <algorithm>

namespace fincept::mcp::mcp_token {

enum class TokenPlan {
    Use,    ///< the stored token
    Create, ///< nothing stored: mint one and store it
    Wait,   ///< the store could not be read: stay stopped, retry later — never mint
};

struct TokenDecision {
    TokenPlan plan = TokenPlan::Wait;
    QString token;          ///< Use only
    QString reason;         ///< Wait only: completes "Could not read secure storage: …"
    bool transient = false; ///< Wait only: a retry can succeed without the user
};

inline TokenDecision decide(const SecretRead& stored) {
    TokenDecision d;
    if (stored.failed()) {
        d.plan = TokenPlan::Wait;
        d.reason = stored.reason();
        d.transient = stored.transient();
        return d;
    }
    const QString value = stored.found() ? stored.value.trimmed() : QString();
    if (value.isEmpty()) {
        d.plan = TokenPlan::Create; // read fine, nothing (or a blank value) stored
        return d;
    }
    d.plan = TokenPlan::Use;
    d.token = value;
    return d;
}

/// Wait before retry number `attempt` (1 = the first retry): 5 s doubling to a
/// 5-minute ceiling. Permanent errors (key unavailable, damaged row) are retried
/// on the same schedule — the user may fix them meanwhile (Settings > Security).
inline int retry_delay_ms(int attempt) {
    constexpr int kFirstMs = 5 * 1000;
    constexpr int kCeilingMs = 5 * 60 * 1000;
    const int n = std::clamp(attempt, 1, 16);
    const qint64 ms = qint64(kFirstMs) << (n - 1);
    return int(std::min<qint64>(ms, kCeilingMs));
}

} // namespace fincept::mcp::mcp_token
