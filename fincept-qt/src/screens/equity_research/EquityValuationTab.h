#pragma once
// src/screens/equity_research/EquityValuationTab.h
//
// Equity Research → Valuation. Intrinsic value four ways — FCFE at the cost of equity,
// FCFF at the WACC with a net-debt bridge, a dividend discount model and residual income
// (the last two for banks and insurers) — with bull / base / bear scenarios, the
// projection as a table and a chart, a configurable sensitivity grid, multiples against
// the company's own history (bands) and its peers, Altman / Piotroski / Beneish scores,
// and the cross-check multiples.
//
// Currency: statement-derived values are in the REPORTING currency; the price is in the
// QUOTE currency, often a minor unit (London pence). Every comparison converts the price
// to the reporting currency first (pence → pounds, then FX when the currencies differ),
// and both are shown.
//
// Split across:
//   EquityValuationTab.cpp      — layout, inputs, risk-free / CAPM / WACC, scores, multiples
//   EquityValuationTab_Dcf.cpp  — model run, results, scenarios, projection, sensitivity,
//                                 own-history and peer multiples
#include "services/equity/EquityResearchModels.h"
#include "services/equity/EquityValuationModels.h"

#include <QDoubleSpinBox>
#include <QHash>
#include <QLabel>
#include <QPushButton>
#include <QStringList>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QFrame;
class QTableWidget;
class QVBoxLayout;

namespace fincept::ui {
class BarChart;
class TimeSeriesChart;
} // namespace fincept::ui

namespace fincept::screens {

namespace er_ui {
class Segmented;
class StatusStrip;
} // namespace er_ui

class EquityValuationTab : public QWidget {
    Q_OBJECT
  public:
    explicit EquityValuationTab(QWidget* parent = nullptr);

    /// Called by EquityResearchScreen when this tab is shown for `symbol`.
    void set_symbol(const QString& symbol);

    /// Fed by EquityResearchScreen whenever the info / statements of the current symbol land.
    void set_stock_info(const services::equity::StockInfo& info);
    void set_financials(const services::equity::FinancialsData& data);

  protected:
    void changeEvent(QEvent* e) override;
    /// Subscribes to the live US 10-year Treasury yield while visible.
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

  private slots:
    void on_calculate_clicked();

  private:
    enum class Method { Fcfe, Fcff, Ddm, ResidualIncome };

    // ── EquityValuationTab.cpp: UI ───────────────────────────────────────────
    void build_ui();
    QWidget* build_dcf_section();
    QWidget* build_history_section();
    QWidget* build_scoring_section();
    QWidget* build_multiples_section();
    QFrame* make_box(const char* title_key, QVBoxLayout*& body);
    QLabel* add_key_label(QWidget* parent, const char* key);
    QDoubleSpinBox* make_spin(double min, double max, double value, const QString& suffix, int decimals);

    // ── EquityValuationTab.cpp: inputs ───────────────────────────────────────
    void pre_fill_inputs();
    void apply_base_source();
    void apply_method_visibility();
    void clear_results();
    void run_scoring_models();
    void run_multiples();
    void update_statements_note();
    QString reporting_currency() const;
    QString quote_currency() const;
    /// The quote converted to the reporting currency (pence → pounds → FX); NaN without a rate.
    double price_in_reporting() const;
    /// Market cap in the reporting currency; NaN without a rate.
    double market_cap_in_reporting() const;
    void set_badge(QLabel* badge, const QString& text, const char* tone);
    QString fmt_rep(double value) const;   ///< per-share value, reporting currency
    QString fmt_quote(double value) const; ///< per-share value, quote currency as quoted
    QString fmt_amount(double value) const; ///< compact amount, reporting currency

    // Risk-free / CAPM / WACC
    bool record_treasury_yield(double yield_pct, qint64 fetched_ms);
    void on_treasury_yield(double yield_pct, qint64 fetched_ms);
    void request_local_risk_free();
    void set_rf_value(double pct);
    double cost_of_equity_pct() const;
    double wacc_pct() const;
    void apply_discount();
    void update_assumptions_note();

    // ── EquityValuationTab_Dcf.cpp ───────────────────────────────────────────
    services::equity::valuation::DcfInputs dcf_inputs() const;
    /// Per-share value of the current method for a set of inputs (reporting currency).
    double value_for(Method m, double growth, double discount, double terminal) const;
    void render_results();
    void render_projection(const services::equity::valuation::DcfResult& res);
    void render_scenarios();
    void update_sensitivity();
    void render_history();
    /// A fresh, configured multiples chart in place of the current one: a TimeSeriesChart
    /// cannot drop panes, so clear() kept every P/B pane of every earlier render.
    void reset_hist_chart();
    void request_peer_multiples();

