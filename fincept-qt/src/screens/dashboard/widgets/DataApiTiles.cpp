#include "screens/dashboard/widgets/DataApiTiles.h"

#include "core/agent/AgentRiskMarkers.h"
#include "core/market/MarketProfile.h"
#include "core/market/MarketSessions.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "screens/dashboard/widgets/QuoteGrid.h"
#include "screens/dashboard/widgets/TileBilling.h"
#include "services/fincept_data/FinceptDataService.h"
#include "services/markets/MarketDataService.h"
#include "services/portfolio/PortfolioService.h"
#include "storage/cache/CacheManager.h"
#include "storage/repositories/WatchlistRepository.h"
#include "ui/charts/DonutChart.h"
#include "ui/charts/TimeSeriesChart.h"
#include "ui/formatting/DateFormat.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/tables/DataTable.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/StatTile.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QPointer>
#include <QtConcurrent/QtConcurrent>

#include <algorithm>
#include <cmath>

namespace fincept::screens::widgets {

namespace {

namespace da_fmt = fincept::ui::formatting;
namespace fd = fincept::services::fincept_data;
using fincept::services::MarketDataService;

double da_num(const QJsonValue& v) {
    return v.isDouble() ? v.toDouble() : std::numeric_limits<double>::quiet_NaN();
}

struct DaMaturity {
    const char* key;
    const char* label;
    double years;
};
constexpr DaMaturity kDaTreasury[] = {{"month_1", "1M", 1.0 / 12}, {"month_3", "3M", 0.25}, {"month_6", "6M", 0.5},
                                      {"year_1", "1Y", 1},         {"year_2", "2Y", 2},     {"year_3", "3Y", 3},
                                      {"year_5", "5Y", 5},         {"year_7", "7Y", 7},     {"year_10", "10Y", 10},
                                      {"year_20", "20Y", 20},      {"year_30", "30Y", 30}};

/// Maturity axis: square root of years spreads the short end.
double da_x(double years) {
    return std::sqrt(std::max(0.0, years));
}

QString da_years_label(double years) {
    if (years < 1.0)
        return QStringLiteral("%1M").arg(qRound(years * 12));
    return QStringLiteral("%1Y").arg(qRound(years));
}

void da_set(ui::StatTile* t, const QString& value, const QString& tip = {}) {
    if (!t)
        return;
    t->set_value(value);
    if (!tip.isEmpty())
        t->set_tooltip_text(tip);
}

} // namespace

// ── DataApiTile ──────────────────────────────────────────────────────────────

DataApiTile::DataApiTile(const QString& title, QWidget* parent, const QString& accent)
    : BaseWidget(title, parent, accent) {
    // refresh / LOAD: a billed /data call ("market_data" credits) — "spend" unless the plan includes it.
    set_agent_risk(tile_billing::agent_kind(QStringLiteral("market_data")));
    footer_ = new QLabel(this);
    footer_->setProperty(fincept::core::agent::kAgentRiskProperty, "spend"); // its LOAD link sends the billed call
    if (tile_billing::plan_includes(QStringLiteral("market_data")))
        ui::EmptyState::mark_agent_risk(footer_, QString()); // included: the LOAD link is free
    footer_->setWordWrap(true);
    // Plain text, except the LOAD link of offer_load() on a tile whose /data part
    // is secondary (a source name or reason never turns into markup).
    footer_->setTextFormat(Qt::PlainText);
    footer_->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
    connect(footer_, &QLabel::linkActivated, this, [this](const QString&) { request_refresh(); });
    offer_title_ = tr("Fincept data");
    offer_run_ = tr("LOAD");
    // The plan table arrived / changed: a plan that includes /data at no credit
    // cost loads the offered request now, as a free tile would.
    connect(&billing::PlanAccess::instance(), &billing::PlanAccess::changed, this, [this]() {
        const char* kind = tile_billing::agent_kind(QStringLiteral("market_data"));
        set_agent_risk(kind); // agents: a load the plan includes is free
        ui::EmptyState::mark_agent_risk(footer_, kind ? QString::fromLatin1(kind) : QString());
        if (offering_ && isVisible() && tile_billing::plan_includes(QStringLiteral("market_data")))
            request_refresh();
    });
}

DataApiTile::~DataApiTile() {
    fd::FinceptDataService::instance().cancel(this);
}

void DataApiTile::set_load_offer(const QString& title, const QString& what, const QString& run_text,
                                 ui::LoadingSkeleton::Shape shape) {
    offer_title_ = title;
    offer_what_ = what;
    offer_run_ = run_text;
    offer_shape_ = shape;
}

QString DataApiTile::cost_note(const QString& dataset) const {
    // The cost per call is in the dataset catalogue once the server has been asked.
    const auto& cat = fd::FinceptDataService::instance().catalog();
    const auto it = cat.datasets.constFind(dataset);
    const double credits = it != cat.datasets.constEnd() ? it->credits_per_call : 0.0;
    const QString fallback =
        credits > 0.0 ? tr("%1 credit(s) per load · a repeat while the source has nothing newer is free")
                            .arg(QString::number(credits, 'g', 3))
                      : tr("Each load is billed like a /market call · a repeat while the source has nothing newer "
                           "is free");
    // The account's own price when the server sent its plan table (Pro may pay less).
    return tile_billing::cost_note(QStringLiteral("market_data"), credits, fallback);
}

void DataApiTile::offer_load(const QString& dataset) {
    offering_ = true;
    const QString note = cost_note(dataset);
    if (errors_in_footer_) {
        footer_->setTextFormat(Qt::RichText);
        footer_->setText(QStringLiteral("%1 — <a href=\"load\">%2</a> · %3")
                             .arg(offer_title_.toHtmlEscaped(), offer_run_.toHtmlEscaped(), note.toHtmlEscaped()));
        footer_->setToolTip(offer_what_);
        return;
    }
    ui::EmptyState::Spec spec = ui::EmptyState::preview_spec(offer_title_, offer_what_, offer_run_, note,
                                                             ui::EmptyState::Tone::Warning, offer_shape_);
    show_empty_state(spec, [this]() { request_refresh(); });
}

QString DataApiTile::remembered_key(const QString& path) {
    return QStringLiteral("dashboard/data") + path;
}

void DataApiTile::remember(const QString& path, const fd::DataResult& r) {
    const fd::DataMeta& m = r.meta;
    const QJsonObject meta{{QStringLiteral("as_of"), m.as_of.isValid() ? m.as_of.toString(Qt::ISODateWithMs) : QString()},
                           {QStringLiteral("age"), m.age_seconds},
                           {QStringLiteral("max_age"), m.max_age_seconds},
                           {QStringLiteral("stale"), m.stale},
                           {QStringLiteral("stale_reason"), m.stale_reason},
                           {QStringLiteral("source"), m.source_name},
                           {QStringLiteral("delayed"), m.delayed_minutes},
                           {QStringLiteral("received_ms"), static_cast<double>(r.received_ms)}};
    const QByteArray text =
        QJsonDocument(QJsonObject{{QStringLiteral("data"), r.data}, {QStringLiteral("meta"), meta}})
            .toJson(QJsonDocument::Compact);
    constexpr int kDaRememberMaxBytes = 512 * 1024; // a /data page is a few KB; never keep a runaway one
    if (text.size() > kDaRememberMaxBytes)
        return;
    constexpr int kDaRememberTtlSec = 14 * 24 * 3600;
    CacheManager::instance().put(remembered_key(path), QString::fromUtf8(text), kDaRememberTtlSec,
                                 QStringLiteral("dashboard"));
}

bool DataApiTile::recall(const QString& path, fd::DataResult* out) {
    const std::optional<QString> text = CacheManager::instance().try_get(remembered_key(path));
    if (!text || text->isEmpty())
        return false;
    const QJsonObject o = QJsonDocument::fromJson(text->toUtf8()).object();
    if (!o.value(QStringLiteral("data")).isObject())
        return false;
    const QJsonObject m = o.value(QStringLiteral("meta")).toObject();
    fd::DataResult r;
    r.data = o.value(QStringLiteral("data")).toObject();
    r.meta.as_of = QDateTime::fromString(m.value(QStringLiteral("as_of")).toString(), Qt::ISODateWithMs);
    if (r.meta.as_of.isValid())
        r.meta.as_of = r.meta.as_of.toUTC();
    r.meta.age_seconds = m.value(QStringLiteral("age")).toInt();
    r.meta.max_age_seconds = m.value(QStringLiteral("max_age")).toInt();
    r.meta.stale = m.value(QStringLiteral("stale")).toBool();
    r.meta.stale_reason = m.value(QStringLiteral("stale_reason")).toString();
    r.meta.source_name = m.value(QStringLiteral("source")).toString();
    r.meta.delayed_minutes = m.value(QStringLiteral("delayed")).toInt();
    r.received_ms = static_cast<qint64>(m.value(QStringLiteral("received_ms")).toDouble());
    r.from_client_cache = true; // nothing was asked for it
    *out = r;
    return true;
}

void DataApiTile::fetch_data(const QString& dataset, const QString& path, const fd::QueryParams& query, bool user,
                             OkFn on_ok, bool quiet) {
    // A plan that includes /data at no credit cost (the server's table) loads by
    // itself like a free tile; without a table every call stays manual.
    if (!user && !tile_billing::plan_includes(QStringLiteral("market_data"))) {
        // Billing rule: an automatic request is never sent. The remembered answer,
        // else (for the tile's main request, nothing on screen yet) the LOAD offer.
        fd::DataResult kept;
        if (recall(path, &kept)) {
            if (!quiet) {
                hide_empty_state();
                offering_ = false;
                has_data_ = true;
                set_loading(false);
            }
            delivering_remembered_ = true;
            on_ok(kept);
            delivering_remembered_ = false;
        } else if (!quiet && !has_data_) {
            set_loading(false);
            offer_load(dataset);
        }
        return;
    }
    if (!has_data_ && !errors_in_footer_ && !quiet) {
        hide_empty_state(); // the LOAD offer gives way to the spinner
        set_loading(true);
    }
    fd::FinceptDataService::instance().fetch(
        dataset, path, query, this,
        [this, on_ok, path, quiet](const fd::DataResult& r) {
            if (!r.ok()) {
                if (quiet)
                    return; // the tile's main request carries the state
                const QString why = r.error.kind == fd::ErrorKind::ServerLacksApi
                                        ? fd::FinceptDataService::unavailable_text()
                                        : fd::FinceptDataService::error_text(r.error);
                footer_->setTextFormat(Qt::PlainText);
                if (errors_in_footer_)
                    footer_->setText(why);
                else if (!has_data_)
                    set_error(why);
                else
                    footer_->setText(tr("Last refresh failed — showing earlier data: %1").arg(why));
                return;
            }
            remember(path, r);
            if (!quiet) {
                hide_empty_state();
                offering_ = false;
                has_data_ = true;
                set_loading(false);
            }
            on_ok(r);
        },
        /*refresh=*/user);
}

void DataApiTile::set_footer(const QString& prefix, const fd::DataResult& r) {
    QString fresh = fd::FinceptDataService::freshness_text(r, QDateTime::currentMSecsSinceEpoch());
    if (delivering_remembered_)
        fresh += QStringLiteral(" · ") + tr("saved from the last load — refresh asks for newer data (billed)");
    footer_->setTextFormat(Qt::PlainText);
    footer_->setToolTip(QString());
    footer_->setText(prefix.isEmpty() ? fresh : prefix + QStringLiteral(" · ") + fresh);
}

void DataApiTile::on_theme_changed() {
    set_css_cached(footer_, QString("color:%1;font-size:%2px;padding:0 6px 2px 6px;background:transparent;")
                               .arg(ui::colors::TEXT_TERTIARY())
                               .arg(ui::fonts::px(-4)));
}

// ── Yield curve ──────────────────────────────────────────────────────────────

YieldCurveWidget::YieldCurveWidget(const QJsonObject& cfg, QWidget* parent)
    : DataApiTile(tr("YIELD CURVE"), parent, ui::colors::INFO()) {
    auto* bar = new QWidget(this);
    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(6, 3, 6, 3);
    curve_combo_ = new QComboBox(bar);
    curve_combo_->addItem(tr("Region default"), QString());
    curve_combo_->addItem(tr("US Treasury (Fed H.15)"), QStringLiteral("treasury"));
    curve_combo_->addItem(tr("Euro area AAA (ECB)"), QStringLiteral("euro"));
    hl->addWidget(curve_combo_, 1);
    content_layout()->addWidget(bar);

    kpis_ = new ui::KpiStrip(this);
    kpis_->set_spacing(4);
    for (const auto& [k, l] : {std::pair{"10y", tr("10Y")}, std::pair{"2s10s", tr("2s10s")},
                               std::pair{"3m10y", tr("3M10Y")}, std::pair{"chg", tr("10Y vs 1M")}})
        kpis_->add_tile(QString::fromLatin1(k), l)->set_compact(true);
    content_layout()->addWidget(kpis_);

    chart_ = new ui::TimeSeriesChart(this);
    chart_->set_x_is_time(false);
    chart_->set_show_mode_buttons(false);
    chart_->set_zoom_enabled(false);
    chart_->set_value_formatter(ui::TimeSeriesChart::Axis::Left, [](double v) { return da_fmt::pct(v, 2, false); });
    auto x_label = [](double x) { return da_years_label(x * x); };
    chart_->set_time_formatter(x_label);
    chart_->set_tick_time_formatter(x_label);
    chart_->set_export_name(QStringLiteral("yield_curve"));
    chart_->setMinimumHeight(90);
    content_layout()->addWidget(chart_, 1);
    content_layout()->addWidget(footer_);

    connect(curve_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        curve_cfg_ = curve_combo_->currentData().toString();
        has_data_ = false;
        load(false);
        emit config_changed(config());
    });
    connect(this, &BaseWidget::refresh_requested, this, [this]() { load(true); });
    connect(&core::market::MarketProfileStore::instance(), &core::market::MarketProfileStore::profile_changed, this,
            [this]() {
                if (curve_cfg_.isEmpty() && isVisible()) {
                    has_data_ = false;
                    load(false);
                }
            });
    set_load_offer(tr("Yield curve"),
                   tr("Government yields across maturities, the 2s10s and 3m10y spreads and the 10-year change."),
                   tr("LOAD CURVE"), ui::LoadingSkeleton::Shape::Chart);
    on_theme_changed();
    apply_config(cfg);
}

