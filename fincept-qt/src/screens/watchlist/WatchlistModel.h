#pragma once
// WatchlistModel — the Watchlist table's item model.
//
// Rows are the list's entries in their stored (manual) order. With a grouping
// active the model is a two-level tree: group rows (sector, exchange, currency,
// asset class, custom group, ...) whose children are the entries; without one
// it is flat. Every registry column (WatchlistColumns) is a model column; the
// view hides / orders them per the user's layout.
//
// Live data is pushed in per symbol (set_quote / set_info / set_perf / ...).
// A quote updates its row in place (dataChanged), so selection, scroll and
// sort survive a tick; a price move starts a short tick flash (FlashRole).
// Group membership that depends on data (sector, exchange, ...) is re-derived
// when that data arrives, coalesced into one model reset.
//
// WatchlistProxyModel sorts (numeric sort keys, missing values always last,
// group rows stay in group order) and filters (symbol / name / notes / group /
// sector / exchange, case-insensitive).

#include "core/market/MarketSessions.h"
#include "core/symbol/SymbolGroup.h"
#include "core/symbol/SymbolRef.h"
#include "screens/watchlist/WatchlistColumns.h"
#include "services/markets/MarketDataTypes.h"
#include "services/markets/MarketPerformance.h"
#include "storage/repositories/WatchlistRepository.h"

#include <QAbstractItemModel>
#include <QDateTime>
#include <QHash>
#include <QSet>
#include <QSortFilterProxyModel>
#include <QStringList>
#include <QVector>

#include <functional>

class QTimer;

namespace fincept::screens {

enum class WatchlistGrouping { None, Sector, Industry, Exchange, Currency, AssetClass, Country, Custom };

/// Stable key ("none", "sector", ...) for saved layouts, and back.
QString watchlist_grouping_key(WatchlistGrouping g);
WatchlistGrouping watchlist_grouping_from_key(const QString& key);
QString watchlist_grouping_label(WatchlistGrouping g);
QVector<WatchlistGrouping> watchlist_groupings();

/// SymbolRef asset-class slug of an instrument: "equity" (stocks, ETFs, funds),
/// "index", "fx", "crypto", "future". Uses Yahoo's quote type when known, else
/// the symbol's shape ("^GSPC", "EURUSD=X", "GC=F", "BTC-USD").
QString watchlist_asset_class(const QString& symbol, const QString& quote_type = {});

/// History-load state of a row's daily series (performance columns).
enum class WatchlistHistoryState : quint8 { None, Loading, Ready, Failed };

/// Everything known about one list entry.
struct WatchlistRowState {
    WatchlistStock stock;
    QString resolved_name; ///< MarketDataService::resolve_names

    bool has_quote = false;
    services::QuoteData quote;
    qint64 quote_ms = 0; ///< wall-clock ms the last quote arrived
    QString error;       ///< last quote error (cleared by the next quote)

    bool has_info = false;
    services::InfoData info;

    WatchlistHistoryState history = WatchlistHistoryState::None;
    services::PerformanceStats perf;

    QVector<double> spark;

    qint64 flash_ms = 0; ///< start of the current tick flash (0 = none)
    int flash_dir = 0;   ///< +1 up tick, -1 down tick

    int alerts_active = 0;
    int alerts_total = 0;
    bool alert_fired_today = false;
    QString alerts_tip;

    QString display_name() const;
    /// Quote currency as reported, else the one implied by the symbol (may be empty).
    QString currency() const;
    QString asset_class() const;
    /// Exchange hint for venue lookups.
    QString exchange_hint() const;
    SymbolRef symbol_ref() const;
};

class WatchlistModel : public QAbstractItemModel {
    Q_OBJECT
  public:
    enum Role {
        SortRole = Qt::UserRole + 1, ///< double (NaN = missing) or lower-case string
        SymbolRole,                  ///< the row's symbol (empty on group rows)
        IsGroupRole,                 ///< bool
        GroupKeyRole,                ///< group key (group rows and their children)
        FlashRole,                   ///< double -1..1: tick flash strength and direction
        FilterTextRole,              ///< lower-case haystack for the filter box
        BaselineRole,                ///< sparkline baseline (previous close)
        PriceDecimalsRole,           ///< int: decimals of the row's price (range-bar labels)
    };

