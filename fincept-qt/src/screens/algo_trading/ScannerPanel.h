// src/screens/algo_trading/ScannerPanel.h
#pragma once
#include "services/algo_trading/AlgoTradingTypes.h"

#include <QEvent>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QWidget>

class QTimer;

namespace fincept::ui {
class AutoLoadPane;
class InputMemory;
} // namespace fincept::ui

namespace fincept::screens {

class ScannerView;

/// Market scanner — find the symbols matching a rule set. The controller of ScannerView:
/// presets, universes (AlgoUniverses), broker accounts, the scan (AlgoScanner) behind a
/// ui::AutoLoadPane (default scan on first show, remembered result with its "as of",
/// "Inputs changed" on other inputs, quiet refresh after 15 min), the selected symbol's
/// candles (CandleDataFetcher, off the GUI thread, dropped on hide), CSV export and the
/// CREATE ALERT / OPEN IN BUILDER hand-offs.
class ScannerPanel : public QWidget {
    Q_OBJECT
  public:
    explicit ScannerPanel(QWidget* parent = nullptr);

  signals:
    void create_alert_requested(const QJsonArray& conditions, const QString& logic, const QStringList& symbols,
                                const QString& timeframe, const QString& data_source, const QString& account_id);
    /// A result was opened (double click / Enter / OPEN IN BUILDER): the screen routes it to
    /// the Builder's backtest symbol.
    void symbol_activated(const QString& symbol);

  protected:
    void changeEvent(QEvent* event) override;
    // Re-reads the broker accounts so one connected after this tab was built shows up.
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

  private:
    void build_ui();
    void populate_accounts();
    void connect_service();
    void apply_preset(int index);
    void retranslate_inputs();
    void rebuild_range_options();
    void prefill_close_from_price(const QString& symbol, double price);
    /// Universe combo from AlgoUniverses (user lists, region large caps, NSE lists).
    void fill_universes();
    void load_universe();
    void save_universe();
    /// CSV of the last scan: plain numbers and a currency column (never the on-screen text).
    void export_results();
    /// Quote currency of a scanned symbol on the scan's exchange / broker; empty = unknown.
    QString result_currency(const QString& symbol) const;
    void on_action(const QString& action);
    void on_scan_result(const QJsonObject& data);
    void on_scan_error(const QString& msg);
    // ── Results pane (ui::AutoLoadPane): the default scan runs on first show ──
    /// StartFn: validates and starts AlgoScanner (queued behind a scan in flight —
    /// AlgoScanner has no cancel, its answers carry no id).
    void start_scan(quint64 request_id);
    /// RenderFn: status, unevaluated symbols, the table or the "no match" state.
    void apply_scan_payload(const QJsonObject& payload);
    /// First show: the user's remembered inputs, else the MACD Bullish preset on the
    /// region's large caps (capped), so the default scan has something to run.
    void apply_first_show_inputs();
    /// Hash of what the next scan would use (rules, symbols, bars, range, source).
    QString inputs_signature() const;
    /// The user's edits of the rules / symbols, kept per view (InputMemory values).
    void remember_inputs();
    // ── Detail chart of the selected result ──
    void schedule_detail_fetch(const QString& symbol);
    void fetch_detail(const QString& symbol);

    ScannerView* view_ = nullptr;
    // Listing venue of the bare symbols being scanned (from the loaded universe;
    // empty = the broker's / region's default — see AlgoTradingService).
    QString exchange_hint_;
    // The last scan's payload and the exchange hint / broker / bars / source it ran with
    // (prices in each symbol's currency, the format-change re-render, the export, the chart).
    QJsonObject last_scan_;
    QString scan_hint_;
    QString scan_broker_id_;
    QString scan_account_id_;
    QString scan_timeframe_;
    QString scan_source_;
    int scan_lookback_days_ = 0;

    fincept::ui::AutoLoadPane* results_pane_ = nullptr;
    fincept::ui::InputMemory* inputs_ = nullptr;
    bool scan_inflight_ = false;
    quint64 inflight_id_ = 0; ///< pane request the scan in flight answers (0 = orphaned)
    quint64 queued_id_ = 0;   ///< pane request waiting for the scan in flight to end
    bool first_inputs_done_ = false;
    bool applying_inputs_ = false; ///< programmatic input changes: not the user's edits

    QTimer* detail_timer_ = nullptr; ///< debounces the chart fetch while arrowing through results
    QString detail_pending_;
    quint64 detail_seq_ = 0;         ///< bumped per fetch and on hide: older answers are dropped
    quint64 detail_inflight_ = 0;    ///< seq of the fetch in flight (0 = none)
    bool detail_dropped_ = false;    ///< a pending / running fetch was dropped by a hide: redo on show
};

} // namespace fincept::screens
