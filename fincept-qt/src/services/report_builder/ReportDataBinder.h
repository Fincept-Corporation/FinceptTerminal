#pragma once
// ReportDataBinder — fills the Report Builder's bound blocks (core/report/ReportBinding.h).
//
// A bound block remembers its recipe (source + params) and keeps the last fetched numbers
// as a snapshot; refresh() fetches the recipe again through the terminal's existing
// services — DataHub market:quote / market:info / market:history (MarketDataService),
// MarketDataService::fetch_quotes, EquityResearchService (financials, peers, news),
// NewsService (headlines), PortfolioService (holdings, allocation, summary), the user's
// watchlists (WatchlistRepository on the library's DB worker + batched quotes), the
// economic calendar (ConnectorService: the free data_economic_calendar connector) — and writes
// the new snapshot + as-of into the block as ONE undo step ("Refresh <recipe>").
//
// Rules: nothing blocks the GUI thread (every source is asynchronous; big replies are
// shaped on a worker); at most 4 fetches at a time, identical recipes share one fetch;
// every fetch has a request id and a timeout, so a reply for a block whose recipe changed
// meanwhile, or that was refreshed again, is dropped; a failure keeps the last snapshot
// and is reported per block (state(), not written into the document). Works on the
// service's open report, so it runs whether or not the Report Builder screen is open.
//
// GUI-thread singleton.

#include "core/report/ReportBinding.h"
#include "core/report/ReportDocument.h"

#include <QDateTime>
#include <QHash>
#include <QMap>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QVector>

#include <functional>
#include <memory>

namespace fincept::services {

/// The shaped result of one fetch: what to write into a block of `block_type`.
struct BindFetchResult {
    bool ok = false;
    bool cancelled = false;       ///< stopped by the user / a document switch (not a failure)
    QString error;                ///< plain sentence when !ok
    QMap<QString, QString> patch; ///< config keys to set (an empty value removes the key)
    bool has_content = false;
    QString content;
    QString suggested_title;      ///< used only when the block has no title of its own
    QDateTime as_of;              ///< when the data was taken (or its market time)
    QString source_label;         ///< "Yahoo Finance", "Portfolio" ...
};

class ReportDataBinder : public QObject {
    Q_OBJECT
  public:
    static ReportDataBinder& instance();

    enum class State { Idle, Queued, Loading, Failed };
    struct BlockState {
        State state = State::Idle;
        QString message; ///< Failed: why (the last snapshot is still shown)
        QDateTime since;
    };

    /// Refresh one block of the open report. false = it has no refreshable recipe.
    bool refresh(int component_id);
    /// Refresh every bound block (or only `ids`). Returns how many were started.
    int refresh_all(const QVector<int>& ids = {});
    /// Stop everything in flight; blocks keep what they show.
    void cancel_all();

    using FetchDone = std::function<void(const BindFetchResult&)>;
    /// Fetch a recipe without touching any document, shaped for a block of `block_type`
    /// ("table", "chart", "stats_block", "market_data", "list", "sparkline"). `done` runs on
    /// the GUI thread unless `ctx` died. Returns the request id (0 = invalid recipe).
    quint64 fetch(const report::Binding& binding, const QString& block_type, QObject* ctx, FetchDone done);
    void cancel(quint64 request_id);

    /// What a successful fetch writes into block `comp` (fetched with `fetched_with`): the
    /// config patch (an empty value removes the key) and the content. The same rules the
    /// open report uses — for refreshing a report copy that is not open (scheduled runs).
    struct BindPatch {
        QMap<QString, QString> config;
        bool has_content = false;
        QString content;
    };
    static BindPatch make_patch(const report::ReportComponent& comp, const report::Binding& fetched_with,
                                const BindFetchResult& r);
    /// The recipe a block is fetched with: its binding with the report's {{variables}}
    /// expanded (a block bound to {{symbol}} follows the variable).
    static report::Binding effective_binding(const report::ReportComponent& comp, const report::ReportMetadata& meta);

    BlockState state(int component_id) const { return states_.value(component_id); }
    bool busy() const { return !jobs_.isEmpty(); }
    /// Blocks still waiting in the current refresh-all batch, and its size.
    int batch_pending() const { return batch_pending_; }
    int batch_total() const { return batch_total_; }

  signals:
    void block_state_changed(int component_id);
    /// Refresh-all progress (done of total) and its end.
    void progress(int done, int total);
    void batch_finished(int ok, int failed);

  private:
    ReportDataBinder();
    Q_DISABLE_COPY(ReportDataBinder)

    struct Job {
        quint64 id = 0;
        report::Binding binding;
        QString block_type;
        QString key;
        QVector<int> component_ids;
        QVector<int> batch_ids; ///< members counted in the refresh-all batch
        QPointer<QObject> ctx;
        bool has_ctx = false;
        FetchDone done;
        QObject* scope = nullptr; ///< owns this run's subscriptions, connections and timer
        bool running = false;
        bool settled = false;
    };
    using JobPtr = std::shared_ptr<Job>;

    quint64 enqueue(const report::Binding& b, const QString& block_type, int component_id, bool in_batch);
    void pump();
    void start(const JobPtr& job);
    void settle(quint64 job_id, const BindFetchResult& r);
    void deliver(const JobPtr& job, const BindFetchResult& r);
    void apply_to_component(const JobPtr& job, int component_id, const BindFetchResult& r);
    void set_state(int component_id, State s, const QString& message = {});
    void note_batch_done(int component_id, bool ok);
    int timeout_ms(const report::Binding& b) const;

    // Sources (each settles the job exactly once: result, error or timeout).
    void run_quote(const JobPtr& job);
    void run_quotes(const JobPtr& job);
    void run_info(const JobPtr& job);
    void run_history(const JobPtr& job);
    void run_financials(const JobPtr& job);
    void run_peers(const JobPtr& job);
    void run_news(const JobPtr& job);
    void run_headlines(const JobPtr& job);
    void run_portfolio(const JobPtr& job);
    void run_portfolio_summary(const JobPtr& job, const QString& portfolio_id);
    void run_watchlist(const JobPtr& job);
    void run_quote_list(quint64 job_id, const report::Binding& b, const QString& block_type, const QStringList& syms,
                        const QString& title);
    // Economic calendar: one catalogue-connector run per day (ReportEconCalendar.h), the first alone.
    struct EconRun; ///< defined in ReportDataBinder.cpp
    void run_econ_calendar(const JobPtr& job);
    void econ_run_day(quint64 job_id, const std::shared_ptr<EconRun>& run, const QDate& day, bool first);
    void econ_finish(quint64 job_id, const std::shared_ptr<EconRun>& run);

    QHash<quint64, JobPtr> jobs_;
    QVector<quint64> queue_;
    int running_ = 0;
    quint64 next_id_ = 1;
    QHash<int, quint64> latest_for_component_;
    QHash<int, BlockState> states_;
    QHash<int, bool> batch_members_;
    int batch_total_ = 0;
    int batch_pending_ = 0;
    int batch_ok_ = 0;
    int batch_failed_ = 0;
    bool pump_scheduled_ = false;
};

} // namespace fincept::services
