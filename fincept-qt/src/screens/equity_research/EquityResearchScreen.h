#pragma once
#include "core/symbol/IGroupLinked.h"
#include "screens/common/ISectionLinked.h"
#include "screens/common/IStatefulScreen.h"
#include "services/equity/EquityResearchModels.h"

#include <QHash>
#include <QHideEvent>
#include <QLabel>
#include <QList>
#include <QPair>
#include <QPushButton>
#include <QShowEvent>
#include <QTabWidget>
#include <QTimer>
#include <QWidget>

#include <functional>

class QLineEdit;

namespace fincept::symbol_dnd {
class SymbolDragFilter;
}

namespace fincept::services::equity {
struct LoadFailure;
struct SymbolResolution;
}

namespace fincept::screens {

class EquityOverviewTab;
class EquityFinancialsTab;
class EquityFilingsTab;
class EquityAnalysisTab;
class EquityTechnicalsTab;
class EquityPeersTab;
class EquityNewsTab;
class EquitySentimentTab;
class EquityValuationTab;
class EquityOptionsTab;
class EquityAnalystTab;
class EquityOwnershipTab;
class EquityDividendsTab;
class EquityEarningsTab;

class EquityResearchScreen : public QWidget, public IStatefulScreen, public IGroupLinked, public ISectionLinked {
    Q_OBJECT
    Q_INTERFACES(fincept::IGroupLinked)
  public:
    explicit EquityResearchScreen(QWidget* parent = nullptr);

    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return "equity_research"; }

    void set_group(SymbolGroup g) override;
    SymbolGroup group() const override { return link_group_; }
    void on_group_symbol_changed(const SymbolRef& ref) override;
    SymbolRef current_symbol() const override;

    /// ISectionLinked — a tab by its key (overview financials filings analysis technicals peers news
    /// sentiment valuation dividends earnings options analyst ownership; also "tab:<key>"), and
    /// "financials/<part>" (overview | income | balance | cashflow | ratios | peers; or the part as
    /// focus). The tab's own switch: it loads the symbol on screen as a click would.
    bool open_section(const QString& section, const QString& focus) override;

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void changeEvent(QEvent* event) override;

  private slots:
    void on_quote_loaded(services::equity::QuoteData quote);
    void on_info_loaded(services::equity::StockInfo info);
    void on_tab_changed(int index);
    // BUY/SELL from the title bar → routes the current symbol to the existing
    // Equity Trading order ticket (same form + paper/live engine) via EventBus.
    void on_trade_clicked(bool is_buy);
    // Opens the "Download Price Data (CSV)" dialog and exports yfinance OHLCV
    // history for current_symbol_ to a user-chosen file.
    void on_download_csv_clicked();
    void on_financials_loaded(services::equity::FinancialsData data);

  private:
    void build_ui();
    QWidget* build_title_bar();
    QWidget* build_quote_bar();
    void update_quote_bar(const services::equity::QuoteData& q);
    /// The translated VOL / H-L fields of the quote bar (also re-applied on a language switch).
    void update_quote_details(const services::equity::QuoteData& q);
    /// MKT CAP / REC from the instrument's info (also re-applied on a language switch).
    void update_info_labels(const services::equity::StockInfo& info);
    /// Blank every quote-bar field that belongs to the previous symbol (change, volume,
    /// H/L, market cap, recommendation) while the new symbol's data is in flight.
    void reset_quote_bar();
    /// `publish_to_group` = false when the symbol was translated from a linked panel's own
    /// form (see research_symbol_from_ref): echoing the yfinance form back would make e.g.
    /// Equity Trading select "RELIANCE.NS" as a broker symbol.
    /// `allow_resolve` = false for the reload of a resolved symbol, so a failure there can
    /// never start another lookup (no resolve loops).
    void load_symbol(const QString& symbol, bool publish_to_group = true, bool allow_resolve = true);

    // ── Failure display + symbol resolution ─────────────────────────────────
    /// Symbol-attributed failure from the service: header text/tooltip, stale marking of a
    /// quote that is already on screen, and the resolve trigger.
    void on_load_failed(const services::equity::LoadFailure& failure);
    /// Ask the service what Yahoo calls `query`; a slow answer for a symbol the user has
    /// since replaced is dropped (load_seq_).
    void start_resolve(const QString& query);
    void show_symbol_not_found(const services::equity::SymbolResolution& resolution);
    void show_candidates_menu(QWidget* anchor);
    /// price_label_ tone: "" (normal), "stale" (last price kept after a failed refresh) or
    /// "error" (no price for this symbol), plus the tooltip carrying the full reason.
    void set_price_tone(const QString& tone, const QString& tooltip);
    /// The far-right hint: the command-bar tip, or what the typed text resolved to.
    void update_hint();
    /// Lazy tabs: only the tab on screen is handed the symbol (and fetches); the others
    /// load the first time they are shown. Each tab's set_symbol() is a no-op for the
    /// symbol it already shows.
    void activate_tab(QWidget* tab);
    /// Deferred tabs: the tab widget holds an empty page for each; the tab itself is built
    /// inside it the first time the page is shown (activate_tab). Returns the tab for `page`
    /// (`page` itself for a tab that was built up front).
    QWidget* materialize_tab(QWidget* page);
    /// The tab-widget page whose kResearchTabKeyProp is `key` (nullptr if none).
    QWidget* tab_page(const char* key) const;
    /// "OPEN IN ▾": the active symbol to Equity Trading / News / Backtesting / a watchlist.
    void show_open_in_menu(QWidget* anchor);
    void retranslateUi();
    void hub_subscribe_broker_quote();
    void hub_unsubscribe_broker_quote();
    // Show the BUY/SELL buttons only when a usable broker (paper, or live + connected)
    // trades the market the current symbol is listed on.
    void update_trade_buttons();

