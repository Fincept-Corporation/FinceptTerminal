// src/screens/portfolio/PortfolioHeatmap.cpp
#include "screens/portfolio/PortfolioHeatmap.h"

#include "screens/portfolio/PortfolioPanelHeader.h"
#include "ui/theme/Theme.h"

#include <QGridLayout>
#include <QScrollArea>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens {

PortfolioHeatmap::PortfolioHeatmap(QWidget* parent) : QWidget(parent) {
    // Heatmap now shares the top 40% with chart + sector panel (commit #4),
    // not the full left column. Slightly wider band reads better at this size.
    setMinimumWidth(200);
    setMaximumWidth(240);
    build_ui();
}

void PortfolioHeatmap::build_ui() {
    setStyleSheet(
        QString("background:%1; border-right:1px solid %2;").arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));

    // Outer layout has zero margins so the unified header sits flush with the
    // panel edges; an inner content layout holds the body with its own padding.
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Unified panel header — HOLDINGS title + 3 mode buttons in the controls slot.
    auto header = make_panel_header("HOLDINGS", this);

    auto make_mode_btn = [this, &header](const QString& text) {
        auto* btn = new QPushButton(text);
        btn->setFixedSize(38, 20);
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        // 10px / 700, square corners. Active = AMBER fill on BG_BASE text.
        btn->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:1px solid %2;"
                                   "  font-size:10px; font-weight:700; }"
                                   "QPushButton:checked { background:%3; color:%4; border-color:%3; }"
                                   "QPushButton:hover:!checked { color:%5; border-color:%5; }")
                               .arg(ui::colors::TEXT_TERTIARY(), ui::colors::BORDER_DIM(), ui::colors::AMBER(),
                                    ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY()));
        header.controls_slot->layout()->addWidget(btn);
        return btn;
    };

    pnl_btn_ = make_mode_btn("PNL");
    weight_btn_ = make_mode_btn("WT");
    day_btn_ = make_mode_btn("DAY");
    pnl_btn_->setChecked(true);

    auto set_mode = [this](portfolio::HeatmapMode m) {
        mode_ = m;
        pnl_btn_->setChecked(m == portfolio::HeatmapMode::Pnl);
        weight_btn_->setChecked(m == portfolio::HeatmapMode::Weight);
        day_btn_->setChecked(m == portfolio::HeatmapMode::DayChange);
        rebuild_blocks();
        emit mode_changed(m);
    };
    connect(pnl_btn_, &QPushButton::clicked, this, [=]() { set_mode(portfolio::HeatmapMode::Pnl); });
    connect(weight_btn_, &QPushButton::clicked, this, [=]() { set_mode(portfolio::HeatmapMode::Weight); });
    connect(day_btn_, &QPushButton::clicked, this, [=]() { set_mode(portfolio::HeatmapMode::DayChange); });

    layout->addWidget(header.header);

    // ── Content area below the header ────────────────────────────────────────
    auto* body = new QWidget(this);
    body->setStyleSheet("background:transparent;");
    auto* body_layout_outer = new QVBoxLayout(body);
    body_layout_outer->setContentsMargins(8, 8, 8, 6);
    body_layout_outer->setSpacing(5);
    layout->addWidget(body, 1);

    // Re-aim downstream code: the rest of build_ui() adds widgets to `layout`,
    // but we want them in `body`. Swap the variable name without a wide rewrite.
    layout = body_layout_outer;

    // Scrollable blocks area
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(QString("QScrollArea { border:none; background:transparent; }"
                                  "QScrollBar:vertical { width:4px; background:transparent; }"
                                  "QScrollBar::handle:vertical { background:%1; }")
                              .arg(ui::colors::BORDER_BRIGHT()));

    blocks_container_ = new QWidget(this);
    blocks_container_->setStyleSheet("background:transparent;");
    scroll->setWidget(blocks_container_);
    layout->addWidget(scroll, 1);

    // ── Top movers footer ────────────────────────────────────────────────────
    // The previous panel duplicated the StatsRibbon and the POSITIONS table:
    //   - 8-row selected-holding inspector → already in POSITIONS row data
    //   - RISK SCORE gauge                  → already in StatsRibbon RISK chip
    //   - HOLDINGS / CONC.TOP3 / VOL30      → already in StatsRibbon
    // All three were deleted. Top movers stays — it's not in the ribbon and
    // is contextual to the heatmap (the visible blocks).
    auto* movers_sep = new QWidget(this);
    movers_sep->setFixedHeight(1);
    movers_sep->setStyleSheet(QString("background:%1;").arg(ui::colors::BORDER_DIM()));
    layout->addWidget(movers_sep);

    auto* movers_header = new QLabel("TOP MOVERS");
    movers_header->setStyleSheet(
        QString("color:%1; font-size:10px; font-weight:700; letter-spacing:1px;"
                "  padding-top:4px;")
            .arg(ui::colors::TEXT_TERTIARY()));
    layout->addWidget(movers_header);

    top_gainer_ = new QLabel;
    top_gainer_->setStyleSheet(QString("color:%1; font-size:11px; font-weight:600;").arg(ui::colors::POSITIVE()));
    layout->addWidget(top_gainer_);

    top_loser_ = new QLabel;
    top_loser_->setStyleSheet(QString("color:%1; font-size:11px; font-weight:600;").arg(ui::colors::NEGATIVE()));
    layout->addWidget(top_loser_);
}

void PortfolioHeatmap::set_holdings(const QVector<portfolio::HoldingWithQuote>& holdings) {
    holdings_ = holdings;
    rebuild_blocks();
    update_top_movers();
}