QString YieldCurveWidget::curve() const {
    if (!curve_cfg_.isEmpty())
        return curve_cfg_;
    return core::market::MarketProfileStore::instance().current().rates_curve == QLatin1String("euro-curve")
               ? QStringLiteral("euro")
               : QStringLiteral("treasury");
}

QJsonObject YieldCurveWidget::config() const {
    return curve_cfg_.isEmpty() ? QJsonObject{} : QJsonObject{{"curve", curve_cfg_}};
}

void YieldCurveWidget::apply_config(const QJsonObject& cfg) {
    curve_cfg_ = cfg.value("curve").toString();
    const int idx = curve_combo_->findData(curve_cfg_);
    curve_combo_->blockSignals(true);
    curve_combo_->setCurrentIndex(idx >= 0 ? idx : 0);
    curve_combo_->blockSignals(false);
    if (isVisible())
        load(false);
}

void YieldCurveWidget::showEvent(QShowEvent* e) {
    DataApiTile::showEvent(e);
    load(false);
}

void YieldCurveWidget::load(bool refresh) {
    const bool euro = curve() == QLatin1String("euro");
    set_title(euro ? tr("YIELD CURVE · EURO AREA AAA") : tr("YIELD CURVE · US TREASURY"));
    auto put_curve = [this](const QString& name, const QVector<QPointF>& pts, ui::ChartColorRole role, bool dashed) {
        if (pts.size() < 2)
            return;
        ui::TimeSeriesChart::SeriesSpec s;
        s.name = name;
        s.points = pts;
        s.color = role;
        s.dashed = dashed;
        s.rebase = false;
        chart_->add_series(s);
    };
    auto bp = [](double v) { return std::isfinite(v) ? da_fmt::bp(v * 100.0) : da_fmt::placeholder(); };

    if (euro) {
        fetch_data(QStringLiteral("rates.euro_curve"), QStringLiteral("/data/rates/euro-curve"), {}, refresh,
                   [this, put_curve, bp](const fd::DataResult& r) {
                       QVector<QPointF> pts;
                       double y3m = NAN, y2 = NAN, y10 = NAN;
                       for (const auto& v : r.data.value("points").toArray()) {
                           const auto o = v.toObject();
                           const double years = da_num(o.value("years"));
                           const double rate = da_num(o.value("rate"));
                           if (!std::isfinite(years) || !std::isfinite(rate))
                               continue;
                           pts.append(QPointF(da_x(years), rate));
                           if (std::abs(years - 0.25) < 1e-6)
                               y3m = rate;
                           if (std::abs(years - 2) < 1e-6)
                               y2 = rate;
                           if (std::abs(years - 10) < 1e-6)
                               y10 = rate;
                       }
                       std::sort(pts.begin(), pts.end(), [](auto& a, auto& b) { return a.x() < b.x(); });
                       chart_->clear();
                       put_curve(r.data.value("date").toString(), pts, ui::ChartColorRole::Accent, false);
                       chart_->set_state(pts.size() >= 2 ? ui::TimeSeriesChart::State::Ready
                                                         : ui::TimeSeriesChart::State::Empty);
                       da_set(kpis_->tile("10y"), std::isfinite(y10) ? da_fmt::pct(y10, 2, false) : da_fmt::placeholder());
                       da_set(kpis_->tile("2s10s"), bp(y10 - y2), tr("10-year minus 2-year yield"));
                       da_set(kpis_->tile("3m10y"), bp(y10 - y3m), tr("10-year minus 3-month yield"));
                       da_set(kpis_->tile("chg"), da_fmt::placeholder(), tr("The ECB curve endpoint has no history"));
                       set_footer(tr("ECB AAA spot curve of %1").arg(r.data.value("date").toString()), r);
                   });
        return;
    }
    const QDate today = QDate::currentDate();
    const fd::QueryParams q{{QStringLiteral("start_date"), today.addDays(-380).toString(Qt::ISODate)}};
    fetch_data(QStringLiteral("rates.treasury"), QStringLiteral("/data/rates/treasury"), q, refresh,
               [this, put_curve, bp](const fd::DataResult& r) {
                   const QJsonArray rows = r.data.value("rows").toArray();
                   auto curve_of = [](const QJsonObject& row) {
                       QVector<QPointF> pts;
                       for (const auto& m : kDaTreasury) {
                           const double v = da_num(row.value(QLatin1String(m.key)));
                           if (std::isfinite(v))
                               pts.append(QPointF(da_x(m.years), v));
                       }
                       return pts;
                   };
                   // Latest usable row, and the rows on/before 1 month and 1 year earlier.
                   int last = -1;
                   for (int i = rows.size() - 1; i >= 0 && last < 0; --i)
                       if (curve_of(rows[i].toObject()).size() >= 5)
                           last = i;
                   if (last < 0) {
                       chart_->set_state(ui::TimeSeriesChart::State::Empty, tr("No Treasury curve in the answer"));
                       return;
                   }
                   const QJsonObject now_row = rows[last].toObject();
                   const QDate d0 = QDate::fromString(now_row.value("date").toString(), Qt::ISODate);
                   auto row_before = [&](const QDate& d) -> QJsonObject {
                       for (int i = last; i >= 0; --i) {
                           const auto o = rows[i].toObject();
                           if (QDate::fromString(o.value("date").toString(), Qt::ISODate) <= d &&
                               curve_of(o).size() >= 5)
                               return o;
                       }
                       return {};
                   };
                   const QJsonObject m1 = row_before(d0.addMonths(-1));
                   const QJsonObject y1 = row_before(d0.addYears(-1));
                   chart_->clear();
                   put_curve(tr("Today (%1)").arg(da_fmt::calendar_date(d0, da_fmt::DateStyle::Short)), curve_of(now_row),
                             ui::ChartColorRole::Accent, false);
                   if (!m1.isEmpty())
                       put_curve(tr("1M ago"), curve_of(m1), ui::ChartColorRole::Muted, true);
                   if (!y1.isEmpty())
                       put_curve(tr("1Y ago"), curve_of(y1), ui::ChartColorRole::Info, true);
                   chart_->set_state(ui::TimeSeriesChart::State::Ready);
                   const double y10 = da_num(now_row.value("year_10"));
                   const double y2 = da_num(now_row.value("year_2"));
                   const double y3m = da_num(now_row.value("month_3"));
                   da_set(kpis_->tile("10y"), std::isfinite(y10) ? da_fmt::pct(y10, 2, false) : da_fmt::placeholder());
                   da_set(kpis_->tile("2s10s"), bp(y10 - y2),
                          tr("10-year minus 2-year yield; negative = inverted curve"));
                   da_set(kpis_->tile("3m10y"), bp(y10 - y3m), tr("10-year minus 3-month yield"));
                   const double m1_10 = da_num(m1.value("year_10"));
                   da_set(kpis_->tile("chg"), bp(y10 - m1_10), tr("Change of the 10-year yield over one month"));
                   if (auto* t = kpis_->tile("2s10s"))
                       t->set_tone(std::isfinite(y10 - y2) && y10 < y2 ? ui::StatTile::Tone::Warning
                                                                        : ui::StatTile::Tone::Neutral);
                   set_footer(tr("US Treasury par yields, Fed H.15"), r);
               });
}

