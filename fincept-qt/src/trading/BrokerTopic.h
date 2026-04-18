#pragma once
// BrokerTopic — helper for building and parsing `broker:<id>:<account_id>:<channel>` topics.
//
// Canonical topic shape (Phase 7, see docs/DATAHUB_TOPICS.md):
//   broker:<broker_id>:<account_id>:positions        — TTL 5s
//   broker:<broker_id>:<account_id>:orders           — TTL 5s
//   broker:<broker_id>:<account_id>:balance          — TTL 30s
//   broker:<broker_id>:<account_id>:ticks:<symbol>   — push-only, coalesce 100ms
//
// `<account_id>` defaults to `default` when the caller doesn't have an explicit
// one — single-account and multi-account code paths use the same format.

#include <QString>

namespace fincept::trading {

/// Canonical default account id used when the caller has no explicit one.
inline constexpr const char* kDefaultAccountId = "default";

/// Build `broker:<broker_id>:<account_id>:<channel>` or, when `sym` is
/// non-empty, `broker:<broker_id>:<account_id>:<channel>:<sym>` (used by
/// tick channels). `account_id` falls back to `default` when empty.
inline QString broker_topic(const QString& broker_id, const QString& account_id, const QString& channel,
                            const QString& sym = QString()) {
    const QString acct = account_id.isEmpty() ? QStringLiteral("default") : account_id;
    QString topic = QStringLiteral("broker:") + broker_id + QLatin1Char(':') + acct + QLatin1Char(':') + channel;
    if (!sym.isEmpty())
        topic.append(QLatin1Char(':')).append(sym);
    return topic;
}

} // namespace fincept::trading
