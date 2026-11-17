// src/screens/portfolio/PortfolioHeatmap.cpp
#include "screens/portfolio/PortfolioHeatmap.h"

#include "screens/portfolio/PortfolioFormat.h"
#include "ui/charts/Treemap.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/PanelHeader.h"
#include "ui/widgets/ThemeHelpers.h"

#include <QEvent>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens {

namespace {

/// 90th percentile of |v| (at least `floor`), the colour clamp of the map.
double phm_robust_clamp(QVector<double> v, double floor) {
    for (double& x : v)
        x = std::abs(x);
    v.erase(std::remove_if(v.begin(), v.end(), [](double x) { return !std::isfinite(x); }), v.end());
    if (v.isEmpty())
        return floor;
    std::sort(v.begin(), v.end());
    const double q = v[std::min<qsizetype>(v.size() - 1, static_cast<qsizetype>(std::floor(0.9 * (v.size() - 1) + 0.5)))];
    return std::max(floor, q);
}

} // namespace

PortfolioHeatmap::PortfolioHeatmap(QWidget* parent) : QWidget(parent) {
    setMinimumWidth(ui::metrics::width(220));
    build_ui();
    ui::formatting::on_format_changed(this, [this]() { rebuild(); }); // re-format the held holdings
}

void PortfolioHeatmap::build_ui() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto header = ui::make_panel_header(tr("HOLDINGS MAP"), this);
    title_label_ = header.title_label;
    auto* slot = header.controls_slot->layout();
    auto make = [this, slot](const QString& text, const QString& tip, portfolio::HeatmapMode m) {
        auto* b = new QPushButton(text, this);
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setToolTip(tip);
        b->setObjectName("phmMode");
        connect(b, &QPushButton::clicked, this, [this, m]() {
            mode_ = m;
            day_btn_->setChecked(m == portfolio::HeatmapMode::DayChange);
            pnl_btn_->setChecked(m == portfolio::HeatmapMode::Pnl);
            weight_btn_->setChecked(m == portfolio::HeatmapMode::Weight);
            rebuild();
            emit mode_changed(m);
        });
        slot->addWidget(b);
        return b;
    };
    day_btn_ = make(tr("DAY"), tr("Colour by today's price change"), portfolio::HeatmapMode::DayChange);
    // "&" in a button text is a mnemonic marker: "P&L" rendered as "PL" with the L underlined.
    pnl_btn_ = make(tr("P&L").replace(QLatin1Char('&'), QLatin1String("&&")), tr("Colour by unrealised P&L %"),
                    portfolio::HeatmapMode::Pnl);
    weight_btn_ = make(tr("SIZE"), tr("Size only (weight), no colour scale"), portfolio::HeatmapMode::Weight);
    day_btn_->setChecked(true);
    layout->addWidget(header.header);

    map_ = new ui::Treemap(this);
    map_->set_grouped(true);
    connect(map_, &ui::Treemap::item_clicked, this, [this](const QString& id) {
        selected_symbol_ = id;
        emit symbol_selected(id);
    });
    layout->addWidget(map_, 1);

    footer_ = new QLabel(this);
    footer_->setObjectName("phmFooter");
    footer_->setWordWrap(true);
    footer_->setContentsMargins(8, 3, 8, 3);
    layout->addWidget(footer_);
    apply_styles();
}

void PortfolioHeatmap::apply_styles() {
    // Also runs on every appearance change (PortfolioScreen::refresh_theme): set only a
    // changed sheet (each set re-polishes the widget).
    const auto set_css = [](QWidget* w, const QString& css) {
        if (w && w->styleSheet() != css)
            w->setStyleSheet(css);
    };
    const QString qss = QString("QPushButton#phmMode { background:transparent; color:%1; border:1px solid %2;"
                                "  padding:1px 6px; font-size:%3px; font-weight:700; }"
                                "QPushButton#phmMode:checked { color:%4; border-color:%4; background:%5; }")
                            .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_DIM(), ui::fonts::px(-4),
                                 ui::colors::AMBER(), ui::colors::ACCENT_BG());
    for (auto* b : {day_btn_, pnl_btn_, weight_btn_})
        set_css(b, qss);
    set_css(footer_, QString("#phmFooter { background:%1; color:%2; font-size:%3px; border-top:1px solid %4; }")
                         .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_SECONDARY(), ui::fonts::px(-4),
                              ui::colors::BORDER_DIM()));
}

void PortfolioHeatmap::set_holdings(const QVector<portfolio::HoldingWithQuote>& holdings) {
    holdings_ = holdings;
    rebuild();
}

void PortfolioHeatmap::set_metrics(const portfolio::ComputedMetrics&) {
    // Risk figures live in the stats ribbon; the map shows positions only.
}

