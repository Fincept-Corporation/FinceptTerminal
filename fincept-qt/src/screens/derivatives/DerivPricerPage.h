#pragma once
// DerivPricerPage — the PRICER page of the Derivatives tab: the five calculators
// (bonds, equity options, FX options, IR swaps, CDS) that used to be the whole
// screen, plus the D2 upgrade of the equity-option card: model and exercise
// (Black-Scholes-Merton, CRR binomial, Barone-Adesi-Whaley, Black-76, Bachelier),
// payoffs (vanilla, digitals, barriers, Asians, Kirk spread), continuous or
// discrete dividends, Greeks to third order and dollar Greeks for a position,
// "load from market" (spot, dividend yield, Treasury rate, chain IV) through the
// shared symbol box, and an optional QuantLib cross-check that never runs by itself.
//
// Every calculation is local C++ (microseconds to a few milliseconds for a
// 500-step tree) and runs through one ui::AutoLoadPane (a view per calculator), so
// the page opens priced. State keys are the screen's long-standing ones
// ("instrument", "inputs"), extended with new input keys that older state ignores.

#include "screens/derivatives/DerivCommon.h"
#include "screens/derivatives/DerivContext.h"
#include "services/options/OptionModels.h"

#include <QJsonObject>
#include <QList>
#include <QPair>
#include <QPointer>
#include <QVariantMap>
#include <QWidget>

class QBoxLayout;
class QComboBox;
class QDateEdit;
class QDoubleSpinBox;
class QGridLayout;
class QLabel;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class QTableWidget;
class QVBoxLayout;

namespace fincept::core::instrument {
struct Instrument;
}
namespace fincept::ui {
class AutoLoadPane;
class SymbolSearchBox;
} // namespace fincept::ui

namespace fincept::screens::deriv {

class DerivPricerPage : public QWidget {
    Q_OBJECT
  public:
    explicit DerivPricerPage(DerivContext* ctx, QWidget* parent = nullptr);
    ~DerivPricerPage() override;

    /// {"instrument": index, "inputs": {key: value}, "chart_view": index}.
    QVariantMap save_state() const;
    void restore_state(const QVariantMap& state);

    /// Fill the equity-option card from a chain contract and price it.
    void load_contract(const ContractRef& contract);

    int active_instrument() const { return active_; }
    /// Open calculator `index` (0 bonds … 4 CDS) — what its instrument button does (deep links).
    void select_instrument(int index) { on_instrument_changed(index); }
    /// Re-price the open calculator (F5).
    void refresh() { on_calculate(); }
    QString instrument_name(int index) const;

  signals:
    /// Inputs or the instrument changed (the screen persists its state).
    void state_changed();
    void instrument_changed(const QString& name);

