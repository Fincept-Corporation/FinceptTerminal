// src/screens/equity_research/EquityDataTab.h
#pragma once
// EquityDataTab — shared frame of the Equity Research tabs fed by the Fincept
// /data API (OPTIONS, ANALYST, OWNERSHIP; see FinceptDataService):
//
//   [tab controls …]                      as of 15:45 · 3 min old · source   [⟳ REFRESH]
//   <message line: failure reason / notice — hidden when there is none>
//   <body: the tab's own content>
//
// Lazy and billing-aware like the other tabs: nothing is requested until the
// screen hands the tab a symbol while it is shown (set_symbol), the same symbol
// is never fetched twice, and REFRESH is the only re-fetch (the service answers
// it from its copy while the server's freshness window lasts).
//
// Billing rule (every /data call is billed): set_symbol() never sends a request by
// itself unless the account's plan includes market data at no credit cost
// (billing::PlanAccess). It shows the remembered answer of this tab and symbol —
// the last one loaded, kept 14 days in CacheManager, with its "as of" — and
// otherwise a LOAD offer (the header button, marked "spend"); only LOAD / REFRESH
// and the tab's own controls send a request.
//
// Every answer carries the token of the request that asked; an answer for a
// symbol / expiration the user has since left is dropped (is_current()). The
// service never runs a callback after this widget is destroyed (it is passed as
// the callback context), and the destructor withdraws pending callbacks.
#include "services/fincept_data/FinceptDataModels.h"
#include "ui/widgets/LoadingOverlay.h"

#include <QColor>
#include <QLabel>
#include <QPushButton>
#include <QStringList>
#include <QTableWidget>
#include <QWidget>

class QHBoxLayout;
class QTimer;
class QVBoxLayout;

namespace fincept::screens {

class EquityDataTab : public QWidget {
    Q_OBJECT
  public:
    ~EquityDataTab() override;

    /// Called by EquityResearchScreen when this tab is shown for `symbol`.
    /// A no-op for the symbol already loaded (or loading).
    void set_symbol(const QString& symbol);

  signals:
    /// A "did you mean" suggestion of a not-found symbol was clicked.
    void research_requested(const QString& symbol);

  protected:
    explicit EquityDataTab(const QString& object_name, QWidget* parent = nullptr);

    // ── Implemented by each tab ───────────────────────────────────────────────
    /// Forget what is shown (a new symbol is coming).
    virtual void clear_view() = 0;
    /// Ask for symbol()'s data. `refresh` = the user pressed REFRESH.
    virtual void request(bool refresh) = 0;
    /// Overlay text while loading ("LOADING OPTIONS…").
    virtual QString loading_text() const = 0;
    /// Text for a 404 on this tab. Default: the server's sentence.
    virtual QString not_found_text(const services::fincept_data::DataError& e) const;
    /// Re-apply translated texts (headers, captions) and re-render cached data.
    virtual void retranslate() {}
    /// Render a remembered answer (an earlier successful load of this tab and symbol,
    /// see the billing rule above) exactly as a fresh one. False: cannot (the tab then
    /// offers LOAD instead).
    virtual bool apply_remembered(const services::fincept_data::DataResult& /*r*/) { return false; }
    /// Part of the remembered-answer key besides the tab and the symbol (e.g. the strike band).
    virtual QString remember_variant() const { return {}; }
    /// This successful answer is the tab's main view, worth keeping for the next visit
    /// (not, e.g., a second page). Default: every answer.
    virtual bool rememberable(const services::fincept_data::DataResult& /*r*/) const { return true; }

    // ── For the tabs ──────────────────────────────────────────────────────────
    const QString& symbol() const { return symbol_; }
    QHBoxLayout* controls_layout() const { return controls_; }
    QVBoxLayout* body_layout() const { return body_layout_; }
    QWidget* body_widget() const { return body_; }

    /// Before asking: shows the loading overlay; returns the token the answer must match.
    quint64 start_request();
    /// The answer for `token` is still wanted.
    bool is_current(quint64 token) const { return token == token_; }
    /// After an answer: freshness line on success; on failure the reason (the data
    /// already on screen stays — a failed refresh does not blank the tab).
    void finish(const services::fincept_data::DataResult& r);
    /// True once data for symbol() is on screen.
    bool has_data() const { return has_data_; }

    // ── Shared widgets / formatting ───────────────────────────────────────────
    QLabel* make_caption(const QString& text, QWidget* parent) const;
    QLabel* make_section(const QString& text, QWidget* parent) const;
    QLabel* make_note(QWidget* parent) const;
    QPushButton* make_button(const QString& text, QWidget* parent) const;
    static QTableWidget* make_table(QWidget* parent);
    /// Sets the headers and row count, sorting off while filling.
    static void reset_table(QTableWidget* t, const QStringList& headers, int rows);
    /// Height that shows every row (tables inside the scroll area have no scroll bar of their own).
    static void fit_table_height(QTableWidget* t);
    static QTableWidgetItem* text_item(const QString& text, const QColor& fg = {});
    /// Right-aligned, sorts by `value` (NaN sorts first).
    static QTableWidgetItem* number_item(double value, const QString& text, const QColor& fg = {});
    /// yyyy-MM-dd, sorts chronologically ("—" for an invalid date).
    static QTableWidgetItem* date_item(const QDate& date);
    static QString num(double v, int decimals = 2);
    static QString count(double v);              ///< 1,234,567 or —
    static QString big(double v);                ///< 1.23T / 456.7B or —
    static QString percent(double pct, int decimals = 2, bool signed_ = false); ///< already in percent
    static QColor sign_color(double v);

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void changeEvent(QEvent* event) override;

  private:
    void build_frame();
    void retranslate_frame();
    void update_freshness();
    void on_availability_changed();
    void set_message(const QString& html, const QString& tone);
    /// set_symbol() / availability: the remembered answer, else the LOAD offer — a
    /// request only when the plan includes market data at no credit cost.
    void show_remembered_or_offer();
    void show_load_offer();
    QString remembered_key() const;
    void remember(const services::fincept_data::DataResult& r) const;
    bool recall(services::fincept_data::DataResult* out) const;

    QString symbol_;
    quint64 token_ = 0;
    bool has_data_ = false;
    bool offering_ = false;           ///< the LOAD offer is up (nothing loaded for this symbol)
    bool showing_remembered_ = false; ///< the data on screen is a remembered answer
    bool replaying_ = false;          ///< inside show_remembered_or_offer(): finish() must not re-remember
    bool unavailable_shown_ = false; ///< the "needs a newer server" notice is up
    bool service_used_ = false;      ///< FinceptDataService was touched (else never constructed)
    services::fincept_data::DataResult last_ok_;

    QWidget* header_ = nullptr; ///< the controls / freshness / refresh row
    QHBoxLayout* controls_ = nullptr;
    QLabel* fresh_label_ = nullptr;
    QPushButton* refresh_btn_ = nullptr;
    QLabel* message_ = nullptr;
    QWidget* body_ = nullptr;
    QVBoxLayout* body_layout_ = nullptr;
    QTimer* age_timer_ = nullptr;
    ui::LoadingOverlay* overlay_ = nullptr;
};

} // namespace fincept::screens
