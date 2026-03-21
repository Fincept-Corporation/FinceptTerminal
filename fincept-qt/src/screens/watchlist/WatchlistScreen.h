#pragma once
#include <QWidget>
#include <QListWidget>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QShowEvent>
#include <QHideEvent>
#include "ui/tables/DataTable.h"
#include "storage/repositories/WatchlistRepository.h"
#include "services/markets/MarketDataService.h"

namespace fincept::screens {

/// Full-screen Watchlist — manage multiple watchlists with live quotes.
/// Layout: Watchlist Sidebar | Stock Table + Controls
class WatchlistScreen : public QWidget {
    Q_OBJECT
public:
    explicit WatchlistScreen(QWidget* parent = nullptr);

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

private:
    void build_ui();
    QWidget* build_sidebar();
    QWidget* build_main_panel();
    void load_watchlists();
    void load_stocks();
    void fetch_quotes();
    void populate_table(const QVector<services::QuoteData>& quotes);

    // Data
    QVector<fincept::Watchlist>      watchlists_;
    QVector<fincept::WatchlistStock> stocks_;
    QString current_wl_id_;

    // Sidebar
    QListWidget* wl_list_       = nullptr;
    QLabel*      wl_count_      = nullptr;

    // Main panel
    QLabel*      panel_title_   = nullptr;
    QLabel*      stock_count_   = nullptr;
    QLineEdit*   add_input_     = nullptr;
    ui::DataTable* table_       = nullptr;
    QTimer*      refresh_timer_ = nullptr;
};

} // namespace fincept::screens
