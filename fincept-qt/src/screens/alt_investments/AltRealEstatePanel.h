#pragma once
// AltRealEstatePanel — the Alternative Investments REAL ESTATE workbench (package A6).
//
//   PRO FORMA        a multi-year property model: income, expenses, purchase and financing, exit.
//                    Going-in cap, unlevered / levered IRR, equity multiple, DSCR, LTV, debt yield,
//                    cash-on-cash, a yearly table, cash flow by year, and the exit cap x rent growth
//                    sensitivity of the IRR (ui::SensitivityTable). Local; runs on first show with the
//                    remembered inputs (ui::InputMemory), again on RUN.
//   REIT COMPARISON  up to 10 listed REITs from Yahoo Finance: price, dividend yield, market cap,
//                    debt / equity, price / book, net debt / EBITDA, 1-year total return; P/FFO only
//                    where the user typed FFO per share. Never automatic (a rate-limited free source):
//                    the last result with its "as of", else a preview with RUN.
// Python through services::alt::AltInvestmentsService (real_estate_proforma.py).

#include "screens/alt_investments/AltWorkbenchKit.h"

#include <QHash>
#include <QJsonObject>
#include <QWidget>

class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTableView;
class QTabWidget;

namespace fincept::ui {
class AutoLoadPane;
class BarChart;
class DataGridEditor;
class InputMemory;
class KpiStrip;
class SensitivityTable;
class SymbolSearchBox;
} // namespace fincept::ui

namespace fincept::screens::alt {

class AltRealEstatePanel : public QWidget {
    Q_OBJECT
  public:
    explicit AltRealEstatePanel(QWidget* parent = nullptr);
    ~AltRealEstatePanel() override;

  private:
    QWidget* build_proforma_page(QWidget* parent);
    QWidget* build_reit_page(QWidget* parent);
    QDoubleSpinBox* add_money(QWidget* group, const QString& key, const QString& label, double value);
    QDoubleSpinBox* add_pct(QWidget* group, const QString& key, const QString& label, double value, double lo,
                            double hi, int decimals = 2);
    QSpinBox* add_int(QWidget* group, const QString& key, const QString& label, int value, int lo, int hi,
                      const QString& suffix);
    void reset_example();
    QJsonObject proforma_input() const;
    void start_proforma(quint64 id);
    void render_proforma(const QJsonValue& payload);

    QJsonObject reit_input() const;
    QString reit_view_key() const;
    void update_reit_view();
    void add_reit_symbols(const QStringList& symbols);
    void start_reits(quint64 id);
    void render_reits(const QJsonValue& payload);
    void cancel_service(quint64 pane_request);

    QHash<quint64, quint64> service_ids_;
    QTabWidget* tabs_ = nullptr;

    // Pro forma
    QHash<QString, QDoubleSpinBox*> money_;
    QHash<QString, QDoubleSpinBox*> pct_;
    QHash<QString, QSpinBox*> ints_;
    fincept::ui::InputMemory* pf_memory_ = nullptr;
    AltWbActions pf_actions_;
    fincept::ui::AutoLoadPane* pf_pane_ = nullptr;
    fincept::ui::KpiStrip* pf_kpis_ = nullptr;
    QLabel* pf_notes_ = nullptr;
    fincept::ui::BarChart* pf_chart_ = nullptr;
    fincept::ui::SensitivityTable* pf_sens_ = nullptr;
    AltWbRowsModel* pf_model_ = nullptr;
    QTableView* pf_table_ = nullptr;
    AltWbExport pf_export_;

    // REIT comparison
    fincept::ui::SymbolSearchBox* reit_add_ = nullptr;
    fincept::ui::DataGridEditor* reit_grid_ = nullptr;
    QLineEdit* reit_rows_store_ = nullptr; ///< hidden: the grid as JSON, remembered by InputMemory
    fincept::ui::InputMemory* reit_memory_ = nullptr;
    QLabel* reit_status_ = nullptr;
    QPushButton* reit_run_ = nullptr;
    AltWbActions reit_actions_;
    fincept::ui::AutoLoadPane* reit_pane_ = nullptr;
    QLabel* reit_notes_ = nullptr;
    AltWbRowsModel* reit_model_ = nullptr;
    QTableView* reit_table_ = nullptr;
    AltWbExport reit_export_;
    bool reit_loading_ = false;
};

} // namespace fincept::screens::alt
