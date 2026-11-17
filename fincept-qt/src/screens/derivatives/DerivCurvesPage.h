#pragma once
// DerivCurvesPage — CURVES: free curves through CurveDataService (cross-tab X6) and the
// rates calculators priced off them.
//   RATES        — the U.S. Treasury daily par curve (keyless, public domain) bootstrapped
//                  to zero rates, discount factors and forwards; the par yields are
//                  EDITABLE (your overrides per tenor, "Reset to Treasury"); a fixed/float
//                  swap (par rate, PV, DV01), an FRA and a cap / floor off the curve. The
//                  Treasury curve is a proxy for a risk-free curve (no free OIS curve
//                  exists): labelled so.
//   KEY-RATE DV01 — the swap of the RATES calculator or a bond, bucketed by par tenor.
//   SWAPTION     — European payer / receiver swaption, Black-76 or Bachelier.
//   BOND         — a fixed-coupon bond off the curve: curve price, Z-spread, asset-swap
//                  spread, duration and convexity.
//   FUTURES      — contract months of a futures root (Yahoo Finance, delayed): the curve,
//                  contango / backwardation, calendar spreads and annualised roll yields.
//   CARRY        — futures fair value by cost of carry, basis, implied repo and yield.
// Only RATES is built with the page; every other view is built the first time it is
// opened. Fetches run as child processes off the GUI thread (CurveDataService); the
// key-rate bumps run on a worker with a request id and a cancel flag; the other
// calculators are microseconds and run inline (edits debounced).

#include "screens/derivatives/DerivCommon.h"
#include "services/options/CurveDataService.h"
#include "services/options/RatesMath.h"

#include <QHash>
#include <QMap>
#include <QStringList>
#include <QWidget>

#include <atomic>
#include <memory>
#include <vector>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class QStandardItem;
class QStandardItemModel;
class QTableView;
class QTimer;

namespace fincept::ui {
class AutoLoadPane;
class BarChart;
class InputMemory;
class KpiStrip;
class TimeSeriesChart;
} // namespace fincept::ui

namespace fincept::screens::deriv {

class DerivCurvesPage : public QWidget {
    Q_OBJECT
  public:
    explicit DerivCurvesPage(QWidget* parent = nullptr);
    ~DerivCurvesPage() override;
    void refresh();

  signals:
    /// "ADD SWAP TO BOOK" on the RATES swap calculator (fixed rate decimal).
    void add_swap_to_book(double start_years, double tenor_years, double fixed_rate, int freq, double notional,
                          bool payer);
    /// "ADD TO BOOK" on a contract month of the FUTURES view.
    void add_future_to_book(const QString& contract_symbol, const QString& root, double price);

  protected:
    void changeEvent(QEvent* e) override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

  private:
    enum View { kRates = 0, kFutures, kKeyRates, kSwaption, kBond, kCarry, kViewCount };

    struct KeyRateOut {
        services::options::rates::KeyRateResult kr;
        double zero_dv01 = 0;
        QString what;
    };

    void build();
    QWidget* ensure_view(int view);
    QWidget* build_rates(QWidget* parent);
    QWidget* build_futures(QWidget* parent);
    QWidget* build_key_rates(QWidget* parent);
    QWidget* build_swaption(QWidget* parent);
    QWidget* build_bond(QWidget* parent);
    QWidget* build_carry(QWidget* parent);
    void set_view(int index);
    void schedule();
    void recompute_view();

    // Curve (published Treasury par curve + the user's overrides).
    void on_curve_changed();
    void rebuild_curve();
    void ensure_curve();
    void apply_override_edit(QStandardItem* item);
    void reset_overrides();
    void load_overrides();
    void save_overrides();
    bool has_curve() const { return !zero_.empty(); }

    void render_futures();
    void render_rates();
    void recompute_calculators();
    void run_key_rates();
    void render_key_rates(const KeyRateOut& out);
    void compute_swaption();
    void compute_bond();
    void compute_carry();
    void update_swaption_units(bool convert);
    void update_key_rate_rows();
    int selected_future() const;
    void use_future_in_carry(int row);
    void add_selected_future_to_book(int row);
    void futures_menu(const QPoint& pos);
    void retranslate();
    ui::ChartExport::Table export_table() const;
    QPixmap export_image() const;
    report::ReportPayload report_payload() const;
    QVector<QPair<QString, QString>> view_inputs() const;

    TrBook book_;
    ui::InputMemory* memory_ = nullptr;
    QHash<int, QVector<QStringList>> calc_rows_; ///< view → (label, value, note) of its tiles, for exports
    QList<QPushButton*> tab_btns_;
    QVector<int> tab_views_; ///< the view of each tab button
    QStackedWidget* views_ = nullptr;
    QVector<QWidget*> holders_;
    QVector<bool> built_;
    int view_ = kRates;
    QLabel* status_ = nullptr;
    DerivResultBar* result_bar_ = nullptr;
    QTimer* debounce_ = nullptr;
    quint64 fut_req_ = 0;
    quint64 ust_req_ = 0;
    quint64 curve_req_ = 0;

    // Curve state
    services::options::CurveDataService::TreasuryPtr treasury_;
    bool treasury_stale_ = false;
    QMap<QString, double> overrides_; ///< tenor label → par yield (percent)
    std::vector<double> eff_tenors_;
    std::vector<double> eff_par_;
    QStringList eff_labels_;
    int applied_overrides_ = 0;
    services::options::rates::ZeroCurve zero_;
    bool rendering_rates_ = false;
    bool rates_dirty_ = false;

