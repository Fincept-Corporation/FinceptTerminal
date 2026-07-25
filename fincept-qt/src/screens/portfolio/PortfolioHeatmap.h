// src/screens/portfolio/PortfolioHeatmap.h
#pragma once
#include "screens/portfolio/PortfolioTypes.h"

#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVector>
#include <QWidget>

class QGridLayout;

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

  protected:
    void changeEvent(QEvent* event) override;

  signals:
    void symbol_selected(QString symbol);
    void mode_changed(portfolio::HeatmapMode mode);

  private:
    void build_ui();
    void rebuild_blocks();
    QColor block_color(const portfolio::HoldingWithQuote& h) const;
    void update_top_movers();
    void retranslateUi();

    // Header
    QLabel* title_label_ = nullptr;
    QLabel* movers_header_ = nullptr;

    // Mode buttons
    QPushButton* pnl_btn_ = nullptr;
    QPushButton* weight_btn_ = nullptr;
    QPushButton* day_btn_ = nullptr;

    // Blocks container. Blocks are allocated once and reused across refreshes —
    // rebuilding them (and re-running setStyleSheet on every one) fired on every
    // 60 s poll AND on every selection change, which is a full CSS reparse per
    // holding for data that usually has not changed.
    QWidget* blocks_container_ = nullptr;
    QGridLayout* blocks_grid_ = nullptr;
    QVector<QPushButton*> blocks_;
    int stretch_row_ = -1;

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
