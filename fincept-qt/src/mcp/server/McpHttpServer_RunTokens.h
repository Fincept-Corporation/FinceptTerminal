#pragma once
// McpHttpServer_RunTokens.h — run-scoped bearer tokens of McpHttpServer.
//
// The server's long-lived token (McpHttpServer::token()) is what EXTERNAL
// clients keep in their config. In-app harness calls (terminal AI answered by
// Claude Code / Codex, Chat Mode harness tasks) get their own token instead:
// minted for one call by McpHttpServer::begin_run(), carrying a tool policy the
// server enforces itself (tools/list hides, tools/call refuses), and dead after
// end_run() or its time limit — a token that leaks out of a child process is
// worthless minutes later. Modelled on TerminalMcpBridge::begin_run().
//
// Header-only, QtCore only, thread-safe: the HTTP listener (GUI thread) checks
// tokens, worker threads mint and retire them.

#include "auth/ConstantTime.h"

#include <QByteArray>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QRandomGenerator>
#include <QString>

#include <array>
#include <optional>

namespace fincept::mcp {

/// Which tools a run-scoped token may see and call (the user's permissions in
/// Settings › AI Agents still apply on top).
enum class RunToolScope {
    All,          ///< every tool the server offers
    NoNavigation, ///< no tool that moves the user to another screen (category "navigation": ui_open, ui_workspace)
};

struct RunTokenPolicy {
    RunToolScope scope = RunToolScope::All;
    /// Every tool call of this token runs as a PAPER run: nothing it does may
    /// place, change or cancel a real order (ScopedPaperRun; refused outright).
    bool paper_only = false;
    /// Short label for the activity log ("ai_chat · claude").
    QString label;
    /// The harness adapter id the token was minted for ("claude", "codex", …),
    /// empty when unknown. Selects the client profile (McpClientProfile.h) for
    /// every request made with this token — more reliable than clientInfo.
    QString harness_id;
    /// The run this token belongs to (stable, non-secret): handed to tool handlers
    /// as ScopedCallClient::run_id(). McpHttpServer::begin_run() fills one in when
    /// the caller leaves it empty.
    QString run_id;
};

/// The scope hides tools of this category.
inline bool run_scope_hides_category(RunToolScope scope, const QString& category) {
    return scope == RunToolScope::NoNavigation && category == QLatin1String("navigation");
}

class McpRunTokenTable {
  public:
    static constexpr int kMaxLiveTokens = 64;
    static constexpr qint64 kMinTtlMs = 1000;
    static constexpr qint64 kMaxTtlMs = 6LL * 3600 * 1000;

    /// A new token valid until end() or now + ttl (clamped to [kMinTtlMs, kMaxTtlMs]).
    QString begin(const RunTokenPolicy& policy, qint64 ttl_ms, qint64 now_ms) {
        Entry e;
        e.policy = policy;
        e.created_ms = now_ms;
        e.expires_ms = now_ms + qBound(kMinTtlMs, ttl_ms, kMaxTtlMs);
        const QString token = mint();
        QMutexLocker lock(&mutex_);
        sweep_locked(now_ms);
        while (runs_.size() >= kMaxLiveTokens) {
            // Hard ceiling, oldest first: a runaway caller cannot grow the table.
            auto oldest = runs_.begin();
            for (auto it = runs_.begin(); it != runs_.end(); ++it) {
                if (it.value().created_ms < oldest.value().created_ms)
                    oldest = it;
            }
            runs_.erase(oldest);
        }
        runs_.insert(token, e);
        return token;
    }

    /// Retire a token (no-op for an unknown or empty one).
    void end(const QString& token) {
        if (token.isEmpty())
            return;
        QMutexLocker lock(&mutex_);
        runs_.remove(token);
    }

    /// The policy of a live token; nullopt for anything else (unknown, ended,
    /// expired). Compares against every live token in constant time per token,
    /// so the supplied value's timing reveals nothing.
    std::optional<RunTokenPolicy> check(const QString& supplied, qint64 now_ms) const {
        if (supplied.isEmpty())
            return std::nullopt;
        const QByteArray s = supplied.toUtf8();
        QMutexLocker lock(&mutex_);
        std::optional<RunTokenPolicy> hit;
        for (auto it = runs_.constBegin(); it != runs_.constEnd(); ++it) {
            const bool same = auth::constant_time_equals(s, it.key().toUtf8());
            if (same && it.value().expires_ms > now_ms)
                hit = it.value().policy;
        }
        return hit;
    }

    int live(qint64 now_ms) const {
        QMutexLocker lock(&mutex_);
        int n = 0;
        for (auto it = runs_.constBegin(); it != runs_.constEnd(); ++it)
            n += it.value().expires_ms > now_ms ? 1 : 0;
        return n;
    }

    void clear() {
        QMutexLocker lock(&mutex_);
        runs_.clear();
    }

  private:
    struct Entry {
        RunTokenPolicy policy;
        qint64 created_ms = 0;
        qint64 expires_ms = 0;
    };

    static QString mint() {
        std::array<quint32, 8> words{};
        QRandomGenerator::system()->fillRange(words.data(), static_cast<qsizetype>(words.size()));
        const QByteArray raw(reinterpret_cast<const char*>(words.data()), static_cast<qsizetype>(sizeof(words)));
        return QStringLiteral("run_") +
               QString::fromLatin1(raw.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
    }

    void sweep_locked(qint64 now_ms) {
        for (auto it = runs_.begin(); it != runs_.end();) {
            if (it.value().expires_ms <= now_ms)
                it = runs_.erase(it);
            else
                ++it;
        }
    }

    mutable QMutex mutex_;
    QHash<QString, Entry> runs_;
};

} // namespace fincept::mcp
