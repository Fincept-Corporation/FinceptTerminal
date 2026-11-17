#include "screens/dashboard/widgets/PortfolioSummaryWidget.h"

#include "screens/portfolio/PortfolioFormat.h"
#include "services/portfolio/PortfolioService.h"
#include "ui/charts/DonutChart.h"
#include "ui/formatting/DateFormat.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/tables/BarCellDelegate.h"
#include "ui/tables/DataTable.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/StatTile.h"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMap>
#include <QPushButton>
#include <QTimer>

#include <algorithm>
#include <cmath>

namespace fincept::screens::widgets {

namespace {

namespace ps_fmt = fincept::ui::formatting;
using fincept::services::PortfolioService;

constexpr int kPsValueRole = Qt::UserRole + 9;
enum PsCol { PsSym = 0, PsWeight, PsValue, PsDay, PsPnl, PsCount };

ui::StatTile::Tone ps_tone(double v) {
    if (!std::isfinite(v) || v == 0)
        return ui::StatTile::Tone::Neutral;
    return v > 0 ? ui::StatTile::Tone::Positive : ui::StatTile::Tone::Negative;
}

} // namespace

PortfolioSummaryWidget::PortfolioSummaryWidget(const QJsonObject& cfg, QWidget* parent)
    : PortfolioTileBase(tr("PORTFOLIO SUMMARY"), parent, ui::colors::AMBER()) {
    kpis_ = new ui::KpiStrip(this);
    kpis_->set_spacing(4);
    kpis_->add_tile("value", tr("VALUE"))->set_compact(true);
    kpis_->add_tile("day", tr("DAY P&L"))->set_compact(true);
    kpis_->add_tile("pnl", tr("UNREALISED"))->set_compact(true);
    kpis_->add_tile("pos", tr("POSITIONS"))->set_compact(true);
    content_layout()->addWidget(kpis_);

    auto* body = new QWidget(this);
    auto* bl = new QHBoxLayout(body);
    bl->setContentsMargins(0, 0, 0, 0);
    bl->setSpacing(4);

    auto* left = new QWidget(body);
    auto* ll = new QVBoxLayout(left);
    ll->setContentsMargins(0, 0, 0, 0);
    ll->setSpacing(2);
    auto* modes = new QWidget(left);
    modes->setObjectName("psModes");
    auto* ml = new QHBoxLayout(modes);
    ml->setContentsMargins(4, 0, 4, 0);
    ml->setSpacing(2);
    alloc_mode_ = new QButtonGroup(this);
    const QString mode_labels[] = {tr("BY SECTOR"), tr("BY HOLDING")};
    for (int i = 0; i < 2; ++i) {
        auto* b = new QPushButton(mode_labels[i], modes);
        b->setObjectName("psMode");
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        alloc_mode_->addButton(b, i);
        ml->addWidget(b);
    }
    ml->addStretch();
    ll->addWidget(modes);
    donut_ = new ui::DonutChart(left);
    donut_->set_max_slices(8);
    donut_->set_other_label(tr("Other"));
    ll->addWidget(donut_, 1);
    bl->addWidget(left, 2);

    table_ = new ui::DataTable(body);
    table_->set_headers({tr("HOLDING"), tr("WEIGHT"), tr("VALUE"), tr("DAY"), tr("P&L")});
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->setColumnWidth(PsSym, 90);
    auto* wbar = new ui::BarCellDelegate(ui::BarCellDelegate::Mode::Bar, table_);
    wbar->set_value_role(kPsValueRole);
    table_->setItemDelegateForColumn(PsWeight, wbar);
    table_->setToolTip(tr("Double-click a holding to open it in Equity Research"));
    connect(table_, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        if (auto* it = table_->item(row, PsSym))
            open_symbol(it->text());
    });
    install_symbol_menu(table_, PsSym);
    bl->addWidget(table_, 3);
    content_layout()->addWidget(body, 1);

    footer_ = new QLabel(this);
    footer_->setWordWrap(true);
    content_layout()->addWidget(footer_);