    // ── DCF inputs ───────────────────────────────────────────────────────────
    er_ui::Segmented* method_seg_ = nullptr;
    QComboBox* base_source_ = nullptr;
    QDoubleSpinBox* fcf_input_ = nullptr;      // base cash flow (millions, reporting currency)
    QDoubleSpinBox* growth_input_ = nullptr;   // stage-1 growth %
    QDoubleSpinBox* terminal_input_ = nullptr; // terminal growth %
    QDoubleSpinBox* years_input_ = nullptr;
    QDoubleSpinBox* shares_input_ = nullptr;   // millions
    QCheckBox* fade_check_ = nullptr;
    QDoubleSpinBox* rf_input_ = nullptr;       // risk-free %
    QDoubleSpinBox* erp_input_ = nullptr;      // equity risk premium %
    QDoubleSpinBox* crp_input_ = nullptr;      // country risk premium %
    QDoubleSpinBox* beta_input_ = nullptr;
    QDoubleSpinBox* kd_input_ = nullptr;       // pre-tax cost of debt %
    QDoubleSpinBox* tax_input_ = nullptr;      // tax rate %
    QDoubleSpinBox* discount_input_ = nullptr; // the rate used (cost of equity or WACC) %
    QPushButton* capm_btn_ = nullptr;
    QPushButton* calc_btn_ = nullptr;
    QLabel* assumptions_lbl_ = nullptr;
    QLabel* rf_source_lbl_ = nullptr;
    QLabel* discount_key_ = nullptr;
    QLabel* base_key_ = nullptr;
    QList<QWidget*> fcff_only_;  ///< cost of debt / tax rows (FCFF)
    QList<QWidget*> dcf_only_;   ///< base / fade rows (not DDM / residual income)

    // ── Results ──────────────────────────────────────────────────────────────
    QLabel* intrinsic_val_ = nullptr;
    QLabel* intrinsic_quote_val_ = nullptr;
    QLabel* current_price_val_ = nullptr;
    QLabel* margin_val_ = nullptr;
    QLabel* bridge_val_ = nullptr;
    QLabel* terminal_share_val_ = nullptr;
    QLabel* implied_growth_val_ = nullptr;
    QLabel* verdict_badge_ = nullptr;
    QLabel* dcf_note_ = nullptr;
    QLabel* scenario_lbl_[3] = {nullptr, nullptr, nullptr};
    QTableWidget* proj_table_ = nullptr;
    ui::BarChart* proj_chart_ = nullptr;
    QTableWidget* sens_table_ = nullptr;
    QLabel* sens_title_ = nullptr;
    QComboBox* sens_cols_ = nullptr;  ///< columns: terminal growth / stage-1 growth
    QDoubleSpinBox* sens_step_rows_ = nullptr;
    QDoubleSpinBox* sens_step_cols_ = nullptr;
    QComboBox* sens_size_ = nullptr;

    // ── Own history / peers ──────────────────────────────────────────────────
    ui::TimeSeriesChart* hist_chart_ = nullptr;
    QTableWidget* hist_table_ = nullptr;
    QLabel* hist_note_ = nullptr;
    QPushButton* peers_btn_ = nullptr;

    // ── Scoring ──────────────────────────────────────────────────────────────
    QLabel* altman_val_ = nullptr;
    QLabel* altman_badge_ = nullptr;
    QLabel* altman2_val_ = nullptr;
    QLabel* altman2_badge_ = nullptr;
    QLabel* piotroski_val_ = nullptr;
    QLabel* piotroski_badge_ = nullptr;
    QLabel* beneish_val_ = nullptr;
    QLabel* beneish_badge_ = nullptr;
    QLabel* scoring_note_ = nullptr;
    QTableWidget* score_history_ = nullptr;  ///< Altman Z″ / Piotroski F / Beneish M per fiscal year
    QTableWidget* piotroski_grid_ = nullptr; ///< the nine Piotroski tests, pass / fail per year

    // ── Multiples ────────────────────────────────────────────────────────────
    QLabel* ev_ebitda_val_ = nullptr;
    QLabel* ev_revenue_val_ = nullptr;
    QLabel* price_sales_val_ = nullptr;
    QLabel* fcf_yield_val_ = nullptr;
    QLabel* graham_val_ = nullptr;
    QLabel* multiples_note_ = nullptr;

    er_ui::StatusStrip* status_ = nullptr;

    // ── State ────────────────────────────────────────────────────────────────
    QString current_symbol_;
    QString prefilled_for_;
    services::equity::StockInfo last_info_;
    services::equity::FinancialsData last_financials_;
    QVector<services::equity::Candle> daily_;
    QVector<services::equity::PeerData> peers_;
    /// Peer medians are on their way (fetched by default for every symbol: the
    /// PEER MEDIAN column is never left blank behind a button).
    bool peers_pending_ = false;
    /// set_symbol() ran while the tab was hidden: the peer fetch starts on the next show.
    bool peers_auto_on_show_ = false;
    bool financials_loaded_ = false;
    bool financials_failed_ = false;
    Method method_ = Method::Fcfe;
    QStringList prefill_notes_;
    double base_ttm_ = 0, base_fy_ = 0, base_avg3_ = 0, base_avg5_ = 0; ///< FCFE bases (absolute)
    double fcff_ttm_ = 0, fcff_fy_ = 0, fcff_avg3_ = 0, fcff_avg5_ = 0; ///< FCFF bases
    double net_debt_ = 0, minority_ = 0;
    double dps_ = 0, bvps_ = 0, roe_ = 0, payout_ = 0;

    enum class RfSource { Fallback, Market, Local, Reference, User };
    RfSource rf_source_ = RfSource::Fallback;
    QString rf_label_;
    double rf_shown_pct_ = 0.0;
    qint64 rf_fetched_ms_ = 0;
    double live_rf_pct_ = 0.0; ///< newest US 10Y yield seen (%), 0 = none yet
    qint64 live_rf_ms_ = 0;
    bool hub_active_ = false;
    bool discount_overridden_ = false;

    QHash<QLabel*, const char*> i18n_labels_;
};

} // namespace fincept::screens
