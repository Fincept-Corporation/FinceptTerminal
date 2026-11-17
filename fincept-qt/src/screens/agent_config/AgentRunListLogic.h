#pragma once
// AgentRunListLogic.h — the pure rules of the agents screen's ONE run list:
// every engine's runs in one table (coding-agent harness runs, local agentic
// tasks, Chat Mode cloud / local runs), with one status vocabulary, one key per
// row, one filter and one order.
//
// Header-only, Qt Core only (no widgets, no services): the model in
// AgentRunsPanel.cpp feeds it snapshots; tests can use it as is.

#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QTimeZone>

namespace fincept::screens::agent_runs {

/// Where a run lives.
enum class Engine {
    Harness, ///< CodingAgentService (Claude Code, Codex, BETA harnesses)
    Agentic, ///< AgentService local agentic task (Python agent_tasks)
    Chat,    ///< TaskRunService run of Chat Mode (engine cloud | local)
};

inline QString engine_id(Engine e) {
    switch (e) {
        case Engine::Harness:
            return QStringLiteral("harness");
        case Engine::Agentic:
            return QStringLiteral("agentic");
        case Engine::Chat:
            break;
    }
    return QStringLiteral("chat");
}

inline bool engine_from_id(const QString& id, Engine* out) {
    if (id == QLatin1String("harness"))
        *out = Engine::Harness;
    else if (id == QLatin1String("agentic"))
        *out = Engine::Agentic;
    else if (id == QLatin1String("chat"))
        *out = Engine::Chat;
    else
        return false;
    return true;
}

/// "harness:<run id>" — unique across engines (ids of different engines may collide).
inline QString run_key(Engine e, const QString& id) {
    return engine_id(e) + QLatin1Char(':') + id;
}

/// Splits a run key; a bare id (no known engine prefix) is taken as a harness run id
/// (the old Workbench saved bare harness run ids).
inline bool split_run_key(const QString& key, Engine* engine, QString* id) {
    const qsizetype colon = key.indexOf(QLatin1Char(':'));
    if (colon > 0) {
        Engine e;
        if (engine_from_id(key.left(colon), &e)) {
            *engine = e;
            *id = key.mid(colon + 1);
            return !id->isEmpty();
        }
    }
    if (key.isEmpty())
        return false;
    *engine = Engine::Harness;
    *id = key;
    return true;
}

// ── One status vocabulary ────────────────────────────────────────────────────
// queued · running · waiting (needs input / approval) · paused · succeeded ·
// failed · cancelled · interrupted

/// A status in which a run still holds or waits for a slot (counts as ACTIVE).
inline bool is_active_status(const QString& status) {
    return status == QLatin1String("queued") || status == QLatin1String("running") ||
           status == QLatin1String("waiting");
}

/// A finished run (CLEAR FINISHED / the finished filters).
inline bool is_finished_status(const QString& status) {
    return status == QLatin1String("succeeded") || status == QLatin1String("failed") ||
           status == QLatin1String("cancelled") || status == QLatin1String("interrupted");
}

/// A Python agent_tasks status in the common vocabulary.
inline QString normalize_agentic_status(const QString& raw) {
    const QString s = raw.trimmed().toLower();
    if (s == QLatin1String("running") || s == QLatin1String("planning"))
        return QStringLiteral("running");
    if (s == QLatin1String("pending") || s == QLatin1String("queued") || s == QLatin1String("created") ||
        s.isEmpty())
        return QStringLiteral("queued");
    if (s == QLatin1String("paused_for_input") || s == QLatin1String("waiting"))
        return QStringLiteral("waiting");
    if (s == QLatin1String("paused"))
        return QStringLiteral("paused");
    if (s == QLatin1String("completed") || s == QLatin1String("done") || s == QLatin1String("succeeded"))
        return QStringLiteral("succeeded");
    if (s == QLatin1String("cancelled") || s == QLatin1String("canceled"))
        return QStringLiteral("cancelled");
    if (s == QLatin1String("interrupted"))
        return QStringLiteral("interrupted");
    return QStringLiteral("failed"); // failed, failed_budget, error and anything unknown
}

/// A Chat Mode run_status_id() ("completed", "waiting_input", "budget_stopped" …) in
/// the common vocabulary.
inline QString normalize_chat_status(const QString& raw) {
    const QString s = raw.trimmed().toLower();
    if (s == QLatin1String("queued"))
        return QStringLiteral("queued");
    if (s == QLatin1String("planning") || s == QLatin1String("running"))
        return QStringLiteral("running");
    if (s.startsWith(QLatin1String("waiting")))
        return QStringLiteral("waiting");
    if (s == QLatin1String("paused"))
        return QStringLiteral("paused");
    if (s == QLatin1String("completed") || s == QLatin1String("succeeded"))
        return QStringLiteral("succeeded");
    if (s == QLatin1String("cancelled"))
        return QStringLiteral("cancelled");
    if (s == QLatin1String("interrupted"))
        return QStringLiteral("interrupted");
    return QStringLiteral("failed"); // failed, budget_stopped, unknown
}

/// An agent_tasks timestamp (ISO-8601 in UTC without a zone, or epoch seconds /
/// milliseconds) as epoch ms; 0 when unreadable.
inline qint64 parse_task_time_ms(const QString& text) {
    const QString t = text.trimmed();
    if (t.isEmpty())
        return 0;
    bool ok = false;
    const double num = t.toDouble(&ok);
    if (ok && num > 0)
        return num > 1e12 ? static_cast<qint64>(num) : static_cast<qint64>(num * 1000.0);
    QDateTime dt = QDateTime::fromString(t, Qt::ISODateWithMs);
    if (!dt.isValid())
        dt = QDateTime::fromString(t.left(19), Qt::ISODate);
    if (!dt.isValid())
        return 0;
    if (dt.timeSpec() == Qt::LocalTime)
        dt.setTimeZone(QTimeZone::UTC); // the runner writes UTC without a zone
    return dt.toMSecsSinceEpoch();
}

// ── One row, one filter, one order ──────────────────────────────────────────

struct RunRow {
    Engine engine = Engine::Harness;
    QString id;
    QString title;
    QString status;        ///< common vocabulary (above)
    QString pill;          ///< the status word shown ("RATE LIMITED", "NEEDS INPUT" …)
    QString engine_detail; ///< harness id, or "cloud" / "local" for Chat Mode
    QString engine_label;  ///< "Claude Code 1.0", "AGENTIC", "CHAT · CLOUD"
    QString model;
    QString origin;        ///< "schedule name", "AI assistant", "follow-up", "chat" …
    QString search;        ///< title + prompt + model, for the text filter
    QString error;
    qint64 created_ms = 0;
    qint64 started_ms = 0;
    qint64 finished_ms = 0;
    qint64 tokens_in = -1; ///< -1 = not reported
    qint64 tokens_out = -1;
    double cost = -1.0;    ///< < 0 = not reported
    bool cost_in_credits = false;

