#pragma once

#include "services/equity/EquityResearchModels.h"
#include "services/equity/EquityValuationModels.h"

#include <QDoubleSpinBox>
#include <QHash>
#include <QLabel>
#include <QPushButton>
#include <QStringList>
#include <QWidget>

class QCheckBox;
class QFrame;
class QTableWidget;
class QVBoxLayout;

namespace fincept::screens {

/// Equity Research → Valuation. A levered-FCF DCF with editable, labelled assumptions,
/// Altman Z / Piotroski F / Beneish M computed from the annual statements, and a few
/// cross-checking multiples. Everything is computed locally and synchronously
/// (services/equity/EquityValuationModels) — no network round-trip, no tier gate.
class EquityValuationTab : public QWidget {
    Q_OBJECT
  public:
    explicit EquityValuationTab(QWidget* parent = nullptr);

    /// Called by EquityResearchScreen when this tab is shown for `symbol`. Requests the
    /// annual statements when none are held for it.
    void set_symbol(const QString& symbol);

    /// Fed by EquityResearchScreen whenever the info / statements of the current symbol land.
    void set_stock_info(const services::equity::StockInfo& info);
    void set_financials(const services::equity::FinancialsData& data);

  protected:
    void changeEvent(QEvent* e) override;
    /// Subscribes to the live US 10-year Treasury yield while visible (CLAUDE.md P3 / D3).
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

  private slots:
    void on_calculate_clicked();

  private:
    // ── UI builders ──────────────────────────────────────────────
    void build_ui();
    QWidget* build_dcf_section();
    QWidget* build_scoring_section();
    QWidget* build_multiples_section();
    QFrame* make_box(const char* title_key, QVBoxLayout*& body);
    QLabel* add_key_label(QWidget* parent, const char* key);

    // ── Data → UI ────────────────────────────────────────────────
    void pre_fill_dcf_inputs();
    void clear_results();
    void run_scoring_models();
    void run_multiples();
    void update_statements_note();
    /// Statements and the quote are in different currencies (ADRs): per-share outputs
    /// are not comparable with the price.
    bool currency_mismatch() const;
    QString statements_currency() const;

    void set_badge(QLabel* badge, const QString& text, const char* tone);
    QString fmt_money(double value, const QString& currency) const;
    /// Per-share value over a discount-rate × terminal-growth grid around the inputs;
    /// cells are coloured against `price` when it is comparable (> 0).
    void update_sensitivity(const services::equity::valuation::DcfInputs& base, double price);

    // ── Risk-free rate → CAPM discount ───────────────────────────
    /// Keeps a plausible US 10Y print (percent); false for a bad tick.
    bool record_treasury_yield(double yield_pct, qint64 fetched_ms);
    /// A hub delivery: replaces the fallback, or a market value older than the refresh
    /// window, unless the analyst typed their own rate.
    void on_treasury_yield(double yield_pct, qint64 fetched_ms);
    void set_rf_value(double pct);
    /// max(floor, risk-free + β × premium) in percent as the spin box rounds it; NaN when
    /// the current symbol reports no beta.
    double capm_discount_pct() const;
    bool discount_matches_capm() const;
    void apply_capm_discount();
    void update_assumptions_note();
    QString rf_tooltip() const;

    // ── DCF inputs ───────────────────────────────────────────────
    QDoubleSpinBox* fcf_input_ = nullptr;      // base-year free cash flow (millions)
    QDoubleSpinBox* growth_input_ = nullptr;   // projection growth %
    QDoubleSpinBox* terminal_input_ = nullptr; // terminal growth %
    QDoubleSpinBox* rf_input_ = nullptr;       // risk-free rate % (10-year government yield)
    QDoubleSpinBox* discount_input_ = nullptr; // discount rate %
    QDoubleSpinBox* years_input_ = nullptr;    // projection years
    QDoubleSpinBox* shares_input_ = nullptr;   // shares outstanding (millions)
    QCheckBox* fade_check_ = nullptr;          // fade growth to the terminal rate
    QPushButton* calc_btn_ = nullptr;
    QLabel* assumptions_lbl_ = nullptr;

    // ── DCF results ──────────────────────────────────────────────
    QLabel* intrinsic_val_ = nullptr;
    QLabel* current_price_val_ = nullptr;
    QLabel* margin_val_ = nullptr;
    QLabel* terminal_share_val_ = nullptr;
    QLabel* implied_growth_val_ = nullptr; // reverse DCF: growth the price assumes
    QLabel* verdict_badge_ = nullptr;
    QLabel* dcf_note_ = nullptr;
    QTableWidget* sens_table_ = nullptr; // discount × terminal growth → value per share
    QLabel* sens_title_ = nullptr;

    // ── Scoring ──────────────────────────────────────────────────
    QLabel* altman_val_ = nullptr;
    QLabel* altman_badge_ = nullptr;
    QLabel* altman2_val_ = nullptr; // Z'' for non-manufacturers
    QLabel* altman2_badge_ = nullptr;
    QLabel* piotroski_val_ = nullptr;
    QLabel* piotroski_badge_ = nullptr;
    QLabel* beneish_val_ = nullptr;
    QLabel* beneish_badge_ = nullptr;
    QLabel* scoring_note_ = nullptr;

    // ── Multiples ────────────────────────────────────────────────
    QLabel* ev_ebitda_val_ = nullptr;
    QLabel* ev_revenue_val_ = nullptr;
    QLabel* price_sales_val_ = nullptr;
    QLabel* fcf_yield_val_ = nullptr;
    QLabel* graham_val_ = nullptr;
    QLabel* multiples_note_ = nullptr;

    // ── State ────────────────────────────────────────────────────
    QString current_symbol_;
    QString prefilled_for_; ///< symbol whose DCF inputs were pre-filled (never overwrite edits)
    services::equity::StockInfo last_info_;
    services::equity::FinancialsData last_financials_;
    bool financials_loaded_ = false;
    bool financials_failed_ = false;
    QStringList prefill_notes_; ///< FCF / growth pre-fill notes; the discount note is rebuilt

    // ── Risk-free state ──────────────────────────────────────────
    enum class RfSource { Fallback, Market, User };
    RfSource rf_source_ = RfSource::Fallback; ///< where the RISK-FREE input's value came from
    double rf_shown_pct_ = 0.0;               ///< last value set by code (tells a real edit apart)
    qint64 rf_fetched_ms_ = 0;                ///< fetch time of the market value in the input
    double live_rf_pct_ = 0.0;                ///< newest US 10Y yield seen (%), 0 = none yet
    qint64 live_rf_ms_ = 0;
    bool hub_active_ = false;

    QHash<QLabel*, const char*> i18n_labels_; ///< static label → translatable source key
};

} // namespace fincept::screens