    /// MIME type of a row drag inside / between watchlist views (JSON
    /// {"list": id, "symbols": [...]}); the drag also carries the shared
    /// symbol MIME so any panel accepts it as a symbol link.
    static constexpr const char* kRowsMime = "application/x-fincept-watchlist-rows";

    /// Reorder request from a drop. `anchor` is the symbol the moved rows go
    /// before (or after, when `after`); empty = end. `group_key` is the group
    /// dropped into (custom grouping), valid when `has_group`.
    using MoveHandler = std::function<void(const QStringList& moved, const QString& anchor, bool after,
                                           const QString& group_key, bool has_group)>;
    /// A symbol dropped in from another panel or list.
    using ExternalDropHandler = std::function<void(const QList<SymbolRef>& refs)>;

    explicit WatchlistModel(QObject* parent = nullptr);
    ~WatchlistModel() override;

    // ── Structure ───────────────────────────────────────────────────────────
    /// Replace the entries (manual order). Live data of symbols still listed is kept.
    void set_stocks(const QString& list_id, const QVector<WatchlistStock>& stocks);
    QString list_id() const { return list_id_; }
    void set_grouping(WatchlistGrouping g);
    WatchlistGrouping grouping() const { return grouping_; }
    bool grouped() const { return grouping_ != WatchlistGrouping::None; }
    void set_link_group(SymbolGroup g) { link_group_ = g; }
    void set_move_handler(MoveHandler h) { move_handler_ = std::move(h); }
    void set_external_drop_handler(ExternalDropHandler h) { external_drop_handler_ = std::move(h); }

    // ── Data feeds ──────────────────────────────────────────────────────────
    /// `received_ms`: when the hub published the quote (DataHub::last_publish_ms); 0 = now.
    void set_quote(const QString& sym, const services::QuoteData& q, qint64 received_ms = 0);
    void set_error(const QString& sym, const QString& error);
    void set_info(const QString& sym, const services::InfoData& info);
    void set_history_state(const QString& sym, WatchlistHistoryState st);
    void set_perf(const QString& sym, const services::PerformanceStats& perf);
    void set_spark(const QString& sym, const QVector<double>& values);
    void set_names(const QHash<QString, QString>& names);
    void set_stock_fields(const QString& sym, const QString& notes, const QString& group);
    void set_alert_summary(const QString& sym, int active, int total, bool fired_today, const QString& tip);
    void clear_alert_summaries();
    void set_display_currency(const QString& ccy);
    QString display_currency() const { return display_ccy_; }

    /// Re-evaluate time-dependent cells (market state, staleness, FX) — call periodically.
    void refresh_time_cells();
    /// Repaint every cell (theme / format settings changed).
    void refresh_all();

    // ── Lookup ──────────────────────────────────────────────────────────────
    const QVector<WatchlistRowState>& states() const { return rows_; }
    const WatchlistRowState* state(const QString& sym) const;
    QStringList symbols() const;
    bool is_group(const QModelIndex& idx) const;
    /// The entry behind `idx` (any column), or nullptr for group rows.
    const WatchlistRowState* state_at(const QModelIndex& idx) const;
    QString symbol_at(const QModelIndex& idx) const;
    QModelIndex index_of_symbol(const QString& sym, int column = 0) const;
    QModelIndex group_index(const QString& key) const;
    QStringList group_keys() const;
    QString group_label(const QString& key) const;
    QString group_key_of(const WatchlistRowState& r) const;
    QStringList group_members(const QString& key) const;

