#pragma once
// MarketPanelModel — one Markets panel as a table model over MarketsQuoteStore.
// Rows = the panel's symbols in panel order; columns = every MarketColumns column
// (the view hides). No colour or font is stored: data() reads the ui::colors tokens
// on every call, so a theme change needs only a repaint (dataChanged).
//
// Ticks: the store's rows_changed / flash_tick / linked_symbol_changed become
// dataChanged for this model's own rows. Structure changes (edit, add, remove,
// move) use begin/end insert / remove / move rows; never a reset on a tick.
//
// MarketPanelProxy sorts with SortRole, missing and n/a values last in both
// directions, and never re-sorts on its own (dynamicSortFilter false): a sorted
// panel re-sorts when the store says so (resort_due). Prices are in each
// instrument's own currency: a price column of a panel that mixes currencies is
// sorted on USD-normalised keys (a row with no FX rate sorts with the missing
// values), never on raw numbers in different currencies.

#include "core/symbol/SymbolGroup.h"
#include "screens/markets/MarketsQuoteStore.h"

#include <QAbstractTableModel>
#include <QHash>
#include <QPointer>
#include <QSortFilterProxyModel>
#include <QStringList>

namespace fincept::screens {

class MarketPanelModel : public QAbstractTableModel {
    Q_OBJECT
  public:
    enum Role {
        SortRole = Qt::UserRole + 1, ///< double (NaN = missing), or a lower-case string
        SymbolRole,                  ///< the row's symbol
        FlashRole,                   ///< double -1..1 (LAST, CHG, CHG % only)
        BaselineRole,                ///< sparkline baseline (previous close)
        PriceDecimalsRole,           ///< int
        StateRole,                   ///< int markets_logic::RowState
        LinkedRole,                  ///< bool: the symbol-group's current symbol
        NaRole,                      ///< bool: the cell does not apply to this asset class
        SessionRole,                 ///< int markets_logic::Session (for the session dot)
        TickerRole,                  ///< display ticker ("USD/JPY", "NIFTY 50", "AAPL")
        FailedRole,                  ///< bool: last request failed ("!" glyph)
        NoteRole,                    ///< QString: resolver note (renamed ticker); empty = none
        CurrencyRole,                ///< QString: price currency as quoted ("USD", "GBp"); empty for
                                     ///< index levels, FX rates and unknown currencies (not money)
    };

    explicit MarketPanelModel(MarketsQuoteStore* store, QObject* parent = nullptr);

    void set_symbols(const QStringList& symbols);
    const QStringList& symbols() const { return symbols_; }
    void insert_symbol(const QString& symbol, int at);
    void remove_symbol(const QString& symbol);
    /// Move the row at `from` to position `to` (panel order).
    void move_row(int from, int to);
    int row_of(const QString& symbol) const { return row_index_.value(symbol, -1); }
    QString symbol_at(int row) const { return row >= 0 && row < symbols_.size() ? symbols_.at(row) : QString(); }

    /// The link group whose colour paints the linked-symbol row.
    void set_link_group(fincept::SymbolGroup group);
    /// Text of the internal PANEL column (MOVERS): symbol -> panel title.
    void set_panel_names(const QHash<QString, QString>& names);
    /// Resolver notes shown on rows ("TATAMOTORS is now TMPV"): symbol -> note.
    void set_notes(const QHash<QString, QString>& notes);

    /// CSV export (spec 3.10): headers of one column (range columns give <id>_low / <id>_high;
    /// NAME gives NAME, SYMBOL and CURRENCY, the ISO code of the row's price columns) and the
    /// cells of one row in that column: plain machine-readable numbers (fmt::export_number).
    QStringList export_headers(int column) const;
    QStringList export_cells(int row, int column) const;

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    /// Re-read every cell (theme, format settings, language).
    void refresh_all();

  private:
    void on_rows_changed(const QSet<QString>& symbols);
    void on_flash_tick(const QSet<QString>& symbols);
    void on_linked_changed(const QString& symbol);
    void rebuild_index();
    void update_session(const QString& symbol);
    QString row_tooltip(const QString& symbol, const MarketRow* r) const;
    QString session_text(const QString& symbol, const MarketRow* r) const;

    QPointer<MarketsQuoteStore> store_;
    QStringList symbols_;
    QHash<QString, int> row_index_;
    QHash<QString, int> session_;     ///< symbol -> markets_logic::Session (venue fallback cached)
    QHash<QString, QString> panel_names_;
    QHash<QString, QString> notes_;
    QString linked_;
    fincept::SymbolGroup link_group_ = fincept::SymbolGroup::None;
};

class MarketPanelProxy : public QSortFilterProxyModel {
    Q_OBJECT
  public:
    explicit MarketPanelProxy(QObject* parent = nullptr);

    /// Decides, once per sort, whether a price column mixes currencies (then its keys
    /// are normalised to USD), then sorts.
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

  protected:
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

  private:
    /// USD per one unit of `ccy` as quoted (pence included); NaN without a rate. Cached per sort.
    double usd_per_unit(const QString& ccy) const;

    bool normalise_ = false;                    ///< the sorted price column mixes currencies
    mutable QHash<QString, double> usd_factor_; ///< currency -> usd_per_unit(), this sort only
};

} // namespace fincept::screens