// ── Volatility ───────────────────────────────────────────────────────────────

VolatilityWidget::VolatilityWidget(QWidget* parent)
    : DataApiTile(tr("VOLATILITY"), parent, ui::colors::WARNING()) {
    errors_in_footer_ = true; // the VIX family is live; the futures curve is the /data extra
    grid_ = new QuoteGrid(this);
    grid_->set_optional_columns(QuoteGrid::OptTrend | QuoteGrid::OptDay);
    content_layout()->addWidget(grid_, 3);

    structure_ = new QLabel(this);
    structure_->setWordWrap(true);
    content_layout()->addWidget(structure_);

    chart_ = new ui::TimeSeriesChart(this);
    chart_->set_x_is_time(false);
    chart_->set_show_mode_buttons(false);
    chart_->set_zoom_enabled(false);
    chart_->set_show_legend(false);
    chart_->set_time_formatter([](double x) { return QCoreApplication::translate("VolatilityWidget", "%1 days").arg(qRound(x)); });
    chart_->set_tick_time_formatter([](double x) { return QStringLiteral("%1d").arg(qRound(x)); });
    chart_->set_export_name(QStringLiteral("vix_term_structure"));
    chart_->setMinimumHeight(80);
    content_layout()->addWidget(chart_, 2);
    content_layout()->addWidget(footer_);

    connect(this, &BaseWidget::refresh_requested, this, [this]() {
        QStringList t;
        for (const auto& e : grid_->entries())
            t << MarketDataService::quote_topic(e.symbol);
        datahub::DataHub::instance().request(t, true);
        load_curve(true);
    });
    // The quotes are live and free; the futures curve is a billed /data call.
    set_load_offer(tr("VIX futures curve not loaded"),
                   tr("Cboe VIX futures settlements: contango or backwardation and the front-to-second spread."),
                   tr("LOAD CURVE"), ui::LoadingSkeleton::Shape::Chart);
    on_theme_changed();
}

