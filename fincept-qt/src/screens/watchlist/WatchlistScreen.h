#pragma once
#include "screens/IStatefulScreen.h"
#include "services/markets/MarketDataService.h"
#include "storage/repositories/WatchlistRepository.h"
#include "ui/tables/DataTable.h"

#include <QHideEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QShowEvent>
#include <QSplitter>
#include <QTimer>
#include <QWidget>

namespace fincept::screens {

/// Full-screen Watchlist — manage multiple watchlists with live quotes.
/// Layout: Watchlist Sidebar | Stock Table + Controls
class WatchlistScreen : public QWidget, public IStatefulScreen {
    Q_OBJECT
  public:
    explicit WatchlistScreen(QWidget* parent = nullptr);

    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return "watchlist"; }
    int state_version() const override { return 1; }

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
    void refresh_theme();

  private:
    void build_ui();
    QWidget* build_sidebar();
    QWidget* build_main_panel();
    void load_watchlists();
    void load_stocks();
    void fetch_quotes();
    void populate_table(const QVector<services::QuoteData>& quotes);

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
    QPushButton* add_btn_ = nullptr;
    QPushButton* remove_btn_ = nullptr;
    ui::DataTable* table_ = nullptr;
    QTimer* refresh_timer_ = nullptr;
    QSplitter* splitter_ = nullptr;
};

} // namespace fincept::screens