    // Futures
    QComboBox* root_ = nullptr;
    ui::AutoLoadPane* fut_pane_ = nullptr;
    services::options::CurveDataService::FuturesPtr futures_;
    ui::KpiStrip* fut_kpis_ = nullptr;
    ui::TimeSeriesChart* fut_chart_ = nullptr;
    QTableView* fut_table_ = nullptr;
    QStandardItemModel* fut_model_ = nullptr;
    QLabel* fut_note_ = nullptr;

    // Rates
    ui::AutoLoadPane* ust_pane_ = nullptr;
    ui::KpiStrip* ust_kpis_ = nullptr;
    ui::TimeSeriesChart* ust_chart_ = nullptr;
    QTableView* ust_table_ = nullptr;
    QStandardItemModel* ust_model_ = nullptr;
    QLabel* ovr_note_ = nullptr;
    QPushButton* ovr_reset_ = nullptr;
    QLineEdit* ovr_memory_ = nullptr; ///< hidden: the overrides as JSON, through InputMemory
    QDoubleSpinBox* sw_start_ = nullptr;
    QDoubleSpinBox* sw_tenor_ = nullptr;
    QDoubleSpinBox* sw_rate_ = nullptr;
    QComboBox* sw_freq_ = nullptr;
    QDoubleSpinBox* sw_notional_ = nullptr;
    QComboBox* sw_side_ = nullptr;
    QDoubleSpinBox* fra_start_ = nullptr;
    QDoubleSpinBox* fra_end_ = nullptr;
    QDoubleSpinBox* fra_rate_ = nullptr;
    QDoubleSpinBox* cap_strike_ = nullptr;
    QDoubleSpinBox* cap_vol_ = nullptr;
    QComboBox* cap_kind_ = nullptr;
    QComboBox* cap_model_ = nullptr;
    ui::KpiStrip* calc_kpis_ = nullptr;
    QLabel* ust_note_ = nullptr;

    // Key-rate DV01
    QComboBox* kr_instrument_ = nullptr;
    QDoubleSpinBox* kr_coupon_ = nullptr;
    QDoubleSpinBox* kr_maturity_ = nullptr;
    QComboBox* kr_freq_ = nullptr;
    QDoubleSpinBox* kr_face_ = nullptr;
    QWidget* kr_bond_row_ = nullptr;
    QLabel* kr_swap_label_ = nullptr;
    ui::KpiStrip* kr_kpis_ = nullptr;
    ui::BarChart* kr_chart_ = nullptr;
    QTableView* kr_table_ = nullptr;
    QStandardItemModel* kr_model_ = nullptr;
    QLabel* kr_note_ = nullptr;
    quint64 kr_req_ = 0;
    std::shared_ptr<std::atomic<bool>> kr_cancel_;
    bool kr_has_result_ = false;

    // Swaption
    QDoubleSpinBox* sv_expiry_ = nullptr;
    QDoubleSpinBox* sv_tenor_ = nullptr;
    QDoubleSpinBox* sv_strike_ = nullptr;
    QComboBox* sv_side_ = nullptr;
    QComboBox* sv_model_ = nullptr;
    QDoubleSpinBox* sv_vol_ = nullptr;
    QComboBox* sv_freq_ = nullptr;
    QDoubleSpinBox* sv_notional_ = nullptr;
    ui::KpiStrip* sv_kpis_ = nullptr;
    ui::TimeSeriesChart* sv_chart_ = nullptr;
    QLabel* sv_note_ = nullptr;
    double sv_last_fwd_ = 0.04;

    // Bond
    QDoubleSpinBox* bd_coupon_ = nullptr;
    QComboBox* bd_freq_ = nullptr;
    QDoubleSpinBox* bd_maturity_ = nullptr;
    QDoubleSpinBox* bd_price_ = nullptr;
    QComboBox* bd_kind_ = nullptr;
    QDoubleSpinBox* bd_face_ = nullptr;
    ui::KpiStrip* bd_kpis_ = nullptr;
    ui::TimeSeriesChart* bd_chart_ = nullptr;
    QLabel* bd_note_ = nullptr;

    // Carry
    QDoubleSpinBox* cy_spot_ = nullptr;
    QSpinBox* cy_days_ = nullptr;
    QCheckBox* cy_use_curve_ = nullptr;
    QDoubleSpinBox* cy_rate_ = nullptr;
    QDoubleSpinBox* cy_carry_ = nullptr;
    QDoubleSpinBox* cy_yield_ = nullptr;
    QSpinBox* cy_d1_days_ = nullptr;
    QDoubleSpinBox* cy_d1_amt_ = nullptr;
    QSpinBox* cy_d2_days_ = nullptr;
    QDoubleSpinBox* cy_d2_amt_ = nullptr;
    QDoubleSpinBox* cy_market_ = nullptr;
    QPushButton* cy_use_future_ = nullptr;
    QLabel* cy_source_ = nullptr;
    ui::KpiStrip* cy_kpis_ = nullptr;
    ui::TimeSeriesChart* cy_chart_ = nullptr;
    QLabel* cy_note_ = nullptr;
};

} // namespace fincept::screens::deriv
