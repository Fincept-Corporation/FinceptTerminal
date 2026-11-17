#pragma once
#include "screens/dashboard/DashboardStatusBar.h"
#include "screens/dashboard/DashboardToolBar.h"
#include "screens/dashboard/MarketPulsePanel.h"
#include "screens/dashboard/TickerBar.h"
#include "screens/common/ISectionLinked.h"
#include "screens/dashboard/canvas/DashboardCanvas.h"
#include "services/markets/MarketDataService.h"

namespace fincept::ui {
class EmptyState;
class NotifToast;
} // namespace fincept::ui

class QLabel;

#include <QHash>
#include <QHideEvent>
#include <QScrollArea>
#include <QShowEvent>
#include <QSplitter>
#include <QTimer>
#include <QWidget>

#include <functional>

namespace fincept::screens {

/// Main dashboard screen — toolbar, ticker, draggable widget grid, market pulse, status bar.
///
/// The ticker driver subscribes to `market:quote:<sym>` on the DataHub for
/// every user-configured ticker symbol. A cached per-symbol QuoteData is
/// pushed to the TickerBar on each delivery via `rebuild_ticker_from_cache()`.
/// `symbols_changed` wipes old subscriptions and re-subscribes to the new
/// set. Cadence is owned by the hub scheduler, not a local refresh timer.
class DashboardScreen : public QWidget, public ISectionLinked {
    Q_OBJECT
  public:
    explicit DashboardScreen(QWidget* parent = nullptr);

    /// ISectionLinked — "add_widget" (the ADD WIDGET picker) | "templates" (the TEMPLATES picker; a
    /// template replaces the layout only after its own confirmation). focus unused.
    bool open_section(const QString& section, const QString& focus) override;

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

  private slots:
    // A slot (not a plain method) so WindowFrame's F5 / View ▸ Refresh probe,
    // which looks the handler up by name, can reach it.
    void on_refresh_clicked();

  private:
    void refresh_theme();
    void build_default_layout();
    void save_layout();
    void restore_layout();
    /// LAYOUTS menu: save the current layout under a name, switch to / delete a
    /// named layout (SettingsRepository "dashboard.named_layouts"), export the
    /// current layout to a JSON file, import one.
    void show_layouts_menu(QWidget* anchor);
    /// Template picked in the TemplatePicker: applied at once when the current
    /// layout holds nothing of the user's own, else after confirm_replace().
    void request_template(const QString& template_id);
    /// Run `replace` (which swaps the current layout for another) — directly when
    /// nothing would be lost, otherwise after asking: save the current layout
    /// under a name first, replace without saving, or cancel. `what` names the
    /// action in the question ("Applying the template").
    void confirm_replace(const QString& what, std::function<void()> replace);
    /// Store the current layout as named layout `name`; marks it saved.
    bool save_named_layout(const QString& name);
    /// Non-blocking name prompt; `on_name` runs with a non-empty name.
    void prompt_layout_name(const QString& suggestion, std::function<void(const QString&)> on_name);
    /// COMPACT button state from the layout's row height.
    void sync_compact_button();
    /// ADD WIDGET (toolbar, Ctrl+N, the empty dashboard's primary action).
    void open_add_widget_dialog();
    /// TEMPLATES (toolbar's template button, the empty dashboard's secondary action).
    void open_template_picker();
    /// "Dashboard is empty" over the canvas while it has no tile and none queued
    /// (the user closed every tile); built on first need, hidden once a tile exists.
    void update_canvas_empty_state();
    void refresh_ticker();

    void hub_resubscribe_ticker(bool force_fetch);
    void hub_unsubscribe_ticker();
    void rebuild_ticker_from_cache();
    /// Slim, dismissible Fincept Terminal Enterprise strip under the ticker.
    /// One line high; dismissal lasts until the next release.
    QWidget* build_enterprise_strip();

    DashboardToolBar* toolbar_ = nullptr;
    TickerBar* ticker_bar_ = nullptr;
    QWidget* ent_strip_ = nullptr;
    QLabel* ent_strip_text_ = nullptr;
    QScrollArea* scroll_area_ = nullptr;
    DashboardCanvas* canvas_ = nullptr;
    QSplitter* content_split_ = nullptr;
    MarketPulsePanel* market_pulse_ = nullptr;
    DashboardStatusBar* status_bar_ = nullptr;
    fincept::ui::NotifToast* notif_toast_ = nullptr;
    fincept::ui::EmptyState* canvas_empty_ = nullptr; ///< over the scroll viewport; null until first needed
    QTimer* save_timer_ = nullptr;
    bool pulse_visible_ = true;
    bool layout_restored_ = false;
    bool split_sized_ = false;

    QHash<QString, services::QuoteData> ticker_cache_;
    QStringList ticker_subscribed_;
    bool hub_active_ = false;
};

} // namespace fincept::screens