    connect(alloc_mode_, &QButtonGroup::idClicked, this, [this](int id) {
        by_holding_ = id == 1;
        render();
        emit config_changed(config());
    });

    auto& svc = PortfolioService::instance();
    connect(&svc, &PortfolioService::summary_loaded, this, [this](const portfolio::PortfolioSummary& s) {
        if (!has_portfolio() || s.portfolio.id != portfolio_id())
            return;
        summary_ = s;
        have_summary_ = true;
        set_loading(false);
        render();
    });
    connect(&svc, &PortfolioService::summary_error, this, [this](const QString& pid, const QString& err) {
        if (pid != portfolio_id())
            return;
        if (!have_summary_)
            set_error(err);
        else
            footer_->setText(tr("Last refresh failed: %1").arg(err));
    });
    for (auto sig : {&PortfolioService::asset_added, &PortfolioService::asset_sold, &PortfolioService::ledger_changed})
        connect(&svc, sig, this, [this](const QString& pid) {
            if (pid == portfolio_id() && isVisible())
                PortfolioService::instance().refresh_summary(pid);
        });
    refresh_timer_ = new QTimer(this);
    refresh_timer_->setInterval(60'000);
    connect(refresh_timer_, &QTimer::timeout, this, [this]() {
        if (has_portfolio())
            PortfolioService::instance().load_summary(portfolio_id());
    });
    connect(this, &BaseWidget::refresh_requested, this, [this]() {
        if (has_portfolio())
            PortfolioService::instance().refresh_summary(portfolio_id());
    });

    // Settings > Currency & Number Format: re-format the held summary, no fetch.
    ps_fmt::on_format_changed(this, [this]() { render(); });

    on_theme_changed();
    set_loading(true);
    apply_config(cfg);
}

QJsonObject PortfolioSummaryWidget::config() const {
    QJsonObject o = PortfolioTileBase::config();
    o.insert("alloc", by_holding_ ? "holding" : "sector");
    return o;
}

void PortfolioSummaryWidget::apply_config(const QJsonObject& cfg) {
    by_holding_ = cfg.value("alloc").toString() == QLatin1String("holding");
    if (auto* b = alloc_mode_->button(by_holding_ ? 1 : 0))
        b->setChecked(true);
    PortfolioTileBase::apply_config(cfg);
}

void PortfolioSummaryWidget::on_portfolio_selected() {
    have_summary_ = false;
    summary_ = portfolio::PortfolioSummary{};
    donut_->clear();
    table_->setRowCount(0);
    for (const char* k : {"value", "day", "pnl", "pos"})
        if (auto* t = kpis_->tile(QString::fromLatin1(k))) {
            t->set_value(ps_fmt::placeholder());
            t->clear_delta();
        }
    if (!has_portfolio()) {
        set_loading(false);
        footer_->setText(tr("No portfolio yet — create one in the Portfolio screen"));
        return;
    }
    set_loading(true);
    if (isVisible())
        PortfolioService::instance().load_summary(portfolio_id());
}

void PortfolioSummaryWidget::showEvent(QShowEvent* e) {
    PortfolioTileBase::showEvent(e);
    refresh_timer_->start();
    if (has_portfolio())
        PortfolioService::instance().load_summary(portfolio_id());
}

void PortfolioSummaryWidget::hideEvent(QHideEvent* e) {
    PortfolioTileBase::hideEvent(e);
    refresh_timer_->stop();
}

void PortfolioSummaryWidget::render() {
    if (!have_summary_)
        return;
    const auto& s = summary_;
    const QString ccy = s.base_currency.isEmpty() ? base_currency() : s.base_currency;
    const auto compact = ui::formatting::Compact::FromSettings;
    // "≈" when a holding was converted at a stale FX rate (the rate's age in the tooltip).
    const QString approx = pfmt::approx_prefix(s);
    const QString stale_fx = pfmt::fx_stale_line(s);

    if (auto* t = kpis_->tile("value")) {
        t->set_value(approx + ps_fmt::money(s.total_market_value, ccy, compact));
        t->set_sub_label(tr("cost %1").arg(ps_fmt::money(s.total_cost_basis, ccy, compact)));
        t->set_tooltip_text(tr("Market value of the valued holdings, in %1").arg(ccy) +
                            (stale_fx.isEmpty() ? QString() : QLatin1Char('\n') + stale_fx));
        if (!s.complete)
            t->set_state(ui::StatTile::State::Stale, tr("Totals cover %1 of %2 positions")
                                                         .arg(s.valued_positions)
                                                         .arg(s.total_positions));
        else
            t->set_state(ui::StatTile::State::Normal);
    }
    if (auto* t = kpis_->tile("day")) {
        t->set_value(approx + ps_fmt::signed_money(s.total_day_change, ccy, compact));
        t->set_delta(s.total_day_change_percent);
        t->set_tone(ps_tone(s.total_day_change));
        t->set_tooltip_text(tr("Change of the valued holdings since the previous close, in %1").arg(ccy) +
                            (stale_fx.isEmpty() ? QString() : QLatin1Char('\n') + stale_fx));
    }
    if (auto* t = kpis_->tile("pnl")) {
        t->set_value(approx + ps_fmt::signed_money(s.total_unrealized_pnl, ccy, compact));
        t->set_delta(s.total_unrealized_pnl_percent);
        t->set_tone(ps_tone(s.total_unrealized_pnl));
        QString tip = tr("Realised %1 · dividends %2")
                          .arg(ps_fmt::signed_money(s.total_realized_pnl, ccy, compact),
                               ps_fmt::money(s.total_dividends, ccy, compact));
        // Price / FX split of the unrealised P&L (Settings > Currency & Number Format,
        // shown only when something is held in a foreign currency) — as the Portfolio screen.
        QString split;
        if (s.valued_positions > 0 && pfmt::fx_split_shown(s)) {
            if (std::isfinite(s.total_price_pnl) && std::isfinite(s.total_fx_pnl)) {
                split = tr("Price %1 · FX %2")
                            .arg(ps_fmt::signed_money(s.total_price_pnl, ccy, compact),
                                 ps_fmt::signed_money(s.total_fx_pnl, ccy, compact));
                tip += QLatin1Char('\n') + tr("Unrealised P&L split into the move of each price in its own currency "
                                              "and the move of that currency against %1 since purchase.")
                                               .arg(ccy);
            } else {
                split = tr("FX split unavailable");
                tip += QLatin1Char('\n') + pfmt::fx_pnl_unknown_reason();
            }
        }
        t->set_sub_label(split);
        if (!stale_fx.isEmpty())
            tip += QLatin1Char('\n') + stale_fx;
        t->set_tooltip_text(tip);
    }
    if (auto* t = kpis_->tile("pos")) {
        t->set_value(QString::number(s.total_positions));
        t->set_sub_label(tr("%1 up · %2 down").arg(s.gainers).arg(s.losers));
    }

    // Allocation: by sector (unresolved sectors grouped) or by holding.
    QVector<ui::DonutSlice> slices;
    if (by_holding_) {
        QVector<const portfolio::HoldingWithQuote*> hs;
        for (const auto& h : s.holdings)
            if (std::isfinite(h.market_value) && h.market_value > 0)
                hs.append(&h);
        std::sort(hs.begin(), hs.end(), [](auto* a, auto* b) { return a->symbol < b->symbol; });
        for (const auto* h : hs)
            slices.append({h->symbol, h->symbol, h->market_value, {}});
    } else {
        QMap<QString, double> by_sector; // ordered by name → stable colours
        for (const auto& h : s.holdings)
            if (std::isfinite(h.market_value) && h.market_value > 0)
                by_sector[h.sector.isEmpty() ? tr("Unclassified") : h.sector] += h.market_value;
        for (auto it = by_sector.cbegin(); it != by_sector.cend(); ++it)
            slices.append({it.key(), it.key(), it.value(), {}});
    }
    donut_->set_value_formatter([ccy](double v) { return ps_fmt::money(v, ccy, ui::formatting::Compact::FromSettings); });
    donut_->set_center_text(by_holding_ ? tr("HOLDINGS") : tr("SECTORS"),
                            ps_fmt::money(s.total_market_value, ccy, compact));
    donut_->set_slices(slices);

    // Largest holdings
    QVector<const portfolio::HoldingWithQuote*> rows;
    for (const auto& h : s.holdings)
        rows.append(&h);
    std::sort(rows.begin(), rows.end(), [](auto* a, auto* b) {
        const double x = std::isfinite(a->market_value) ? a->market_value : -1;
        const double y = std::isfinite(b->market_value) ? b->market_value : -1;
        return x > y;
    });
    table_->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        const auto& h = *rows[i];
        auto put = [&](int c, const QString& text, const QString& color = {}) {
            auto* it = table_->item(i, c);
            if (!it) {
                it = new QTableWidgetItem;
                if (c != PsSym)
                    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                table_->setItem(i, c, it);
            }
            it->setText(text);
            it->setForeground(QColor(color.isEmpty() ? QString(ui::colors::TEXT_PRIMARY()) : color));
            return it;
        };
        put(PsSym, h.symbol)->setToolTip(h.name.isEmpty() ? h.symbol : h.symbol + QStringLiteral(" — ") + h.name);
        auto* w = put(PsWeight, std::isfinite(h.weight) ? ps_fmt::pct(h.weight, 1, false) : ps_fmt::placeholder());
        w->setData(kPsValueRole, std::isfinite(h.weight) ? QVariant(h.weight) : QVariant());
        put(PsValue, std::isfinite(h.market_value) ? ps_fmt::money(h.market_value, ccy, compact) : ps_fmt::placeholder());
        auto tone = [](double v) -> QString {
            if (!std::isfinite(v) || v == 0)
                return ui::colors::TEXT_SECONDARY();
            return v > 0 ? ui::colors::POSITIVE() : ui::colors::NEGATIVE();
        };
        put(PsDay, ps_fmt::pct(h.day_change_percent), tone(h.day_change_percent));
        put(PsPnl, ps_fmt::pct(h.unrealized_pnl_percent), tone(h.unrealized_pnl_percent));
    }

