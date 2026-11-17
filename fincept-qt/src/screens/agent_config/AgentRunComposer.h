#pragma once
// AgentRunComposer.h — the NEW RUN section of the agents screen: the shared
// HarnessRunOptionsWidget (harness, model, effort, mode, tools, limits, folder,
// template, prompt) and RUN. It only starts runs; the run itself is shown in
// the screen's inspector (AgentRunPanel). Moved from the Agent Workbench's RUN
// tab (its left half); the strings keep the old translation context
// ("fincept::screens::AgentRunPanel").
//
// Nothing here runs by itself: RUN / Ctrl+Enter is the only way a run starts,
// and RUN carries fincept_agent_risk = spend.

#include <QList>
#include <QMetaObject>
#include <QVariantMap>
#include <QWidget>

class QLabel;
class QPushButton;

namespace fincept::screens {

class HarnessRunOptionsWidget;

class AgentRunComposer : public QWidget {
    Q_OBJECT
  public:
    explicit AgentRunComposer(QWidget* parent = nullptr);

    /// Put a prompt into the composer (a template, a schedule) without running it.
    void set_prompt(const QString& prompt);
    /// Open the starter-prompt list.
    void open_templates();
    /// Insert the starter prompt `index` of agent_prompt_templates() (0-based).
    void insert_template(int index);
    /// Focus the prompt editor.
    void focus_prompt();

    QVariantMap save_state() const;
    void restore_state(const QVariantMap& state);

  signals:
    void run_started(const QString& run_id);
    /// The composer's options changed (the screen saves its state).
    void state_changed();

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void changeEvent(QEvent* event) override;

  private:
    void retranslate();
    void refresh_run_button();
    void refresh_hint();
    void on_run();

    QLabel* title_ = nullptr;
    HarnessRunOptionsWidget* options_ = nullptr;
    QLabel* hint_ = nullptr;
    QPushButton* run_btn_ = nullptr;
    QList<QMetaObject::Connection> conns_;
};

} // namespace fincept::screens
