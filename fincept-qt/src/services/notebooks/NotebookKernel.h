#pragma once
// NotebookKernel — one long-lived Python process per open notebook (the Code
// tab's "kernel"): cells run one at a time in the same interpreter, so
// variables, imports and loaded data persist between cells, as in Jupyter.
//
// The process is scripts/notebook_cell_runner.py in "kernel" mode, started on
// the first run (never on open), with the app's standard Python environment
// (PythonRunner::build_python_env) and the notebook's interpreter (venv-numpy2,
// or venv-numpy1 for the vectorbt / backtesting.py stack). It is a dedicated
// process, like the exchange daemon: it does not hold one of PythonRunner's
// three shared slots while it idles. Protocol: one JSON command per stdin
// line; events on stdout as "\x1eFNB {json}" lines (see the script's header).
//
// Interrupt = KeyboardInterrupt in the running cell. A cell that ignores it is
// stopped by the kernel itself after a grace period (it reports, then exits);
// should even that not happen this class kills the process. Either way the run
// finishes with kernel_lost = true and the next run starts a fresh kernel.
// Every signal is emitted on the owner (GUI) thread; nothing here blocks.

#include <QByteArray>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QProcess>
#include <QString>

class QTimer;

namespace fincept::services {

class NotebookKernel : public QObject {
    Q_OBJECT
  public:
    enum class State {
        Off,          ///< no process (not started yet, shut down, or stopped)
        Starting,     ///< process launched, "ready" not received yet
        Idle,         ///< ready, nothing running
        Busy,         ///< a cell is running
        Interrupting, ///< interrupt sent, waiting for the cell to stop
        Unavailable,  ///< cannot start: Python not set up / environment missing (status_reason())
    };
    Q_ENUM(State)

    explicit NotebookKernel(QObject* parent = nullptr);
    ~NotebookKernel() override;

    /// "numpy2" (default) or "numpy1". Takes effect at the next start; the
    /// caller restarts a running kernel when the user switches.
    void set_environment(const QString& env);
    QString environment() const { return env_; }
    /// Current directory of the kernel (the notebook's folder). Next start.
    void set_working_dir(const QString& dir) { working_dir_ = dir; }
    /// Chart colours for matplotlib figures, from the app's theme:
    /// {"bg", "fg", "dim", "grid", "edge", "cycle": [colour, ...]}. Sent to a
    /// running kernel at once; given to the next one at start.
    void set_chart_style(const QJsonObject& style);

    State state() const { return state_; }
    bool alive() const;
    /// Python version reported by the running kernel ("3.11.9"), empty before.
    QString python_version() const { return version_; }
    /// Why the kernel cannot start (State::Unavailable) or why it stopped.
    QString status_reason() const { return reason_; }
    /// Id of the cell run in flight, empty when idle.
    QString current_run() const { return run_id_; }
    qint64 current_run_elapsed_ms() const;

    /// Empty when `env`'s interpreter can be started, else a plain sentence
    /// (Python setup still installing / not set up / numpy1 env missing).
    static QString start_problem(const QString& env);

    /// Start the process now (no code runs). False with `why` when it cannot.
    bool start(QString* why = nullptr);
    /// Run `code` as cell run `run_id` (starting the kernel if needed). One run
    /// at a time: false when another is in flight or the kernel cannot start.
    bool execute(const QString& run_id, const QString& code, int timeout_s, QString* why = nullptr);
    /// KeyboardInterrupt in the running cell (no-op when idle).
    void interrupt();
    /// Stop the process (variables are lost) and start a fresh one.
    void restart();
    /// Stop the process. A run in flight finishes as cancelled.
    void shutdown();

    /// Ask for the variables event again (sent after every run anyway).
    void request_variables();
    /// Help for a dotted name ("pd.DataFrame.merge"): reply() with type "inspect".
    void inspect(const QString& request_id, const QString& name);
    /// Completions of `prefix` ("df.ro"): reply() with type "complete".
    void complete(const QString& request_id, const QString& prefix);
    /// A variable's value (table for DataFrames): reply() with type "preview".
    void preview(const QString& request_id, const QString& name);

  signals:
    void state_changed(fincept::services::NotebookKernel::State state);
    void ready(const QString& python_version);
    void exec_started(const QString& run_id);
    /// Printed text of the running cell, in order, batched by the kernel (~10/s).
    void exec_stream(const QString& run_id, const QString& text, bool is_stderr);
    /// "result", "display" or "error" event of the running cell.
    void exec_event(const QString& run_id, const QJsonObject& event);
    /// Exactly once per accepted execute(). status: ok | error | cancelled | timeout.
    void exec_finished(const QString& run_id, const QString& status, qint64 elapsed_ms, bool truncated,
                       bool kernel_lost);
    void variables_changed(const QJsonArray& items, int total);
    /// "inspect" / "complete" / "preview" answers (carry the request id).
    void reply(const QJsonObject& event);
    /// The process ended. `unexpected` when nobody asked it to (crash, killed).
    void stopped(const QString& reason, bool unexpected);

  private:
    void set_state(State s);
    void send(const QJsonObject& command);
    void on_stdout();
    void on_stderr();
    void handle_line(const QByteArray& raw);
    void handle_event(const QJsonObject& ev);
    void finish_run(const QString& status, qint64 elapsed_ms, bool truncated, bool kernel_lost);
    void fail_run(const QString& ename, const QString& evalue);
    void on_process_gone(QProcess* proc, const QString& reason);
    void detach_process();

    QString env_ = QStringLiteral("numpy2");
    QString working_dir_;
    QJsonObject chart_style_;
    QProcess* proc_ = nullptr;
    State state_ = State::Off;
    QString version_;
    QString reason_;
    QString run_id_;
    QElapsedTimer run_clock_;
    QByteArray out_buf_;
    QByteArray err_tail_;
    QTimer* backstop_ = nullptr; ///< kills a kernel that does not answer an interrupt / passes its deadline
    bool stopping_ = false;      ///< the current process is being shut down on purpose
};

} // namespace fincept::services