  protected:
    void changeEvent(QEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;

  private:
    // ── Build (DerivPricerPage.cpp) ──
    // Layout: a compact input column (each calculator's cards, fields sized from the input
    // font by form_) beside the results on a wide page, above them on a narrow one.
    void build();
    QWidget* build_instrument_bar();
    QWidget* build_bonds(QWidget* parent);
    QWidget* build_equity(QWidget* parent);
    QWidget* build_fx(QWidget* parent);
    QWidget* build_swaps(QWidget* parent);
    QWidget* build_credit(QWidget* parent);
    QWidget* build_results(QWidget* parent);
    QGridLayout* grid_in(QVBoxLayout* body, QWidget** host);
    QLabel* field(QGridLayout* grid, int row, int col, const char* src, QWidget* input, int span = 1);
    QPushButton* calc_button(QWidget* parent, const char* src);
    void bind_enter(QWidget* card, QPushButton* button);
    /// Lays out one calculator's page (`page` is already the cards' parent): its cards in a row
    /// that apply_responsive() turns. Returns `page`.
    QWidget* card_page(QWidget* page, const QList<QWidget*>& cards);
    /// Inputs beside or above the results, cards stacked or side by side — from the page width
    /// and the font (resizeEvent, ThemeManager::appearance_settled).
    void apply_responsive();
    /// Only the calculator in view sizes the input stack (no blank band under a short one).
    void fit_stack();
    void track(const QString& key, QWidget* input);
    void retranslate();
    void on_instrument_changed(int index);
    void on_calculate();
    void on_calculate_secondary();
    void update_equity_rows();
    /// The filled dividend rows. A row that cannot be read (days not a positive number, an amount that
    /// is not one, or one in another currency than the underlying's) is left out and, when `error` is
    /// given, the first such row is described there.
    std::vector<services::options::models::CashDividend> dividends(QString* error = nullptr) const;
    QString dividends_text() const;
    void set_dividends_text(const QString& text);

    // ── Load from market (D2) ──
    void on_symbol_picked(const fincept::core::instrument::Instrument& inst);
    void apply_rate_for_tenor();
    void apply_chain_iv(const DerivContext::ChainPtr& chain);
    void set_market_status(const QString& text, bool warn = false);

    // ── Compute + render (DerivPricerCompute.cpp) ──
    QJsonObject compute_view(const QString& view, QString* error) const;
    QJsonObject compute_equity(QString* error) const;
    QJsonObject compute_equity_iv(QString* error) const;
    void render_payload(const QJsonValue& payload);
    void display_results(const QJsonObject& result, const QJsonObject& chart);
    void clear_results();
    QWidget* make_equity_chart(QWidget* parent, int which) const;
    void run_quantlib_cross_check(QPushButton* button, QLabel* out);
    /// The QuantLib route that prices what the equity card shows, its request body and the
    /// factor between its answer and the local price (digitals pay 1 there); false when none.
    bool quantlib_route(QString* endpoint, QJsonObject* body, double* scale, QString* caveat) const;
    ui::ChartExport::Table result_table() const;
    report::ReportPayload report_payload() const;

    DerivContext* ctx_ = nullptr;
    TrBook book_;
    int active_ = 0; // 0 bonds, 1 equity, 2 fx, 3 swaps, 4 credit
    QList<QPair<QString, QWidget*>> persisted_;

    // Layout
    FormFit form_{15};
    QBoxLayout* split_ = nullptr; ///< inputs | results (or inputs over results)
    QWidget* inputs_col_ = nullptr;
    QWidget* results_frame_ = nullptr;
    struct CardRow {
        QBoxLayout* layout = nullptr;
        QList<QWidget*> cards;
    };
    QList<CardRow> card_rows_;
    int wide_ = -1; ///< -1 not decided yet, 0 inputs above the results, 1 beside them

    QStackedWidget* stack_ = nullptr;
    QList<QPushButton*> instr_btns_;
    ui::AutoLoadPane* pane_ = nullptr;
    QWidget* results_body_ = nullptr;
    QLabel* results_title_ = nullptr;
    QLabel* results_meta_ = nullptr;
    DerivResultBar* result_bar_ = nullptr;

    // What is on screen, for COPY / CSV / PNG / ADD TO REPORT.
    QString last_title_;
    QString last_note_;
    QVector<QPair<QString, QString>> last_rows_;
    QPointer<QWidget> last_chart_;
    QJsonObject last_result_;
    QJsonObject last_charts_;
    QPointer<QWidget> chart_slot_; ///< equity: holds the chart picked in chart_pick_
    QPointer<QComboBox> chart_pick_;
    int chart_view_ = 0;

    // Bonds
    QDateEdit* bond_issue_ = nullptr;
    QDateEdit* bond_settle_ = nullptr;
    QDateEdit* bond_maturity_ = nullptr;
    QDoubleSpinBox* bond_coupon_ = nullptr;
    QDoubleSpinBox* bond_ytm_ = nullptr;
    QComboBox* bond_freq_ = nullptr;
    QDoubleSpinBox* bond_clean_price_ = nullptr;
    QDateEdit* ytm_issue_ = nullptr;
    QDateEdit* ytm_settle_ = nullptr;
    QDateEdit* ytm_maturity_ = nullptr;
    QDoubleSpinBox* ytm_coupon_ = nullptr;
    QComboBox* ytm_freq_ = nullptr;

    // Equity option (pricing card)
    ui::SymbolSearchBox* sym_box_ = nullptr;
    QLabel* market_status_ = nullptr;
    QString market_symbol_;
    QString market_ccy_ = QStringLiteral("USD");
    quint64 chain_request_ = 0;
    QDoubleSpinBox* opt_spot_ = nullptr;
    QDoubleSpinBox* opt_strike_ = nullptr;
    QDoubleSpinBox* opt_time_ = nullptr;
    QDoubleSpinBox* opt_vol_ = nullptr;
    QDoubleSpinBox* opt_rate_ = nullptr;
    QDoubleSpinBox* opt_div_ = nullptr;
    QComboBox* opt_type_ = nullptr;
    QComboBox* opt_exercise_ = nullptr;
    QComboBox* opt_model_ = nullptr;
    QComboBox* opt_payoff_ = nullptr;
    QComboBox* opt_div_mode_ = nullptr;
    QSpinBox* opt_steps_ = nullptr;
    QDoubleSpinBox* opt_normal_vol_ = nullptr;
    QDoubleSpinBox* opt_cash_ = nullptr;
    QComboBox* opt_barrier_kind_ = nullptr;
    QDoubleSpinBox* opt_barrier_ = nullptr;
    QDoubleSpinBox* opt_rebate_ = nullptr;
    QDoubleSpinBox* opt_f2_ = nullptr;
    QDoubleSpinBox* opt_vol2_ = nullptr;
    QDoubleSpinBox* opt_rho_ = nullptr;
    QDoubleSpinBox* opt_qty_ = nullptr;
    QDoubleSpinBox* opt_mult_ = nullptr;
    QDoubleSpinBox* opt_avg_so_far_ = nullptr;  ///< Asian: average observed so far (0 = averaging not started)
    QDoubleSpinBox* opt_avg_elapsed_ = nullptr; ///< Asian: years already averaged
    QWidget* grp_asian_ = nullptr;
    quint64 snapshot_request_ = 0;
    QTableWidget* opt_divs_ = nullptr;
    QLabel* opt_spot_label_ = nullptr;
    QLabel* opt_strike_label_ = nullptr;
    QWidget* grp_steps_ = nullptr;
    QWidget* grp_normal_ = nullptr;
    QWidget* grp_cash_ = nullptr;
    QWidget* grp_barrier_ = nullptr;
    QWidget* grp_spread_ = nullptr;
    QWidget* grp_divs_ = nullptr;
    QWidget* grp_yield_ = nullptr;
    QLabel* model_note_ = nullptr;

    // Implied volatility card
    QDoubleSpinBox* iv_price_ = nullptr;
    QDoubleSpinBox* iv_spot_ = nullptr;
    QDoubleSpinBox* iv_strike_ = nullptr;
    QDoubleSpinBox* iv_time_ = nullptr;
    QDoubleSpinBox* iv_rate_ = nullptr;
    QDoubleSpinBox* iv_div_ = nullptr;
    QComboBox* iv_type_ = nullptr;
    QComboBox* iv_model_ = nullptr;

    // FX
    QDoubleSpinBox* fx_spot_ = nullptr;
    QDoubleSpinBox* fx_strike_ = nullptr;
    QDoubleSpinBox* fx_time_ = nullptr;
    QDoubleSpinBox* fx_vol_ = nullptr;
    QDoubleSpinBox* fx_dom_ = nullptr;
    QDoubleSpinBox* fx_for_ = nullptr;
    QComboBox* fx_type_ = nullptr;
    QDoubleSpinBox* fx_notional_ = nullptr;
    QComboBox* fx_ccy_ = nullptr; ///< domestic (premium) currency

    // Swap
    QDateEdit* swap_eff_ = nullptr;
    QDateEdit* swap_mat_ = nullptr;
    QDoubleSpinBox* swap_fixed_ = nullptr;
    QComboBox* swap_freq_ = nullptr;
    QDoubleSpinBox* swap_disc_ = nullptr;
    QDoubleSpinBox* swap_notional_ = nullptr;
    QComboBox* swap_ccy_ = nullptr;

    // CDS
    QDateEdit* cds_val_ = nullptr;
    QDateEdit* cds_mat_ = nullptr;
    QDoubleSpinBox* cds_recovery_ = nullptr;
    QDoubleSpinBox* cds_spread_ = nullptr;
    QDoubleSpinBox* cds_notional_ = nullptr;
    QDoubleSpinBox* cds_coupon_ = nullptr;
    QDoubleSpinBox* cds_rate_ = nullptr;
    QComboBox* cds_ccy_ = nullptr;
};

} // namespace fincept::screens::deriv