    // ── Values (shared by the view, footer, heatmap and export) ─────────────
    /// Numeric value of `column` for `r`; NaN when absent or not numeric.
    double value(const WatchlistRowState& r, int column) const;
    /// Cell text as displayed.
    QString display_text(const WatchlistRowState& r, int column) const;
    /// Cell text for export: numbers at full precision in C locale, no grouping.
    QStringList export_cells(const WatchlistRowState& r, int column) const;
    /// Export header(s) of a column (range columns export low and high).
    static QStringList export_headers(int column);
    /// Session status of the row's venue (cached ~10 s); nullptr venue when unknown.
    core::market::SessionStatus session(const WatchlistRowState& r, const core::market::Venue** venue) const;
    /// Quote older than the staleness limit, or failing since its last update.
    bool is_stale(const WatchlistRowState& r) const;

    // ── QAbstractItemModel ──────────────────────────────────────────────────
    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& idx, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& idx) const override;
    Qt::DropActions supportedDropActions() const override;
    Qt::DropActions supportedDragActions() const override;
    QStringList mimeTypes() const override;
    QMimeData* mimeData(const QModelIndexList& indexes) const override;
    bool canDropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                         const QModelIndex& parent) const override;
    bool dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                      const QModelIndex& parent) override;

  signals:
    /// Group membership changed (after the model reset that applied it).
    void groups_rebuilt();
    /// A row's data changed (quote, info, perf, ...); coalesce before heavy work.
    void row_data_changed(const QString& symbol);

  private:
    struct Group {
        QString key;
        QString label;
        QVector<int> members; ///< row indexes, manual order
    };
    struct SessionCache {
        qint64 at_ms = 0;
        const core::market::Venue* venue = nullptr;
        core::market::SessionStatus status;
    };

    int row_of(const QString& sym) const { return row_by_symbol_.value(sym, -1); }
    int row_index(const QModelIndex& idx) const; ///< rows_ index of a symbol row, -1 for groups
    QModelIndex model_index(int row, int column) const;
    void rebuild_groups();
    void schedule_regroup();
    void apply_regroup();
    void check_group_key(int row);
    void emit_row_changed(int row);
    void emit_columns_changed(const QVector<int>& columns, const QList<int>& roles = {});
    void on_flash_tick();

    QVariant group_data(int group, int column, int role) const;
    QVariant cell_data(const WatchlistRowState& r, int column, int role) const;
    QString cell_tooltip(const WatchlistRowState& r, int column) const;
    QVariant cell_foreground(const WatchlistRowState& r, int column) const;
    double flash_strength(const WatchlistRowState& r) const;

    QString list_id_;
    QVector<WatchlistRowState> rows_;
    QHash<QString, int> row_by_symbol_;
    WatchlistGrouping grouping_ = WatchlistGrouping::None;
    QVector<Group> groups_;
    QVector<int> row_group_; ///< rows_ index -> groups_ index
    QVector<int> row_pos_;   ///< rows_ index -> position inside its group
    QHash<QString, QString> names_;
    QString display_ccy_; ///< the user's display currency (set in the constructor) until the screen picks one
    SymbolGroup link_group_ = SymbolGroup::None;
    MoveHandler move_handler_;
    ExternalDropHandler external_drop_handler_;

    QTimer* regroup_timer_ = nullptr;
    QTimer* flash_timer_ = nullptr;
    QSet<int> flashing_;
    mutable QHash<QString, SessionCache> session_cache_;
};

class WatchlistProxyModel : public QSortFilterProxyModel {
    Q_OBJECT
  public:
    explicit WatchlistProxyModel(QObject* parent = nullptr);

    void set_filter_text(const QString& text);
    QString filter_text() const { return filter_; }
    /// The sort comparison (source indexes), honouring the current sort order's
    /// missing-last rule — for reproducing the visible order outside the view.
    bool less(const QModelIndex& left, const QModelIndex& right) const { return lessThan(left, right); }

  protected:
    bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const override;
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

  private:
    QString filter_;
};

} // namespace fincept::screens