void VolatilityWidget::showEvent(QShowEvent* e) {
    DataApiTile::showEvent(e);
    if (!hub_active_)
        subscribe_quotes();
    load_curve(false);
}

void VolatilityWidget::hideEvent(QHideEvent* e) {
    DataApiTile::hideEvent(e);
    hub_active_ = false;
}

void VolatilityWidget::subscribe_quotes() {
    QStringList syms{"^VIX9D", "^VIX", "^VIX3M", "^VIX6M", "^VVIX"};
    for (const auto& s : core::market::MarketProfileStore::instance().current().volatility)
        if (!syms.contains(s))
            syms.prepend(s);
    QVector<QuoteGridEntry> entries;
    const QHash<QString, QString> labels{{"^VIX9D", tr("VIX 9-day")}, {"^VIX", tr("VIX 30-day")},
                                         {"^VIX3M", tr("VIX 3-month")}, {"^VIX6M", tr("VIX 6-month")},
                                         {"^VVIX", tr("VVIX (vol of VIX)")}, {"^INDIAVIX", tr("India VIX")}};
    for (const auto& s : syms)
        entries.append({s, labels.value(s, s), {}});
    grid_->set_entries(entries);
    auto& hub = datahub::DataHub::instance();
    hub.unsubscribe(this);
    QStringList topics;
    for (const auto& s : syms)
        topics << MarketDataService::quote_topic(s);
    track_topics(topics, /*drive_loading_overlay=*/false);
    for (const auto& s : syms) {
        const QString t = MarketDataService::quote_topic(s);
        hub.subscribe(this, t, [this, s, t](const QVariant& v) {
            if (!v.canConvert<services::QuoteData>())
                return;
            const auto q = v.value<services::QuoteData>();
            grid_->update_quote(s, q);
            note_topic_value(t);
            if (s == QLatin1String("^VIX") && q.price > 0) {
                spot_ = q.price;
                render_curve();
            }
        });
        hub.subscribe(this, MarketDataService::sparkline_topic(s), [this, s](const QVariant& v) {
            if (v.canConvert<QVector<double>>())
                grid_->update_sparkline(s, v.value<QVector<double>>());
        });
    }
    hub_active_ = true;
}