    QStringList foot{approx.isEmpty() ? tr("All totals in %1").arg(ccy) : tr("All totals in %1 (≈ stale FX rate)").arg(ccy)};
    if (!s.as_of.isEmpty())
        foot << tr("prices of %1").arg(ps_fmt::calendar_date(QDate::fromString(s.as_of, Qt::ISODate)));
    if (!s.unpriced.isEmpty())
        foot << tr("no quote: %1").arg(s.unpriced.join(QStringLiteral(", ")));
    if (!s.fx_missing.isEmpty())
        foot << tr("no FX rate yet: %1").arg(s.fx_missing.join(QStringLiteral(", ")));
    footer_->setText(foot.join(QStringLiteral(" · ")));
}

void PortfolioSummaryWidget::on_theme_changed() {
    if (auto* m = findChild<QWidget*>(QStringLiteral("psModes")))
        set_css_cached(m, QString("QWidget#psModes{background:transparent;}"
                                 "QPushButton#psMode{background:%1;color:%2;border:1px solid %3;padding:0 6px;"
                                 "font-size:%4px;font-weight:bold;}"
                                 "QPushButton#psMode:checked{background:%5;color:%6;border-color:%6;}")
                             .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_DIM())
                             .arg(ui::fonts::px(-5))
                             .arg(ui::colors::ACCENT_BG(), ui::colors::AMBER()));
    set_css_cached(footer_, QString("color:%1;font-size:%2px;padding:0 6px 2px 6px;background:transparent;")
                               .arg(ui::colors::TEXT_TERTIARY())
                               .arg(ui::fonts::px(-4)));
    render(); // cell colours are baked into the items
}

void PortfolioSummaryWidget::retranslateUi() {
    PortfolioTileBase::retranslateUi();
    set_title(tr("PORTFOLIO SUMMARY"));
    table_->set_headers({tr("HOLDING"), tr("WEIGHT"), tr("VALUE"), tr("DAY"), tr("P&L")});
    render();
}

} // namespace fincept::screens::widgets
