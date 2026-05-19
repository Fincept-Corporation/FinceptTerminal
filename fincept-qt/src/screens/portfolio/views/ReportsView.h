// src/screens/portfolio/views/ReportsView.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"

#include <QLabel>
#include <QTabWidget>
#include <QTableWidget>
#include <QWidget>

namespace fincept::screens {

/// Reports & PME detail view with transaction history, performance attribution, and export.
class ReportsView : public QWidget {
    Q_OBJECT
  public:
    explicit ReportsView(QWidget* parent = nullptr);

    void set_data(const portfolio::PortfolioSummary& summary, const QString& currency);

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void build_ui();
    void retranslateUi();
    void update_summary();
    void update_transactions();
    void update_attribution();

    QTabWidget* tabs_ = nullptr;
    int summary_tab_index_ = -1;
    int txn_tab_index_ = -1;
    int attr_tab_index_ = -1;

    // Summary tab
    QWidget* summary_panel_ = nullptr;

    // Transactions tab
    QTableWidget* txn_table_ = nullptr;
    QLabel* txn_title_ = nullptr;

    // Attribution tab
    QTableWidget* attr_table_ = nullptr;
    QLabel* attr_title_ = nullptr;

    bool has_data_ = false;

    portfolio::PortfolioSummary summary_;
    QString currency_;
};

} // namespace fincept::screens
