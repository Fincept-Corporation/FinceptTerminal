// src/screens/portfolio/PortfolioBlotter.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"

#include <QHash>
#include <QPointer>
#include <QTableWidget>
#include <QWidget>

namespace fincept::screens {

class PortfolioSparkline;

/// 11-column sortable positions table matching Bloomberg blotter style.
class PortfolioBlotter : public QWidget {
    Q_OBJECT
  public:
    explicit PortfolioBlotter(QWidget* parent = nullptr);

    void set_holdings(const QVector<portfolio::HoldingWithQuote>& holdings);
    void set_selected_symbol(const QString& symbol);

  signals:
    void symbol_selected(QString symbol);
    void sort_changed(portfolio::SortColumn col, portfolio::SortDirection dir);

  private:
    void build_ui();
    void populate_table();
    void on_header_clicked(int section);
    void on_row_clicked(int row, int col);
    QString format_value(double v, int dp = 2) const;

    QTableWidget* table_ = nullptr;

    void fetch_sparklines();

    QVector<portfolio::HoldingWithQuote> holdings_;
    QVector<portfolio::HoldingWithQuote> sorted_;
    QString selected_symbol_;
    portfolio::SortColumn sort_col_ = portfolio::SortColumn::Weight;
    portfolio::SortDirection sort_dir_ = portfolio::SortDirection::Desc;

    // Real sparkline data keyed by symbol — populated async via fetch_sparklines()
    QHash<QString, QVector<double>> sparkline_cache_;
};

} // namespace fincept::screens