void PortfolioHeatmap::set_selected_symbol(const QString& symbol) {
    selected_symbol_ = symbol;
}

void PortfolioHeatmap::set_currency(const QString& currency) {
    if (currency.isEmpty() || currency == currency_)
        return;
    currency_ = currency;
    rebuild();
}

void PortfolioHeatmap::rebuild() {
    QVector<ui::TreemapItem> items;
    QVector<double> colour_values;
    QStringList not_shown;
    const QString ccy = currency_;
    const portfolio::HoldingWithQuote* best = nullptr;
    const portfolio::HoldingWithQuote* worst = nullptr;
    for (const auto& h : holdings_) {
        if (!h.valued() || !(h.market_value > 0)) {
            not_shown << h.symbol;
            continue;
        }
        ui::TreemapItem it;
        it.id = h.symbol;
        it.label = h.symbol;
        it.sublabel = h.name;
        it.group = h.sector.isEmpty() ? tr("Unclassified") : h.sector;
        it.weight = h.market_value;
        it.value = mode_ == portfolio::HeatmapMode::DayChange ? h.day_change_percent
                   : mode_ == portfolio::HeatmapMode::Pnl      ? h.unrealized_pnl_percent
                                                              : portfolio::kAbsent;
        colour_values.append(it.value);
        items.append(it);
        if (std::isfinite(h.day_change_percent)) {
            if (!best || h.day_change_percent > best->day_change_percent)
                best = &h;
            if (!worst || h.day_change_percent < worst->day_change_percent)
                worst = &h;
        }
    }
    map_->set_color_clamp(mode_ == portfolio::HeatmapMode::Pnl ? phm_robust_clamp(colour_values, 5.0)
                                                               : phm_robust_clamp(colour_values, 1.0));
    map_->set_show_legend(mode_ != portfolio::HeatmapMode::Weight);
    map_->set_value_formatter([](double v) { return pfmt::pct(v, 2); });
    map_->set_weight_formatter([ccy](double v) { return pfmt::money_c(v, ccy); });
    QHash<QString, const portfolio::HoldingWithQuote*> by_sym;
    for (const auto& h : holdings_)
        by_sym.insert(h.symbol, &h);
    map_->set_tooltip_provider([by_sym, ccy](const ui::TreemapItem& it) {
        const auto* h = by_sym.value(it.id, nullptr);
        if (!h)
            return QString();
        QVector<QPair<QString, QString>> rows = {
            {PortfolioHeatmap::tr("Value"), pfmt::money(h->market_value, ccy)},
            {PortfolioHeatmap::tr("Weight"), pfmt::upct(h->weight, 1)},
            {PortfolioHeatmap::tr("Today"), QStringLiteral("%1 (%2)").arg(pfmt::pct(h->day_change_percent),
                                                                          pfmt::smoney(h->day_change_value, ccy))},
            {PortfolioHeatmap::tr("Unrealised"), QStringLiteral("%1 (%2)").arg(pfmt::pct(h->unrealized_pnl_percent),
                                                                               pfmt::smoney(h->unrealized_pnl, ccy))},
            {PortfolioHeatmap::tr("Sector"), h->sector.isEmpty() ? pfmt::dash() : h->sector},
        };
        return ui::chart_style::tooltip_html(h->name.isEmpty() ? h->symbol : QStringLiteral("%1 — %2").arg(h->symbol, h->name),
                                             rows);
    });
    map_->set_items(items);
    map_->set_state(items.isEmpty() ? ui::PaintedChart::Empty : ui::PaintedChart::Ready,
                    holdings_.isEmpty() ? tr("No positions yet.") : tr("No position could be valued yet."));

    QStringList foot;
    if (best && worst && best != worst)
        foot << tr("▲ %1 %2  ▼ %3 %4")
                    .arg(best->symbol, pfmt::pct(best->day_change_percent), worst->symbol,
                         pfmt::pct(worst->day_change_percent));
    if (!not_shown.isEmpty())
        foot << tr("Not shown (not valued): %1").arg(not_shown.join(QStringLiteral(", ")));
    footer_->setText(foot.join(QStringLiteral("\n")));
    footer_->setVisible(!foot.isEmpty());
}

void PortfolioHeatmap::refresh_theme() {
    apply_styles();
    rebuild();
}

void PortfolioHeatmap::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void PortfolioHeatmap::retranslateUi() {
    if (title_label_)
        title_label_->setText(tr("HOLDINGS MAP"));
    day_btn_->setText(tr("DAY"));
    pnl_btn_->setText(tr("P&L").replace(QLatin1Char('&'), QLatin1String("&&")));
    weight_btn_->setText(tr("SIZE"));
    rebuild();
}

} // namespace fincept::screens
