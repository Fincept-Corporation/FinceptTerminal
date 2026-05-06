#pragma once
// ModelChatPanel — per-agent decision timeline, the public-trace heart of
// Alpha Arena. Two stacked widgets:
//   * Top: agent picker (combobox) + auto-scroll toggle.
//   * Bottom: scrollable list of decisions, newest first. Click any row to
//     pop a modal showing the full prompt, response, parsed actions and
//     risk verdict.
//
// Reads aa_decisions + aa_prompts via AlphaArenaRepo. Refreshes on
// AlphaArenaEngine::decision_received.
//
// Reference: fincept-qt/.grill-me/alpha-arena-grill.md §10.3 (ModelChat tab).

#include <QComboBox>
#include <QListWidget>
#include <QString>
#include <QWidget>

namespace fincept::screens::alpha_arena {

class ModelChatPanel : public QWidget {
    Q_OBJECT
  public:
    explicit ModelChatPanel(QWidget* parent = nullptr);

    /// Bind to a competition. Triggers reload of the agent picker.
    void set_competition(const QString& competition_id);

  public slots:
    /// Refresh the timeline for the currently-selected agent.
    void refresh();

    /// Engine notification — caller passes both ids; if either matches the
    /// current selection, refresh().
    void on_decision_received(const QString& decision_id, const QString& agent_id);

  private:
    void show_decision_detail(const QString& decision_id);
    void reload_agent_picker();

    QString competition_id_;
    QComboBox* agent_picker_;
    QListWidget* timeline_;
};

} // namespace fincept::screens::alpha_arena
