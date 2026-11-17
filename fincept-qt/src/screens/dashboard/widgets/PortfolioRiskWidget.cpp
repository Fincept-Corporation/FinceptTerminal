#include "screens/dashboard/widgets/PortfolioRiskWidget.h"

#include "core/events/EventBus.h"
#include "services/portfolio/PortfolioService.h"
#include "ui/formatting/DateFormat.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/tables/BarCellDelegate.h"
#include "ui/tables/DataTable.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/StatTile.h"

#include <QHeaderView>
#include <QLabel>

#include <algorithm>
#include <cmath>
#include <functional>
#include <utility>

namespace fincept::screens::widgets {

namespace {

namespace pr_fmt = fincept::ui::formatting;
using fincept::services::PortfolioService;

constexpr int kPrValueRole = Qt::UserRole + 7;
enum PrCol { PrSym = 0, PrWeight, PrVol, PrBeta, PrContrib, PrCount };

void pr_set(ui::StatTile* t, const std::optional<double>& v, const std::function<QString(double)>& fmt,
            const QString& tip) {
    if (!t)
        return;
    t->set_tooltip_text(tip);
    t->set_value(v && std::isfinite(*v) ? fmt(*v) : pr_fmt::placeholder());
}

} // namespace

PortfolioRiskWidget::PortfolioRiskWidget(const QJsonObject& cfg, QWidget* parent)
    : PortfolioTileBase(tr("PORTFOLIO RISK"), parent, ui::colors::WARNING()) {
    kpis_ = new ui::KpiStrip(this);
    kpis_->set_spacing(4);
    for (const auto& [key, label] : {std::pair{"vol", tr("VOLATILITY")}, std::pair{"beta", tr("BETA")},
                                     std::pair{"var", tr("VaR 95% 1D")}, std::pair{"cvar", tr("ES 95% 1D")},
                                     std::pair{"te", tr("TRACKING ERR")}, std::pair{"div", tr("DIVERSIFICATION")}})
        kpis_->add_tile(QString::fromLatin1(key), label)->set_compact(true);
    content_layout()->addWidget(kpis_);

    table_ = new ui::DataTable(this);
    table_->set_headers({tr("HOLDING"), tr("WEIGHT"), tr("VOL"), tr("BETA"), tr("RISK SHARE")});
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->setColumnWidth(PrSym, 110);
    auto* bar = new ui::BarCellDelegate(ui::BarCellDelegate::Mode::Bar, table_);
    bar->set_value_role(kPrValueRole);
    bar->set_range(0.0, 1.0);
    bar->set_bar_color(QColor(ui::colors::WARNING()));
    table_->setItemDelegateForColumn(PrContrib, bar);
    table_->setToolTip(tr("Share of the portfolio's ex-ante variance each holding contributes (sums to 100 %)"));
    content_layout()->addWidget(table_, 1);
    install_symbol_menu(table_, PrSym);

    footer_ = new QLabel(this);
    footer_->setWordWrap(true);
    content_layout()->addWidget(footer_);

    auto& svc = PortfolioService::instance();
    connect(&svc, &PortfolioService::summary_loaded, this, [this](const portfolio::PortfolioSummary& s) {
        if (!has_portfolio() || s.portfolio.id != portfolio_id())
            return;
        summary_ = s;
        have_summary_ = true;
        if (!analytics_requested_ && isVisible()) {
            analytics_requested_ = true;
            if (s.holdings.isEmpty()) {
                // Not requested: the next summary (a trade added) asks for analytics.
                analytics_requested_ = false;
                set_waiting(tr("This portfolio has no holdings"));
                // Nothing to measure: say so over the empty KPI row / table.
                show_empty_state(ui::EmptyState::no_data_spec(
                                     tr("No holdings yet"),
                                     tr("Risk figures need at least one position in %1. Add a trade or import "
                                        "positions in the Portfolio screen.")
                                         .arg(portfolio().name),
                                     tr("OPEN PORTFOLIO")),
                                 []() {
                                     EventBus::instance().publish(
                                         "nav.switch_screen",
                                         QVariantMap{{"screen_id", QStringLiteral("portfolio")}, {"exclusive", true}});
                                 });
                return;
            }
            PortfolioService::instance().fetch_market_analytics(s);
        }
    });
    connect(&svc, &PortfolioService::summary_error, this, [this](const QString& pid, const QString& err) {
        if (pid == portfolio_id())
            set_waiting(err);
    });
    connect(&svc, &PortfolioService::market_analytics_loaded, this,
            [this](const QString& pid, bool ok, const QString& err) {
                if (pid != portfolio_id())
                    return;
                const auto a = PortfolioService::instance().last_market_analytics(pid);
                if (!ok && a.portfolio_id.isEmpty()) {
                    set_waiting(err.isEmpty() ? tr("Risk analytics failed") : err);
                    return;
                }
                render(a);
                if (!ok)
                    footer_->setText(footer_->text() + QStringLiteral(" · ") +
                                     tr("last refresh failed: %1").arg(err));
            });
    connect(this, &BaseWidget::refresh_requested, this, [this]() {
        if (!has_portfolio())
            return;
        if (have_summary_)
            PortfolioService::instance().fetch_market_analytics(summary_, /*force=*/true);
        else
            PortfolioService::instance().load_summary(portfolio_id());
    });

    // Settings > Currency & Number Format: re-format the service's kept run, no fetch.
    pr_fmt::on_format_changed(this, [this]() {
        if (!has_portfolio())
            return;
        const auto a = PortfolioService::instance().last_market_analytics(portfolio_id());
        if (!a.portfolio_id.isEmpty())
            render(a);
    });

    on_theme_changed();
    set_loading(true);
    apply_config(cfg);
}

void PortfolioRiskWidget::set_waiting(const QString& text) {
    set_loading(false);
    footer_->setText(text);
}

void PortfolioRiskWidget::on_portfolio_selected() {
    have_summary_ = false;
    analytics_requested_ = false;
    table_->setRowCount(0);
    for (const char* k : {"vol", "beta", "var", "cvar", "te", "div"})
        if (auto* t = kpis_->tile(QString::fromLatin1(k)))
            t->set_value(pr_fmt::placeholder());
    if (!has_portfolio()) {
        set_waiting(tr("No portfolio yet — create one in the Portfolio screen"));
        return;
    }
    // A cached run for this portfolio shows at once; a fresh one follows the summary.
    const auto cached = PortfolioService::instance().last_market_analytics(portfolio_id());
    if (!cached.portfolio_id.isEmpty())
        render(cached);
    if (isVisible())
        PortfolioService::instance().load_summary(portfolio_id());
}

void PortfolioRiskWidget::showEvent(QShowEvent* e) {
    PortfolioTileBase::showEvent(e);
    if (has_portfolio() && !analytics_requested_)
        PortfolioService::instance().load_summary(portfolio_id());
}

void PortfolioRiskWidget::render(const portfolio::MarketAnalytics& a) {
    set_loading(false);
    if (has_portfolio())
        hide_empty_state(); // "No holdings yet" from an earlier summary
    const QString ccy = a.base_currency.isEmpty() ? base_currency() : a.base_currency;
    const double nav = have_summary_ ? summary_.total_market_value : services::kMarketDataAbsent;
    auto pct_of = [](double f) { return pr_fmt::pct(f * 100.0, 1, false); };
    pr_set(kpis_->tile("vol"), a.ann_vol, pct_of,
           tr("Ex-ante annualised volatility at current weights, %n daily observation(s)", "", a.observations));
    pr_set(kpis_->tile("beta"), a.beta, [](double v) { return pr_fmt::number(v, 2); },
           tr("Ex-ante beta vs %1").arg(a.benchmark));
    if (auto* t = kpis_->tile("beta")) {
        if (a.benchmark_corr)
            t->set_sub_label(tr("corr %1").arg(pr_fmt::number(*a.benchmark_corr, 2)));
    }
    auto money_of = [nav, ccy, pct_of](double f) {
        return std::isfinite(nav) && nav > 0 ? pr_fmt::money(-f * nav, ccy, ui::formatting::Compact::FromSettings)
                                             : QStringLiteral("-") + pct_of(f);
    };
    pr_set(kpis_->tile("var"), a.var_95_1d, money_of,
           tr("1-day historical Value at Risk at 95 %: a loss exceeded on about 1 day in 20"));
    if (auto* t = kpis_->tile("var"); t && a.var_95_1d)
        t->set_sub_label(QStringLiteral("-") + pct_of(*a.var_95_1d));
    pr_set(kpis_->tile("cvar"), a.cvar_95_1d, money_of, tr("Expected shortfall: the average loss on the worst 5 % of days"));
    if (auto* t = kpis_->tile("cvar"); t && a.cvar_95_1d)
        t->set_sub_label(QStringLiteral("-") + pct_of(*a.cvar_95_1d));
    pr_set(kpis_->tile("te"), a.tracking_error, pct_of, tr("Ex-ante annualised tracking error vs %1").arg(a.benchmark));
    pr_set(kpis_->tile("div"), a.diversification_ratio, [](double v) { return pr_fmt::ratio(v, 2); },
           tr("Sum of weighted holding volatilities over portfolio volatility; above 1 = risk cancelled by "
              "imperfect correlation"));

    // Holdings by risk share
    struct Row {
        QString sym;
        double weight = services::kMarketDataAbsent;
        std::optional<double> vol, beta, contrib;
    };
    QVector<Row> rows;
    for (auto it = a.holdings.constBegin(); it != a.holdings.constEnd(); ++it) {
        Row r;
        r.sym = it.key();
        r.vol = it->ann_vol;
        r.beta = it->beta;
        r.contrib = it->risk_contribution;
        for (const auto& h : summary_.holdings)
            if (h.symbol == it.key())
                r.weight = h.weight;
        rows.append(r);
    }
    std::sort(rows.begin(), rows.end(), [](const Row& x, const Row& y) {
        return x.contrib.value_or(-1.0) > y.contrib.value_or(-1.0);
    });
    table_->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        const auto& r = rows[i];
        auto put = [&](int c, const QString& text, QVariant value = {}) {
            auto* it = new QTableWidgetItem(text);
            it->setForeground(QColor(ui::colors::TEXT_PRIMARY()));
            if (c != PrSym)
                it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            if (value.isValid())
                it->setData(kPrValueRole, value);
            table_->setItem(i, c, it);
        };
        put(PrSym, r.sym);
        put(PrWeight, std::isfinite(r.weight) ? pr_fmt::pct(r.weight, 1, false) : pr_fmt::placeholder());
        put(PrVol, r.vol ? pct_of(*r.vol) : pr_fmt::placeholder());
        put(PrBeta, r.beta ? pr_fmt::number(*r.beta, 2) : pr_fmt::placeholder());
        put(PrContrib, r.contrib ? pct_of(*r.contrib) : pr_fmt::placeholder(),
            r.contrib ? QVariant(std::clamp(*r.contrib, 0.0, 1.0)) : QVariant());
    }

