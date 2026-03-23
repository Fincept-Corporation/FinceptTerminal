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

  private:
    void build_ui();
    void update_summary();
    void update_transactions();
    void update_attribution();

    QTabWidget* tabs_ = nullptr;

    // Summary tab
    QWidget* summary_panel_ = nullptr;

    // Transactions tab
    QTableWidget* txn_table_ = nullptr;

    // Attribution tab
    QTableWidget* attr_table_ = nullptr;

    portfolio::PortfolioSummary summary_;
    QString currency_;
};

} // namespace fincept::screens
