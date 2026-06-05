#pragma once

#include <QJsonArray>
#include <QPoint>
#include <QPointer>
#include <QQueue>
#include <QString>
#include <QWidget>

#ifdef HAS_QT_WEBENGINE
class QWebEngineView;
#endif

namespace fincept::ui {

class KLineChartWidget : public QWidget {
    Q_OBJECT
public:
    explicit KLineChartWidget(QWidget* parent = nullptr);

    void set_candles(const QJsonArray& candles);
    void update_candle(const QJsonObject& candle);
    void add_indicator(const QString& name, const QString& pane_id = QString());
    void remove_indicator(const QString& name, const QString& pane_id = QString());
    // Draw/clear a locked dashed horizontal line at an open position's entry price.
    void set_position_line(double price, const QString& label, const QString& color_hex);
    void clear_position_line();
    void clear();

    bool is_available() const;

    // Enable the right-click trading menu (Buy/Sell @ cursor price + watchlist).
    // Off by default so non-trading charts (e.g. Equity Research) are unaffected.
    void set_trading_menu_enabled(bool on) { trading_menu_ = on; }
    bool trading_menu_enabled() const { return trading_menu_; }
    // Called by the internal web view on right-click: looks up the price under
    // `local_y` (view coords) via JS, then pops the trade menu at `global_pos`.
    void show_chart_context_menu(int local_y, const QPoint& global_pos);

signals:
    void chart_ready();
    void buy_requested(double price);
    void sell_requested(double price);
    void add_to_watchlist_requested();

protected:
    void showEvent(QShowEvent* e) override;
    // QWebEngineView routes input to an internal render widget (its focusProxy),
    // so QWebEngineView::contextMenuEvent never fires for our subclass — the
    // right-click is swallowed by Chromium. We instead filter the proxy's raw
    // right mouse-press to drive the trading menu reliably (incl. on macOS).
    bool eventFilter(QObject* obj, QEvent* ev) override;

private:
    void ensure_web_view();
    void install_chart_event_filter(); // (re)attach the proxy filter once it exists
    void emit_trade(bool is_buy, double price);
    void run_js(const QString& js);
    void flush_pending();
    void on_load_finished(bool ok);

#ifdef HAS_QT_WEBENGINE
    QWebEngineView* web_view_ = nullptr;
#endif
    bool page_ready_ = false;
    bool initialized_ = false;
    bool trading_menu_ = false;
    bool menu_open_ = false;             // re-entrancy guard (avoid double menus)
    QPointer<QWidget> filtered_proxy_;   // the render widget we've filtered
    QQueue<QString> pending_js_;
};

} // namespace fincept::ui