void VolatilityWidget::on_topic_errors_changed() {
    for (const auto& e : grid_->entries())
        grid_->set_row_error(e.symbol, topic_error_tooltip(MarketDataService::quote_topic(e.symbol)));
}

void VolatilityWidget::load_curve(bool refresh) {
    fetch_data(QStringLiteral("volatility.vix_futures"), QStringLiteral("/data/volatility/vix-futures"), {}, refresh,
               [this](const fd::DataResult& r) {
                   futures_.clear();
                   for (const auto& v : r.data.value("contracts").toArray()) {
                       const auto o = v.toObject();
                       if (o.value("weekly").toBool())
                           continue;
                       const double dte = da_num(o.value("days_to_expiration"));
                       const double px = da_num(o.value("settlement"));
                       if (std::isfinite(dte) && std::isfinite(px))
                           futures_.append(QPointF(dte, px));
                   }
                   const QString st = r.data.value("structure").toString();
                   const double spread = da_num(r.data.value("front_to_second_month_pct"));
                   const QString st_text = st == QLatin1String("contango")        ? tr("Contango")
                                           : st == QLatin1String("backwardation") ? tr("Backwardation — near-term stress")
                                                                                  : tr("Flat / unknown");
                   structure_->setText(tr("VIX futures: %1 · 2nd vs front month %2 · settlements of %3")
                                           .arg(st_text, da_fmt::pct(spread, 2),
                                                r.data.value("settlement_date").toString()));
                   structure_->setStyleSheet(QString("color:%1;font-size:%2px;padding:0 6px;background:transparent;")
                                                 .arg(st == QLatin1String("backwardation") ? ui::colors::WARNING()
                                                                                           : ui::colors::TEXT_SECONDARY())
                                                 .arg(ui::fonts::px(-4)));
                   render_curve();
                   set_footer(tr("Cboe VIX futures settlements"), r);
               });
}

