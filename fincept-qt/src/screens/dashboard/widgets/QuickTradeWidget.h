#pragma once
#include "screens/dashboard/widgets/BaseWidget.h"
#include "services/markets/MarketDataService.h"

#include <QComboBox>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

namespace fincept::screens::widgets {

/// Quick Trade Widget — symbol lookup + order estimate.
/// Fetches live quote via yfinance on symbol search.
/// The OPEN BUY / SELL TICKET button hands off to the Equity Trading order
/// ticket (`equity.open_order_ticket`), which owns account selection,
/// paper/live routing and the confirmation; this tile never places an order.
///
/// The LOOKUP action re-subscribes to `market:quote:<sym>` on the DataHub;
/// push updates keep the price/change labels live without polling.
class QuickTradeWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit QuickTradeWidget(const QJsonObject& cfg = {}, QWidget* parent = nullptr);

    /// Persists the looked-up symbol so the tile comes back on it.
    QJsonObject config() const override;
    void apply_config(const QJsonObject& cfg) override;

  protected:
    void on_theme_changed() override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void retranslateUi() override;
    /// Follows the tile's symbol link group (unlinked by default).
    void on_linked_symbol(const SymbolRef& ref) override;
    SymbolRef linked_symbol() const override;

  private:
    void apply_styles();
    void lookup_symbol();
    void on_side_changed(int idx);
    void submit_order();
    /// EST. TOTAL = qty x (limit/stop price when one is entered, else last).
    void update_estimate();

    /// Render the current_symbol_ quote card from a QuoteData snapshot.
    void apply_quote(const services::QuoteData& q);
    void hub_unsubscribe_all();
    /// A fresh tile's symbol: the region's largest tradable stock (the
    /// stock_quote rule, canvas/WidgetRegistry.cpp), else AAPL.
    static QString region_default_symbol();

    // Symbol lookup bar
    QLineEdit* symbol_input_ = nullptr;
    QPushButton* lookup_btn_ = nullptr;

    // Quote display
    QLabel* sym_label_ = nullptr;
    QLabel* price_label_ = nullptr;
    QLabel* change_label_ = nullptr;

    // Order form
    QComboBox* side_combo_ = nullptr; // BUY / SELL
    QComboBox* order_type_ = nullptr; // MARKET / LIMIT / STOP
    QLineEdit* qty_input_ = nullptr;
    QLineEdit* price_input_ = nullptr; // for LIMIT/STOP
    QLabel* est_total_ = nullptr;
    QPushButton* submit_btn_ = nullptr;

    double current_price_ = 0;
    QString current_symbol_;

    // Widgets needing theme-aware restyling
    QWidget* quote_card_ = nullptr;
    QFrame* separator_ = nullptr;
    QLabel* qty_lbl_ = nullptr;
    QLabel* price_lbl_ = nullptr;

    bool hub_active_ = false;
    bool symbol_from_config_ = false; ///< the config named a symbol (else the default is a guess)
    bool first_show_done_ = false;    ///< the link group's symbol was considered on the first show
};

} // namespace fincept::screens::widgets
