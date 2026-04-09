#include "screens/dashboard/widgets/SectorHeatmapWidget.h"

#include "ui/theme/Theme.h"

#include <QFrame>
#include <QLabel>

#include <cmath>

namespace fincept::screens::widgets {

QStringList SectorHeatmapWidget::sector_symbols() {
    return {"XLK", "XLV", "XLF", "XLE", "XLY", "XLI", "XLB", "XLU", "XLRE", "XLC", "XLP", "SOXX"};
}

QMap<QString, QString> SectorHeatmapWidget::sector_labels() {
    return {
        {"XLK", "Technology"},     {"XLV", "Healthcare"},  {"XLF", "Financials"},     {"XLE", "Energy"},
        {"XLY", "Consumer Disc."}, {"XLI", "Industrials"}, {"XLB", "Materials"},      {"XLU", "Utilities"},
        {"XLRE", "Real Estate"},   {"XLC", "Comm. Svc."},  {"XLP", "Consumer Stap."}, {"SOXX", "Semis"},
    };
}

SectorHeatmapWidget::SectorHeatmapWidget(QWidget* parent) : BaseWidget("SECTOR HEATMAP", parent) {
    grid_container_ = new QWidget;
    grid_ = new QGridLayout(grid_container_);
    grid_->setContentsMargins(4, 4, 4, 4);
    grid_->setSpacing(3);

    content_layout()->addWidget(grid_container_);

    connect(this, &BaseWidget::refresh_requested, this, &SectorHeatmapWidget::refresh_data);

    apply_styles();
    set_loading(true);
    refresh_data();
}

void SectorHeatmapWidget::apply_styles() {
    // The grid cells are rebuilt dynamically in populate() using current tokens.
    // Re-populate with existing data to pick up new theme colors.
    // If no data yet, nothing to restyle.
}

void SectorHeatmapWidget::on_theme_changed() {
    apply_styles();
    // Re-populate the grid with current data so cells pick up new token values
    if (grid_->count() > 0)
        refresh_data();
}

void SectorHeatmapWidget::refresh_data() {
    set_loading(true);

    services::MarketDataService::instance().fetch_quotes(sector_symbols(),
                                                         [this](bool ok, QVector<services::QuoteData> quotes) {
                                                             set_loading(false);
                                                             if (!ok || quotes.isEmpty()) {
                                                                 if (grid_->count() == 0)
                                                                     set_error("Failed to fetch sector data.");
                                                                 return;
                                                             }
                                                             populate(quotes);
                                                         });
}

void SectorHeatmapWidget::populate(const QVector<services::QuoteData>& quotes) {
    // Clear grid
    QLayoutItem* item;
    while ((item = grid_->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    auto labels = sector_labels();
    int idx = 0;
    for (const auto& q : quotes) {
        auto* cell = new QFrame;
        cell->setMinimumSize(80, 40);

        int intensity = static_cast<int>(std::min(std::abs(q.change_pct) * 60.0, 200.0));
        QColor tint(q.change_pct >= 0 ? ui::colors::POSITIVE() : ui::colors::NEGATIVE());
        tint.setAlpha(40 + intensity);
        QString bg_color = tint.name(QColor::HexArgb);

        cell->setStyleSheet(
            QString("background: %1; border: 1px solid %2; border-radius: 2px;").arg(bg_color, ui::colors::BORDER_DIM));

        auto* cl = new QVBoxLayout(cell);
        cl->setContentsMargins(4, 2, 4, 2);
        cl->setSpacing(0);

        auto* name = new QLabel(labels.value(q.symbol, q.symbol));
        name->setStyleSheet(QString("color: %1; font-size: 9px; font-weight: bold; background: transparent;")
                                .arg(ui::colors::TEXT_PRIMARY));
        name->setAlignment(Qt::AlignCenter);
        cl->addWidget(name);

        auto* chg = new QLabel(QString("%1%2%").arg(q.change_pct >= 0 ? "+" : "").arg(q.change_pct, 0, 'f', 2));
        chg->setStyleSheet(QString("color: %1; font-size: 11px; font-weight: bold; background: transparent;")
                               .arg(q.change_pct >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE));
        chg->setAlignment(Qt::AlignCenter);
        cl->addWidget(chg);

        grid_->addWidget(cell, idx / 3, idx % 3);
        ++idx;
    }
}

} // namespace fincept::screens::widgets
