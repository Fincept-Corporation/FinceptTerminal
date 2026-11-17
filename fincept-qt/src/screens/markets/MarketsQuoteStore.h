#pragma once
// MarketsQuoteStore — the one data owner of the Markets screen (spec 2.6 / 3.5 / 3.6).
//
// Holds the state of every symbol the screen shows (panels, pulse ribbon, detail
// pane, overview views) and is the only class in screens/markets that talks to the
// DataHub for quotes and sparklines. Each unique symbol is subscribed once however
// many panels list it.
//
// Two subscription sets, each under its own owner QObject:
//   live set   ribbon symbols + the detail symbol: subscribed while active; the hub
//              refreshes them at the quote TTL (30 s).
//   panel set  every panel symbol not in the live set. Interval LIVE: stays
//              subscribed while active. Interval 1M..1H: a cycle subscribes it in
//              stages of 40, waits until every symbol resolved (value / error /
//              35 s watchdog), unsubscribes and schedules the next cycle. OFF: one
//              cycle on activation when the data is older than 5 min, then F5 only.
// Sparklines: only on request (want_sparklines, visible rows), at most 40 per stage.
//
// Hub deliveries only write the row and mark the symbol; one queued flush per
// event-loop turn emits rows_changed(symbols) + coverage_changed().

#include "core/instrument/Instrument.h"
#include "screens/markets/MarketsLogic.h"
#include "services/markets/MarketDataTypes.h"

#include <QHash>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QVector>

class QTimer;

namespace fincept::screens {

struct MarketRow {
    QString symbol;
    QString name; ///< from the quote (QuoteData::name) or resolve_names; empty = unknown
    core::instrument::AssetClass cls = core::instrument::AssetClass::Unknown;
    enum class Load { Idle, Loading, Ok, Failed };
    Load load = Load::Idle;
    bool has_quote = false;
    services::QuoteData quote;
    qint64 quote_ms = 0;      ///< when the quote was published (wall clock ms)
    QString error;            ///< reason of the last failed request; empty = none since the last quote
    qint64 error_ms = 0;
    qint64 requested_ms = 0;  ///< when the outstanding request started (watchdog); 0 = none
    bool timed_out = false;   ///< failed by the watchdog
    QVector<double> spark;
    enum class Spark { None, Loading, Ready, Failed };
    Spark spark_state = Spark::None;
    qint64 flash_ms = 0;
    int flash_dir = 0;
};

class MarketsQuoteStore : public QObject {
    Q_OBJECT
  public:
    explicit MarketsQuoteStore(QObject* parent = nullptr);
    ~MarketsQuoteStore() override;

    /// Every panel's symbols, panels in reading order (panels on screen first).
    void set_panel_symbols(const QVector<QStringList>& per_panel_in_reading_order);
    /// Pulse-ribbon symbols (always live while active).
    void set_live_symbols(const QStringList& ribbon);
    /// The detail pane's symbol (live while active); empty = none.
    void set_detail_symbol(const QString& symbol);
    QString detail_symbol() const { return detail_symbol_; }

    /// 0 = LIVE (panel set stays subscribed), else 60 000 .. 3 600 000 ms between cycles.
    void set_interval_ms(int ms);
    int interval_ms() const { return interval_ms_; }
    /// false = OFF (one cycle on activation when data is older than 5 min, then F5 only).
    void set_auto(bool on);
    bool auto_refresh() const { return auto_; }
    /// The screen is visible: subscribe / start timers. false: drop every subscription.
    void set_active(bool active);
    bool active() const { return active_; }

    /// F5: a forced cycle now (resets the hub's backoff). Ignored while a forced cycle runs.
    void refresh_now();
    /// Rows on screen (+ margin) whose 5D sparkline should be loaded; debounced by the caller.
    void want_sparklines(const QStringList& symbols);

    const MarketRow* row(const QString& symbol) const;
    /// Every row symbol of the panel set (unique, reading order).
    const QStringList& panel_symbols() const { return panel_symbols_; }
    markets_logic::RowState row_state(const MarketRow& r) const;
    bool is_stale(const MarketRow& r) const;
    /// -1..1 tick flash strength (sign = direction), 0 when idle.
    double flash_strength(const MarketRow& r) const;
    /// Name for display: the instrument name, else the display symbol, else the symbol.
    QString display_name(const QString& symbol) const;

