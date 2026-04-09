#pragma once
#include "screens/dashboard/widgets/BaseWidget.h"
#include "services/markets/MarketDataService.h"

#include <QComboBox>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

namespace fincept::screens::widgets {

/// Quick Trade Widget — symbol lookup + order entry form.
/// Fetches live quote via yfinance on symbol search.
/// Order submission connects to the trading service (or shows confirmation dialog).
class QuickTradeWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit QuickTradeWidget(QWidget* parent = nullptr);

  protected:
    void on_theme_changed() override;

  private:
    void apply_styles();
    void lookup_symbol();
    void on_side_changed(int idx);
    void submit_order();

    // Symbol lookup bar
    QLineEdit* symbol_input_ = nullptr;
    QPushButton* lookup_btn_ = nullptr;

    // Quote display
    QLabel* sym_label_ = nullptr;
    QLabel* price_label_ = nullptr;
    QLabel* change_label_ = nullptr;
    QLabel* bid_label_ = nullptr;
    QLabel* ask_label_ = nullptr;

    // Order form
    QComboBox* side_combo_ = nullptr; // BUY / SELL / SHORT
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
};

} // namespace fincept::screens::widgets