void VolatilityWidget::render_curve() {
    chart_->clear();
    if (futures_.size() < 2) {
        chart_->set_state(ui::TimeSeriesChart::State::Empty, tr("No futures curve yet"));
        return;
    }
    ui::TimeSeriesChart::SeriesSpec s;
    s.name = tr("VIX futures");
    s.points = futures_;
    if (spot_ > 0)
        s.points.prepend(QPointF(0, spot_));
    s.color = ui::ChartColorRole::Warning;
    s.rebase = false;
    chart_->add_series(s);
    chart_->set_state(ui::TimeSeriesChart::State::Ready);
}

// ── Crypto overview ──────────────────────────────────────────────────────────

CryptoOverviewWidget::CryptoOverviewWidget(QWidget* parent)
    : DataApiTile(tr("CRYPTO OVERVIEW"), parent, ui::colors::AMBER()) {
    kpis_ = new ui::KpiStrip(this);
    kpis_->set_spacing(4);
    for (const auto& [k, l] : {std::pair{"mcap", tr("TOTAL MCAP")}, std::pair{"chg", tr("24H")},
                               std::pair{"vol", tr("24H VOLUME")}, std::pair{"btc", tr("BTC DOM.")}})
        kpis_->add_tile(QString::fromLatin1(k), l)->set_compact(true);
    content_layout()->addWidget(kpis_);

    auto* body = new QWidget(this);
    auto* bl = new QHBoxLayout(body);
    bl->setContentsMargins(0, 0, 0, 0);
    donut_ = new ui::DonutChart(body);
    donut_->set_value_formatter([](double v) { return da_fmt::pct(v, 1, false); });
    bl->addWidget(donut_, 1);
    trending_ = new ui::DataTable(body);
    trending_->set_headers({tr("TRENDING"), tr("SYMBOL"), tr("RANK")});
    trending_->horizontalHeader()->setStretchLastSection(true);
    trending_->setColumnWidth(0, 120);
    bl->addWidget(trending_, 1);
    content_layout()->addWidget(body, 1);
    content_layout()->addWidget(footer_);
    connect(this, &BaseWidget::refresh_requested, this, [this]() { load(true); });
    set_load_offer(tr("Crypto market overview"),
                   tr("Total crypto market cap and volume, the 24-hour change, BTC / ETH dominance and the trending "
                      "coins (CoinGecko)."),
                   tr("LOAD OVERVIEW"), ui::LoadingSkeleton::Shape::Cards);
    on_theme_changed();
}

void CryptoOverviewWidget::showEvent(QShowEvent* e) {
    DataApiTile::showEvent(e);
    load(false);
}

