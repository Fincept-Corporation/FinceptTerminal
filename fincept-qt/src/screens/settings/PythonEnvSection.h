#pragma once
// PythonEnvSection.h — Python venv package manager panel for SettingsScreen.
// Shows all packages from requirements-numpy1.txt and requirements-numpy2.txt,
// their installed versions, and allows install/upgrade via uv pip.

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QTableWidget>
#include <QWidget>

namespace fincept::screens {

class PythonEnvSection : public QWidget {
    Q_OBJECT
  public:
    explicit PythonEnvSection(QWidget* parent = nullptr);

    /// Trigger a package list refresh — called when section becomes visible.
    void reload();

  protected:
    void showEvent(QShowEvent* e) override;

  private:
    // ── UI ────────────────────────────────────────────────────────────────────
    QTableWidget*  pkg_table_           = nullptr;
    QLineEdit*     search_input_        = nullptr;
    QComboBox*     venv_filter_         = nullptr;  // All / Trading / Analytics
    QLabel*        status_lbl_          = nullptr;
    QProgressBar*  install_bar_         = nullptr;  // hidden when idle
    QLabel*        install_log_         = nullptr;  // last stdout line from uv
    QPushButton*   refresh_btn_         = nullptr;
    QPushButton*   install_missing_btn_ = nullptr;
    QPushButton*   upgrade_all_btn_     = nullptr;
    QPushButton*   batch_action_btn_    = nullptr;  // "Install/Upgrade Selected"

    // ── Processes — signal-driven, never waitForFinished on UI thread ─────────
    QProcess*      list_proc_           = nullptr;  // uv pip list, reused sequentially
    QProcess*      action_proc_         = nullptr;  // uv pip install / upgrade

    // ── Data ──────────────────────────────────────────────────────────────────
    struct PackageRow {
        QString name;          // canonical: lowercase, hyphens/dots → underscore
        QString display_name;  // original name from requirements file
        QString venv;          // "venv-numpy1" or "venv-numpy2"
        QString venv_label;    // "Trading" or "Analytics"
        QString required_spec; // raw spec, e.g. ">=2.2.3,<3.0"
        QString installed_ver; // from uv pip list; empty if not installed
        bool    missing = true;
    };

    QList<PackageRow>       all_packages_;  // merged from both req files
    QMap<QString, QString>  installed_v1_;  // canonical_name → version, venv-numpy1
    QMap<QString, QString>  installed_v2_;  // canonical_name → version, venv-numpy2
    bool                    v1_loaded_ = false;
    bool                    v2_loaded_ = false;

    // ── Action queue — drained one batch at a time ────────────────────────────
    struct ActionBatch {
        QString     venv;      // "venv-numpy1" or "venv-numpy2"
        QStringList packages;  // raw spec strings to pass to uv
        bool        upgrade = false;
    };
    QList<ActionBatch> action_queue_;

    // ── stdout accumulators — readyReadStandardOutput fires many times ────────
    QByteArray list_stdout_buf_;
    QByteArray action_stdout_buf_;

    // ── Builders ──────────────────────────────────────────────────────────────
    void build_ui();

    // ── Load phase ────────────────────────────────────────────────────────────
    void load_packages();
    void start_list_venv(const QString& venv_name);
    void on_list_finished(const QString& venv_name, int exit_code);
    void parse_requirements(const QString& req_file,
                            const QString& venv_name,
                            const QString& venv_label);
    void merge_and_populate_table();
    QString find_req_file(const QString& filename) const;

    // ── Action phase ──────────────────────────────────────────────────────────
    void start_action(const QList<ActionBatch>& batches);
    void run_next_batch();
    void on_action_finished(int exit_code);

    // ── Table helpers ─────────────────────────────────────────────────────────
    void apply_filter();
    void on_row_action_clicked(int row);
    QList<ActionBatch> build_batches_for_selected() const;

    // ── Misc ──────────────────────────────────────────────────────────────────
    void set_actions_enabled(bool enabled);
    void show_status(const QString& msg, bool error = false);

    static QString canonicalise(const QString& name);
};

} // namespace fincept::screens
