// src/screens/portfolio/PortfolioHeatmap.cpp
#include "screens/portfolio/PortfolioHeatmap.h"

#include "ui/theme/Theme.h"

#include <QGridLayout>
#include <QScrollArea>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens {

PortfolioHeatmap::PortfolioHeatmap(QWidget* parent) : QWidget(parent) {
    setMinimumWidth(180);
    setMaximumWidth(220);
    build_ui();
}

void PortfolioHeatmap::build_ui() {
    setStyleSheet(
        QString("background:%1; border-right:1px solid %2;").arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 6);
    layout->setSpacing(5);

    // Header: title + mode buttons
    auto* header = new QHBoxLayout;
    auto* title = new QLabel("HOLDINGS");
    title->setStyleSheet(
        QString("color:%1; font-size:11px; font-weight:700; letter-spacing:1.5px;").arg(ui::colors::TEXT_SECONDARY));
    header->addWidget(title);
    header->addStretch();

    auto make_mode_btn = [&](const QString& text) {
        auto* btn = new QPushButton(text);
        btn->setFixedSize(38, 20);
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:1px solid %2;"
                                   "  font-size:9px; font-weight:700; border-radius:2px; }"
                                   "QPushButton:checked { background:%3; color:%4; border-color:%3; }"
                                   "QPushButton:hover:!checked { color:%5; border-color:%5; }")
                               .arg(ui::colors::TEXT_TERTIARY, ui::colors::BORDER_DIM, ui::colors::AMBER,
                                    ui::colors::BG_BASE, ui::colors::TEXT_PRIMARY));
        header->addWidget(btn);
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

    layout->addLayout(header);

    // Scrollable blocks area
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(QString("QScrollArea { border:none; background:transparent; }"
                                  "QScrollBar:vertical { width:4px; background:transparent; }"
                                  "QScrollBar::handle:vertical { background:%1; }")
                              .arg(ui::colors::BORDER_BRIGHT));

    blocks_container_ = new QWidget(this);
    blocks_container_->setStyleSheet("background:transparent;");
    scroll->setWidget(blocks_container_);
    layout->addWidget(scroll, 1);

    // Selected holding detail
    detail_panel_ = new QWidget(this);
    detail_panel_->setStyleSheet(
        QString("background:%1; border:1px solid %2; padding:4px;").arg(ui::colors::BG_RAISED, ui::colors::BORDER_DIM));
    detail_panel_->setVisible(false);

    auto* dp_layout = new QVBoxLayout(detail_panel_);
    dp_layout->setContentsMargins(6, 4, 6, 4);
    dp_layout->setSpacing(2);

    detail_symbol_ = new QLabel;
    detail_symbol_->setStyleSheet(QString("color:%1; font-size:14px; font-weight:700;").arg(ui::colors::CYAN));
    dp_layout->addWidget(detail_symbol_);

    auto add_detail_row = [&](QLabel*& lbl, const QString& prefix) {
        auto* row = new QHBoxLayout;
        auto* lab = new QLabel(prefix);
        lab->setStyleSheet(QString("color:%1; font-size:9px; font-weight:600;").arg(ui::colors::TEXT_TERTIARY));
        row->addWidget(lab);
        lbl = new QLabel("--");
        lbl->setAlignment(Qt::AlignRight);
        lbl->setStyleSheet(QString("color:%1; font-size:10px; font-weight:700;").arg(ui::colors::TEXT_PRIMARY));
        row->addWidget(lbl);
        dp_layout->addLayout(row);
    };

    add_detail_row(detail_price_, "PRICE");
    add_detail_row(detail_change_, "CHG%");
    add_detail_row(detail_qty_, "QTY");
    add_detail_row(detail_cost_, "COST");
    add_detail_row(detail_mv_, "MKT VAL");
    add_detail_row(detail_pnl_, "P&L");
    add_detail_row(detail_pnl_pct_, "P&L%");
    add_detail_row(detail_weight_, "WEIGHT");

    layout->addWidget(detail_panel_);

    // Risk gauge
    auto* risk_header = new QLabel("RISK SCORE");
    risk_header->setStyleSheet(
        QString("color:%1; font-size:8px; font-weight:700; letter-spacing:1px;").arg(ui::colors::TEXT_TERTIARY));
    layout->addWidget(risk_header);

    risk_bar_ = new QWidget(this);
    risk_bar_->setFixedHeight(10);
    risk_bar_->setStyleSheet(QString("background:%1; border-radius:2px;").arg(ui::colors::BG_BASE));
    layout->addWidget(risk_bar_);

    risk_value_ = new QLabel("--");
    risk_value_->setAlignment(Qt::AlignCenter);
    risk_value_->setStyleSheet(QString("color:%1; font-size:12px; font-weight:700;").arg(ui::colors::TEXT_SECONDARY));
    layout->addWidget(risk_value_);

    // Top movers
    auto* movers_header = new QLabel("TOP MOVERS");
    movers_header->setStyleSheet(
        QString("color:%1; font-size:8px; font-weight:700; letter-spacing:1px;").arg(ui::colors::TEXT_TERTIARY));
    layout->addWidget(movers_header);

    top_gainer_ = new QLabel;
    top_gainer_->setStyleSheet(QString("color:%1; font-size:10px; font-weight:600;").arg(ui::colors::POSITIVE));
    layout->addWidget(top_gainer_);

    top_loser_ = new QLabel;
    top_loser_->setStyleSheet(QString("color:%1; font-size:10px; font-weight:600;").arg(ui::colors::NEGATIVE));
    layout->addWidget(top_loser_);

    // Quick stats
    auto add_stat = [&](QLabel*& lbl, const QString& prefix) {
        auto* row = new QHBoxLayout;
        auto* lab = new QLabel(prefix);
        lab->setStyleSheet(QString("color:%1; font-size:9px; font-weight:600;").arg(ui::colors::TEXT_TERTIARY));
        row->addWidget(lab);
        lbl = new QLabel("--");
        lbl->setAlignment(Qt::AlignRight);
        lbl->setStyleSheet(QString("color:%1; font-size:10px; font-weight:700;").arg(ui::colors::TEXT_SECONDARY));
        row->addWidget(lbl);
        layout->addLayout(row);
    };

    add_stat(stat_holdings_, "HOLDINGS");
    add_stat(stat_conc_, "CONC. TOP3");
    add_stat(stat_vol_, "VOL 30D");
}

