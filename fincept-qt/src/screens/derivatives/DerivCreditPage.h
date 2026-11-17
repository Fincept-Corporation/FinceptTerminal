#pragma once
// DerivCreditPage — CREDIT: single-name credit default swaps on a spread term structure
// the user TYPES (no free single-name CDS quote feed exists; sovereign / index levels are
// not prefilled either — the terminal has no free keyless source for them):
//   * a piecewise-constant hazard curve bootstrapped from the par spreads (tenor by tenor,
//     every quote repriced to par), survival probability and hazard chart, a table of
//     hazard / survival / risky annuity / repriced spread / CS01 bucket per tenor;
//   * a position (buy or sell protection, notional, maturity, running coupon with the
//     100 / 500 bp standard buttons): PV, upfront, risky annuity, CS01, recovery and
//     interest-rate sensitivities, jump-to-default;
//   * an upfront <-> running-spread converter (quoted flat-spread convention).
// Maths: services/options/CreditMath (ISDA-style standard model, simplified; documented
// there). Discounting: the U.S. Treasury curve from CurveDataService (proxy for the risk-
// free curve) or a flat rate. Every fit runs on a worker thread with a request id and a
// cancel flag; edits are debounced; the page opens computed from the remembered or the
// sample inputs.

#include "screens/derivatives/DerivCommon.h"

#include <QJsonObject>
#include <QPair>
#include <QVector>
#include <QWidget>

#include <atomic>
#include <memory>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QStandardItemModel;
class QTableView;
class QTimer;

namespace fincept::ui {
class AutoLoadPane;
class InputMemory;
class KpiStrip;
class TimeSeriesChart;
} // namespace fincept::ui

namespace fincept::screens::deriv {

class DerivCreditPage : public QWidget {
    Q_OBJECT
  public:
    explicit DerivCreditPage(QWidget* parent = nullptr);
    ~DerivCreditPage() override;

    /// Recompute now (RUN / F5).
    void refresh();

  protected:
    void changeEvent(QEvent* e) override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

  private:
    void build();
    QWidget* build_inputs(QWidget* parent);
    QWidget* build_results(QWidget* parent);
    void retranslate();
    void schedule();
    void set_quote_rows(const QVector<QPair<QString, QString>>& rows);
    void save_quotes();
    void restore_quotes();
    void update_discount_label();
    void render(const QJsonObject& payload);
    QString currency() const;
    ui::ChartExport::Table export_table() const;
    QPixmap export_image() const;
    report::ReportPayload report_payload() const;

    TrBook book_;
    ui::InputMemory* memory_ = nullptr;
    std::shared_ptr<std::atomic<bool>> cancel_;
    QTimer* debounce_ = nullptr;
    QJsonObject last_;
    bool dirty_ = false;       ///< inputs changed since the result on screen was computed
    bool filling_ = false;     ///< the quote table is being filled by code
    bool curve_requested_ = false;
    quint64 curve_req_ = 0;

    // Inputs
    QTableView* quotes_view_ = nullptr;
    QStandardItemModel* quotes_ = nullptr;
    QLineEdit* quotes_memory_ = nullptr; ///< hidden: the quote rows as JSON, through InputMemory
    QDoubleSpinBox* recovery_ = nullptr;
    QComboBox* freq_ = nullptr;
    QComboBox* daycount_ = nullptr;
    QComboBox* discount_ = nullptr;
    QDoubleSpinBox* flat_rate_ = nullptr;
    QLabel* discount_label_ = nullptr;
    QComboBox* side_ = nullptr;
    QDoubleSpinBox* notional_ = nullptr;
    QComboBox* ccy_ = nullptr;
    QDoubleSpinBox* maturity_ = nullptr;
    QDoubleSpinBox* coupon_ = nullptr;
    QDoubleSpinBox* conv_spread_ = nullptr;
    QDoubleSpinBox* conv_upfront_ = nullptr;
    QLabel* conv_spread_out_ = nullptr;
    QLabel* conv_upfront_out_ = nullptr;

    // Results
    QLabel* status_ = nullptr;
    DerivResultBar* result_bar_ = nullptr;
    ui::AutoLoadPane* pane_ = nullptr;
    ui::KpiStrip* kpis_ = nullptr;
    ui::KpiStrip* risk_kpis_ = nullptr;
    ui::TimeSeriesChart* chart_ = nullptr;
    QTableView* table_ = nullptr;
    QStandardItemModel* table_model_ = nullptr;
    QLabel* note_ = nullptr;
};

} // namespace fincept::screens::deriv
