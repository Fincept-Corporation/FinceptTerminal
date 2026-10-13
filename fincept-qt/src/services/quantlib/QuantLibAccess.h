#pragma once
// QuantLibAccess.h — QuantLib cloud entitlements and failure classification.
//
// Every QuantLib route on the server declares a tier with
// quantlib_auth.require_tier(...). What a call costs depends on the caller's
// plan (FinceptAPI plans.quantlib_credit_cost):
//
//   Pro / enterprise accounts : every endpoint, 0 credits per call
//   Free accounts             : free-tier endpoints at 0 credits,
//                               basic-tier endpoints at 1 credit per call,
//                               standard / pro / enterprise tiers -> HTTP 403
//
// The server is authoritative — the table here only drives badges and copy in
// the UI, never blocks a request. A 403 is always shown as "needs Pro".

#include "mcp/McpTypes.h"

#include <QJsonValue>
#include <QString>

namespace fincept::services {

/// Tier an endpoint declares on the server.
enum class QuantLibTier { Free, Basic, Standard, Pro, Enterprise };

/// What one call costs the current plan.
enum class QuantLibCost {
    Included,  ///< 0 credits: a free-tier endpoint, or any endpoint on Pro
    OneCredit, ///< Free plan calling a basic-tier endpoint
    NeedsPro,  ///< Free plan calling a standard/pro/enterprise endpoint (HTTP 403)
};

/// Server tier of `endpoint` ("pricing/bs/price"). Unknown endpoints map to Pro,
/// mirroring the server's plans.normalize_endpoint_tier().
QuantLibTier quantlib_endpoint_tier(const QString& endpoint);

QuantLibCost quantlib_cost(QuantLibTier tier, bool paid_plan);

/// True while the signed-in account carries paid (Pro) entitlements.
bool quantlib_paid_plan();

enum class QuantLibFailureKind {
    NeedsPro,     ///< 403 — the plan does not include the endpoint
    OutOfCredits, ///< 402 — balance cannot cover the call
    AuthRequired, ///< 401 — no / expired session
    RateLimited,  ///< 429 — hourly or concurrency budget exhausted
    InvalidInput, ///< 400 / 422 — the request body failed validation
    Network,      ///< no HTTP answer (DNS, TLS, timeout, connection reset)
    Server,       ///< 5xx or an unreadable response
    Computation,  ///< HTTP 200 {"success": false, ...} — the computation itself failed
};

struct QuantLibFailure {
    QuantLibFailureKind kind = QuantLibFailureKind::Computation;
    int http_status = 0;    ///< 0 when the client did not report one
    QString message;        ///< server or transport text (never empty)
    bool retryable = false; ///< a plain re-run may succeed (network, 5xx, 429)
};

/// Classifies a failed QuantLibClient result. Uses `result.data.http_status`
/// when the client reports it and falls back to the server's / Qt's error text.
QuantLibFailure classify_quantlib_failure(const mcp::ToolResult& result);

/// Some handlers answer success with an in-band {"error": "<text>"} payload.
/// Returns that text, or an empty string when the payload is a real result
/// (a numeric "error" field — e.g. a calibration residual — is a result).
QString quantlib_inline_error(const QJsonValue& payload);

} // namespace fincept::services
