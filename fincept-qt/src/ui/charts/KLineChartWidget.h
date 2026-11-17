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

    // Push the active theme to the page: surfaces, text, grid, crosshair,
    // the up/down colours of the user's direction palette, the categorical
    // indicator-line colours and the user's font. Runs automatically when the
    // page loads and on every ThemeManager::theme_changed; call it only after
    // changing something the theme reads outside ThemeManager.
    void apply_theme();
    // Time axis zone as an IANA id ("America/New_York", "Asia/Kolkata").
    // Empty = the system time zone (the default).
    void set_timezone(const QString& iana);
    QString timezone() const { return timezone_; }
    // Decimal places for prices and volumes (e.g. 5/0 for FX, 2/0 for equities,
    // 8/4 for crypto). Negative values are clamped to 0.
    void set_price_precision(int price, int volume = 0);

    // All three setters above may be called before the page has loaded: the
    // latest value of each is kept and pushed once the page is ready (before
    // any queued candle data), and again if the page is ever reloaded.

    // True when the WebEngine runtime is deployed AND no chart in this process
    // has seen it fail to start (see runtime_failed()). Hosts check this once at
    // construction to pick KLineChart vs. their native fallback chart.
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
    // The embedded Chromium renderer died, or the chart page failed to load
    // (#411: a deployment missing resources/v8_context_snapshot.bin aborts every
    // renderer at startup, leaving a permanently blank view). The widget is inert
    // afterwards — calls become no-ops. Hosts should swap in their native chart
    // and replay their cached candles. Emitted queued and at most once, so a
    // handler may deleteLater() this widget.
    void runtime_failed();
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
    // Sends the theme / time zone / precision state (only when the page is ready).
    void push_page_state();
    QString theme_js() const;
    void on_load_finished(bool ok);
    void fail_runtime(const QString& reason);
    // Load watchdog: if the page has neither finished nor failed kLoadWatchdogMs after the
    // load started (a renderer that hangs without dying never reports loadFinished),
    // fail_runtime() so the host swaps in its native chart instead of an empty area forever.
    // Re-arms while the widget is hidden — only a visible chart is judged.
    void arm_load_watchdog();
    static constexpr int kLoadWatchdogMs = 10'000;

#ifdef HAS_QT_WEBENGINE
    QWebEngineView* web_view_ = nullptr;
#endif
    bool page_ready_ = false;
    bool initialized_ = false;
    bool failed_ = false; // renderer died / page failed to load — see runtime_failed()
    bool trading_menu_ = false;
    bool menu_open_ = false; // re-entrancy guard (avoid double menus)
    // One install_chart_event_filter() retry chain at a time (only read in the
    // HAS_QT_WEBENGINE build — [[maybe_unused]] keeps -Wunused-private-field quiet).
    [[maybe_unused]] bool proxy_retry_pending_ = false;
    QPointer<QWidget> filtered_proxy_; // the render widget we've filtered
    QQueue<QString> pending_js_;
    // Page state, re-sent on load (see push_page_state()).
    QString timezone_;           // IANA id; resolved to the system zone in the constructor
    int price_precision_ = -1;   // -1 = not set (klinecharts default)
    int volume_precision_ = -1;
};

} // namespace fincept::ui
