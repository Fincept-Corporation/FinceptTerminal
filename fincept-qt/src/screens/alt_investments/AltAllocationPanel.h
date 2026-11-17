#pragma once
// AltAllocationPanel — "ALLOCATION & LIQUIDITY" mode of the Alternative Investments screen (A9):
// the alternatives sleeve inside one portfolio. Liquid holdings come from the Portfolio tab
// (services::PortfolioService, read only), private-market funds from the fund book (A3, their latest
// NAV rolled forward and their unfunded commitments) and other alternatives are entered here with
// their liquidity. Shows allocation by class, the liquidity ladder, how many times assets liquid
// within 90 days cover the unfunded commitments, and stress cases (the denominator effect).
// Arithmetic: AltAllocationMath.h. Created by make_allocation_panel() (AltPanels.h).

#include "services/alt_investments/AltHoldingStore.h"

#include <QJsonObject>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QComboBox;
class QLabel;
class QPushButton;
class QTimer;

namespace fincept::portfolio {
struct PortfolioSummary;
}
namespace fincept::report {
struct ReportPayload;
}
namespace fincept::ui {
class DataGridEditor;
class ResultView;
} // namespace fincept::ui

namespace fincept::screens::alt {

class AltAllocationPanel : public QWidget {
    Q_OBJECT
  public:
    explicit AltAllocationPanel(QWidget* parent = nullptr);
    ~AltAllocationPanel() override;

  protected:
    void showEvent(QShowEvent* e) override;

  private:
    void build_ui();
    void on_portfolio_changed();
    void load_sources();
    void recompute();
    void save_holdings();
    fincept::report::ReportPayload report_payload() const;

    QComboBox* portfolio_combo_ = nullptr;
    QLabel* source_note_ = nullptr;
    fincept::ui::DataGridEditor* grid_ = nullptr;
    QLabel* grid_note_ = nullptr;
    fincept::ui::ResultView* view_ = nullptr;
    QTimer* save_timer_ = nullptr;

    QString portfolio_id_;
    QString base_ccy_;
    bool portfolios_requested_ = false;
    bool filling_grid_ = false;
    quint64 generation_ = 0; ///< bumped on every portfolio switch: older replies are dropped
    bool have_summary_ = false;
    bool have_funds_ = false;
    bool have_holdings_ = false;
    struct Liquid {
        QString name;
        QString asset_class;
        double value = 0;
    };
    QVector<Liquid> liquid_positions_;
    int unpriced_ = 0;
    QVector<fincept::services::alt::AltFundPosition> funds_;
    QJsonObject last_;
};

} // namespace fincept::screens::alt