void PortfolioHeatmap::set_metrics(const portfolio::ComputedMetrics& metrics) {
    metrics_ = metrics;
    // Risk score / concentration / volatility are now in the StatsRibbon —
    // the heatmap no longer duplicates them. Keep the metrics_ field around
    // in case future heatmap modes want to colour blocks by risk score.
}

void PortfolioHeatmap::set_selected_symbol(const QString& symbol) {
    selected_symbol_ = symbol;
    rebuild_blocks();
}

void PortfolioHeatmap::set_currency(const QString& currency) {
    currency_ = currency;
}

QColor PortfolioHeatmap::block_color(const portfolio::HoldingWithQuote& h) const {
    double val = 0;
    switch (mode_) {
        case portfolio::HeatmapMode::Pnl:
            val = h.unrealized_pnl_percent;
            break;
        case portfolio::HeatmapMode::Weight:
            val = h.weight;
            break;
        case portfolio::HeatmapMode::DayChange:
            val = h.day_change_percent;
            break;
    }

    if (mode_ == portfolio::HeatmapMode::Weight) {
        // Amber: darker base, brighter at higher weights
        double t = std::min(val / 40.0, 1.0);
        int r = static_cast<int>(100 + t * 117); // 100 → 217
        int g = static_cast<int>(50 + t * 69);   // 50  → 119
        int b = static_cast<int>(6);
        return QColor(r, g, b);
    }

    // Green/Red: solid colors, brightness scales with magnitude
    double intensity = std::min(std::abs(val) / 20.0, 1.0); // saturates at ±20%
    if (val >= 0) {
        // Dark green → bright green
        int g = static_cast<int>(80 + intensity * 123); // 80 → 203
        int r = static_cast<int>(intensity * 22);
        return QColor(r, g, static_cast<int>(intensity * 30));
    } else {
        // Dark red → bright red
        int r = static_cast<int>(100 + intensity * 120); // 100 → 220
        return QColor(r, static_cast<int>(intensity * 20), static_cast<int>(intensity * 20));
    }
}

void PortfolioHeatmap::rebuild_blocks() {
    // Clear existing blocks — delete children first, then layout
    if (auto* old_layout = blocks_container_->layout()) {
        QLayoutItem* item;
        while ((item = old_layout->takeAt(0)) != nullptr) {
            if (item->widget())
                item->widget()->deleteLater();
            delete item;
        }
        delete old_layout;
    }

    auto* grid = new QGridLayout(blocks_container_);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(2);

    int col = 0, row = 0;
    for (const auto& h : holdings_) {
        auto* block = new QPushButton;
        bool selected = (h.symbol == selected_symbol_);

        int min_h = static_cast<int>(std::max(40.0, 40.0 + h.weight * 0.8));
        block->setFixedHeight(std::min(min_h, 70));
        block->setCursor(Qt::PointingHandCursor);

        QColor bg = block_color(h);
        QString border_style = selected ? QString("border:2px solid %1;").arg(ui::colors::AMBER())
                                        : QString("border:1px solid %1;").arg(ui::colors::BORDER_DIM());

        // Square corners (was border-radius:2px). Block label 11px (was 10px).
        block->setStyleSheet(QString("QPushButton { background:rgb(%1,%2,%3); %4"
                                     "  text-align:left; padding:4px 6px;"
                                     "  color:%6; font-size:11px; font-weight:700; }"
                                     "QPushButton:hover { border-color:%5; }")
                                 .arg(bg.red())
                                 .arg(bg.green())
                                 .arg(bg.blue())
                                 .arg(border_style, ui::colors::AMBER(), ui::colors::TEXT_PRIMARY()));

        // Content
        double chg_val = mode_ == portfolio::HeatmapMode::DayChange ? h.day_change_percent
                         : mode_ == portfolio::HeatmapMode::Pnl     ? h.unrealized_pnl_percent
                                                                    : h.weight;
        QString chg_str = mode_ == portfolio::HeatmapMode::Weight
                              ? QString("%1%").arg(QString::number(chg_val, 'f', 1))
                              : QString("%1%2%").arg(chg_val >= 0 ? "+" : "").arg(QString::number(chg_val, 'f', 1));

        block->setText(QString("%1\n%2").arg(h.symbol, chg_str));

        connect(block, &QPushButton::clicked, this, [this, sym = h.symbol]() {
            selected_symbol_ = sym;
            rebuild_blocks();
            emit symbol_selected(sym);
        });

        grid->addWidget(block, row, col);
        col++;
        if (col >= 2) {
            col = 0;
            row++;
        }
    }

    grid->setRowStretch(row + 1, 1); // push blocks up
}

void PortfolioHeatmap::update_top_movers() {
    if (holdings_.isEmpty()) {
        top_gainer_->setText("--");
        top_loser_->setText("--");
        return;
    }

    auto best = std::max_element(holdings_.begin(), holdings_.end(), [](const auto& a, const auto& b) {
        return a.day_change_percent < b.day_change_percent;
    });
    auto worst = std::min_element(holdings_.begin(), holdings_.end(), [](const auto& a, const auto& b) {
        return a.day_change_percent < b.day_change_percent;
    });

    top_gainer_->setText(QString("\u25B2 %1  %2%3%")
                             .arg(best->symbol)
                             .arg(best->day_change_percent >= 0 ? "+" : "")
                             .arg(QString::number(best->day_change_percent, 'f', 2)));

    top_loser_->setText(QString("\u25BC %1  %2%3%")
                            .arg(worst->symbol)
                            .arg(worst->day_change_percent >= 0 ? "+" : "")
                            .arg(QString::number(worst->day_change_percent, 'f', 2)));
}

void PortfolioHeatmap::refresh_theme() {
    setStyleSheet(
        QString("background:%1; border-right:1px solid %2;").arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));

    // Rebuild blocks picks up new theme colors for borders/text
    rebuild_blocks();
}

} // namespace fincept::screens
