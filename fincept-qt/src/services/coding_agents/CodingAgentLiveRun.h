#pragma once
// CodingAgentLiveRun.h — PRIVATE to CodingAgentService: the per-process state of
// one running agent CLI. Not part of the service's public API.

#include "services/coding_agents/AgentStreamParsers.h"
#include "services/harness/HarnessLaunch.h"
#include "services/harness/IHarnessAdapter.h"

#include <QByteArray>
#include <QPointer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <memory>

namespace fincept::services::coding_agents {

inline constexpr qint64 kCodingAgentMaxRunMs = static_cast<qint64>(kRunTimeLimitMaxSeconds) * 1000; // hard cap
inline constexpr int kCodingAgentStopGraceMs = 3000;
inline constexpr int kCodingAgentMaxEventsPerRun = 5000;
inline constexpr int kCodingAgentStderrTailLines = 50;
inline constexpr int kCodingAgentStderrEventLimit = 200;

struct LiveRun {
    QString run_id;
    QString harness; // registry id
    // ── Launch, prepared on the GUI thread without I/O; the folders and per-run
    //    files are created on the service's file thread before the process starts.
    bool preparing = true;           // false once the process was started (or failed to)
    harness::LaunchSpec spec;
    QProcessEnvironment env;
    QStringList host_env_names;      // Flatpak: variables mirrored to the host CLI (harness::host_env_names)
    QString workdir;
    QString display_name;            // "Claude Code"
    QString version;                 // the installation's version ("" = unknown)
    QString shown_command;           // redacted, for the transcript and the run header
    QString tools_note;              // "http://… (server `fincept`)" or "off"
    bool resume = false;
    QPointer<QProcess> proc;
    QPointer<QTimer> watchdog;
    QPointer<QTimer> kill_timer;
    LineBuffer out_buf;
    LineBuffer err_buf;
    /// The adapter's parser for this run (adapter->make_parser()). Never null
    /// once the run is live.
    std::unique_ptr<harness::IStreamParser> parser;
    QStringList stderr_tail;
    int stderr_events = 0;
    int dropped_events = 0;
    bool stop_requested = false;
    bool timed_out = false;
    qint64 time_limit_ms = 0; // what the watchdog enforces (the run's limit, or the hard cap)
    QByteArray stdin_payload;
    QStringList temp_paths; // per-run files (MCP config, prompt file, schema); deleted with the run
    qint64 pid = 0;
    void* job = nullptr; // Windows job object HANDLE (kill-on-close); unused elsewhere
    bool job_assigned = false;
    harness::CancelPlan cancel;
    // Last values pushed into the run record, so sync_facts() only emits on change.
    QString synced_session;
    qint64 synced_in = -1;
    qint64 synced_out = -1;
    qint64 synced_cached = -1;
    bool synced_cost = false;

    const StreamFacts& facts() const { return parser->facts(); }
    QVector<AgentEvent> parse(const QString& line) { return parser->parse_line(line); }
};

} // namespace fincept::services::coding_agents
