#include "screens/dashboard/widgets/ScreenerWidget.h"

#include "screens/dashboard/widgets/DashboardScreens.h"
#include "screens/dashboard/widgets/RegionPin.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/tables/DataTable.h"
#include "ui/theme/Theme.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>

#include <cmath>

namespace fincept::screens::widgets {

namespace {

namespace sc_fmt = fincept::ui::formatting;
using fincept::services::ScreenerQuery;
using fincept::services::ScreenerService;

enum ScCol { ScSym = 0, ScName, ScLast, ScPct, ScCap, ScMetric, ScCount };

struct ScScreen {
    QString id;
    QString label;
    QString metric_field; ///< catalogue field shown in the last column
    QString metric_label;
    bool pct_metric = false;
};

QVector<ScScreen> sc_screens(const core::market::MarketProfile& p) {
    QVector<ScScreen> out{
        {"largest", ScreenerWidget::tr("Largest companies"), "pe", ScreenerWidget::tr("P/E"), false},
        {"value", ScreenerWidget::tr("Value — P/E 0–15"), "pe", ScreenerWidget::tr("P/E"), false},
        {"dividend", ScreenerWidget::tr("Dividend — yield ≥ 4 %"), "dividend_yield", ScreenerWidget::tr("YIELD"), true},
        {"quality", ScreenerWidget::tr("Quality — ROE 20–100 %, largest first"), "roe", ScreenerWidget::tr("ROE"), true},
        {"growth", ScreenerWidget::tr("Growth — EPS growth ≥ 25 %"), "eps_growth", ScreenerWidget::tr("EPS G"), true},
        {"low_beta", ScreenerWidget::tr("Defensive — beta < 0.8"), "beta", ScreenerWidget::tr("BETA"), false},
    };
    if (p.screener_regions == QStringList{QStringLiteral("us")}) {
        out += QVector<ScScreen>{
            {"y:undervalued_growth_stocks", ScreenerWidget::tr("Yahoo: Undervalued growth (US)"), "pe",
             ScreenerWidget::tr("P/E"), false},
            {"y:undervalued_large_caps", ScreenerWidget::tr("Yahoo: Undervalued large caps (US)"), "pe",
             ScreenerWidget::tr("P/E"), false},
            {"y:growth_technology_stocks", ScreenerWidget::tr("Yahoo: Growth technology (US)"), "pe",
             ScreenerWidget::tr("P/E"), false},
            {"y:most_shorted_stocks", ScreenerWidget::tr("Yahoo: Most shorted (US)"), "pe", ScreenerWidget::tr("P/E"),
             false},
        };
    }
    return out;
}

ScreenerQuery sc_query(const QString& id, const core::market::MarketProfile& p) {
    using dash_screens::filter;
    ScreenerQuery q;
    q.count = 50;
    if (id.startsWith(QLatin1String("y:"))) {
        q.predefined_id = id.mid(2);
        return q;
    }
    QJsonArray extra;
    q.sort_field = QStringLiteral("market_cap");
    if (id == QLatin1String("value")) {
        extra.append(filter("pe", "btwn", QJsonArray{0, 15}));
    } else if (id == QLatin1String("dividend")) {
        extra.append(filter("dividend_yield", "gte", 4));
        q.sort_field = QStringLiteral("dividend_yield");
    } else if (id == QLatin1String("quality")) {
        // Capped and sorted by size: sorted by ROE, the screen was topped by
        // companies with near-zero or negative equity (ROE in the tens of
        // thousands of percent), which says nothing about quality.
        extra.append(filter("roe", "btwn", QJsonArray{20, 100}));
    } else if (id == QLatin1String("growth")) {
        extra.append(filter("eps_growth", "gte", 25));
        q.sort_field = QStringLiteral("eps_growth");
    } else if (id == QLatin1String("low_beta")) {
        extra.append(filter("beta", "btwn", QJsonArray{0, 0.8}));
    }
    q.criteria = dash_screens::region_criteria(p, extra);
    // The region's own companies on their home venues — not APC.F / MSF.DE
    // (Apple, Microsoft quoted in Frankfurt) topping "largest Eurozone companies".
    dash_screens::primary_listings_only(p, q.criteria);
    q.sort_asc = false;
    return q;
}

} // namespace

ScreenerWidget::ScreenerWidget(const QJsonObject& cfg, QWidget* parent)
    : BaseWidget(tr("STOCK SCREENER"), parent, ui::colors::CYAN()) {
    auto* bar = new QWidget(this);
    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(6, 3, 6, 3);
    screen_combo_ = new QComboBox(bar);
    screen_combo_->setAccessibleName(tr("Screen"));
    hl->addWidget(screen_combo_, 1);
    content_layout()->addWidget(bar);
    connect(screen_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        if (filling_)
            return;
        screen_id_ = screen_combo_->currentData().toString();
        has_rows_ = false;
        run(false);
        emit config_changed(config());
    });

