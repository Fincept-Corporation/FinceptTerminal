#include "core/HealthMonitor.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "python/PythonRunner.h"
#include "trading/AccountManager.h"
#include "trading/BrokerAccount.h"
#include "trading/DataStreamManager.h"

#include <QJsonArray>

namespace fincept {

namespace {
constexpr const char* HM_TAG = "HealthMonitor";
} // namespace

// ── HealthStatus ─────────────────────────────────────────────────────────────

bool HealthStatus::all_ok() const {
    for (const auto& c : checks) {
        if (!c.ok)
            return false;
    }
    return true;
}

QJsonObject HealthStatus::to_json() const {
    QJsonArray arr;
    for (const auto& c : checks) {
        arr.append(QJsonObject{
            {"name", c.name},
            {"ok", c.ok},
            {"detail", c.detail},
        });
    }
    return QJsonObject{
        {"all_ok", all_ok()},
        {"checks", arr},
    };
}

// ── HealthMonitor ────────────────────────────────────────────────────────────

HealthMonitor& HealthMonitor::instance() {
    static HealthMonitor inst;
    return inst;
}

HealthStatus HealthMonitor::check() const {
    HealthStatus status;
    status.checks.append(check_broker_connections());
    status.checks.append(check_websocket_streams());
    status.checks.append(check_python_pool());
    status.checks.append(check_datahub());
    LOG_DEBUG(HM_TAG, QString("Health snapshot: all_ok=%1 (%2 checks)")
                          .arg(status.all_ok() ? "true" : "false")
                          .arg(status.checks.size()));
    return status;
}

// Broker connection states across all accounts. Healthy if every active live
// account is Connected and none are in TokenExpired / Error. Paper-only or
// inactive accounts are ignored (they have no live connection to assess).
HealthStatus::Check HealthMonitor::check_broker_connections() const {
    HealthStatus::Check c;
    c.name = "broker_connections";
    try {
        auto& am = trading::AccountManager::instance();
        const auto accounts = am.list_accounts();

        int considered = 0;
        int connected = 0;
        int token_expired = 0;
        int errored = 0;
        QStringList problems;

        for (const auto& acct : accounts) {
            // Only live accounts hold a real broker connection. Paper-mode and
            // deactivated accounts are out of scope for connection health.
            if (!acct.is_active || acct.trading_mode != "live")
                continue;
            ++considered;
            const auto state = am.connection_state(acct.account_id);
            switch (state) {
                case trading::ConnectionState::Connected:
                    ++connected;
                    break;
                case trading::ConnectionState::TokenExpired:
                    ++token_expired;
                    problems.append(QString("%1 (token expired)").arg(acct.display_name));
                    break;
                case trading::ConnectionState::Error:
                    ++errored;
                    problems.append(QString("%1 (error)").arg(acct.display_name));
                    break;
                case trading::ConnectionState::Disconnected:
                case trading::ConnectionState::Connecting:
                    problems.append(QString("%1 (%2)")
                                        .arg(acct.display_name,
                                             trading::connection_state_str(state)));
                    break;
            }
        }

        if (considered == 0) {
            c.ok = true;
            c.detail = "No active live broker accounts";
        } else if (token_expired == 0 && errored == 0 && connected == considered) {
            c.ok = true;
            c.detail = QString("%1/%2 live accounts connected").arg(connected).arg(considered);
        } else {
            c.ok = false;
            c.detail = QString("%1/%2 connected; issues: %3")
                           .arg(connected)
                           .arg(considered)
                           .arg(problems.join(", "));
        }
    } catch (...) {
        c.ok = false;
        c.detail = "Exception while reading broker connection states";
    }
    return c;
}

// WebSocket stream health — how many per-account data streams are currently
// active. DataStreamManager tracks one AccountDataStream per active account but
// does not expose a per-stream "is the socket connected" flag publicly, so this
// check reports the count of managed streams vs the number of active live
// accounts that should have one.
HealthStatus::Check HealthMonitor::check_websocket_streams() const {
    HealthStatus::Check c;
    c.name = "websocket_streams";
    try {
        auto& dsm = trading::DataStreamManager::instance();
        const int active_streams = dsm.active_stream_ids().size();

        // Count active live accounts as the "expected" stream count.
        int expected = 0;
        const auto accounts = trading::AccountManager::instance().list_accounts();
        for (const auto& acct : accounts) {
            if (acct.is_active && acct.trading_mode == "live")
                ++expected;
        }

        if (expected == 0) {
            c.ok = true;
            c.detail = QString("%1 stream(s) active; no live accounts expecting a stream")
                           .arg(active_streams);
        } else if (active_streams >= expected) {
            c.ok = true;
            c.detail = QString("%1 stream(s) active for %2 live account(s)")
                           .arg(active_streams)
                           .arg(expected);
        } else {
            c.ok = false;
            c.detail = QString("%1 stream(s) active but %2 live account(s) expect one")
                           .arg(active_streams)
                           .arg(expected);
        }
    } catch (...) {
        c.ok = false;
        c.detail = "Exception while reading data stream manager state";
    }
    return c;
}

// Python process pool — PythonRunner enforces a concurrency limit but does not
// publicly expose active / queued counts (those members are private). We can
// only verify the interpreter is available. TODO: surface active_count_ /
// queue_.size() from PythonRunner if a public accessor is added so this check
// can report real pool pressure / backlog.
HealthStatus::Check HealthMonitor::check_python_pool() const {
    HealthStatus::Check c;
    c.name = "python_pool";
    try {
        const bool available = python::PythonRunner::instance().is_available();
        c.ok = available;
        c.detail = available
                       ? "Python interpreter available (pool active/queued counts not exposed)"
                       : "Python interpreter not available";
    } catch (...) {
        c.ok = false;
        c.detail = "Exception while querying Python runner";
    }
    return c;
}

// DataHub producer / topic health — counts known topics and how many have a
// recorded error. Healthy if no topic is currently in an error state.
HealthStatus::Check HealthMonitor::check_datahub() const {
    HealthStatus::Check c;
    c.name = "datahub";
    try {
        const auto stats = datahub::DataHub::instance().stats();
        const int topic_count = stats.size();

        int errored = 0;
        int subscribed_topics = 0;
        QStringList errored_topics;
        for (const auto& ts : stats) {
            if (ts.subscriber_count > 0)
                ++subscribed_topics;
            if (!ts.last_error.isEmpty()) {
                ++errored;
                if (errored_topics.size() < 5)
                    errored_topics.append(ts.topic);
            }
        }

        if (errored == 0) {
            c.ok = true;
            c.detail = QString("%1 topic(s), %2 with subscribers, no errors")
                           .arg(topic_count)
                           .arg(subscribed_topics);
        } else {
            c.ok = false;
            QString sample = errored_topics.join(", ");
            if (errored > errored_topics.size())
                sample += ", ...";
            c.detail = QString("%1 topic(s), %2 in error state: %3")
                           .arg(topic_count)
                           .arg(errored)
                           .arg(sample);
        }
    } catch (...) {
        c.ok = false;
        c.detail = "Exception while reading DataHub stats";
    }
    return c;
}

} // namespace fincept