    struct Coverage {
        int total = 0;
        int priced = 0;
        int failed = 0;
        int loading = 0;
        int timed_out = 0;
        int up = 0;
        int down = 0;
        int flat = 0;
        qint64 newest_quote_ms = 0;
        int max_delay_min = -1;
        bool cycle_running = false;
        bool cycle_forced = false;
        bool all_failed = false; ///< every symbol of the last cycle failed
    };
    Coverage coverage() const;
    /// "<display>: <reason>" per failed panel symbol, reading order.
    QStringList failed_lines() const;
    /// The single reason shared by every failure, or empty when they differ.
    QString shared_failure_reason() const;

    void set_linked_symbol(const QString& symbol);
    QString linked_symbol() const { return linked_; }

  signals:
    void rows_changed(const QSet<QString>& symbols); ///< coalesced, once per event-loop turn
    void flash_tick(const QSet<QString>& symbols);
    void coverage_changed();
    void cycle_started();
    void cycle_finished();
    /// Sorted panels may re-sort: after a cycle and, in LIVE mode, at most every 5 s.
    void resort_due();
    void linked_symbol_changed(const QString& symbol);

  private:
    MarketRow& ensure_row(const QString& symbol);
    void warm_start(MarketRow& r);
    void prune_rows();
    QSet<QString> live_set() const;

    void subscribe_quote(QObject* owner, const QString& symbol);
    void unsubscribe_quote(QObject* owner, const QString& symbol);
    void on_quote(const QString& symbol, const QVariant& value);
    void on_quote_error(const QString& symbol, const QString& error);
    void on_spark(const QString& symbol, const QVariant& value);
    void on_spark_error(const QString& symbol, const QString& error);

    void sync_live_subscriptions();
    void start_cycle(bool forced, const QStringList& only = {});
    void next_stage();
    void maybe_finish_cycle();
    void finish_cycle();
    void schedule_next_cycle();
    void check_watchdog();
    void next_spark_stage();

    void mark_changed(const QString& symbol);
    void flush();
    void on_flash_timer();
    void request_names();

    QHash<QString, MarketRow> rows_;
    QStringList panel_symbols_;    ///< union of panel symbols, reading order
    QSet<QString> panel_set_;
    QStringList live_symbols_;     ///< ribbon
    QString detail_symbol_;
    QString linked_;

    int interval_ms_ = 60'000;
    bool auto_ = true;
    bool active_ = false;

    QObject* live_owner_ = nullptr;
    QObject* panel_owner_ = nullptr;
    QObject* spark_owner_ = nullptr;
    QSet<QString> live_subscribed_;
    QSet<QString> panel_subscribed_;
    QSet<QString> spark_subscribed_;

    // Cycle
    bool cycle_running_ = false;
    bool cycle_forced_ = false;
    bool first_cycle_done_ = false;
    bool last_cycle_all_failed_ = false;
    qint64 cycle_started_ms_ = 0;
    qint64 last_cycle_end_ms_ = 0;
    QStringList cycle_queue_;     ///< symbols not yet staged
    QSet<QString> cycle_pending_; ///< staged, not resolved
    QSet<QString> stage_pending_;
    QSet<QString> logged_failures_;
    QSet<QString> names_asked_;

    // Sparklines
    QStringList spark_queue_;
    QSet<QString> spark_wanted_;
    QSet<QString> spark_stage_pending_;

    QTimer* stage_timer_ = nullptr;
    QTimer* spark_stage_timer_ = nullptr;
    QTimer* watchdog_timer_ = nullptr;
    QTimer* interval_timer_ = nullptr;
    QTimer* flash_timer_ = nullptr;
    QTimer* age_timer_ = nullptr;
    QTimer* resort_timer_ = nullptr;

    QSet<QString> pending_changed_;
    bool flush_scheduled_ = false;
    QSet<QString> flashing_;
};

} // namespace fincept::screens
