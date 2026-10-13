#pragma once

#include "services/equity/EquityResearchModels.h"

#include <QDoubleSpinBox>
#include <QHash>
#include <QLabel>
#include <QPushButton>
#include <QWidget>

class QFrame;
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

    // ── DCF inputs ───────────────────────────────────────────────
    QDoubleSpinBox* fcf_input_ = nullptr;      // base-year free cash flow (millions)
    QDoubleSpinBox* growth_input_ = nullptr;   // projection growth %
    QDoubleSpinBox* terminal_input_ = nullptr; // terminal growth %
    QDoubleSpinBox* discount_input_ = nullptr; // discount rate %
    QDoubleSpinBox* years_input_ = nullptr;    // projection years
    QDoubleSpinBox* shares_input_ = nullptr;   // shares outstanding (millions)
    QPushButton* calc_btn_ = nullptr;
    QLabel* assumptions_lbl_ = nullptr;

    // ── DCF results ──────────────────────────────────────────────
    QLabel* intrinsic_val_ = nullptr;
    QLabel* current_price_val_ = nullptr;
    QLabel* margin_val_ = nullptr;
    QLabel* terminal_share_val_ = nullptr;
    QLabel* verdict_badge_ = nullptr;
    QLabel* dcf_note_ = nullptr;

    // ── Scoring ──────────────────────────────────────────────────
    QLabel* altman_val_ = nullptr;
    QLabel* altman_badge_ = nullptr;
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

    QHash<QLabel*, const char*> i18n_labels_; ///< static label → translatable source key
};

} // namespace fincept::screens
