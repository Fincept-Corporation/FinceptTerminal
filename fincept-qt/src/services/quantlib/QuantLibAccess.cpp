// QuantLibAccess.cpp — QuantLib cloud entitlements and failure classification.

#include "services/quantlib/QuantLibAccess.h"

#include "auth/AuthManager.h"

#include <QJsonObject>
#include <QStringList>

#include <initializer_list>
#include <utility>

namespace fincept::services {

namespace quantlib_access_detail {

// Endpoint tier rules, first match wins. A key ending in '/' matches every
// endpoint under that prefix; any other key matches one endpoint exactly.
//
// Transcribed from the server's route declarations (FinceptAPI
// quantlib_api/*.py, require_tier(...) on each of the 590 routes) and checked
// against all of them: Free plan = 31 endpoints at 0 credits, 90 at 1 credit,
// 469 need Pro. Keep in step with the server when a route's tier changes —
// the server stays authoritative either way (a 403 always reads "needs Pro").
struct TierRule {
    const char* key;
    QuantLibTier tier;
};

constexpr TierRule kQuantLibTierRules[] = {
    // core: free foundations, basic distributions/stats, paid pricing ops
    {"core/types/", QuantLibTier::Free},
    {"core/conventions/", QuantLibTier::Free},
    {"core/math/", QuantLibTier::Free},
    {"core/periods/day-count-fraction", QuantLibTier::Free},
    {"core/autodiff/", QuantLibTier::Standard},
    {"core/ops/discount-cashflows", QuantLibTier::Standard},
    {"core/ops/forward-rate", QuantLibTier::Standard},
    {"core/ops/zero-rate-convert", QuantLibTier::Standard},
    {"core/ops/black-scholes", QuantLibTier::Pro},
    {"core/ops/black76", QuantLibTier::Pro},
    {"core/ops/gbm-paths", QuantLibTier::Pro},
    {"core/ops/var", QuantLibTier::Pro},
    {"core/legs/", QuantLibTier::Pro},
    {"core/periods/", QuantLibTier::Pro},
    {"core/", QuantLibTier::Basic},
    {"scheduling/", QuantLibTier::Free},
    // statistics: time-series model fitting / forecasting is standard tier
    {"statistics/timeseries/ar/", QuantLibTier::Standard},
    {"statistics/timeseries/arima/", QuantLibTier::Standard},
    {"statistics/timeseries/egarch/", QuantLibTier::Standard},
    {"statistics/timeseries/garch/", QuantLibTier::Standard},
    {"statistics/timeseries/gjr-garch/", QuantLibTier::Standard},
    {"statistics/timeseries/ma/", QuantLibTier::Standard},
    {"statistics/", QuantLibTier::Basic},
    {"numerical/", QuantLibTier::Basic},
    {"solver/bootstrap/", QuantLibTier::Pro},
    {"solver/calibration/", QuantLibTier::Pro},
    {"solver/finance/asw-spread", QuantLibTier::Pro},
    {"solver/finance/implied-vol", QuantLibTier::Pro},
    {"solver/finance/implied-vol-black76", QuantLibTier::Pro},
    {"solver/finance/oas", QuantLibTier::Pro},
    {"solver/", QuantLibTier::Standard},
    {"economics/", QuantLibTier::Standard},
    {"analysis/", QuantLibTier::Standard},
    {"curves/", QuantLibTier::Standard},
    {"pricing/", QuantLibTier::Pro},
    {"instruments/", QuantLibTier::Pro},
    {"stochastic/", QuantLibTier::Pro},
    {"volatility/", QuantLibTier::Pro},
    {"portfolio/", QuantLibTier::Pro},
    {"risk/", QuantLibTier::Pro},
    {"models/", QuantLibTier::Enterprise},
    {"regulatory/", QuantLibTier::Enterprise},
    {"ml/", QuantLibTier::Enterprise},
    {"physics/", QuantLibTier::Enterprise},
};

// HTTP reason phrase (as Qt reports it: "... - server replied: Forbidden") → status.
int status_from_reason(const QString& lower_text) {
    static const std::pair<const char*, int> kReasons[] = {
        {"server replied: forbidden", 403},
        {"server replied: payment required", 402},
        {"server replied: unauthorized", 401},
        {"server replied: too many requests", 429},
        {"server replied: unprocessable", 422},
        {"server replied: bad request", 400},
        {"server replied: not found", 404},
        {"server replied: internal server error", 500},
        {"server replied: bad gateway", 502},
        {"server replied: service unavailable", 503},
        {"server replied: gateway time", 504},
    };
    for (const auto& [phrase, status] : kReasons) {
        if (lower_text.contains(QLatin1String(phrase)))
            return status;
    }
    return 0;
}

bool contains_any(const QString& lower_text, std::initializer_list<const char*> needles) {
    for (const char* n : needles) {
        if (lower_text.contains(QLatin1String(n)))
            return true;
    }
    return false;
}

} // namespace quantlib_access_detail

QuantLibTier quantlib_endpoint_tier(const QString& endpoint) {
    for (const auto& rule : quantlib_access_detail::kQuantLibTierRules) {
        const QLatin1String key(rule.key);
        if (key.endsWith(QLatin1Char('/')) ? endpoint.startsWith(key) : endpoint == key)
            return rule.tier;
    }
    return QuantLibTier::Pro;
}

QuantLibCost quantlib_cost(QuantLibTier tier, bool paid_plan) {
    if (paid_plan)
        return QuantLibCost::Included;
    switch (tier) {
        case QuantLibTier::Free:
            return QuantLibCost::Included;
        case QuantLibTier::Basic:
            return QuantLibCost::OneCredit;
        case QuantLibTier::Standard:
        case QuantLibTier::Pro:
        case QuantLibTier::Enterprise:
            break;
    }
    return QuantLibCost::NeedsPro;
}

bool quantlib_paid_plan() {
    const auto& auth = auth::AuthManager::instance();
    return auth.is_authenticated() && auth.session().has_paid_plan();
}

QuantLibFailure classify_quantlib_failure(const mcp::ToolResult& result) {
    using quantlib_access_detail::contains_any;

    QuantLibFailure f;
    f.message = result.error.trimmed();
    if (f.message.isEmpty())
        f.message = QStringLiteral("The QuantLib request failed without an error message.");
    const QString lower = f.message.toLower();

    // Preferred signal: the HTTP status, when the client reports it.
    if (result.data.isObject())
        f.http_status = result.data.toObject().value(QStringLiteral("http_status")).toInt(0);
    if (f.http_status == 0)
        f.http_status = quantlib_access_detail::status_from_reason(lower);

    // The server's own wording, for clients that pass the body text through.
    if (f.http_status == 0 || f.http_status == 200) {
        if (contains_any(lower, {"included in pro", "needs pro", "upgrade at"}))
            f.http_status = 403;
        else if (contains_any(lower, {"enough credits", "insufficient_credits", "insufficient credits",
                                      "pro pass has expired", "recharge"}))
            f.http_status = 402;
        else if (contains_any(lower, {"api key required", "session expired", "unauthenticated",
                                      "invalid api key", "logged in from another device"}))
            f.http_status = 401;
        else if (contains_any(lower, {"too many simultaneous requests", "rate limit", "request budget",
                                      "too many requests", "requests per hour", "hourly"}))
            f.http_status = 429;
        else if (lower.startsWith(QLatin1String("validation error")) || lower.startsWith(QLatin1String("body ->")) ||
                 lower.contains(QLatin1String("field required")))
            f.http_status = 422;
    }

    switch (f.http_status) {
        case 401:
            f.kind = QuantLibFailureKind::AuthRequired;
            return f;
        case 402:
            f.kind = QuantLibFailureKind::OutOfCredits;
            return f;
        case 403:
            f.kind = QuantLibFailureKind::NeedsPro;
            return f;
        case 429:
            f.kind = QuantLibFailureKind::RateLimited;
            f.retryable = true;
            return f;
        case 400:
        case 404:
        case 422:
            f.kind = QuantLibFailureKind::InvalidInput;
            return f;
        default:
            break;
    }
    if (f.http_status >= 500) {
        f.kind = QuantLibFailureKind::Server;
        f.retryable = true;
        return f;
    }

    // An HTTP error Qt could not name (HTTP/2 carries no reason phrase), or a
    // response that was not JSON at all.
    if (lower.contains(QLatin1String("server replied")) || lower.startsWith(QLatin1String("json parse error")) ||
        lower.startsWith(QLatin1String("unexpected response"))) {
        f.kind = QuantLibFailureKind::Server;
        f.retryable = true;
        return f;
    }

    // No HTTP answer at all: transport-level failure (Qt's error text). The
    // needles are Qt's own phrases — a bare "network" would also match a
    // computation error such as "neural network did not converge".
    if (contains_any(lower, {"connection refused", "connection closed", "remote host closed", "timed out",
                             "operation canceled", "temporary network failure", "network session",
                             "network unreachable", "host unreachable", "ssl handshake", "proxy"}) ||
        (lower.startsWith(QLatin1String("host ")) && lower.contains(QLatin1String("not found")))) {
        f.kind = QuantLibFailureKind::Network;
        f.retryable = true;
        return f;
    }

    // HTTP 200 with {"success": false, "message": ...}: the computation failed.
    f.kind = QuantLibFailureKind::Computation;
    return f;
}

QString quantlib_inline_error(const QJsonValue& payload) {
    if (!payload.isObject())
        return {};
    const QJsonValue err = payload.toObject().value(QStringLiteral("error"));
    return err.isString() ? err.toString().trimmed() : QString();
}

} // namespace fincept::services