void CryptoOverviewWidget::load(bool refresh) {
    fetch_data(QStringLiteral("crypto.global"), QStringLiteral("/data/crypto/global"), {}, refresh,
               [this](const fd::DataResult& r) {
                   const auto& d = r.data;
                   const double mcap = da_num(d.value("total_market_cap_usd"));
                   const double chg = da_num(d.value("market_cap_change_percentage_24h_usd"));
                   const auto dom = d.value("market_cap_percentage").toObject();
                   const double btc = da_num(dom.value("btc"));
                   const double eth = da_num(dom.value("eth"));
                   // CoinGecko publishes these totals in USD only (deliberately not converted).
                   da_set(kpis_->tile("mcap"), da_fmt::money(mcap, QStringLiteral("USD"), da_fmt::Compact::FromSettings));
                   if (auto* t = kpis_->tile("chg")) {
                       t->set_value(da_fmt::pct(chg));
                       t->set_tone(chg > 0 ? ui::StatTile::Tone::Positive
                                           : chg < 0 ? ui::StatTile::Tone::Negative : ui::StatTile::Tone::Neutral);
                   }
                   da_set(kpis_->tile("vol"),
                          da_fmt::money(da_num(d.value("total_volume_usd")), QStringLiteral("USD"),
                                        da_fmt::Compact::FromSettings));
                   da_set(kpis_->tile("btc"), da_fmt::pct(btc, 1, false),
                          tr("Bitcoin's share of the total crypto market cap"));
                   QVector<ui::DonutSlice> slices;
                   if (std::isfinite(btc))
                       slices.append({QStringLiteral("btc"), QStringLiteral("BTC"), btc, {}});
                   if (std::isfinite(eth))
                       slices.append({QStringLiteral("eth"), QStringLiteral("ETH"), eth, {}});
                   if (std::isfinite(btc) && std::isfinite(eth))
                       slices.append({QStringLiteral("other"), tr("Others"), std::max(0.0, 100.0 - btc - eth), {}});
                   donut_->set_center_text(tr("DOMINANCE"));
                   donut_->set_slices(slices);
                   set_footer(tr("CoinGecko global market"), r);
               });
    // Secondary: the global block carries the state (LOAD offer, errors).
    fetch_data(
        QStringLiteral("crypto.trending"), QStringLiteral("/data/crypto/trending"), {}, refresh,
        [this](const fd::DataResult& r) {
            const auto coins = r.data.value("coins").toArray();
            trending_->setRowCount(coins.size());
            for (int i = 0; i < coins.size(); ++i) {
                const auto o = coins[i].toObject();
                const QStringList cells{o.value("name").toString(), o.value("symbol").toString().toUpper(),
                                        o.value("market_cap_rank").isDouble()
                                            ? QString::number(o.value("market_cap_rank").toInt())
                                            : da_fmt::placeholder()};
                for (int c = 0; c < 3; ++c) {
                    auto* it = new QTableWidgetItem(cells[c]);
                    it->setForeground(QColor(ui::colors::TEXT_PRIMARY()));
                    trending_->setItem(i, c, it);
                }
            }
        },
        /*quiet=*/true);
}

// ── Earnings calendar ────────────────────────────────────────────────────────

EarningsCalendarWidget::EarningsCalendarWidget(const QJsonObject& cfg, QWidget* parent)
    : DataApiTile(tr("EARNINGS CALENDAR"), parent, ui::colors::CYAN()) {
    auto* bar = new QWidget(this);
    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(6, 3, 6, 3);
    scope_combo_ = new QComboBox(bar);
    scope_combo_->addItem(tr("My region"), QStringLiteral("region"));
    scope_combo_->addItem(tr("All listings"), QStringLiteral("all"));
    scope_combo_->addItem(tr("My portfolios & watchlists"), QStringLiteral("mine"));
    hl->addWidget(scope_combo_, 1);
    content_layout()->addWidget(bar);

    table_ = new ui::DataTable(this);
    table_->set_headers({tr("DATE"), tr("WHEN"), tr("SYMBOL"), tr("COMPANY"), tr("EPS EST"), tr("MCAP")});
    table_->horizontalHeader()->setStretchLastSection(false);
    table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    // Sized to their text; COMPANY takes the rest. A narrow tile drops WHEN and
    // EPS first (resizeEvent) instead of pushing MCAP off the edge.
    for (int c : {0, 1, 2, 4, 5})
        table_->horizontalHeader()->setSectionResizeMode(c, QHeaderView::ResizeToContents);
    table_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table_->setToolTip(tr("Double-click a row to open it in Equity Research"));
    connect(table_, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        if (auto* it = table_->item(row, 2))
            open_symbol(it->text());
    });
    install_symbol_menu(table_, 2);
    content_layout()->addWidget(table_, 1);
    content_layout()->addWidget(footer_);

    connect(scope_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        scope_ = scope_combo_->currentData().toString();
        render();
        emit config_changed(config());
    });
    connect(this, &BaseWidget::refresh_requested, this, [this]() { load(true); });
    connect(&core::market::MarketProfileStore::instance(), &core::market::MarketProfileStore::profile_changed, this,
            [this]() { render(); });
    set_load_offer(tr("Earnings calendar"),
                   tr("Earnings reports of the next 14 days, largest companies first — for your region, all listings "
                      "or your own symbols."),
                   tr("LOAD EARNINGS"), ui::LoadingSkeleton::Shape::Table);
    on_theme_changed();
    apply_config(cfg);
}

QJsonObject EarningsCalendarWidget::config() const {
    return QJsonObject{{"scope", scope_}};
}

void EarningsCalendarWidget::apply_config(const QJsonObject& cfg) {
    scope_ = cfg.value("scope").toString(QStringLiteral("region"));
    const int idx = scope_combo_->findData(scope_);
    scope_combo_->blockSignals(true);
    scope_combo_->setCurrentIndex(idx >= 0 ? idx : 0);
    scope_combo_->blockSignals(false);
    render();
}

void EarningsCalendarWidget::resizeEvent(QResizeEvent* e) {
    DataApiTile::resizeEvent(e);
    const int w = width();
    table_->setColumnHidden(1, w < 470); // WHEN
    table_->setColumnHidden(4, w < 400); // EPS EST
    table_->setColumnHidden(5, w < 300); // MCAP
}

