// src/algo_engine/RiskLimits.h
#pragma once
// Per-deployment risk limits beyond max order value / max daily loss. Every one
// of them only ever makes a deployment MORE conservative: it blocks new entries
// or (max drawdown) closes the position through the normal exit path and halts.
// Header-only, Qt Core.

#include <QJsonObject>
#include <QString>
#include <QTime>

#include <algorithm>

namespace fincept::algo {

struct DeploymentRiskLimits {
    /// Halt (flatten + no new entries) once realized + unrealized P&L falls this
    /// far from its peak, in the deployment's currency. 0 = off. Latched until a
    /// manual resume.
    double max_drawdown = 0;
    /// Entries per exchange trading day. 0 = unlimited.
    int max_trades_per_day = 0;
    /// New entries only between these exchange-local times ("HH:mm"); empty = any time.
    QString window_start;
    QString window_end;
    /// Refuse new entries outside the venue's regular session (pre / post / closed).
    bool regular_session_only = false;
    /// STOP first closes an open position through the normal (confirmed, approval-
    /// gated) exit path, then stops.
    bool flatten_on_stop = false;

    bool has_window() const {
        return QTime::fromString(window_start, QStringLiteral("HH:mm")).isValid() &&
               QTime::fromString(window_end, QStringLiteral("HH:mm")).isValid();
    }

    /// True when `local` (exchange-local wall-clock time) is inside the entry window
    /// (a window that wraps midnight, 22:00-02:00, is supported). Always true without one.
    bool in_window(const QTime& local) const {
        if (!has_window())
            return true;
        const QTime a = QTime::fromString(window_start, QStringLiteral("HH:mm"));
        const QTime b = QTime::fromString(window_end, QStringLiteral("HH:mm"));
        if (a <= b)
            return local >= a && local <= b;
        return local >= a || local <= b;
    }

    QJsonObject to_json() const {
        QJsonObject o;
        o["max_drawdown"] = max_drawdown;
        o["max_trades_per_day"] = max_trades_per_day;
        o["window_start"] = window_start;
        o["window_end"] = window_end;
        o["regular_session_only"] = regular_session_only;
        o["flatten_on_stop"] = flatten_on_stop;
        return o;
    }

    static DeploymentRiskLimits from_json(const QJsonObject& o) {
        DeploymentRiskLimits r;
        r.max_drawdown = std::max(0.0, o.value(QStringLiteral("max_drawdown")).toDouble(0));
        r.max_trades_per_day = std::max(0, o.value(QStringLiteral("max_trades_per_day")).toInt(0));
        r.window_start = o.value(QStringLiteral("window_start")).toString();
        r.window_end = o.value(QStringLiteral("window_end")).toString();
        r.regular_session_only = o.value(QStringLiteral("regular_session_only")).toBool(false);
        r.flatten_on_stop = o.value(QStringLiteral("flatten_on_stop")).toBool(false);
        return r;
    }
};

} // namespace fincept::algo
