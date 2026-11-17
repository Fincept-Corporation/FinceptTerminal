// src/screens/portfolio/views/TaxLotsView.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"
#include "services/portfolio/PortfolioLots.h"

#include <QWidget>

class QComboBox;
class QLabel;
class QPushButton;
class QSpinBox;
class QTabWidget;
class QTableWidget;

namespace fincept::ui {
class BarChart;
class KpiStrip;
} // namespace fincept::ui

namespace fincept::screens::pfmt {
class PortfolioNoticeBar;
}

namespace fincept::screens {

/// TAX LOTS — open tax lots and realised gains from the ledger replay.
///
/// The relief method and long-term threshold default to the portfolio's settings
/// and can be changed here to compare (what-if; nothing is saved unless the user
/// presses "USE AS PORTFOLIO METHOD"). Realised gains split short / long term and
/// by tax year, with a stacked bar per year; both tables export to CSV.
class TaxLotsView : public QWidget {
    Q_OBJECT
  public:
    explicit TaxLotsView(QWidget* parent = nullptr);

    void set_data(const portfolio::PortfolioSummary& summary, const QString& currency);

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void build_ui();
    void recompute();
    void render();
    void render_open_lots();
    void render_realised();
    void export_csv();
    portfolio::LotMethod selected_method() const;

    pfmt::PortfolioNoticeBar* notice_ = nullptr;
    QComboBox* method_cb_ = nullptr;
    QSpinBox* lt_spin_ = nullptr;
    QComboBox* year_cb_ = nullptr;
    QPushButton* apply_btn_ = nullptr;
    QPushButton* export_btn_ = nullptr;
    ui::KpiStrip* kpis_ = nullptr;
    QTabWidget* tabs_ = nullptr;
    QTableWidget* open_table_ = nullptr;
    QTableWidget* realised_table_ = nullptr;
    ui::BarChart* year_chart_ = nullptr;

    portfolio::PortfolioSummary summary_;
    portfolio::LedgerResult ledger_;
    QString ledger_for_; // portfolio id + method + days the ledger was replayed for
    bool has_data_ = false;
};

} // namespace fincept::screens
