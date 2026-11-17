#pragma once
// DerivScenarioPage — SCENARIOS: what-if analysis of the STRATEGY page's position (or
// of a single option typed here):
//   * GRID   — P&L, delta and vega over spot shocks × volatility shocks at a chosen
//              number of days forward and rate shift (ui::SensitivityTable, the
//              terminal's shared two-axis grid; the unshocked base cell is outlined);
//   * CONE   — the lognormal probability cone of the underlying (±1σ / ±2σ) with the
//              position's breakevens as reference lines (ui::FanChart);
//   * STRESS — named presets (spot ±10 / ±20 %, vol crush / spike, rates ±100 bp, a
//              week of decay, crash, rally) plus the user's own rows (ui::ScenarioGrid).
// Every number is recomputed on a worker thread with a request id and a cancel flag;
// edits are debounced; the page opens computed.

#include "screens/derivatives/DerivCommon.h"
#include "screens/derivatives/DerivContext.h"

#include <QJsonObject>
#include <QWidget>

#include <atomic>
#include <memory>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;
class QSpinBox;
class QStackedWidget;
class QTimer;

namespace fincept::ui {
class AutoLoadPane;
class FanChart;
class InputMemory;
class ScenarioGrid;
class SensitivityTable;
} // namespace fincept::ui

namespace fincept::screens::deriv {

class DerivScenarioPage : public QWidget {
    Q_OBJECT
  public:
    explicit DerivScenarioPage(DerivContext* ctx, QWidget* parent = nullptr);
    ~DerivScenarioPage() override;

    /// Recompute now (RUN / F5).
    void refresh();
    /// Analyse the STRATEGY page's position (switches the source to it).
    void use_strategy_position();
    /// Analyse the position(s) handed over by the BOOK (DerivContext::book_position()).
    void use_book_position();
    /// Analyse one listed contract as a single option (from the chain).
    void load_contract(const ContractRef& contract);

  protected:
    void changeEvent(QEvent* e) override;

  private:
    void build();
    void retranslate();
    void schedule();
    void update_source_rows();
    void render(const QJsonObject& payload);
    void set_view(int index);
    void save_stress();
    void restore_stress();
    bool position(services::options::strategy::Spec* spec, services::options::strategy::Market* m, double* atm_iv,
                  QString* label) const;
    ui::ChartExport::Table export_table() const;
    QPixmap export_image() const;
    report::ReportPayload report_payload() const;

    DerivContext* ctx_ = nullptr;
    TrBook book_;
    ui::InputMemory* memory_ = nullptr;
    std::shared_ptr<std::atomic<bool>> cancel_;
    QTimer* debounce_ = nullptr;
    QJsonObject last_;
    QString currency_ = QStringLiteral("USD");

    QComboBox* source_ = nullptr;
    QLabel* summary_ = nullptr;
    QLabel* status_ = nullptr;
    DerivResultBar* result_bar_ = nullptr;
    QDoubleSpinBox* spot_range_ = nullptr;
    QSpinBox* spot_steps_ = nullptr;
    QDoubleSpinBox* vol_range_ = nullptr;
    QSpinBox* vol_steps_ = nullptr;
    QSlider* days_ = nullptr;
    QLabel* days_label_ = nullptr;
    QDoubleSpinBox* rate_bp_ = nullptr;
    QWidget* single_ = nullptr;
    QDoubleSpinBox* s_spot_ = nullptr;
    QDoubleSpinBox* s_strike_ = nullptr;
    QSpinBox* s_days_ = nullptr;
    QDoubleSpinBox* s_iv_ = nullptr;
    QDoubleSpinBox* s_rate_ = nullptr;
    QComboBox* s_type_ = nullptr;
    QDoubleSpinBox* s_qty_ = nullptr;
    QLineEdit* stress_memory_ = nullptr;

    ui::AutoLoadPane* pane_ = nullptr;
    QList<QPushButton*> tab_btns_;
    QStackedWidget* views_ = nullptr;
    ui::SensitivityTable* grid_ = nullptr;
    ui::FanChart* cone_ = nullptr;
    QLabel* cone_note_ = nullptr;
    ui::ScenarioGrid* stress_ = nullptr;
    QLabel* note_ = nullptr;
};

} // namespace fincept::screens::deriv
