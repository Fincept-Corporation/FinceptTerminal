#pragma once
// CodingAgentLiveRun.h — PRIVATE to CodingAgentService: the per-process state of
// one running agent CLI. Not part of the service's public API.

#include "services/coding_agents/AgentStreamParsers.h"

#include <QByteArray>
#include <QPointer>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QTimer>

namespace fincept::services::coding_agents {

inline constexpr qint64 kCodingAgentMaxRunMs = 4LL * 3600 * 1000; // hard cap on one run's wall-clock time
inline constexpr int kCodingAgentStopGraceMs = 3000;
inline constexpr int kCodingAgentMaxEventsPerRun = 5000;
inline constexpr int kCodingAgentStderrTailLines = 50;
inline constexpr int kCodingAgentStderrEventLimit = 200;

struct LiveRun {
    QString run_id;
    AgentKind agent = AgentKind::Claude;
    QPointer<QProcess> proc;
    QPointer<QTimer> watchdog;
    QPointer<QTimer> kill_timer;
    LineBuffer out_buf;
    LineBuffer err_buf;
    ClaudeStreamParser claude;
    CodexJsonParser codex;
    QStringList stderr_tail;
    int stderr_events = 0;
    int dropped_events = 0;
    bool stop_requested = false;
    bool timed_out = false;
    QByteArray stdin_payload;
    QString mcp_config_path; // temp --mcp-config file (Claude); deleted with the run
    qint64 pid = 0;
    void* job = nullptr; // Windows job object HANDLE (kill-on-close); unused elsewhere
    bool job_assigned = false;
    // Last values pushed into the run record, so sync_facts() only emits on change.
    QString synced_session;
    qint64 synced_in = -1;
    qint64 synced_out = -1;
    bool synced_cost = false;

    const StreamFacts& facts() const { return agent == AgentKind::Codex ? codex.facts() : claude.facts(); }
    QVector<AgentEvent> parse(const QString& line) {
        return agent == AgentKind::Codex ? codex.parse_line(line) : claude.parse_line(line);
    }
};

} // namespace fincept::services::coding_agents
