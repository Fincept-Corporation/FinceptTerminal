#pragma once
// QuoteGrid — the dashboard's live quote table.
//
// Rows are keyed by symbol and updated in place (no clear-and-refill), so the
// selection, the scroll position and the hover survive every tick. Each row:
//   NAME   market-state dot (exchange session: open / pre-post / closed) + label
//   LAST   per-instrument decimals (Yahoo priceHint, FX pips, sub-unit prices)
//   CHG / CHG%
//   DAY    day low…high range bar with the last price
//   TREND  5-day hourly sparkline (market:sparkline:<SYM>), baseline = previous close
//   52W    52-week range bar (optional)
//   VOL    volume (optional)
// A price change flashes its cells (positive / negative wash, ~0.7 s). Optional
// columns drop out as the tile narrows. Optional group header rows ("Energy",
// "Metals") are inserted where an entry's group changes.
//
// The grid is a view only: the owning tile subscribes the hub topics and pushes
// quotes / sparklines / errors in (so BaseWidget's topic tracking keeps working).

#include "services/markets/MarketDataTypes.h"
#include "ui/tables/DataTable.h"

#include <QDateTime>
#include <QHash>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QVector>

namespace fincept::screens::widgets {

struct QuoteGridEntry {
    QString symbol;
    QString label; ///< display name; empty = the symbol
    QString group; ///< optional header row key (already translated)
};

class QuoteGrid : public ui::DataTable {
    Q_OBJECT
  public:
    enum Column { ColName = 0, ColLast, ColChg, ColPct, ColDay, ColTrend, Col52w, ColVol, ColSession, ColCount };
    enum OptionalColumn { OptDay = 0x1, OptTrend = 0x2, Opt52w = 0x4, OptVolume = 0x8, OptSession = 0x10 };

    /// Roles on the NAME cell.
    static constexpr int SymbolRole = Qt::UserRole;       ///< raw symbol (empty on group rows)
    static constexpr int StateRole = Qt::UserRole + 60;   ///< QColor of the market-state dot (invalid = none)
    static constexpr int FlashTimeRole = Qt::UserRole + 61; ///< qint64 ms of the last price change
    static constexpr int FlashUpRole = Qt::UserRole + 62;   ///< bool: the last change was up
    static constexpr int BaselineRole = Qt::UserRole + 63;  ///< double: sparkline baseline (prev close)

    explicit QuoteGrid(QWidget* parent = nullptr);

    /// Optional columns this grid may show (OptionalColumn flags); they still
    /// drop out when the grid is too narrow. Default: day range + trend.
    void set_optional_columns(int flags);
    /// Replace the rows (structural). Cached quotes for symbols that stay are kept.
    void set_entries(const QVector<QuoteGridEntry>& entries);
    const QVector<QuoteGridEntry>& entries() const { return entries_; }

    /// Venue (MarketSessions id) whose session a symbol follows, for symbols the
    /// suffix rules cannot place (index tickers such as ^GSPC).
    void set_venue_hints(const QHash<QString, QString>& hints);

    void update_quote(const QString& symbol, const services::QuoteData& q);
    void update_sparkline(const QString& symbol, const QVector<double>& closes);
    /// Mark a symbol's topic as failed (`tooltip` = the reason; empty clears it).
    void set_row_error(const QString& symbol, const QString& tooltip);
    /// Re-render every row (theme change, language change).
    void rerender();

    /// Symbol of a view row; empty for group header rows.
    QString symbol_at(int row) const;
    bool has_quote(const QString& symbol) const { return quotes_.contains(symbol); }
    const QHash<QString, services::QuoteData>& quotes() const { return quotes_; }

    /// Newest quote time (epoch s) and the largest exchange delay (min, -1 unknown) across rows.
    qint64 latest_market_time() const;
    int max_delay_min() const;

  protected:
    void resizeEvent(QResizeEvent* e) override;

  private:
    void rebuild_rows();
    void render_row(const QString& symbol);
    void apply_column_visibility();
    void on_flash_tick();
    /// SESSION column: venue state and time to the next change, refreshed every 20 s.
    void render_sessions();
    QTableWidgetItem* cell(int row, int col);
    QHash<QString, QString> venue_hints_;
    QTimer* session_timer_ = nullptr;

    QVector<QuoteGridEntry> entries_;
    QHash<QString, int> row_of_;  ///< symbol -> view row
    QHash<QString, services::QuoteData> quotes_;
    QHash<QString, QVector<double>> sparks_;
    QHash<QString, QString> errors_;
    int optional_ = OptDay | OptTrend;
    QTimer* flash_timer_ = nullptr;
    qint64 last_flash_ms_ = 0;
};

/// Paints the market-state dot on the NAME column and the price-change flash
/// on the price columns. Installed as the grid's default delegate.
class QuoteGridDelegate : public QStyledItemDelegate {
  public:
    explicit QuoteGridDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}
    void paint(QPainter* p, const QStyleOptionViewItem& opt, const QModelIndex& idx) const override;
};

} // namespace fincept::screens::widgets
