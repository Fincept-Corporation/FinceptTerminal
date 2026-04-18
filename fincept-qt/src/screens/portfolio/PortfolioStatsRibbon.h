// src/screens/portfolio/PortfolioStatsRibbon.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QWidget>

namespace fincept::screens {

class PortfolioStatsRibbon : public QWidget {
    Q_OBJECT
  public:
    explicit PortfolioStatsRibbon(QWidget* parent = nullptr);

    void set_summary(const portfolio::PortfolioSummary& summary);
    void set_metrics(const portfolio::ComputedMetrics& metrics);
    void refresh_theme();

  private:
    // Hero cell — primary row. Large value, label above, sub below.
    struct HeroCell {
        QWidget* container = nullptr;
        QLabel* label = nullptr;
        QLabel* value = nullptr;
        QLabel* sub = nullptr;
    };

    // Compact cell — secondary row. Single-line "LABEL  value" chip.
    struct ChipCell {
        QWidget* container = nullptr;
        QLabel* label = nullptr;
        QLabel* value = nullptr;
    };

    HeroCell add_hero(QHBoxLayout* row, const QString& label_text, const char* value_color);
    ChipCell add_chip(QHBoxLayout* row, const QString& label_text, const char* value_color);
    void apply_hero_styles(HeroCell& c, const char* value_color);
    void apply_chip_styles(ChipCell& c, const char* value_color);

    QWidget* primary_row_ = nullptr;   // hero KPIs
    QWidget* secondary_row_ = nullptr; // risk chips

    // Primary (hero) cells
    HeroCell total_value_;
    HeroCell pnl_;
    HeroCell day_change_;
    HeroCell positions_;

    // Secondary (chip) cells
    ChipCell cost_basis_;
    ChipCell concentration_;
    ChipCell sharpe_;
    ChipCell beta_;
    ChipCell volatility_;
    ChipCell max_drawdown_;
    ChipCell var95_;
    ChipCell risk_score_;
};

} // namespace fincept::screens
