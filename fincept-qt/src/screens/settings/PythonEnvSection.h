#pragma once
// PythonEnvSection.h — Python venv package manager panel for SettingsScreen.
// Shows all packages from requirements-numpy1.txt and requirements-numpy2.txt,
// their installed versions, and allows install/upgrade via uv pip.
//
// The PYTHON LIBRARIES card on top is the home of the optional full library
// set: first run installs only the essentials, and INSTALL ALL LIBRARIES hands
// the rest to PythonSetupManager, which installs it in the background while the
// terminal stays usable (status-bar chip shows progress).

#include "python/PythonSetupManager.h"

#include <QComboBox>
#include <QEvent>
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
    void changeEvent(QEvent* event) override;

  private:
    // ── UI ────────────────────────────────────────────────────────────────────
    QTableWidget* pkg_table_ = nullptr;
    QLineEdit* search_input_ = nullptr;
    QComboBox* venv_filter_ = nullptr; // All / Trading / Analytics
    QLabel* status_lbl_ = nullptr;
    QProgressBar* install_bar_ = nullptr; // hidden when idle
    QLabel* install_log_ = nullptr;       // last stdout line from uv
    QPushButton* refresh_btn_ = nullptr;
    QPushButton* install_missing_btn_ = nullptr;
    QPushButton* upgrade_all_btn_ = nullptr;
    QPushButton* batch_action_btn_ = nullptr; // "Install/Upgrade Selected"

    // Fixed text widgets captured for retranslateUi.
    QLabel* title_lbl_ = nullptr;
    QLabel* info_lbl_ = nullptr;
    QLabel* warn_text_ = nullptr;

    // ── PYTHON LIBRARIES card (essentials / full set) ─────────────────────────
    QLabel* lib_title_lbl_ = nullptr;
    QLabel* lib_core_key_ = nullptr;
    QLabel* lib_core_val_ = nullptr;
    QLabel* lib_full_key_ = nullptr;
    QLabel* lib_full_val_ = nullptr;
    QLabel* lib_hint_lbl_ = nullptr;
    QProgressBar* lib_bar_ = nullptr;
    QPushButton* lib_setup_btn_ = nullptr; // SET UP PYTHON — only while the essentials are missing
    QPushButton* lib_full_btn_ = nullptr;  // INSTALL ALL LIBRARIES / RETRY INSTALL / INSTALLING…
    python::SetupStatus lib_status_;       // last check_status(); refreshed on run start/end and show

    // ── Processes — signal-driven, never waitForFinished on UI thread ─────────
    QProcess* list_proc_ = nullptr;   // uv pip list, reused sequentially
    QProcess* action_proc_ = nullptr; // uv pip install / upgrade

    // ── Data ──────────────────────────────────────────────────────────────────
    struct PackageRow {
        QString name;          // canonical (PEP 503): PythonSetupManager::canonical_package_name()
        QString display_name;  // original name from requirements file
        QString venv;          // "venv-numpy1" or "venv-numpy2"
        QString venv_label;    // "Trading" or "Analytics"
        QString required_spec; // raw spec, e.g. ">=2.2.3,<3.0"
        QString installed_ver; // from uv pip list; empty if not installed
        bool missing = true;
    };

    QList<PackageRow> all_packages_;      // merged from both req files
    QMap<QString, QString> installed_v1_; // canonical_name → version, venv-numpy1
    QMap<QString, QString> installed_v2_; // canonical_name → version, venv-numpy2

    // ── Action queue — drained one batch at a time ────────────────────────────
    struct ActionBatch {
        QString venv;         // "venv-numpy1" or "venv-numpy2"
        QStringList packages; // raw spec strings to pass to uv
        bool upgrade = false;
    };
    QList<ActionBatch> action_queue_;

    // ── stdout accumulators — readyReadStandardOutput fires many times ────────
    QByteArray list_stdout_buf_;
    QByteArray action_stdout_buf_;

    // ── Builders ──────────────────────────────────────────────────────────────
    void build_ui();
    QWidget* build_library_card();
    /// Re-render the library card. `recheck` re-reads the on-disk status (a few
    /// file checks); progress ticks pass false and only read the live snapshot.
    void refresh_library_card(bool recheck);
    /// Confirm, then hand the full library set to PythonSetupManager (background).
    void offer_full_install(const QString& reason);

    /// Re-apply tr() lookups to every widget whose text we keep a handle to.
    /// Called from changeEvent() on QEvent::LanguageChange.
    void retranslateUi();

    // ── Load phase ────────────────────────────────────────────────────────────
    void load_packages();
    void start_list_venv(const QString& venv_name);
    void on_list_finished(const QString& venv_name, int exit_code);
    void parse_requirements(const QString& req_file, const QString& venv_name, const QString& venv_label);
    void merge_and_populate_table();

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
};

} // namespace fincept::screens
