#include "screens/dashboard/widgets/SectorHeatmapWidget.h"

#include "core/market/MarketProfile.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "screens/dashboard/widgets/RegionPin.h"
#include "services/markets/MarketDataService.h"
#include "ui/charts/Treemap.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QHash>
#include <QLabel>
#include <QPushButton>

#include <algorithm>
#include <cmath>

namespace fincept::screens::widgets {

namespace {

namespace sh_fmt = fincept::ui::formatting;
using fincept::services::MarketDataService;

const char* const kShPeriods[] = {"1D", "1W", "1M", "YTD"};
constexpr double kShClamp[] = {2.0, 4.0, 8.0, 20.0};

/// Cell label for a sector. The treemap draws nothing when the first line does not
/// fit even at its smallest font, so "Consumer Discretionary" / "Communication
/// Services" left large blank cells at the default tile size. Long English names get
/// a trader's short form; the full name stays in the tooltip.
QString sh_cell_label(const QString& english) {
    static const QHash<QString, QString> kShort = {
        {QStringLiteral("Consumer Discretionary"), QStringLiteral("Cons. Discr.")},
        {QStringLiteral("Communication Services"), QStringLiteral("Comm. Svcs")},
        {QStringLiteral("Consumer Staples"), QStringLiteral("Staples")},
        {QStringLiteral("Financial Services"), QStringLiteral("Fin. Services")},
        {QStringLiteral("Industrial Goods & Services"), QStringLiteral("Industrials")},
        {QStringLiteral("Personal & Household Goods"), QStringLiteral("Personal Goods")},
        {QStringLiteral("Construction & Materials"), QStringLiteral("Construction")},
        {QStringLiteral("Automobiles & Parts"), QStringLiteral("Autos")},
        {QStringLiteral("Automobiles & Transport Equipment"), QStringLiteral("Autos & Transport")},
        {QStringLiteral("Electric Appliances & Precision"), QStringLiteral("Electronics")},
        {QStringLiteral("Raw Materials & Chemicals"), QStringLiteral("Chemicals")},
        {QStringLiteral("Steel & Nonferrous Metals"), QStringLiteral("Steel & Metals")},
        {QStringLiteral("Transportation & Logistics"), QStringLiteral("Transport")},
        {QStringLiteral("Commercial & Wholesale Trade"), QStringLiteral("Trading Cos.")},
        {QStringLiteral("Electric Power & Gas"), QStringLiteral("Power & Gas")},
        {QStringLiteral("Financials (ex Banks)"), QStringLiteral("Fin. ex Banks")},
        {QStringLiteral("Real Estate (A-REIT)"), QStringLiteral("A-REITs")},
        {QStringLiteral("Telecommunications"), QStringLiteral("Telecoms")},
        {QStringLiteral("Energy Resources"), QStringLiteral("Energy")},
        {QStringLiteral("Basic Resources"), QStringLiteral("Resources")},
        {QStringLiteral("Food & Beverage"), QStringLiteral("Food & Bev.")},
        {QStringLiteral("Travel & Leisure"), QStringLiteral("Travel")},
        {QStringLiteral("Infrastructure"), QStringLiteral("Infra")},
        {QStringLiteral("Pharmaceutical"), QStringLiteral("Pharma")},
        {QStringLiteral("Retail Trade"), QStringLiteral("Retail")},
    };
    const QString translated = core::market::profile_text(english);
    if (translated != english)
        return translated; // a translation exists: no English abbreviation
    return kShort.value(english, english);
}

} // namespace

SectorHeatmapWidget::SectorHeatmapWidget(const QJsonObject& cfg, QWidget* parent)
    : BaseWidget(tr("SECTOR MAP"), parent, ui::colors::CYAN()) {
    auto* bar = new QWidget(this);
    bar->setObjectName("shBar");
    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(6, 3, 6, 3);
    hl->setSpacing(2);
    periods_ = new QButtonGroup(this);
    for (int i = 0; i < 4; ++i) {
        auto* b = new QPushButton(QString::fromLatin1(kShPeriods[i]), bar);
        b->setObjectName("shPeriod");
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        periods_->addButton(b, i);
        hl->addWidget(b);
    }
    hl->addStretch();
    content_layout()->addWidget(bar);
    connect(periods_, &QButtonGroup::idClicked, this, [this](int id) {
        period_ = id;
        render();
        emit config_changed(config());
    });

    map_ = new ui::Treemap(this);
    map_->set_grouped(false);
    map_->set_show_legend(true);
    map_->set_tooltip_provider([this](const ui::TreemapItem& it) {
        const auto q = quotes_.constFind(it.id);
        QString name = core::market::profile_label_for(tile_profile(region_pin_), it.id);
        if (name.isEmpty())
            name = it.label;
        QString tip = QStringLiteral("<b>%1</b> (%2)<br>%3 %4")
                          .arg(name.toHtmlEscaped(), it.id.toHtmlEscaped(),
                               QString::fromLatin1(kShPeriods[period_]), sh_fmt::pct(it.value));
        if (q != quotes_.constEnd() && q->price > 0)
            tip += QStringLiteral("<br>") +
                   tr("Last %1").arg(sh_fmt::price(q->price, sh_fmt::instrument_for_symbol(it.id, q->currency)));
        const QString err = topic_error_tooltip(period_ == 0 ? MarketDataService::quote_topic(it.id)
                                                             : MarketDataService::performance_history_topic(it.id));
        if (!err.isEmpty())
            tip += QStringLiteral("<br>") + err.toHtmlEscaped();
        return tip;
    });
    connect(map_, &ui::Treemap::item_double_clicked, this, [this](const QString& id) { open_symbol(id); });
    content_layout()->addWidget(map_, 1);

    footer_ = new QLabel(this);
    footer_->setWordWrap(true);
    content_layout()->addWidget(footer_);

    connect(this, &BaseWidget::refresh_requested, this, [this]() {
        QStringList t;
        for (const auto& s : tile_profile(region_pin_).sectors)
            t << MarketDataService::quote_topic(s.symbol) << MarketDataService::performance_history_topic(s.symbol);
        datahub::DataHub::instance().request(t, true);
    });
    connect(&core::market::MarketProfileStore::instance(), &core::market::MarketProfileStore::profile_changed, this,
            [this]() {
                if (region_pin_.isEmpty())
                    reload_region();
            });

    on_theme_changed();
    set_loading(true);
    apply_config(cfg);
}

void SectorHeatmapWidget::reload_region() {
    quotes_.clear();
    stats_.clear();
    map_->clear();
    untrack_topics();
    update_title();
    if (isVisible())
        hub_subscribe_all();
}

void SectorHeatmapWidget::update_title() {
    set_title(tr("SECTOR MAP · %1").arg(core::market::market_profile_name(tile_profile(region_pin_)).toUpper()));
}

QJsonObject SectorHeatmapWidget::config() const {
    QJsonObject o{{"period", QString::fromLatin1(kShPeriods[period_])}};
    store_region_pin(o, region_pin_);
    return o;
}

void SectorHeatmapWidget::apply_config(const QJsonObject& cfg) {
    const QString pin = region_pin_from(cfg);
    const bool region_changed = pin != region_pin_;
    region_pin_ = pin;
    const QString p = cfg.value("period").toString(QStringLiteral("1D"));
    period_ = 0;
    for (int i = 0; i < 4; ++i)
        if (p == QLatin1String(kShPeriods[i]))
            period_ = i;
    if (auto* b = periods_->button(period_))
        b->setChecked(true);
    if (region_changed)
        reload_region();
    update_title();
    render();
}

void SectorHeatmapWidget::showEvent(QShowEvent* e) {
    BaseWidget::showEvent(e);
    if (!hub_active_)
        hub_subscribe_all();
}

void SectorHeatmapWidget::hideEvent(QHideEvent* e) {
    BaseWidget::hideEvent(e);
    hub_active_ = false;
}

void SectorHeatmapWidget::hub_subscribe_all() {
    auto& hub = datahub::DataHub::instance();
    hub.unsubscribe(this);
    const auto& sectors = tile_profile(region_pin_).sectors;
    QStringList topics;
    for (const auto& s : sectors)
        topics << MarketDataService::quote_topic(s.symbol);
    track_topics(topics, /*drive_loading_overlay=*/true);
    for (const auto& s : sectors) {
        const QString sym = s.symbol;
        const QString qt = MarketDataService::quote_topic(sym);
        hub.subscribe(this, qt, [this, sym, qt](const QVariant& v) {
            if (!v.canConvert<services::QuoteData>())
                return;
            quotes_.insert(sym, v.value<services::QuoteData>());
            note_topic_value(qt);
            schedule_render([this]() { render(); });
        });
        hub.subscribe(this, MarketDataService::performance_history_topic(sym), [this, sym](const QVariant& v) {
            if (!v.canConvert<QVector<services::HistoryPoint>>())
                return;
            const auto bars = v.value<QVector<services::HistoryPoint>>();
            if (bars.isEmpty())
                return;
            const auto q = quotes_.constFind(sym);
            stats_.insert(sym, services::compute_performance(bars, q != quotes_.constEnd() && q->price > 0
                                                                       ? q->price
                                                                       : services::kMarketDataAbsent));
            schedule_render([this]() { render(); });
        });
    }
    hub_active_ = true;
}

void SectorHeatmapWidget::on_topic_errors_changed() {
    schedule_render([this]() { render(); });
}

double SectorHeatmapWidget::value_for(const QString& sym) const {
    if (period_ == 0) {
        const auto q = quotes_.constFind(sym);
        return q != quotes_.constEnd() ? q->change_pct : services::kMarketDataAbsent;
    }
    const auto s = stats_.constFind(sym);
    if (s == stats_.constEnd())
        return services::kMarketDataAbsent;
    return period_ == 1 ? s->change_1w_pct : period_ == 2 ? s->change_1m_pct : s->change_ytd_pct;
}

void SectorHeatmapWidget::render() {
    const auto& p = tile_profile(region_pin_);
    bool weighted = true;
    for (const auto& s : p.sectors)
        weighted = weighted && s.weight > 0;
    QVector<ui::TreemapItem> items;
    int with_value = 0;
    for (const auto& s : p.sectors) {
        ui::TreemapItem it;
        it.id = s.symbol;
        it.label = sh_cell_label(s.label);
        it.value = value_for(s.symbol);
        // The treemap already prints the value under the label; the sublabel used to
        // repeat it ("-1.67%" twice per cell). The ETF / index ticker goes there.
        it.sublabel = s.symbol;
        it.weight = weighted ? s.weight : 1.0;
        with_value += std::isfinite(it.value) ? 1 : 0;
        items.append(it);
    }
    map_->set_color_clamp(kShClamp[period_]);
    if (with_value > 0)
        map_->set_items(items);
    footer_->setText(tr("%1 · area: %2 · colour: %3 return, ±%4 % = full colour")
                         .arg(core::market::profile_text(p.sector_set_name),
                              weighted ? tr("approx. index weight") : tr("equal"),
                              QString::fromLatin1(kShPeriods[period_]),
                              sh_fmt::number(kShClamp[period_], 0)));
}

void SectorHeatmapWidget::on_theme_changed() {
    if (auto* bar = findChild<QWidget*>(QStringLiteral("shBar")))
        set_css_cached(bar, QString("QWidget#shBar{background:transparent;}"
                                   "QPushButton#shPeriod{background:%1;color:%2;border:1px solid %3;padding:1px 8px;"
                                   "font-size:%4px;font-weight:bold;}"
                                   "QPushButton#shPeriod:checked{background:%5;color:%6;border-color:%6;}")
                               .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_DIM())
                               .arg(ui::fonts::px(-4))
                               .arg(ui::colors::ACCENT_BG(), ui::colors::AMBER()));
    set_css_cached(footer_, QString("color:%1;font-size:%2px;padding:0 6px 2px 6px;background:transparent;")
                               .arg(ui::colors::TEXT_TERTIARY())
                               .arg(ui::fonts::px(-4)));
}

void SectorHeatmapWidget::retranslateUi() {
    BaseWidget::retranslateUi();
    update_title();
    render();
}

} // namespace fincept::screens::widgets
