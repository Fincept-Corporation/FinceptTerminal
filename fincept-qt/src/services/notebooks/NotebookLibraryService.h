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

#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QList>
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
    /// Earlier cells re-run first, in order, in the same process with their output
    /// discarded: every run is a fresh interpreter, so this is how a cell sees the
    /// variables / imports of the cells that already ran in the session.
    QStringList prefix_code;
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
    /// Rich outputs in arrival order — the runner's "display" events: {"mime": "image/png",
    /// "data": base64} per open matplotlib figure, {"mime": "application/x-table", "columns",
    /// "index", "index_name", "rows", "total_rows", "total_columns"} for a DataFrame / Series.
    QList<QJsonObject> displays;
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
    QStringList tags;      // short lowercase tags ("prices", "backtest")
    bool featured = false; // shown first on the Code tab's start page
    bool network = false;  // downloads market data when run
};

/// A notebook in the terminal's managed file storage (File Manager, source "code_editor").
struct NotebookFileEntry {
    QString path;          ///< absolute path
    QString name;          ///< the name the user knows (original file name)
    QDateTime modified;    ///< file time
    bool from_template = false; ///< a working copy of a library template
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

    /// Notebooks in managed storage (saved with the Code tab, seeded templates),
    /// newest first. Reads the File Manager index and stats each file.
    QVector<NotebookFileEntry> saved_notebooks();

    /// The name to show for a notebook file: its original name when the file is
    /// a managed copy ("1794…_c23e.ipynb" -> "trading_sma_crossover_backtest.ipynb"),
    /// else the file name.
    QString display_name_for(const QString& path);

    /// The catalog entry a managed working copy was made from, or nullptr.
    const NotebookCatalogEntry* template_for(const QString& path);

    /// Execute one cell asynchronously through PythonRunner (queued behind its
    /// concurrency limit) via scripts/notebook_cell_runner.py. Each call is its
    /// own process; state carries over only through request.prefix_code (earlier
    /// cells silently re-run before this one). `on_output` streams
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
