#pragma once
// UsageReporter — the terminal's one usage report (POST /client/usage).
//
// The API derives most analytics from the requests themselves (User-Agent and
// the X-Fincept-Install / X-Fincept-Run headers, see ClientInfo). This adds
// only what requests cannot show: time per screen, active time, device facts
// and counts of problems the server never sees. Silent: it never shows UI,
// never blocks, never wakes an idle terminal, and every failure is logged at
// debug level only. Inert in --smoke-test / --selftest* / --dump-tools runs.
//
// Time. A span counts only while InactivityGuard::user_active() holds (not
// locked, a window visible, input in the last 10 min). Nothing ticks: time is
// settled lazily at every event that can change the answer (a screen shown or
// hidden, lock / unlock, the app (de)activated, a report) from the guard's
// last-input timestamp, plus one coarse single-shot timer aimed at the moment
// the user would turn idle (at most once a minute, normally once per 10 min,
// stopped while idle). While idle an input watcher waits for the user to come
// back. A span is split equally between the screens visible at once, so the
// per-screen sum never exceeds the wall time.
//
// Sending. Only when signed in, active, not IP-blocked and at most once per
// minute: "start" (first signed-in moment) and "login" at once, then
// "periodic" once the server's next_s (default 900 s) has passed. Checks ride
// on events that mean the user is there; one request in flight at a time.
// What is not sent by quit is written to <AppPaths::data()>/usage/ and goes
// out as that run's "quit" report at the next signed-in start.

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

class QJsonDocument;
class QUrl;

namespace fincept {
class DockScreenRouter;
}

namespace fincept::telemetry {

/// Time and opens of one screen not yet reported.
struct UsageScreenDelta {
    qint64 active_ms = 0;
    int opens = 0;
};

class UsageReporter : public QObject {
    Q_OBJECT
  public:
    static UsageReporter& instance();

    /// From TerminalShell::initialise() once the crash check has run.
    /// `previous_exit`: "clean", "crash" or "unknown". GUI thread.
    void start(const QString& previous_exit);

    /// From TerminalShell::shutdown(): settle the time and save what was not
    /// sent for the next start. Never touches the network. GUI thread.
    void shutdown();

    // ── One-line hooks. Each is a cheap no-op while the reporter is not running.

    /// DockScreenRouter: a screen's dock widget was shown or hidden. `bulk` is
    /// the router's "registering / placing panels" flag — a show then is not
    /// an open by itself. GUI thread.
    static void screen_visibility(DockScreenRouter* router, const QString& screen_id, bool visible, bool bulk);

    /// Count a problem (kind: ui_stall, python_error, http_error, crash,
    /// setup_error, action_error). Aggregated by (kind, where, code); the latest
    /// sample is kept, scrubbed (UsageScrub.h). Any thread.
    static void note_issue(const QString& kind, const QString& where, const QString& code,
                           const QString& sample = QString());
    /// A GUI stall that has ended; `context` is UiStallMonitor's ("screen=<id>"). Any thread.
    static void note_ui_stall(const QString& context, qint64 duration_ms);
    /// A failed Python script; only its file name is kept. Any thread.
    static void note_python_error(const QString& script, const QString& code, const QString& error_text);
    /// A Fincept API call that got no answer (status 0) or a 5xx; `where` is the
    /// route with ids removed. Any thread.
    static void note_http_error(const QUrl& url, int status, int network_error, const QString& error_text);

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

  private:
    UsageReporter() = default;

    void on_screen_visibility(DockScreenRouter* router, const QString& screen_id, bool visible, bool bulk);
    void on_screen_changed(DockScreenRouter* router, const QString& screen_id);
    void watch_router(DockScreenRouter* router);
    void count_open(const QString& screen_id);
    void on_auth_state_changed();

    void accrue();
    void arm_activity_watch();
    void set_input_watch(bool on);
    void on_activity_changed();

    void maybe_send();
    void send_current(const QString& reason);
    void on_current_answer(bool ok, const QJsonDocument& doc);
    void send_saved_run(const QString& path);
    void merge_back();
    void back_off();

    QString pending_dir() const;
    QString own_pending_path() const;
    void scan_saved_runs();
    void save_unsent(bool final_report);

    bool running_ = false;
    bool disabled_ = false; ///< the server has no /client/usage (404): quiet for this run
    QString previous_exit_;
    qint64 started_ms_ = 0;

    // Time
    qint64 last_accrue_ms_ = 0;
    qint64 last_input_ms_ = 0; ///< last input known at last_accrue_ms_
    bool gate_open_ = false;   ///< not locked and a window visible, as of last_accrue_ms_
    qint64 active_ms_ = 0;     ///< not yet reported
    QHash<QString, UsageScreenDelta> screens_;
    /// Visible screens per router; the value says whether their open was counted.
    QHash<DockScreenRouter*, QHash<QString, bool>> visible_;
    QTimer idle_timer_;
    bool watching_input_ = false;
    bool input_seen_ = false;

    // Sending
    QString pending_reason_ = QStringLiteral("start");
    bool authenticated_ = false;
    bool seen_signed_out_ = false;
    int seq_ = 1;
    int interval_s_ = 900;
    qint64 last_attempt_ms_ = 0;
    qint64 last_report_ms_ = 0;
    qint64 retry_not_before_ms_ = 0;
    int backoff_s_ = 0;
    bool in_flight_ = false;
    bool saved_runs_scanned_ = false;
    QStringList saved_runs_;
    // The batch on the wire, merged back on a network error / 5xx.
    bool inflight_current_ = false;
    qint64 inflight_active_s_ = 0;
    QHash<QString, UsageScreenDelta> inflight_screens_;
};

} // namespace fincept::telemetry