    QString key() const { return run_key(engine, id); }
    /// When the run started, else when it was created (the list's time column).
    qint64 when_ms() const { return started_ms > 0 ? started_ms : created_ms; }
    /// Wall-clock duration so far (or in total once finished); 0 if never started.
    qint64 duration_ms(qint64 now_ms) const {
        if (started_ms <= 0)
            return 0;
        const qint64 end = finished_ms > 0 ? finished_ms : now_ms;
        return end > started_ms ? end - started_ms : 0;
    }
};

/// The filter of the list. status: "" all, "active" (queued / running / waiting), or a
/// status of the vocabulary. engine: "" all, "harness", "harness:<harness id>",
/// "agentic", "chat", "chat:<cloud|local>". text: case-insensitive substring of
/// RunRow::search.
struct RunFilter {
    QString status;
    QString engine;
    QString text;

    bool active() const { return !status.isEmpty() || !engine.isEmpty() || !text.trimmed().isEmpty(); }

    bool accepts(const RunRow& r) const {
        if (status == QLatin1String("active")) {
            if (!is_active_status(r.status))
                return false;
        } else if (!status.isEmpty() && r.status != status) {
            return false;
        }
        if (!engine.isEmpty()) {
            const qsizetype colon = engine.indexOf(QLatin1Char(':'));
            const QString kind = colon < 0 ? engine : engine.left(colon);
            if (kind != engine_id(r.engine))
                return false;
            if (colon >= 0 && engine.mid(colon + 1) != r.engine_detail)
                return false;
        }
        const QString t = text.trimmed();
        if (!t.isEmpty() && !r.search.contains(t, Qt::CaseInsensitive))
            return false;
        return true;
    }
};

/// The list's natural order: active runs first, then newest first; ties by key so
/// the order is stable across refreshes.
inline bool run_before(const RunRow& a, const RunRow& b) {
    const bool aa = is_active_status(a.status);
    const bool ba = is_active_status(b.status);
    if (aa != ba)
        return aa;
    if (a.when_ms() != b.when_ms())
        return a.when_ms() > b.when_ms();
    return a.key() < b.key();
}

} // namespace fincept::screens::agent_runs
