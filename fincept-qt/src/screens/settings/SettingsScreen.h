#pragma once
// SettingsScreen.h — top-level settings shell. Hosts a left-nav + QStackedWidget
// of sub-section widgets. All section state and UI lives in *Section.h/.cpp
// files in this directory; this class only owns the navigation, the MCP
// event subscription that triggers a cross-section reload, and the centralised
// language-change handler that rebuilds every section so their newly
// constructed widgets pick up the active QTranslator.

#include "core/events/EventBus.h"
#include "screens/common/IStatefulScreen.h"

#include <QHideEvent>
#include <QLabel>
#include <QList>
#include <QPushButton>
#include <QShowEvent>
#include <QStackedWidget>
#include <QVariantMap>
#include <QWidget>

#include <functional>

namespace fincept::screens {

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
    void changeEvent(QEvent* e) override;

  private:
    QStackedWidget* sections_ = nullptr;
    QWidget*        nav_      = nullptr;
    QLabel*         nav_title_ = nullptr;

    void refresh_theme();
    void retranslateUi();

    /// Re-wire signals from section instances to external services. Called
    /// after every section rebuild (language change) so the wiring survives
    /// the destruction of the old section widgets.
    void wire_section_signals();

    /// Rebuild every section widget by constructing fresh instances via the
    /// stored factories. Preserves the current section index. Each rebuilt
    /// section picks up the active QTranslator at construction time.
    void rebuild_sections_for_language_change();

    /// Nav button → source key map used to retranslate button labels and
    /// scope headers without rebuilding the nav.
    struct NavButton { QPushButton* btn; QString source_key; };
    QList<NavButton> nav_buttons_;
    QList<QLabel*>   scope_headers_;  // entries align with scope_header_keys_
    QList<QString>   scope_header_keys_;

    /// Factories for each section index. Used by the language-change rebuild
    /// path so we can recreate widgets without hardcoding the type list twice.
    QList<std::function<QWidget*()>> section_factories_;

    // ── MCP-driven UI sync ────────────────────────────────────────────────────
    QList<EventBus::HandlerId> mcp_event_subs_;
    void subscribe_mcp_events();
    void unsubscribe_mcp_events();
    void reload_visible_section(); // re-shows current section to retrigger its showEvent
};

} // namespace fincept::screens
