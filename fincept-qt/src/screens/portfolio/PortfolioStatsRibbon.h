// src/screens/portfolio/PortfolioStatsRibbon.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QWidget>

class QGridLayout;

namespace fincept::screens {

/// Stats ribbon — the 72px row beneath the command bar.
///
/// Composition (left → right, separated by 1px BORDER_DIM hairlines):
///   [PORTFOLIO VALUE]  [UNREALIZED P&L]  [TODAY]  [RISK & POSITIONING (2×3 chip grid)]
///
/// Designed for unequal cell weights — the first three cells stack a small
/// label, a hero value, and a sub-line; the fourth cell hosts a 2×3 grid of
/// chips (Sharpe, Beta, MDD, Conc, Vol30, Risk). One source of truth for
/// portfolio-level KPIs; the command bar above no longer duplicates them.
class PortfolioStatsRibbon : public QWidget {
    Q_OBJECT
  public:
    explicit PortfolioStatsRibbon(QWidget* parent = nullptr);

    void set_summary(const portfolio::PortfolioSummary& summary);
    void set_metrics(const portfolio::ComputedMetrics& metrics);
    void refresh_theme();

  private:
    // A "hero" cell — small label, big value, smaller sub line.
    struct HeroCell {
        QWidget* container = nullptr;
        QLabel* label = nullptr;
        QLabel* value = nullptr;
        QLabel* sub = nullptr;
    };

    // A grid chip — two-column "LABEL    value" line.
    struct GridChip {
        QLabel* label = nullptr;
        QLabel* value = nullptr;
    };

    QWidget* build_hero(HeroCell& cell, const QString& label_text, int value_px);
    QWidget* build_risk_grid();
    GridChip add_grid_chip(QGridLayout* g, int row, int col, const QString& label_text);
    void apply_hero_value_color(HeroCell& c, const char* color, int value_px);

    // Cells
    HeroCell value_cell_;     // PORTFOLIO VALUE
    HeroCell pnl_cell_;       // UNREALIZED P&L
    HeroCell day_cell_;       // TODAY
    QWidget* risk_cell_ = nullptr;

    // Risk-grid chips
    GridChip sharpe_;
    GridChip conc_;
    GridChip beta_;
    GridChip vol_;
    GridChip mdd_;
    GridChip risk_;
};

} // namespace fincept::screens