void PortfolioHeatmap::set_holdings(const QVector<portfolio::HoldingWithQuote>& holdings) {
    holdings_ = holdings;
    rebuild_blocks();
    update_detail();
    update_top_movers();
    stat_holdings_->setText(QString::number(holdings.size()));
}

void PortfolioHeatmap::set_metrics(const portfolio::ComputedMetrics& metrics) {
    metrics_ = metrics;
    update_risk_gauge();

    stat_conc_->setText(metrics.concentration_top3.has_value()
                            ? QString("%1%").arg(QString::number(*metrics.concentration_top3, 'f', 1))
                            : "--");
    stat_vol_->setText(metrics.volatility.has_value() ? QString("%1%").arg(QString::number(*metrics.volatility, 'f', 1))
                                                      : "--");
}

void PortfolioHeatmap::set_selected_symbol(const QString& symbol) {
    selected_symbol_ = symbol;
    rebuild_blocks();
    update_detail();
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
        QString border_style = selected ? QString("border:2px solid %1;").arg(ui::colors::AMBER)
                                        : QString("border:1px solid %1;").arg(ui::colors::BORDER_DIM);

        block->setStyleSheet(QString("QPushButton { background:rgb(%1,%2,%3); %4"
                                     "  text-align:left; padding:4px 6px; border-radius:2px;"
                                     "  color:%6; font-size:10px; font-weight:700; }"
                                     "QPushButton:hover { border-color:%5; }")
                                 .arg(bg.red())
                                 .arg(bg.green())
                                 .arg(bg.blue())
                                 .arg(border_style, ui::colors::AMBER, ui::colors::TEXT_PRIMARY));

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
            update_detail();
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

void PortfolioHeatmap::update_detail() {
    const portfolio::HoldingWithQuote* found = nullptr;
    for (const auto& h : holdings_) {
        if (h.symbol == selected_symbol_) {
            found = &h;
            break;
        }
    }

    if (!found) {
        detail_panel_->setVisible(false);
        return;
    }

    detail_panel_->setVisible(true);
    const auto& h = *found;
    auto fmt = [](double v, int dp = 2) { return QString::number(v, 'f', dp); };
    auto color = [](double v) -> const char* { return v >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE; };

    detail_symbol_->setText(h.symbol);
    detail_price_->setText(fmt(h.current_price));
    detail_change_->setText(QString("%1%2%").arg(h.day_change_percent >= 0 ? "+" : "").arg(fmt(h.day_change_percent)));
    detail_change_->setStyleSheet(
        QString("color:%1; font-size:9px; font-weight:600;").arg(color(h.day_change_percent)));
    detail_qty_->setText(fmt(h.quantity, h.quantity == std::floor(h.quantity) ? 0 : 2));
    detail_cost_->setText(fmt(h.avg_buy_price));
    detail_mv_->setText(fmt(h.market_value));
    detail_mv_->setStyleSheet(QString("color:%1; font-size:9px; font-weight:600;").arg(ui::colors::WARNING));
    detail_pnl_->setText(QString("%1%2").arg(h.unrealized_pnl >= 0 ? "+" : "").arg(fmt(h.unrealized_pnl)));
    detail_pnl_->setStyleSheet(QString("color:%1; font-size:9px; font-weight:600;").arg(color(h.unrealized_pnl)));
    detail_pnl_pct_->setText(
        QString("%1%2%").arg(h.unrealized_pnl_percent >= 0 ? "+" : "").arg(fmt(h.unrealized_pnl_percent)));
    detail_pnl_pct_->setStyleSheet(
        QString("color:%1; font-size:9px; font-weight:600;").arg(color(h.unrealized_pnl_percent)));
    detail_weight_->setText(QString("%1%").arg(fmt(h.weight, 1)));
}

void PortfolioHeatmap::update_risk_gauge() {
    double rs = metrics_.risk_score.value_or(0);
    const char* rs_color = rs < 30 ? ui::colors::POSITIVE : rs < 60 ? ui::colors::WARNING : ui::colors::NEGATIVE;

    // Use a colored inner bar proportional to score
    double pct = std::min(rs / 100.0, 1.0);
    int bar_width = static_cast<int>(pct * risk_bar_->width());
    risk_bar_->setStyleSheet(
        QString("background: qlineargradient(x1:0, x2:1, stop:0 %1, stop:%2 %1, stop:%3 transparent);")
            .arg(rs_color)
            .arg(pct)
            .arg(std::min(pct + 0.01, 1.0)));

    risk_value_->setText(QString::number(rs, 'f', 0));
    risk_value_->setStyleSheet(QString("color:%1; font-size:10px; font-weight:700;").arg(rs_color));
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
        QString("background:%1; border-right:1px solid %2;").arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM));

    detail_panel_->setStyleSheet(
        QString("background:%1; border:1px solid %2; padding:4px;").arg(ui::colors::BG_RAISED, ui::colors::BORDER_DIM));

    // Rebuild blocks picks up new theme colors for borders/text
    rebuild_blocks();
}

} // namespace fincept::screens
