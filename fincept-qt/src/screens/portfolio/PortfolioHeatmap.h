// src/screens/portfolio/PortfolioHeatmap.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"

#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QWidget>

namespace fincept::screens {

/// 220px left sidebar with color-coded holding blocks, risk gauge, and top movers.
class PortfolioHeatmap : public QWidget {
    Q_OBJECT
  public:
    explicit PortfolioHeatmap(QWidget* parent = nullptr);

    void set_holdings(const QVector<portfolio::HoldingWithQuote>& holdings);
    void set_metrics(const portfolio::ComputedMetrics& metrics);
    void set_selected_symbol(const QString& symbol);
    void set_currency(const QString& currency);
    void refresh_theme();

  signals:
    void symbol_selected(QString symbol);
    void mode_changed(portfolio::HeatmapMode mode);

  private:
    void build_ui();
    void rebuild_blocks();
    QColor block_color(const portfolio::HoldingWithQuote& h) const;
    void update_top_movers();

    // Mode buttons
    QPushButton* pnl_btn_ = nullptr;
    QPushButton* weight_btn_ = nullptr;
    QPushButton* day_btn_ = nullptr;

    // Blocks container
    QWidget* blocks_container_ = nullptr;

    // Top movers footer
    QLabel* top_gainer_ = nullptr;
    QLabel* top_loser_ = nullptr;

    // State
    QVector<portfolio::HoldingWithQuote> holdings_;
    portfolio::ComputedMetrics metrics_;
    portfolio::HeatmapMode mode_ = portfolio::HeatmapMode::Pnl;
    QString selected_symbol_;
    QString currency_ = "USD";
};

} // namespace fincept::screens
