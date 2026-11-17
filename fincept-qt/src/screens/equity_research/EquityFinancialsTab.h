// src/screens/equity_research/EquityFinancialsTab.h
//
// Equity Research → Financials. Wires the free statements feed (EquityResearchService::
// fetch_financials → yfinance_data.py financials: annual, quarterly and Yahoo-TTM statements)
// to EquityFinancialsView, which holds the whole UI:
//   * statements load with the symbol behind a skeleton; the analytics (fin_an) are computed on a
//     worker thread, never on the GUI thread;
//   * each symbol's result is remembered for the session: re-opening it shows it at once and only
//     refreshes it quietly once it is older than 24 h (the service also replays its 30-day saved
//     copy, flagged, while it refreshes);
//   * failures: an error page with RETRY when nothing is on screen, a notice when a refresh fails,
//     a designed page for instruments without statements (ETFs, indices, crypto);
//   * PEERS reuses the peer group the Peers / Valuation tabs load (same service and cache, free).
// Nothing here calls a billed route.
#pragma once

#include "screens/equity_research/EquityFinancialsCommon.h"
#include "services/equity/EquityResearchModels.h"

#include <QFutureWatcher>
#include <QHash>
#include <QStringList>
#include <QWidget>

namespace fincept::services::equity {
struct LoadFailure;
}

namespace fincept::screens {

class EquityFinancialsView;

class EquityFinancialsTab : public QWidget {
    Q_OBJECT
  public:
    explicit EquityFinancialsTab(QWidget* parent = nullptr);
    ~EquityFinancialsTab() override;
    void set_symbol(const QString& symbol);
    /// Section routing hook (for an ISectionLinked-style router; the Equity Research screen does
    /// not route sections yet): "overview" | "income" | "balance" | "cashflow" | "ratios" | "peers".
    void show_section(const QString& key);

  private slots:
    void on_financials_loaded(services::equity::FinancialsData data);

  private:
    void on_load_failed(const services::equity::LoadFailure& f);
    void request();
    void compute();
    void on_computed();
    void remember(const FinBundlePtr& bundle);
    void request_peers();

    QString current_symbol_;
    services::equity::FinancialsData raw_;
    bool have_raw_ = false;
    services::equity::StockInfo info_;
    EquityFinancialsView* view_ = nullptr;
    QFutureWatcher<FinBundlePtr>* watcher_ = nullptr;
    QHash<QString, FinBundlePtr> memo_; ///< this session's results by symbol (LRU, kMemoSize)
    QStringList memo_order_;
    bool peers_pending_ = false;
    /// A failure that arrived while the statements replayed with it were still being computed.
    QString pending_failure_;
    QString pending_failure_code_;
    static constexpr int kMemoSize = 12;
    static constexpr qint64 kQuietRefreshSec = 24 * 3600;
};

} // namespace fincept::screens
