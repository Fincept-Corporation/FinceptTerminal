#pragma once
// ChatModeLeftRail.h — Chat Mode's left rail (2026-11-17 revamp).
//
//   ▌RUNS · CHATS · MEMORY · MCP          (segmented switch, keyboard reachable)
//   [the section]
//
// RUNS = RunListPanel (search, state / engine filters, groups Pinned · Needs you ·
// Today · Yesterday · This week · Older, NEW RUN, SCHEDULES, bulk delete); CHATS =
// ChatSessionPanel (the cloud conversations); MEMORY / MCP = a ChatAgentPanel with
// AgentPanelPages::Agent and its own tab strip hidden. The section picked last is
// remembered (QSettings "chat_mode/left_section"). ChatModeScreen wires the
// sections' signals itself through the accessors.

#include <QString>
#include <QWidget>

#include <memory>

namespace fincept::chat_mode {

class ChatAgentPanel;
class ChatSessionPanel;
class RunListPanel;

class ChatModeLeftRail : public QWidget {
    Q_OBJECT
  public:
    explicit ChatModeLeftRail(QWidget* parent = nullptr);
    ~ChatModeLeftRail() override;

    RunListPanel* runs() const;
    ChatSessionPanel* conversations() const;
    /// MEMORY / MCP (AgentPanelPages::Agent); its pages are built when first shown.
    ChatAgentPanel* agent() const;

    /// "runs" | "conversations" | "memory" | "mcp"; false for an unknown id.
    bool show_section(const QString& id);
    QString section() const;
    /// Keyboard focus into the shown section (its list).
    void focus_section();

  signals:
    void section_changed(const QString& id);

  private:
    struct Impl;
    std::unique_ptr<Impl> d_;
};

} // namespace fincept::chat_mode
