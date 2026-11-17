#pragma once
// AltFundAnalyticsPanel — the Alternative Investments FUND ANALYTICS workbench (package A5): a fund's or
// liquid alternative's monthly returns against a benchmark. CAGR, volatility, Sharpe, Sortino, maximum
// drawdown, VaR / CVaR, beta, Jensen's alpha, up / down capture, tracking error; growth of 100 with the
// drawdown, rolling statistics, the monthly-returns grid, the return distribution, and first-order
// unsmoothing (Geltner) for appraisal-based series.
//
// Source: returns the user pastes / imports (local: runs on first show; the example series is labelled
// as such) or a ticker's monthly total returns from Yahoo Finance; a benchmark ticker is optional. Any
// run that calls Yahoo is never automatic (the last result with its "as of", else a preview with RUN).
// The pasted series stays on this computer (local cache only, never settings / sync).

#include "screens/alt_investments/AltWorkbenchKit.h"

#include <QHash>
#include <QJsonObject>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QSpinBox;
class QTableView;

namespace fincept::ui {
class AutoLoadPane;
class DataGridEditor;
class DistributionChart;
class InputMemory;
class KpiStrip;
class MonthlyReturnsHeatmap;
class SymbolSearchBox;
class TimeSeriesChart;
} // namespace fincept::ui

namespace fincept::screens::alt {

class AltFundAnalyticsPanel : public QWidget {
    Q_OBJECT
  public:
    explicit AltFundAnalyticsPanel(QWidget* parent = nullptr);
    ~AltFundAnalyticsPanel() override;

  private:
    void build_ui();
    bool uses_network() const;
    QJsonObject input() const;
    void update_view();
    void save_series();
    void start(quint64 id);
    void render(const QJsonValue& payload);

    QHash<quint64, quint64> service_ids_;
    bool restoring_ = false;

    QComboBox* source_ = nullptr;
    fincept::ui::SymbolSearchBox* ticker_ = nullptr;
    QComboBox* period_ = nullptr;
    fincept::ui::SymbolSearchBox* benchmark_ = nullptr;
    QDoubleSpinBox* rf_ = nullptr;
    QSpinBox* window_ = nullptr;
    QComboBox* unit_ = nullptr;
    QCheckBox* unsmooth_ = nullptr;
    QPushButton* run_ = nullptr;
    QWidget* series_box_ = nullptr;
    QWidget* ticker_box_ = nullptr;
    fincept::ui::DataGridEditor* grid_ = nullptr;
    QLabel* series_note_ = nullptr;
    fincept::ui::InputMemory* memory_ = nullptr;

    AltWbActions actions_;
    fincept::ui::AutoLoadPane* pane_ = nullptr;
    fincept::ui::KpiStrip* kpis_ = nullptr;
    QLabel* notes_ = nullptr;
    fincept::ui::TimeSeriesChart* growth_ = nullptr;
    fincept::ui::TimeSeriesChart* rolling_ = nullptr;
    fincept::ui::MonthlyReturnsHeatmap* grid_map_ = nullptr;
    fincept::ui::DistributionChart* dist_ = nullptr;
    AltWbRowsModel* stats_model_ = nullptr;
    QTableView* stats_table_ = nullptr;
    AltWbExport export_;
};

} // namespace fincept::screens::alt
