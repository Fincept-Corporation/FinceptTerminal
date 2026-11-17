// src/screens/portfolio/views/IncomeView.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"

#include <QWidget>

class QComboBox;
class QPushButton;
class QTableWidget;

namespace fincept::ui {
class BarChart;
class KpiStrip;
} // namespace fincept::ui

namespace fincept::screens::pfmt {
class PortfolioNoticeBar;
}

namespace fincept::screens {

/// INCOME — dividends and interest received (from the ledger, net of withholding,
/// in the base currency) by month, trailing-12-month and forward income, yield and
/// yield on cost, and the upcoming ex-dividend / earnings dates of the holdings
/// (market info; no billed calendar request).
class IncomeView : public QWidget {
    Q_OBJECT
  public:
    explicit IncomeView(QWidget* parent = nullptr);

    void set_data(const portfolio::PortfolioSummary& summary, const QString& currency);

  protected:
    void changeEvent(QEvent* event) override;

  private:
    struct Payment {
        QDate date;
        QString symbol;
        QString type; ///< DIVIDEND / INTEREST
        double net_local = 0;
        QString currency; ///< major
        double net_base = 0; ///< NaN when the FX of the date is unknown
        double withheld_local = 0;
    };

    void build_ui();
    void reload();
    void render();
    void export_csv();

    pfmt::PortfolioNoticeBar* notice_ = nullptr;
    QComboBox* span_cb_ = nullptr;
    QPushButton* export_btn_ = nullptr;
    ui::KpiStrip* kpis_ = nullptr;
    ui::BarChart* monthly_ = nullptr;
    QTableWidget* holdings_table_ = nullptr;
    QTableWidget* upcoming_table_ = nullptr;
    QTableWidget* payments_table_ = nullptr;

    portfolio::PortfolioSummary summary_;
    QVector<Payment> payments_;
    int unconverted_ = 0;
    bool has_data_ = false;
};

} // namespace fincept::screens