    QLabel* title_label_ = nullptr;
    QLabel* symbol_label_ = nullptr;
    QPushButton* buy_btn_ = nullptr;
    QPushButton* sell_btn_ = nullptr;
    QPushButton* backtest_btn_ = nullptr;
    QPushButton* csv_btn_ = nullptr;
    QPushButton* watch_btn_ = nullptr;
    QPushButton* open_in_btn_ = nullptr;
    QLineEdit* symbol_edit_ = nullptr;
    QPushButton* suggest_btn_ = nullptr; ///< "DID YOU MEAN? ▾" — only while a symbol was not found
    QLabel* hint_label_ = nullptr;
    symbol_dnd::SymbolDragFilter* drag_filter_ = nullptr;

    QLabel* sym_label_ = nullptr;
    QLabel* price_label_ = nullptr;
    QLabel* change_label_ = nullptr;
    QLabel* vol_label_ = nullptr;
    QLabel* hl_label_ = nullptr;
    QLabel* mktcap_label_ = nullptr;
    QLabel* rec_label_ = nullptr;

    QTabWidget* tab_widget_ = nullptr;
    EquityOverviewTab* overview_tab_ = nullptr;
    EquityFinancialsTab* financials_tab_ = nullptr;
    EquityFilingsTab* filings_tab_ = nullptr;
    EquityAnalysisTab* analysis_tab_ = nullptr;
    EquityTechnicalsTab* technicals_tab_ = nullptr;
    EquityPeersTab* peers_tab_ = nullptr;
    EquityNewsTab* news_tab_ = nullptr;
    EquitySentimentTab* sentiment_tab_ = nullptr;
    EquityValuationTab* valuation_tab_ = nullptr;
    // Fed by the Fincept /data API (FinceptDataService): load only when opened.
    EquityOptionsTab* options_tab_ = nullptr;
    EquityAnalystTab* analyst_tab_ = nullptr;
    EquityOwnershipTab* ownership_tab_ = nullptr;
    // Corporate events (equity_research_data.py events): load only when opened.
    EquityDividendsTab* dividends_tab_ = nullptr;
    EquityEarningsTab* earnings_tab_ = nullptr;
    // Built on first show (materialize_tab): filings, analysis, technicals, sentiment,
    // valuation, dividends, earnings, options, analyst, ownership — their pointers above stay
    // null until then. Building all fourteen tabs up front was most of the 1.3–2.3 s stall of
    // opening the screen.
    struct LazyTab {
        std::function<QWidget*(QWidget*)> make; ///< builds the tab inside the given page
        QWidget* content = nullptr;             ///< the tab, once built
    };
    QHash<QWidget*, LazyTab> lazy_tabs_; ///< page → deferred tab

    QTimer* refresh_timer_ = nullptr;
    QString current_symbol_;
    QString current_currency_;
    double last_price_ = 0.0; // freshest quote price, seeds the order ticket
    // Freshest quote for the current symbol — re-rendered when the instrument's
    // currency becomes known (the quote usually lands before the info does).
    services::equity::QuoteData last_quote_;
    // Last info for the current symbol — MKT CAP / REC are re-rendered from it on a language
    // switch instead of being blanked until the symbol is reloaded.
    services::equity::StockInfo last_info_;
    // Last statements for the current symbol: handed to the Valuation tab when it is built
    // after they arrived (it is built on first show — see LazyTab).
    services::equity::FinancialsData last_financials_;
    bool hub_broker_active_ = false;

    // ── Symbol resolution state ──────────────────────────────────────────────
    quint64 load_seq_ = 0;       ///< bumped by every load_symbol(); a resolve answer must match it
    bool resolve_allowed_ = false; ///< this load may start a lookup (false for a resolved reload)
    QString resolve_tried_for_;    ///< the symbol a lookup was already started for (once per load)
    bool symbol_not_found_ = false; ///< Yahoo has no such symbol: the 30 s quote poll stops
    QString resolved_from_;         ///< the typed text current_symbol_ was resolved from, if any
    /// Session memo of typed text → Yahoo symbol (RELIANCE → RELIANCE.NS), so retyping it, or a
    /// linked panel echoing the bare form back, goes straight to the resolved symbol.
    QHash<QString, QString> resolved_aliases_;
    /// "Did you mean" entries for a symbol that was not found: (symbol, menu text).
    QList<QPair<QString, QString>> not_found_candidates_;

    // Symbol group link — SymbolGroup::None when unlinked.
    SymbolGroup link_group_ = SymbolGroup::None;
};

} // namespace fincept::screens