    QStringList bits;
    bits << tr("Ex-ante, current weights") << tr("benchmark %1").arg(a.benchmark);
    if (!a.as_of.isEmpty())
        bits << tr("prices to %1").arg(pr_fmt::calendar_date(QDate::fromString(a.as_of, Qt::ISODate)));
    if (a.coverage > 0 && a.coverage < 0.999)
        bits << tr("covers %1 of the weight").arg(pr_fmt::pct(a.coverage * 100.0, 0, false));
    if (!a.missing.isEmpty())
        bits << tr("no history: %1").arg(a.missing.join(QStringLiteral(", ")));
    footer_->setText(bits.join(QStringLiteral(" · ")));
}

void PortfolioRiskWidget::on_theme_changed() {
    set_css_cached(footer_, QString("color:%1;font-size:%2px;padding:0 6px 2px 6px;background:transparent;")
                               .arg(ui::colors::TEXT_TERTIARY())
                               .arg(ui::fonts::px(-4)));
}

void PortfolioRiskWidget::retranslateUi() {
    PortfolioTileBase::retranslateUi();
    set_title(tr("PORTFOLIO RISK"));
    table_->set_headers({tr("HOLDING"), tr("WEIGHT"), tr("VOL"), tr("BETA"), tr("RISK SHARE")});
}

} // namespace fincept::screens::widgets
