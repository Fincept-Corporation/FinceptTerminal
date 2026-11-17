#pragma once
// HarnessRunOptionsWidget.h — the options of one agent run, shared by the RUN
// composer and the SCHEDULES editor so both offer exactly the same choices:
//
//   harness   installed harnesses first, the rest under "More — not installed"
//             (an install link is shown for those); a status / WARNING line with
//             the sign-in hint, the "API key in environment" note and BETA notes
//   model     "CLI default" first, the registry's list (from the CLI when it can
//             list them, else the adapter's built-in list), editable for any id
//   effort    from the selected model; hidden when the harness has none
//   mode      Read-only analysis | Workspace edits | Full access (only when
//             allowed in Settings › AI Agents, after a confirmation)
//   tools     terminal tools on / off (off and disabled when the harness cannot
//             take the MCP server per run)
//   limits    budget (harnesses that support one), time limit (every harness),
//             max turns (harnesses that support it)
//   folder, template, prompt
//
// Everything about harnesses comes from harness::HarnessRegistry — the widget
// follows its status_changed / defaults_changed live and stores nothing of its
// own. Styling comes from the host's stylesheet (object names awbLabel,
// awbHint, awbBtn); rich-text lines are rebuilt on appearance_settled.

#include "services/harness/HarnessTypes.h"

#include <QString>
#include <QVariantMap>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QGridLayout;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QStandardItemModel;

namespace fincept::screens {

class AgentPlaceholderHighlighter;

struct HarnessRunOptions {
    QString harness;
    QString model;  // "" = the CLI's default
    QString effort; // "" = the model's default
    harness::PermissionMode permission = harness::PermissionMode::ReadOnly;
    bool tools = true;
    double budget_usd = 0.0;
    int time_limit_s = 0;
    int max_turns = 0;
    QString workdir;
    QString prompt;
};

class HarnessRunOptionsWidget : public QWidget {
    Q_OBJECT
  public:
    enum class Purpose { Composer, Schedule };
    explicit HarnessRunOptionsWidget(Purpose purpose, QWidget* parent = nullptr);

    HarnessRunOptions options() const;
    /// Unknown harness ids are shown as "unknown harness (<id>)" and kept as they are.
    void set_options(const HarnessRunOptions& options);
    /// Reset to the user's defaults (preferred harness, its default model / effort).
    void reset_to_defaults();
    QString harness() const;
    /// The selected harness is known and installed (a run can be started).
    bool harness_runnable(QString* why = nullptr) const;
    /// The selected harness is registered in this build (unknown = read-only).
    bool harness_known() const;

    QPlainTextEdit* prompt_edit() const { return prompt_edit_; }
    void set_prompt(const QString& prompt);
    /// Select the prompt's first <PLACEHOLDER> for overtyping.
    void select_first_placeholder();
    /// Open the starter-prompt list (an empty view's primary action).
    void open_templates();
    /// Insert starter prompt `index` (0-based, agent_prompt_templates()) as the
    /// template list does — the screen's TEMPLATES menu. Never starts a run.
    void insert_template(int index);
    /// Disable every input (an unknown-harness schedule is shown read-only).
    void set_read_only(bool read_only);

    QVariantMap save_state() const;
    void restore_state(const QVariantMap& state);

  signals:
    void run_requested(); // Ctrl+Enter in the prompt
    void harness_changed(const QString& harness_id);
    void options_changed();
    /// A template was inserted; `title` lets the schedule editor name a new schedule.
    void template_inserted(const QString& title);

  protected:
    void showEvent(QShowEvent* event) override;
    void changeEvent(QEvent* event) override;

  private:
    void build();
    void retranslate();
    void rebuild_harness_combo();
    void rebuild_models();
    void rebuild_efforts();
    void rebuild_permissions();
    void refresh_status_line();
    void refresh_capabilities();
    void apply_harness_defaults();
    void on_harness_activated(int index);
    void on_permission_activated(int index);
    void on_template_activated(int index);
    void on_status_link(const QString& link);
    void on_browse_folder();
    QString current_model() const;

    Purpose purpose_;
    QLabel* harness_lbl_ = nullptr;
    QComboBox* harness_combo_ = nullptr;
    QStandardItemModel* harness_model_ = nullptr;
    QLabel* status_lbl_ = nullptr;
    QLabel* model_lbl_ = nullptr;
    QComboBox* model_combo_ = nullptr;
    QLabel* effort_lbl_ = nullptr;
    QComboBox* effort_combo_ = nullptr;
    QLabel* mode_lbl_ = nullptr;
    QComboBox* mode_combo_ = nullptr;
    QCheckBox* tools_check_ = nullptr;
    QLabel* tools_hint_ = nullptr; // word-wrapped: a check box label cannot wrap and would set the column width
    QLabel* limits_lbl_ = nullptr;
    QGridLayout* limits_grid_ = nullptr;
    QLabel* budget_lbl_ = nullptr;
    QDoubleSpinBox* budget_spin_ = nullptr;
    QLabel* time_lbl_ = nullptr;
    QSpinBox* time_spin_ = nullptr;
    QLabel* turns_lbl_ = nullptr;
    QSpinBox* turns_spin_ = nullptr;
    QLabel* folder_lbl_ = nullptr;
    QLineEdit* folder_edit_ = nullptr;
    QPushButton* folder_browse_btn_ = nullptr;
    QLabel* template_lbl_ = nullptr;
    QComboBox* template_combo_ = nullptr;
    QLabel* prompt_lbl_ = nullptr;
    QPlainTextEdit* prompt_edit_ = nullptr;
    AgentPlaceholderHighlighter* highlighter_ = nullptr;

    QString harness_;          // selected id ("" until the registry answered)
    QString unknown_harness_;  // a stored id with no adapter, shown read-only
    harness::PermissionMode permission_ = harness::PermissionMode::ReadOnly;
    bool harness_auto_ = true;   // chosen by the widget (follows the user's default), not by the user
    bool model_touched_ = false; // the user picked / typed a model since choosing the harness
    bool effort_touched_ = false;
    bool read_only_ = false;
};

} // namespace fincept::screens
