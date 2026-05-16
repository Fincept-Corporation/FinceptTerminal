#pragma once
#include "core/events/EventBus.h"
#include "core/symbol/IGroupLinked.h"
#include "screens/common/IStatefulScreen.h"
#include "services/markets/MarketDataService.h"
#include "storage/repositories/WatchlistRepository.h"
#include "ui/tables/DataTable.h"

#include <QHash>
#include <QHideEvent>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QListWidget>
#include <QPushButton>
#include <QShowEvent>
#include <QSplitter>
#include <QTimer>
#include <QWidget>

namespace fincept::screens {

/// Full-screen Watchlist — manage multiple watchlists with live quotes.
/// Layout: Watchlist Sidebar | Stock Table + Controls
class WatchlistScreen : public QWidget, public IStatefulScreen, public IGroupLinked {
    Q_OBJECT
    Q_INTERFACES(fincept::IGroupLinked)
  public:
    explicit WatchlistScreen(QWidget* parent = nullptr);

    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return "watchlist"; }
    int state_version() const override { return 1; }

    // IGroupLinked
    void set_group(SymbolGroup g) override { link_group_ = g; }
    SymbolGroup group() const override { return link_group_; }
    void on_group_symbol_changed(const SymbolRef& ref) override;
    SymbolRef current_symbol() const override;

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

  private slots:
    void on_watchlist_selected(int row);
    void on_add_watchlist();
    void on_delete_watchlist();
    void on_add_stock();
    void on_remove_stock();
    void on_refresh();
    void on_export_csv();
    void on_import_csv();
    void refresh_theme();

  private:
    void build_ui();
    QWidget* build_sidebar();
    QWidget* build_main_panel();
    void load_watchlists();
    void load_stocks();
    void fetch_quotes();
    void populate_table(const QVector<services::QuoteData>& quotes);

    void hub_resubscribe_stocks();
    void hub_unsubscribe_all();
    void rebuild_from_cache();

    // Data
    QVector<fincept::Watchlist> watchlists_;
    QVector<fincept::WatchlistStock> stocks_;
    QString current_wl_id_;

    // Sidebar
    QWidget* sidebar_ = nullptr;
    QWidget* sidebar_header_ = nullptr;
    QLabel* sidebar_title_ = nullptr;
    QListWidget* wl_list_ = nullptr;
    QLabel* wl_count_ = nullptr;
    QPushButton* add_wl_btn_ = nullptr;

    // Main panel
    QWidget* main_panel_ = nullptr;
    QWidget* top_bar_ = nullptr;
    QWidget* add_bar_ = nullptr;
    QLabel* panel_title_ = nullptr;
    QLabel* stock_count_ = nullptr;
    QLabel* add_label_ = nullptr;
    QLineEdit* add_input_ = nullptr;
    QPushButton* refresh_btn_ = nullptr;
    QPushButton* del_wl_btn_ = nullptr;
    QPushButton* export_csv_btn_ = nullptr;
    QPushButton* import_csv_btn_ = nullptr;
    QPushButton* add_btn_ = nullptr;
    QPushButton* remove_btn_ = nullptr;
    ui::DataTable* table_ = nullptr;
    QSplitter* splitter_ = nullptr;

    QHash<QString, services::QuoteData> row_cache_;
    bool hub_active_ = false;

    // Symbol group link — SymbolGroup::None when unlinked.
    SymbolGroup link_group_ = SymbolGroup::None;

    // Emit the currently-selected symbol into the linked group, if any.
    // Safe to call with no selection — no-op.
    void publish_selection_to_group();

    // EventBus subscriptions — registered in showEvent, released in hideEvent.
    // MCP watchlist tools publish watchlist.created / .deleted / .updated when
    // the LLM mutates watchlists via Finagent or AI Chat. Each handler
    // reloads watchlists + the current stock list.
    QList<EventBus::HandlerId> mcp_event_subs_;
    void subscribe_mcp_events();
    void unsubscribe_mcp_events();
};

} // namespace fincept::screens
