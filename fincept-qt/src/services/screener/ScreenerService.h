#pragma once
// ScreenerService — equity screens over the Yahoo Finance screener (scripts/screener_data.py).
//
// One-shot, user-initiated runs (RUN / pick a screen), so this is a plain service rather than a
// DataHub producer (CLAUDE.md D2 exception): each run has its own inputs, a single consumer and no
// cadence. What it adds over calling the script directly:
//   * identical queries within kCacheTtlMs are answered from memory, and identical in-flight
//     runs are shared (one Python process, many listeners);
//   * the last result per query is kept after it expires, so a failed refresh can fall back to
//     last-known-good data (P11);
//   * the field catalogue (`fields`) is loaded once per session;
//   * saved screens (SettingsRepository JSON) and watchlist hand-off live here, not in the screen.

#include "core/result/Result.h"
#include "storage/repositories/WatchlistRepository.h"

#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QMetaType>
#include <QObject>
#include <QPair>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

#include <limits>

namespace fincept::services {

/// What to run: one of Yahoo's predefined screens, or a custom criteria document (the `custom`
/// JSON accepted by scripts/screener_data.py: region / exchange / sector / industry / peer_group /
/// filters[{field, op, value}]).
struct ScreenerQuery {
    QString predefined_id;  ///< non-empty → predefined screen; criteria/sort are then ignored
    QJsonObject criteria;   ///< custom screen definition
    QString sort_field;     ///< custom only; empty = script default (market cap)
    bool sort_asc = false;  ///< custom only
    int count = 100;        ///< rows per page, 1..250 (Yahoo's cap)
    int offset = 0;         ///< first row (paging)

    bool is_predefined() const { return !predefined_id.isEmpty(); }
    /// Canonical identity: identical queries share a cache entry and an in-flight run.
    QString cache_key() const;
    /// Persistable form (saved screens, screen state). Paging offset is not persisted.
    QJsonObject to_json() const;
    static ScreenerQuery from_json(const QJsonObject& o);
};

/// A column beyond the always-shown base set (criteria fields, sort field, "@ screen" snapshots).
struct ScreenerColumn {
    QString id;    ///< key into ScreenerRow::fields
    QString field; ///< catalogue field id the value belongs to
    QString label;
    QString unit; ///< pct | ratio | price | mcap | money | per_share | shares | days | years | score
    QString description;
};

struct ScreenerRow {
    static constexpr double kMissing = std::numeric_limits<double>::quiet_NaN();

    QString symbol;
    QString name;
    QString exchange;            ///< Yahoo exchange code, e.g. NMS
    QString exchange_name;       ///< e.g. NasdaqGS
    QString currency;            ///< quote currency; may be a minor unit (GBp, ZAc, ILA, KWF)
    QString market_cap_currency; ///< major unit the market cap is expressed in (GBP for GBp)
    QString financial_currency;  ///< reporting currency of fundamentals
    QString sector;
    QString industry;
    double price = kMissing;
    double change = kMissing;
    double change_pct = kMissing; ///< percent points
    double volume = kMissing;
    double avg_volume_3m = kMissing;
    double market_cap = kMissing;
    double pe = kMissing;
    double forward_pe = kMissing;
    double beta = kMissing;
    qint64 market_time = 0;       ///< quote time, epoch seconds (0 = unknown)
    QHash<QString, double> fields; ///< ScreenerColumn::id → value (NaN = missing)
};

struct ScreenerResult {
    QString key; ///< ScreenerQuery::cache_key() of the query that produced it
    ScreenerQuery query;
    QString title;
    QString description;
    QString criteria_text; ///< human-readable criteria as Yahoo evaluated them
    QString criteria_note; ///< snapshot-vs-live caveat from the script
    QString source;
    int total = 0;  ///< matches in Yahoo's universe (not just this page)
    int offset = 0;
    QString sort_field; ///< server-side sort (catalogue id when known)
    bool sort_asc = false;
    QVector<ScreenerColumn> columns;
    QVector<ScreenerRow> rows;
    qint64 as_of = 0;        ///< latest quote time across rows, epoch seconds (0 = unknown)
    int max_delay_min = -1;  ///< worst exchange data delay in minutes (-1 = unknown)
    QDateTime fetched_at;    ///< when this service received it (UTC)
    QStringList warnings;
};

struct SavedScreen {
    QString name;
    ScreenerQuery query;
    QString saved_at; ///< ISO-8601 UTC
};

class ScreenerService : public QObject {
    Q_OBJECT
  public:
    /// Identical queries inside this window are served from memory.
    static constexpr int kCacheTtlMs = 60'000;

    static ScreenerService& instance();

    /// Run `q`. A result for the identical query younger than kCacheTtlMs is re-emitted (queued,
    /// so callers always see the same asynchronous shape) unless `force`. An identical run already
    /// in flight is shared. Returns the query's cache key, which results_ready / run_failed carry.
    QString run(const ScreenerQuery& q, bool force = false);

    /// Most recent result for `key` — fresh or expired (last-known-good) — or nullptr.
    const ScreenerResult* last_result(const QString& key) const;

    /// Load the field catalogue (`screener_data.py fields`). No-op when loaded or loading.
    void load_catalog();
    bool has_catalog() const { return !catalog_.isEmpty(); }
    bool catalog_loading() const { return catalog_loading_; }
    const QJsonObject& catalog() const { return catalog_; }

    // ── Saved screens (SettingsRepository key "screener.saved_screens") ────────────────────
    Result<QVector<SavedScreen>> saved_screens() const;
    /// Insert or overwrite (case-insensitive name match). Refuses to write when the existing list
    /// cannot be read, so a transient DB error never wipes the user's saved screens.
    Result<void> save_screen(const QString& name, const ScreenerQuery& q);
    Result<void> delete_screen(const QString& name);

    // ── Watchlist hand-off ──────────────────────────────────────────────────────────────────
    struct WatchlistAddReport {
        int added = 0;
        int already_present = 0;
        int failed = 0;
    };
    Result<QVector<Watchlist>> watchlists() const;
    Result<Watchlist> create_watchlist(const QString& name);
    /// Add (symbol, name) pairs, skipping symbols already on the list. Publishes one
    /// `watchlist.updated` event when anything was added.
    Result<WatchlistAddReport> add_to_watchlist(const QString& watchlist_id,
                                                const QVector<QPair<QString, QString>>& symbols_and_names);

  signals:
    void results_ready(const fincept::services::ScreenerResult& result);
    void run_failed(const QString& key, const QString& error);
    void catalog_ready();
    void catalog_failed(const QString& error);
    void saved_screens_changed();

  private:
    ScreenerService() = default;
    Q_DISABLE_COPY(ScreenerService)

    void store(const ScreenerResult& r);
    static Result<QVector<SavedScreen>> parse_saved(const QString& json);
    Result<void> write_saved(const QVector<SavedScreen>& screens);

    QHash<QString, ScreenerResult> cache_;
    QStringList cache_order_; ///< LRU, most recent last
    QSet<QString> inflight_;
    QJsonObject catalog_;
    bool catalog_loading_ = false;
};

} // namespace fincept::services

Q_DECLARE_METATYPE(fincept::services::ScreenerResult)
