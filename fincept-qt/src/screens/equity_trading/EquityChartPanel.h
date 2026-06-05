#pragma once
// EquityChartPanel — the trading-tab price chart.
//
// Prefers the WebEngine-backed KLineChart (same engine as the Equity Research
// tab) when Qt WebEngine is available, and falls back to the Qt-Charts
// EquityChart otherwise. Exposes one uniform API to EquityTradingScreen so the
// screen never needs to know which backend is active. Candle data flows in via
// the existing broker path (BrokerCandle), so every broker that implements
// get_history works with no per-broker wiring.

#include "trading/TradingTypes.h"

#include <QString>
#include <QVector>
#include <QWidget>

class QPushButton;
class QFrame;
class QLabel;
class QResizeEvent;

namespace fincept::ui {
class KLineChartWidget;
}

namespace fincept::screens::equity {

class EquityChart;

class EquityChartPanel : public QWidget {
    Q_OBJECT
  public:
    explicit EquityChartPanel(QWidget* parent = nullptr);

    // Bulk-load historical candles (full reload on symbol/timeframe change).
    void set_candles(const QVector<trading::BrokerCandle>& candles);

    // Roll a live quote into the forming candle (no-op in the fallback backend).
    void on_quote(const trading::BrokerQuote& quote);

    // Active timeframe label, one of TF_LABELS ("1m".."1w").
    QString current_timeframe() const;

    // ── On-chart position overlay (TradingView-style) ────────────────────────
    // Show/refresh the open position for the displayed symbol: a floating P&L
    // card with an EXIT button plus a dashed entry-price line in the chart.
    // `side` is "long"/"short"; `qty` is the absolute size.
    // `currency_code` (e.g. "INR") pins the P&L symbol to the position's own
    // currency; empty falls back to the global preference.
    void set_position(const QString& symbol, const QString& side, double qty, double entry_price,
                      const QString& exchange, const QString& product_type,
                      const QString& currency_code = QString());
    void clear_position();
    // Recompute the card's live P&L from the latest traded price (any backend).
    void update_pnl(double ltp);

  signals:
    void timeframe_changed(const QString& tf);
    // Right-click chart trading: forwarded from the active backend (KLineChart or
    // the Qt-Charts fallback). `price` is the y-axis value under the cursor.
    void buy_requested(double price);
    void sell_requested(double price);
    void add_to_watchlist_requested();
    // Emitted when the user clicks EXIT on the position card. The screen confirms
    // and routes the square-off (it owns the active account).
    void exit_position_requested(const QString& symbol, const QString& exchange,
                                 const QString& product_type, const QString& side, double qty);

  protected:
    void resizeEvent(QResizeEvent* e) override;
    // Drag-to-move the position card (it's mouse-draggable so it never has to
    // sit over the price axis).
    bool eventFilter(QObject* obj, QEvent* ev) override;

  private:
    void build_kline_ui();
    void build_position_card();
    void refresh_position_card();
    void layout_position_card();
    void draw_position_line();
    void set_active_tf(int idx);
    static qint64 interval_ms_for(const QString& tf);

    // ── KLine backend (WebEngine available) ──────────────────────────────────
    fincept::ui::KLineChartWidget* kline_ = nullptr;
    QPushButton* tf_buttons_[6] = {};
    int active_tf_ = 2; // "15m" — matches EquityChart's default

    // Forming-bar state for live updates (milliseconds — BrokerCandle units).
    bool have_bar_ = false;
    qint64 bar_ts_ms_ = 0;
    double bar_open_ = 0;
    double bar_high_ = 0;
    double bar_low_ = 0;
    double bar_close_ = 0;
    double bar_vol_ = 0;

    // ── Fallback backend (no WebEngine) ──────────────────────────────────────
    EquityChart* fallback_ = nullptr;

    // ── Position overlay (floating card; works over either backend) ───────────
    QFrame* pos_card_ = nullptr;
    QLabel* pos_title_lbl_ = nullptr;
    QLabel* pos_pnl_lbl_ = nullptr;
    QPushButton* pos_exit_btn_ = nullptr;
    bool has_position_ = false;
    QString pos_symbol_;
    QString pos_side_; // "long" | "short"
    QString pos_exchange_;
    QString pos_product_;
    QString pos_currency_; // currency code for P&L (e.g. "INR"); empty = global
    double pos_qty_ = 0.0;
    double pos_entry_ = 0.0;
    double pos_ltp_ = 0.0;
    // Last applied P&L sign (+1 / -1 / 0=unset). setStyleSheet() forces a full
    // style re-parse + re-polish, so on the per-tick refresh we only touch the
    // P&L label's stylesheet when the sign (hence color) actually flips.
    int pos_pnl_sign_ = 0;
    // Drag state — once the user moves the card it stays where they put it.
    QPoint pos_drag_offset_;
    bool pos_dragging_ = false;
    bool pos_card_moved_ = false;
};

} // namespace fincept::screens::equity
