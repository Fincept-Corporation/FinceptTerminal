#pragma once
// AgentSchedulesPanel.h — SCHEDULES tab of the Agent Workbench: recurring agent
// briefs (every N minutes, or daily at HH:mm on chosen weekdays). The service
// owns the timer; this panel only edits schedules.

#include "services/coding_agents/CodingAgentTypes.h"

#include <QList>
#include <QMetaObject>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QTimeEdit;

namespace fincept::screens {

class AgentPlaceholderHighlighter;

class AgentSchedulesPanel : public QWidget {
    Q_OBJECT
  public:
    explicit AgentSchedulesPanel(QWidget* parent = nullptr);

  signals:
    void open_run_requested(const QString& run_id);

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void changeEvent(QEvent* event) override;

  private:
    void retranslate();
    void subscribe();
    void unsubscribe();
    void reload_list();
    void load_into_form(const QString& id);
    void clear_form();
    services::coding_agents::CodingAgentSchedule form_schedule() const;
    QString describe(const services::coding_agents::CodingAgentSchedule& s) const;
    void refresh_info();
    void on_kind_changed();
    void on_agent_changed();
    void on_template(int index);
    void on_new();
    bool on_save();
    void on_delete();
    void on_run_now();

    QLabel* list_title_ = nullptr;
    QListWidget* list_ = nullptr;
    QPushButton* new_btn_ = nullptr;
    QPushButton* delete_btn_ = nullptr;

    QLabel* form_title_ = nullptr;
    QLabel* name_lbl_ = nullptr;
    QLineEdit* name_edit_ = nullptr;
    QLabel* agent_lbl_ = nullptr;
    QComboBox* agent_combo_ = nullptr;
    QLabel* model_lbl_ = nullptr;
    QLineEdit* model_edit_ = nullptr;
    QLabel* mode_lbl_ = nullptr;
    QComboBox* mode_combo_ = nullptr;
    QCheckBox* tools_check_ = nullptr;
    QLabel* budget_lbl_ = nullptr;
    QDoubleSpinBox* budget_spin_ = nullptr;
    QLabel* when_lbl_ = nullptr;
    QRadioButton* interval_radio_ = nullptr;
    QRadioButton* daily_radio_ = nullptr;
    QSpinBox* interval_spin_ = nullptr;
    QTimeEdit* time_edit_ = nullptr;
    QList<QCheckBox*> day_checks_; // Monday … Sunday
    QCheckBox* enabled_check_ = nullptr;
    QLabel* template_lbl_ = nullptr;
    QComboBox* template_combo_ = nullptr;
    QLabel* prompt_lbl_ = nullptr;
    QPlainTextEdit* prompt_edit_ = nullptr;
    AgentPlaceholderHighlighter* highlighter_ = nullptr;
    QLabel* info_lbl_ = nullptr;
    QPushButton* save_btn_ = nullptr;
    QPushButton* run_now_btn_ = nullptr;

    QString editing_id_;
    QList<QMetaObject::Connection> conns_;
};

} // namespace fincept::screens
