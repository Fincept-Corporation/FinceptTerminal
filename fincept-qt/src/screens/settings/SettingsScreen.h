#pragma once
// SettingsScreen.h — top-level settings shell. Hosts a left-nav + QStackedWidget
// of sub-section widgets. All section state and UI lives in *Section.h/.cpp
// files in this directory; this class only owns the navigation and the MCP
// event subscription that triggers a cross-section reload.

#include "core/events/EventBus.h"
#include "screens/common/IStatefulScreen.h"

#include <QHideEvent>
#include <QList>
#include <QPushButton>
#include <QShowEvent>
#include <QStackedWidget>
#include <QVariantMap>
#include <QWidget>

namespace fincept::screens {

/// Full-featured settings screen.
/// Sections: Credentials | Appearance | Notifications | Storage & Cache |
///           Data Sources | LLM Config | MCP Servers | Logging | Security |
///           Profiles | Keybindings | Python Env | Developer | Voice
class SettingsScreen : public QWidget, public IStatefulScreen {
    Q_OBJECT
  public:
    explicit SettingsScreen(QWidget* parent = nullptr);

    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return "settings"; }

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

  private:
    QStackedWidget* sections_ = nullptr;
    QWidget*        nav_      = nullptr;

    void refresh_theme();

    // ── MCP-driven UI sync ────────────────────────────────────────────────────
    // MCP settings tools publish settings.changed / llm.provider_changed when
    // the LLM mutates settings via Finagent or AI Chat. Subscribers are active
    // only while the screen is visible (P3 lifecycle).
    QList<EventBus::HandlerId> mcp_event_subs_;
    void subscribe_mcp_events();
    void unsubscribe_mcp_events();
    void reload_visible_section(); // re-shows current section to retrigger its showEvent
};

} // namespace fincept::screens