void EarningsCalendarWidget::showEvent(QShowEvent* e) {
    DataApiTile::showEvent(e);
    load(false);
    // Held + watched symbols for the "mine" scope: repository reads, off the GUI thread.
    QPointer<EarningsCalendarWidget> self(this);
    (void)QtConcurrent::run([self]() {
        QStringList syms = services::PortfolioService::held_symbols();
        auto& repo = WatchlistRepository::instance();
        const auto lists = repo.list_all();
        if (lists.is_ok())
            for (const auto& w : lists.value()) {
                const auto st = repo.get_stocks(w.id);
                if (st.is_ok())
                    for (const auto& s : st.value())
                        syms << s.symbol;
            }
        QMetaObject::invokeMethod(
            QCoreApplication::instance(),
            [self, syms]() {
                if (!self)
                    return;
                self->mine_.clear();
                for (const auto& s : syms)
                    self->mine_.insert(s.trimmed().toUpper());
                if (self->scope_ == QLatin1String("mine"))
                    self->render();
            },
            Qt::QueuedConnection);
    });
}

void EarningsCalendarWidget::load(bool refresh) {
    // A billed calendar call: sent only for LOAD / refresh (`refresh` true); a show
    // draws the remembered page (DataApiTile::fetch_data). The server's page size
    // is at most 100 (the service clamps it).
    const QDate today = QDate::currentDate();
    const fd::QueryParams q{{QStringLiteral("start_date"), today.toString(Qt::ISODate)},
                            {QStringLiteral("end_date"), today.addDays(14).toString(Qt::ISODate)},
                            {QStringLiteral("page"), QStringLiteral("1")},
                            {QStringLiteral("page_size"), QStringLiteral("100")},
                            {QStringLiteral("sort"), QStringLiteral("market_cap")}};
    fetch_data(fd::calendar_dataset_id(fd::CalendarKind::Earnings), fd::calendar_path(fd::CalendarKind::Earnings), q,
               refresh, [this](const fd::DataResult& r) {
                   events_ = fd::parse_calendar(fd::CalendarKind::Earnings, r.data).events;
                   render();
                   set_footer(tr("Next 14 days"), r);
               });
}

void EarningsCalendarWidget::render() {
    const auto& p = core::market::MarketProfileStore::instance().current();
    QVector<const fd::CalendarEvent*> rows;
    const QDate today = QDate::currentDate();
    for (const auto& e : events_) {
        const QString sym = e.symbol.trimmed().toUpper();
        // A remembered page (loaded days ago) keeps reports that are past now.
        if (e.when.isValid() && e.when.toLocalTime().date() < today)
            continue;
        if (scope_ == QLatin1String("mine") && !mine_.contains(sym))
            continue;
        if (scope_ == QLatin1String("region") && !p.countries.isEmpty()) {
            // The region's own listings: its countries, without the US macro
            // calendar countries other regions list for context.
            const auto* v = core::market::venue_for_symbol(sym, e.exchange);
            const bool us_only_for_us = v && v->country == QLatin1String("US") && p.id != QLatin1String("US");
            if (!v || us_only_for_us || !p.countries.contains(v->country))
                continue;
        }
        rows.append(&e);
    }
    std::stable_sort(rows.begin(), rows.end(), [](auto* a, auto* b) { return a->when < b->when; });
    table_->clearSpans();
    if (rows.isEmpty() && has_data_) {
        // Said in the table: the footer note was overwritten by load()'s freshness
        // line right after, leaving a blank table with no explanation.
        table_->setRowCount(1);
        auto* it = new QTableWidgetItem(scope_ == QLatin1String("mine")
                                            ? tr("None of your symbols reports in the next 14 days")
                                            : tr("No reports in this scope in the next 14 days"));
        it->setForeground(QColor(ui::colors::TEXT_SECONDARY()));
        it->setTextAlignment(Qt::AlignCenter);
        table_->setItem(0, 0, it);
        table_->setSpan(0, 0, 1, table_->columnCount());
        return;
    }
    table_->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        const auto& e = *rows[i];
        const QString timing = e.timing == QLatin1String("BMO")   ? tr("Before open")
                               : e.timing == QLatin1String("AMC") ? tr("After close")
                                                                  : e.timing;
        const QStringList cells{e.when.isValid() ? da_fmt::calendar_date(e.when.date(), da_fmt::DateStyle::Short)
                                                 : da_fmt::placeholder(),
                                timing,
                                e.symbol,
                                e.company,
                                std::isfinite(e.eps_estimate) ? da_fmt::number(e.eps_estimate, 2) : da_fmt::placeholder(),
                                std::isfinite(e.market_cap) ? da_fmt::compact(e.market_cap) : da_fmt::placeholder()};
        for (int c = 0; c < cells.size(); ++c) {
            auto* it = new QTableWidgetItem(cells[c]);
            it->setForeground(QColor(c == 2 ? ui::colors::AMBER() : ui::colors::TEXT_PRIMARY()));
            if (c >= 4)
                it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            table_->setItem(i, c, it);
        }
        if (auto* it = table_->item(i, 3))
            it->setToolTip(e.event);
    }
}

} // namespace fincept::screens::widgets
