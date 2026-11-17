#pragma once
// Equity Watchlist — dense, sortable quote list: SYMBOL (display + venue) / LTP /
// CHG% (sign + arrow + colour) / TREND (sparkline). Rows never sit on a bare error:
// a row with an earlier price shows it greyed with a stale marker, a row without one
// shows a short reason in the row itself (full text in the tooltip).

#include "core/instrument/Instrument.h"
#include "screens/equity_trading/EquitySymbolBox.h"
#include "trading/TradingTypes.h"

#include <QComboBox>
#include <QEvent>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QMutex>
#include <QPair>
#include <QPushButton>
#include <QSet>
#include <QTableWidget>
#include <QToolButton>
#include <QVector>
#include <QWidget>

class QTimer;

namespace fincept::screens::equity {

struct WatchlistEntry {
    QString symbol;
    double ltp = 0.0;
    double change_pct = 0.0;
    double volume = 0.0;
    bool has_data = false;
    QString error;         // last quote failure (short form in the row, full in the tooltip)
    qint64 updated_ms = 0; // when the last good quote arrived
};

class EquityWatchlist : public QWidget {
    Q_OBJECT
  public:
    /// How a row is labelled and priced. The key is the screen symbol.
    struct RowInfo {
        QString display;        // "RELIANCE", "NIFTY 50", "EUR/USD"
        QString venue;          // "NSE", "NASDAQ", "FX"
        QString tooltip;        // name / full description
        QString currency;       // price currency as quoted ("INR", "GBp")
        int digits = -1;        // price decimals; -1 = from the value
        QString market_key;     // Yahoo ticker (sparkline); empty = none
        QString blocked_reason; // e.g. "not tradable on Zerodha" — shown, never hidden
    };

    explicit EquityWatchlist(QWidget* parent = nullptr);

    void set_symbols(const QStringList& symbols);
    void set_row_info(const QHash<QString, RowInfo>& info);
    // Patch a single symbol's row in place. No-op if the symbol isn't tracked.
    // Per-tick hot path — must stay O(1) per row, never a full rebuild.
    void update_quote(const trading::BrokerQuote& quote);
    // Mark a row failed: no quote could be had for it. A row that already has a
    // price keeps it greyed with a stale marker; otherwise the row shows a short
    // reason. `reason` (verbatim) goes to the tooltip.
    void set_quote_error(const QString& symbol, const QString& reason);
    // Forget every row's price and error. Used when the price source changes.
    void clear_quotes();
    // A note about the price source as a whole, shown above the rows. Empty hides it.
    void set_feed_notice(const QString& title, const QString& detail);
    void set_active_symbol(const QString& symbol);
    void set_broker_id(const QString& broker_id);
    void add_symbol(const QString& symbol);

    /// The add box (shared symbol search). The screen sets its options / row filter.
    SymbolBox* add_box() const { return add_box_; }

    // Populate the watchlist switcher. `lists` is (id, name) pairs; `active_id`
    // selects the current entry. Programmatic — does not emit watchlist_selected.
    void set_watchlists(const QVector<QPair<QString, QString>>& lists, const QString& active_id);

    /// Width (px) the panel needs to show every ticker in full beside its LTP,
    /// CHG% and a short trend line.
    int content_width() const;

  signals:
    void symbol_selected(const QString& symbol);
    void symbol_added(const QString& symbol); // raw symbol (broker fallback / chart "add")
    void instrument_added(const fincept::core::instrument::Instrument& instrument); // picked in the search box
    void add_unresolved(const QString& text, const QString& message);
    void symbol_removed(const QString& symbol); // user removed a row from the active list
    void watchlist_selected(const QString& id); // user picked another list in the combo
    void watchlist_create_requested(const QString& name);
    void watchlist_rename_requested(const QString& id, const QString& name);
    void watchlist_delete_requested(const QString& id);
    // Row context menu
    void trade_requested(const QString& symbol, bool is_buy);
    void research_requested(const QString& symbol);
    void news_requested(const QString& symbol);
    void alert_requested(const QString& symbol);

  protected:
    void changeEvent(QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    /// Viewport resizes re-fit the columns; keys on the table.
    bool eventFilter(QObject* obj, QEvent* event) override;

  private slots:
    void on_cell_clicked(int row, int col);
    void on_filter_changed(const QString& text);
    void on_watchlist_combo_activated(int index);
    void on_watchlist_menu();
    void on_table_context_menu(const QPoint& pos);

  private:
    enum Col { ColSymbol = 0, ColLtp = 1, ColChg = 2, ColTrend = 3, ColCount = 4 };

    void rebuild_table();
    void retranslateUi();
    // Paint a row's cells + tooltips from its entry (normal, stale, failed, loading).
    void render_row(int row, const WatchlistEntry& e);
    int row_for(const QString& symbol) const; // -1 when not shown
    QString symbol_text(const QString& symbol) const;
    QString price_text(const QString& symbol, double v) const;
    static QString short_reason(const QString& error);
    void sort_entries();
    void on_header_clicked(int section);
    void select_row_by_keyboard(int row);
    void subscribe_sparklines();
    void unsubscribe_sparklines();
    void on_sparkline(const QString& market_key, const QVector<double>& closes);

    struct ColumnNeeds {
        int symbol = 0;
        int ltp = 0;
        int chg = 0;
    };
    ColumnNeeds column_needs(bool price_floor) const;
    void fit_columns();
    void schedule_fit();
    bool fit_pending_ = false;
    qsizetype fitted_ltp_len_ = 0;
    qsizetype fitted_chg_len_ = 0;

    QComboBox* wl_combo_ = nullptr;      // named-watchlist switcher
    QToolButton* wl_menu_btn_ = nullptr; // New / Rename / Delete menu
    QLineEdit* filter_edit_ = nullptr;
    SymbolBox* add_box_ = nullptr;
    QLabel* count_label_ = nullptr;
    QWidget* notice_box_ = nullptr; // set_feed_notice(); hidden while empty
    QLabel* notice_title_ = nullptr;
    QLabel* notice_detail_ = nullptr;
    QTableWidget* table_ = nullptr;
    QTimer* key_select_timer_ = nullptr; // debounces keyboard row changes into symbol_selected

    QVector<WatchlistEntry> entries_;
    // symbol → index into entries_ (== table row). Rebuilt by rebuild_table().
    QHash<QString, int> row_of_;
    QHash<QString, RowInfo> info_;
    QHash<QString, QVector<double>> sparks_; // market key → closes
    QSet<QString> spark_subs_;               // market keys subscribed
    QString active_symbol_;
    QString broker_id_;
    QString pending_key_select_;
    int sort_col_ = -1; // -1 = list order
    bool sort_desc_ = false;
    bool visible_ = false;
    QMutex mutex_;
};

} // namespace fincept::screens::equity
