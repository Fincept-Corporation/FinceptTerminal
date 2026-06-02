#pragma once
// BrokerTokenUtil — shared helpers for broker access-token lifetime tracking.
//
// Every Indian broker expires its access token on a daily wall-clock boundary
// in IST (Zerodha ~06:00, Upstox 03:30, etc.) or on a rolling N-hour window
// (Dhan 24h). Brokers record the computed expiry epoch in their
// TokenExchangeResponse::additional_data JSON under "token_expires_at" so that
// AccountManager can show an accurate connection state on startup *before* the
// live validation sweep runs. The stored value is only a hint — the
// authoritative check is IBroker::validate_session() — so it does not need to be
// exact, only conservative enough to avoid showing a long-dead token as live.

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QTime>
#include <QTimeZone>

namespace fincept::trading {

// IST is a fixed UTC+5:30 offset (India observes no DST).
inline QTimeZone ist_zone() {
    return QTimeZone(5 * 3600 + 30 * 60); // +19800s
}

// Epoch (seconds) of the next occurrence of HH:MM IST. If that time has already
// passed today, returns tomorrow's. Used for brokers whose tokens are flushed at
// a fixed daily IST time.
inline qint64 next_ist_flush_epoch(int hh, int mm) {
    const QTimeZone ist = ist_zone();
    const QDateTime now_ist = QDateTime::currentDateTimeUtc().toTimeZone(ist);
    QDateTime flush(now_ist.date(), QTime(hh, mm), ist);
    if (flush <= now_ist)
        flush = flush.addDays(1);
    return flush.toSecsSinceEpoch();
}

// Epoch (seconds) for a rolling window: now + `hours` (e.g. Dhan 24h tokens).
inline qint64 rolling_expiry_epoch(double hours) {
    return QDateTime::currentSecsSinceEpoch() + static_cast<qint64>(hours * 3600.0);
}

// Merge "token_expires_at" into an additional_data JSON string, preserving any
// existing keys. `base_json` may be empty.
inline QString with_token_expiry(const QString& base_json, qint64 expires_at_epoch) {
    QJsonObject obj = QJsonDocument::fromJson(base_json.toUtf8()).object();
    obj.insert(QStringLiteral("token_expires_at"), static_cast<double>(expires_at_epoch));
    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

// Read "token_expires_at" (epoch seconds) from an additional_data JSON string.
// Returns 0 when absent/unparseable.
inline qint64 token_expires_at_of(const QString& additional_json) {
    if (additional_json.isEmpty())
        return 0;
    const auto obj = QJsonDocument::fromJson(additional_json.toUtf8()).object();
    return static_cast<qint64>(obj.value(QStringLiteral("token_expires_at")).toDouble(0));
}

} // namespace fincept::trading
