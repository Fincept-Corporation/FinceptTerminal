// src/screens/portfolio/PortfolioHeatmap.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"

#include <QVector>
#include <QWidget>

class QLabel;
class QPushButton;

namespace fincept::ui {
class Treemap;
}

namespace fincept::screens {

/// Holdings map (squarified treemap): area = market value in the base currency,
/// grouped by sector, colour = today's move or the unrealised P&L % on the theme's
/// diverging scale with a robust clamp (90th percentile of the book's moves, so one
/// outlier does not wash the rest out). Positions that cannot be valued have no
/// area and are listed underneath instead of disappearing.
class PortfolioHeatmap : public QWidget {
    Q_OBJECT
  public:
    explicit PortfolioHeatmap(QWidget* parent = nullptr);

    void set_holdings(const QVector<portfolio::HoldingWithQuote>& holdings);
    void set_metrics(const portfolio::ComputedMetrics& metrics);
    void set_selected_symbol(const QString& symbol);
    void set_currency(const QString& currency);
    void refresh_theme();

  protected:
    void changeEvent(QEvent* event) override;

  signals:
    void symbol_selected(QString symbol);
    void mode_changed(portfolio::HeatmapMode mode);

  private:
    void build_ui();
    void rebuild();
    void apply_styles();
    void retranslateUi();

    QLabel* title_label_ = nullptr;
    QPushButton* day_btn_ = nullptr;
    QPushButton* pnl_btn_ = nullptr;
    QPushButton* weight_btn_ = nullptr;
    ui::Treemap* map_ = nullptr;
    QLabel* footer_ = nullptr;

    QVector<portfolio::HoldingWithQuote> holdings_;
    portfolio::HeatmapMode mode_ = portfolio::HeatmapMode::DayChange;
    QString selected_symbol_;
    QString currency_; ///< portfolio base currency; empty (no unit printed) until set_currency()
};

} // namespace fincept::screens
