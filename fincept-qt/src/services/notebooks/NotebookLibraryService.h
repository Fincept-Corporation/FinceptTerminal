#pragma once
// NotebookLibraryService.h — Catalog + seeding for the prebuilt "Fincept
// Notebook" library shipped under resources/notebooks/.
//
// Responsibilities:
//   - Parse resources/notebooks/notebooks_manifest.json into a catalog.
//   - Resolve the bundled assets dir at runtime (dev + installed layouts).
//   - Seed copies into <profile>/files/ on first run (registered with the
//     FileManagerService so they appear in the File Manager).
//   - Provide a writable working copy for the Library "OPEN" action so the
//     bundled originals stay pristine and user edits persist + stay in sync.

#include <QHash>
#include <QObject>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>
#include <memory>

class QTimer;

namespace fincept::services {

/// One cell execution request (see NotebookLibraryService::run_cell).
struct NotebookRunRequest {
    QString code;
    QString kernel = QStringLiteral("numpy2"); ///< "numpy2" (default venv) | "numpy1" (vectorbt, bt, ffn, ...)
    int timeout_s = 1800;                      ///< cell time limit; the run is interrupted when it passes
    QString working_dir;                       ///< cwd for the cell (the notebook's folder); empty = scripts dir
};

/// Outcome of running one notebook cell.
struct NotebookRunResult {
    enum class Status { Ok, Error, Cancelled, TimedOut, Failed };
    Status status = Status::Failed;
    /// Output in arrival order as (is_stderr, text) chunks, consecutive chunks of
    /// one stream merged. Capped (see output_truncated).
    QVector<QPair<bool, QString>> streams;
    QString result_text; ///< repr() of the cell's last expression, if any
    QString error_name;  ///< e.g. "ZeroDivisionError", "TimeoutError", "KeyboardInterrupt"
    QString error_value;
    QStringList traceback;
    QString failure;     ///< the runner itself could not run (Status::Failed)
    qint64 elapsed_ms = 0;
    bool output_truncated = false;
};
using NotebookRunCallback = std::function<void(const NotebookRunResult&)>;
/// Live output: called (UI thread, coalesced ~10x/s) with new text.
using NotebookStreamCallback = std::function<void(const QString& text, bool is_stderr)>;

/// One prebuilt notebook described by the manifest.
struct NotebookCatalogEntry {
    QString id;
    QString title;
    QString category;   // Finance, Economics, Trading, Investing, Portfolio, Quant
    QString difficulty; // "Beginner" | "Intermediate" | "Hard"
    int est_minutes = 0;
    QString requirements; // "stdlib" | "pandas + numpy"
    QString summary;
    QString file; // bundled filename, e.g. finance_time_value_of_money.ipynb
};

class NotebookLibraryService : public QObject {
    Q_OBJECT

  public:
    static NotebookLibraryService& instance();

    /// Parsed catalog (loaded + cached on first call). Empty if assets missing.
    const QVector<NotebookCatalogEntry>& catalog();

    /// Absolute path to the bundled notebooks directory, or empty if not found.
    QString assets_dir();

    /// Absolute path to a bundled notebook .ipynb for an entry (read-only asset).
    QString asset_path(const NotebookCatalogEntry& entry);

    /// First-run seeding: copy every bundled notebook into <profile>/files/ and
    /// register each with FileManagerService (source "code_editor"). Idempotent —
    /// guarded by a marker file. Pass force=true to re-seed regardless.
    void seed_into_files(bool force = false);

    /// Ensure a writable working copy of `entry` exists in <profile>/files/ and
    /// return its absolute path (registering it with FileManagerService if new).
    /// Returns the bundled asset path as a last resort if storage is unavailable.
    QString working_copy_for(const NotebookCatalogEntry& entry);

    /// Execute one cell asynchronously through PythonRunner (queued behind its
    /// concurrency limit) via scripts/notebook_cell_runner.py. Each call is its
    /// own process — cells do not share interpreter state. `on_output` streams
    /// stdout/stderr while it runs; `cb` fires exactly once (UI thread) unless
    /// `context` is destroyed first. Returns the run id for cancel_run().
    /// Screens go through here rather than calling PythonRunner themselves (D1).
    QString run_cell(const NotebookRunRequest& request, QObject* context, NotebookStreamCallback on_output,
                     NotebookRunCallback cb);

    /// Interrupt a run (KeyboardInterrupt in the cell; hard stop after a short
    /// grace period). A run still waiting for a Python slot resolves as
    /// Cancelled immediately and its process exits as soon as it starts.
    void cancel_run(const QString& run_id);

    /// Interpreter for a kernel id, or empty with `why` set when it is not installed.
    static QString kernel_python(const QString& kernel, QString* why = nullptr);

  private:
    explicit NotebookLibraryService(QObject* parent = nullptr);
    void load_catalog();
    QString seed_marker_path() const;

    struct ActiveRun;
    void on_run_line(const QString& run_id, const QString& line, bool is_stderr);
    void flush_output();
    void finish_run(const QString& run_id, int exit_code, const QString& runner_error);
    QHash<QString, std::shared_ptr<ActiveRun>> runs_;
    QTimer* flush_timer_ = nullptr;

    QVector<NotebookCatalogEntry> catalog_;
    bool loaded_ = false;
    QString assets_dir_cache_;
    bool assets_dir_resolved_ = false;
};

} // namespace fincept::services