    table_ = new ui::DataTable(this);
    table_->set_headers({tr("SYMBOL"), tr("NAME"), tr("LAST"), tr("CHG%"), tr("MCAP"), tr("P/E")});
    table_->horizontalHeader()->setStretchLastSection(false);
    table_->horizontalHeader()->setSectionResizeMode(ScName, QHeaderView::Stretch);
    // Numbers take the width they need; NAME gets the rest (and is dropped first
    // on a narrow tile — resizeEvent), so LAST / CHG% / MCAP / metric stay visible.
    for (int c : {ScSym, ScLast, ScPct, ScCap, ScMetric})
        table_->horizontalHeader()->setSectionResizeMode(c, QHeaderView::ResizeToContents);
    table_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table_->setToolTip(tr("Double-click a row to open it in Equity Research · click a header to sort"));
    content_layout()->addWidget(table_, 1);
    connect(table_, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        if (auto* it = table_->item(row, ScSym))
            open_symbol(it->text());
    });
    install_symbol_menu(table_, ScSym);

    footer_ = new QLabel(this);
    footer_->setWordWrap(true);
    content_layout()->addWidget(footer_);

    auto& svc = ScreenerService::instance();
    connect(&svc, &ScreenerService::results_ready, this, [this](const services::ScreenerResult& r) {
        if (r.key == pending_key_)
            apply_result(r);
    });
    connect(&svc, &ScreenerService::run_failed, this, [this](const QString& key, const QString& err) {
        if (key != pending_key_)
            return;
        if (!has_rows_)
            set_error(tr("Screen failed: %1").arg(err));
        else
            footer_->setText(tr("Last refresh failed — showing earlier results: %1").arg(err));
    });
    connect(&core::market::MarketProfileStore::instance(), &core::market::MarketProfileStore::profile_changed, this,
            [this]() {
                if (!region_pin_.isEmpty())
                    return; // pinned to its own region (RegionPin.h)
                fill_screens();
                has_rows_ = false;
                if (isVisible())
                    run(false);
            });
    connect(this, &BaseWidget::refresh_requested, this, [this]() { run(true); });
    // Settings > Currency & Number Format: re-format the service's kept result, no fetch.
    sc_fmt::on_format_changed(this, [this]() {
        if (!has_rows_)
            return;
        if (const auto* last = ScreenerService::instance().last_result(pending_key_))
            apply_result(*last);
    });

    on_theme_changed();
    set_loading(true);
    apply_config(cfg);
}

void ScreenerWidget::update_title() {
    set_title(tr("SCREENER · %1")
                  .arg(core::market::market_profile_name(tile_profile(region_pin_))
                           .toUpper()));
}

void ScreenerWidget::fill_screens() {
    filling_ = true;
    screen_combo_->clear();
    for (const auto& s : sc_screens(tile_profile(region_pin_)))
        screen_combo_->addItem(s.label, s.id);
    int idx = screen_combo_->findData(screen_id_);
    if (idx < 0) {
        idx = 0;
        screen_id_ = screen_combo_->itemData(0).toString();
    }
    screen_combo_->setCurrentIndex(idx);
    filling_ = false;
    update_title();
}

QJsonObject ScreenerWidget::config() const {
    QJsonObject o{{"screen", screen_id_}};
    store_region_pin(o, region_pin_);
    return o;
}

void ScreenerWidget::apply_config(const QJsonObject& cfg) {
    region_pin_ = region_pin_from(cfg);
    const QString s = cfg.value("screen").toString();
    if (!s.isEmpty())
        screen_id_ = s;
    fill_screens();
    has_rows_ = false;
    if (isVisible())
        run(false);
}

void ScreenerWidget::resizeEvent(QResizeEvent* e) {
    BaseWidget::resizeEvent(e);
    // Narrow tile: the company name goes first, then the screen's metric.
    const int w = width();
    table_->setColumnHidden(ScName, w < 440);
    table_->setColumnHidden(ScMetric, w < 300);
}

void ScreenerWidget::showEvent(QShowEvent* e) {
    BaseWidget::showEvent(e);
    if (!has_rows_)
        run(false);
}

