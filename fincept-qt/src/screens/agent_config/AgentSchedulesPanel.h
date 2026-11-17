#pragma once
// AgentSchedulesPanel.h — SCHEDULES tab of the Agent Workbench: recurring agent
// briefs (every N minutes, or daily at HH:mm on chosen weekdays). The editor
// uses the SAME HarnessRunOptionsWidget as the RUN composer (harness, model,
// effort, mode, tools, limits, working folder, template, prompt). The service
// owns the timer; this panel only edits schedules. A schedule whose harness this
// build does not have is shown read-only (it can be deleted, never relabelled).

#include "services/coding_agents/CodingAgentTypes.h"

#include <QList>
#include <QMetaObject>
#include <QWidget>

class QCheckBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QStackedWidget;
class QTimeEdit;

namespace fincept::screens {

class HarnessRunOptionsWidget;

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
    void refresh_editability();
    void on_kind_changed();
    void on_new();
    bool on_save();
    void on_delete();
    void on_run_now();

    QLabel* list_title_ = nullptr;
    QStackedWidget* list_stack_ = nullptr; // list | empty state
    QListWidget* list_ = nullptr;
    QLabel* list_empty_lbl_ = nullptr;
    QPushButton* new_btn_ = nullptr;
    QPushButton* delete_btn_ = nullptr;

    QLabel* form_title_ = nullptr;
    QLabel* name_lbl_ = nullptr;
    QLineEdit* name_edit_ = nullptr;
    HarnessRunOptionsWidget* options_ = nullptr;
    QLabel* when_lbl_ = nullptr;
    QRadioButton* interval_radio_ = nullptr;
    QRadioButton* daily_radio_ = nullptr;
    QSpinBox* interval_spin_ = nullptr;
    QTimeEdit* time_edit_ = nullptr;
    QList<QCheckBox*> day_checks_; // Monday … Sunday
    QCheckBox* enabled_check_ = nullptr;
    QLabel* info_lbl_ = nullptr;
    QPushButton* save_btn_ = nullptr;
    QPushButton* run_now_btn_ = nullptr;

    QString editing_id_;
    QList<QMetaObject::Connection> conns_;
};

} // namespace fincept::screens