void ScreenerWidget::run(bool force) {
    const auto& p = tile_profile(region_pin_);
    auto& svc = ScreenerService::instance();
    pending_key_ = svc.run(sc_query(screen_id_, p), force);
    if (const auto* last = svc.last_result(pending_key_); last && !has_rows_)
        apply_result(*last);
    else if (!has_rows_)
        set_loading(true);
}

void ScreenerWidget::apply_result(const services::ScreenerResult& r) {
    has_rows_ = true;
    set_loading(false);
    const auto& p = tile_profile(region_pin_);
    ScScreen screen;
    for (const auto& s : sc_screens(p))
        if (s.id == screen_id_)
            screen = s;
    QString metric_col_id;
    for (const auto& c : r.columns)
        if (c.field == screen.metric_field)
            metric_col_id = c.id;
    table_->set_headers({tr("SYMBOL"), tr("NAME"), tr("LAST"), tr("CHG%"), tr("MCAP"),
                         screen.metric_label.isEmpty() ? tr("P/E") : screen.metric_label});

    // Market caps keep their own currency; a column that mixes currencies says which.
    sc_fmt::CurrencyMix cap_mix;
    for (const auto& row : r.rows)
        cap_mix.add(row.market_cap_currency.isEmpty() ? row.currency : row.market_cap_currency);

    table_->setSortingEnabled(false);
    table_->setRowCount(0);
    table_->setRowCount(r.rows.size());
    for (int i = 0; i < r.rows.size(); ++i) {
        const auto& row = r.rows[i];
        double metric = row.pe;
        if (screen.metric_field == QLatin1String("beta"))
            metric = row.beta;
        else if (!metric_col_id.isEmpty() && row.fields.contains(metric_col_id))
            metric = row.fields.value(metric_col_id);
        const QString metric_text = !std::isfinite(metric) ? sc_fmt::placeholder()
                                    : screen.pct_metric    ? sc_fmt::pct(metric, 1, false)
                                                           : sc_fmt::number(metric, 2);
        const QString mcap_ccy = row.market_cap_currency.isEmpty() ? row.currency : row.market_cap_currency;
        const QStringList texts{row.symbol,
                                row.name,
                                sc_fmt::price(row.price, sc_fmt::instrument_for_symbol(row.symbol, row.currency)),
                                sc_fmt::pct(row.change_pct),
                                std::isfinite(row.market_cap) ? sc_fmt::money_in_column(row.market_cap, mcap_ccy, cap_mix,
                                                                                        sc_fmt::Compact::FromSettings)
                                                              : sc_fmt::placeholder(),
                                metric_text};
        for (int c = 0; c < ScCount; ++c) {
            auto* it = new QTableWidgetItem(texts[c]);
            it->setForeground(QColor(ui::colors::TEXT_PRIMARY()));
            if (c >= ScLast)
                it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            table_->setItem(i, c, it);
        }
        if (auto* it = table_->item(i, ScName))
            it->setToolTip(QStringList{row.name, row.sector, row.industry, row.exchange_name}.join(QStringLiteral(" · ")));
        if (auto* it = table_->item(i, ScPct))
            it->setForeground(QColor(!std::isfinite(row.change_pct) || row.change_pct == 0 ? ui::colors::TEXT_SECONDARY()
                                     : row.change_pct > 0                                  ? ui::colors::POSITIVE()
                                                                                           : ui::colors::NEGATIVE()));
        table_->set_cell_numeric(i, ScLast, std::isfinite(row.price) ? row.price : -1e300);
        table_->set_cell_numeric(i, ScPct, std::isfinite(row.change_pct) ? row.change_pct : -1e300);
        table_->set_cell_numeric(i, ScCap, std::isfinite(row.market_cap) ? row.market_cap : -1e300);
        table_->set_cell_numeric(i, ScMetric, std::isfinite(metric) ? metric : -1e300);
    }
    table_->setSortingEnabled(true);

    QString foot = r.query.is_predefined() ? r.title + QStringLiteral(" · ") + r.source
                                           : dash_screens::result_footer(r, p);
    if (!r.criteria_note.isEmpty())
        footer_->setToolTip(r.criteria_note);
    footer_->setText(foot);
}

void ScreenerWidget::on_theme_changed() {
    set_css_cached(footer_, QString("color:%1;font-size:%2px;padding:0 6px 2px 6px;background:transparent;")
                               .arg(ui::colors::TEXT_TERTIARY())
                               .arg(ui::fonts::px(-4)));
}

void ScreenerWidget::retranslateUi() {
    BaseWidget::retranslateUi();
    fill_screens();
}

} // namespace fincept::screens::widgets
