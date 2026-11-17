#include "screens/dashboard/widgets/RiskMetricsWidget.h"

#include "core/market/MarketProfile.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "screens/dashboard/widgets/RegionPin.h"
#include "services/markets/MarketDataService.h"
#include "ui/charts/TimeSeriesChart.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/StatTile.h"

#include <QLabel>

#include <cmath>

namespace fincept::screens::widgets {

namespace {

namespace mr_fmt = fincept::ui::formatting;
using fincept::services::MarketDataService;

constexpr const char* kMrVix = "^VIX";
constexpr const char* kMrVix3m = "^VIX3M";

/// Volatility regime of a 30-day implied-vol gauge (VIX-style levels).
QString mr_regime(double level, ui::StatTile::Tone* tone) {
    if (level < 15) {
        *tone = ui::StatTile::Tone::Neutral;
        return QCoreApplication::translate("fincept::screens::widgets::RiskMetricsWidget", "Calm (< 15)");
    }
    if (level < 20) {
        *tone = ui::StatTile::Tone::Neutral;
        return QCoreApplication::translate("fincept::screens::widgets::RiskMetricsWidget", "Normal (15–20)");
    }
    if (level < 30) {
        *tone = ui::StatTile::Tone::Warning;
        return QCoreApplication::translate("fincept::screens::widgets::RiskMetricsWidget", "Elevated (20–30)");
    }
    *tone = ui::StatTile::Tone::Warning;
    return QCoreApplication::translate("fincept::screens::widgets::RiskMetricsWidget", "Stress (≥ 30)");
}

} // namespace

RiskMetricsWidget::RiskMetricsWidget(const QJsonObject& cfg, QWidget* parent)
    : BaseWidget(tr("MARKET RISK"), parent, ui::colors::WARNING()), region_pin_(region_pin_from(cfg)) {
    kpis_ = new ui::KpiStrip(this);
    kpis_->set_spacing(4);
    kpis_->add_tile("gauge", tr("VOL INDEX"))->set_compact(true);
    kpis_->add_tile("rv", tr("REALISED VOL 20D"))->set_compact(true);
    kpis_->add_tile("vrp", tr("IMPLIED − REALISED"))->set_compact(true);
    kpis_->add_tile("dd", tr("FROM 52W HIGH"))->set_compact(true);
    kpis_->add_tile("term", tr("VIX / VIX3M"))->set_compact(true);
    content_layout()->addWidget(kpis_);

    chart_ = new ui::TimeSeriesChart(this);
    chart_->set_show_mode_buttons(false);
    chart_->set_range_presets({"3M", "6M", "1Y"});
    chart_->set_export_name(QStringLiteral("volatility_gauge"));
    chart_->setMinimumHeight(70);
    content_layout()->addWidget(chart_, 1);

    footer_ = new QLabel(this);
    footer_->setWordWrap(true);
    content_layout()->addWidget(footer_);

    connect(this, &BaseWidget::refresh_requested, this,
            [this]() { datahub::DataHub::instance().request(topics(), /*force=*/true); });
    connect(&core::market::MarketProfileStore::instance(), &core::market::MarketProfileStore::profile_changed, this,
            [this]() {
                if (region_pin_.isEmpty())
                    reload_region();
            });
    on_theme_changed();
    set_loading(true);
}

void RiskMetricsWidget::reload_region() {
    quotes_.clear();
    stats_.clear();
    gauge_hist_.clear();
    untrack_topics();
    if (isVisible())
        hub_subscribe_all();
}

QJsonObject RiskMetricsWidget::config() const {
    QJsonObject o;
    store_region_pin(o, region_pin_);
    return o;
}

void RiskMetricsWidget::apply_config(const QJsonObject& cfg) {
    const QString pin = region_pin_from(cfg);
    if (pin == region_pin_)
        return;
    region_pin_ = pin;
    reload_region();
}

QStringList RiskMetricsWidget::topics() const {
    QStringList t{MarketDataService::quote_topic(bench_), MarketDataService::performance_history_topic(bench_),
                  MarketDataService::quote_topic(gauge_), MarketDataService::performance_history_topic(gauge_)};
    if (has_term_)
        t << MarketDataService::quote_topic(QString::fromLatin1(kMrVix))
          << MarketDataService::quote_topic(QString::fromLatin1(kMrVix3m));
    t.removeDuplicates();
    return t;
}

void RiskMetricsWidget::showEvent(QShowEvent* e) {
    BaseWidget::showEvent(e);
    if (!hub_active_)
        hub_subscribe_all();
}

void RiskMetricsWidget::hideEvent(QHideEvent* e) {
    BaseWidget::hideEvent(e);
    hub_active_ = false;
}

void RiskMetricsWidget::hub_subscribe_all() {
    const auto& p = tile_profile(region_pin_);
    bench_ = p.benchmark;
    gauge_ = p.volatility.value(0, QString::fromLatin1(kMrVix));
    // Implied vs realised only compares like with like.
    gauge_matches_bench_ = (gauge_ == QLatin1String("^VIX") && bench_ == QLatin1String("^GSPC")) ||
                           (gauge_ == QLatin1String("^INDIAVIX") && bench_ == QLatin1String("^NSEI"));
    has_term_ = p.volatility.contains(QString::fromLatin1(kMrVix3m));
    set_title(tr("MARKET RISK · %1").arg(core::market::market_profile_name(p).toUpper()));

    auto& hub = datahub::DataHub::instance();
    hub.unsubscribe(this);
    const QStringList all = topics();
    track_topics(all, /*drive_loading_overlay=*/true);
    QStringList quote_syms{bench_, gauge_};
    if (has_term_)
        quote_syms << QString::fromLatin1(kMrVix) << QString::fromLatin1(kMrVix3m);
    quote_syms.removeDuplicates();
    for (const QString& sym : quote_syms) {
        const QString t = MarketDataService::quote_topic(sym);
        hub.subscribe(this, t, [this, sym, t](const QVariant& v) {
            if (!v.canConvert<services::QuoteData>())
                return;
            quotes_.insert(sym, v.value<services::QuoteData>());
            note_topic_value(t);
            schedule_render([this]() { render(); });
        });
    }
    for (const QString& sym : QStringList{bench_, gauge_}) {
        const QString t = MarketDataService::performance_history_topic(sym);
        hub.subscribe(this, t, [this, sym, t](const QVariant& v) {
            if (!v.canConvert<QVector<services::HistoryPoint>>())
                return;
            const auto bars = v.value<QVector<services::HistoryPoint>>();
            if (bars.isEmpty()) {
                note_topic_error(t, tr("No daily history for %1").arg(sym));
                return;
            }
            stats_.insert(sym, services::compute_performance(bars));
            if (sym == gauge_) {
                gauge_hist_.clear();
                for (const auto& b : bars)
                    if (b.close > 0)
                        gauge_hist_.append(QPointF(double(b.timestamp) * 1000.0, b.close));
                render_chart();
            }
            note_topic_value(t);
            schedule_render([this]() { render(); });
        });
    }
    hub_active_ = true;
}

void RiskMetricsWidget::on_topic_errors_changed() {
    schedule_render([this]() { render(); });
}

void RiskMetricsWidget::render() {
    // Gauge level + regime
    if (auto* t = kpis_->tile("gauge")) {
        t->set_label(gauge_ == QLatin1String("^INDIAVIX") ? tr("INDIA VIX") : tr("VIX"));
        const auto q = quotes_.constFind(gauge_);
        if (q != quotes_.constEnd() && q->price > 0) {
            ui::StatTile::Tone tone = ui::StatTile::Tone::Neutral;
            t->set_value(mr_fmt::number(q->price, 2));
            t->set_sub_label(mr_regime(q->price, &tone));
            t->set_tone(tone);
            // A rising fear gauge is bad news: invert the delta colours.
            t->set_delta_inverted(true);
            t->set_delta(q->change_pct);
            t->set_state(ui::StatTile::State::Normal);
        } else if (!topic_error(MarketDataService::quote_topic(gauge_)).isEmpty()) {
            t->set_state(ui::StatTile::State::Error, topic_error(MarketDataService::quote_topic(gauge_)));
        }
    }
    const auto bs = stats_.constFind(bench_);
    const QString bench_name = core::market::any_profile_label_for(bench_).isEmpty()
                                   ? bench_
                                   : core::market::any_profile_label_for(bench_);
    if (auto* t = kpis_->tile("rv")) {
        if (bs != stats_.constEnd() && std::isfinite(bs->volatility_20d_pct)) {
            t->set_value(mr_fmt::pct(bs->volatility_20d_pct, 1, false));
            t->set_sub_label(bench_name);
            t->set_tooltip_text(tr("Annualised standard deviation of the last 20 daily log returns of %1").arg(bench_name));
        } else {
            t->set_value(mr_fmt::placeholder());
        }
    }
    if (auto* t = kpis_->tile("vrp")) {
        t->setVisible(gauge_matches_bench_);
        const auto q = quotes_.constFind(gauge_);
        if (gauge_matches_bench_ && q != quotes_.constEnd() && q->price > 0 && bs != stats_.constEnd() &&
            std::isfinite(bs->volatility_20d_pct)) {
            const double spread = q->price - bs->volatility_20d_pct;
            t->set_value(mr_fmt::pp(spread, 1));
            t->set_tone(spread < 0 ? ui::StatTile::Tone::Warning : ui::StatTile::Tone::Neutral);
            t->set_tooltip_text(tr("30-day implied volatility minus 20-day realised volatility of %1. Negative = "
                                   "the market is realising more volatility than options price in.")
                                    .arg(bench_name));
        } else {
            t->set_value(mr_fmt::placeholder());
        }
    }
    if (auto* t = kpis_->tile("dd")) {
        if (bs != stats_.constEnd() && std::isfinite(bs->from_52w_high_pct)) {
            t->set_value(mr_fmt::pct(bs->from_52w_high_pct, 1));
            t->set_sub_label(bench_name);
            t->set_tone(bs->from_52w_high_pct <= -10 ? ui::StatTile::Tone::Warning : ui::StatTile::Tone::Neutral);
            t->set_tooltip_text(tr("%1 vs its 52-week high (%2). −10 % is a correction, −20 % a bear market.")
                                    .arg(bench_name, mr_fmt::number(bs->high_52w, 2)));
        } else {
            t->set_value(mr_fmt::placeholder());
        }
    }
    if (auto* t = kpis_->tile("term")) {
        t->setVisible(has_term_);
        const auto a = quotes_.constFind(QString::fromLatin1(kMrVix));
        const auto b = quotes_.constFind(QString::fromLatin1(kMrVix3m));
        if (has_term_ && a != quotes_.constEnd() && b != quotes_.constEnd() && a->price > 0 && b->price > 0) {
            const double ratio = a->price / b->price;
            t->set_value(mr_fmt::number(ratio, 2));
            t->set_sub_label(ratio >= 1.0 ? tr("Backwardation") : tr("Contango"));
            t->set_tone(ratio >= 1.0 ? ui::StatTile::Tone::Warning : ui::StatTile::Tone::Neutral);
            t->set_tooltip_text(tr("1-month over 3-month implied volatility. Above 1 the curve is inverted — "
                                   "near-term stress priced above the longer term."));
        } else {
            t->set_value(mr_fmt::placeholder());
        }
    }
    footer_->setText(tr("Gauge %1 · benchmark %2 · regime bands are VIX-scale levels").arg(gauge_, bench_));
}

void RiskMetricsWidget::render_chart() {
    chart_->clear();
    chart_->clear_annotations();
    if (gauge_hist_.size() < 2) {
        chart_->set_state(ui::TimeSeriesChart::State::Empty, tr("No history"));
        return;
    }
    ui::TimeSeriesChart::SeriesSpec s;
    s.name = gauge_;
    s.points = gauge_hist_;
    s.style = ui::TimeSeriesChart::Style::Line;
    s.color = ui::ChartColorRole::Warning;
    chart_->add_series(s);
    chart_->add_hline(20.0, tr("20"), ui::ChartColorRole::Muted);
    chart_->add_hline(30.0, tr("30"), ui::ChartColorRole::Warning);
    chart_->set_state(ui::TimeSeriesChart::State::Ready);
}

void RiskMetricsWidget::on_theme_changed() {
    set_css_cached(footer_, QString("color:%1;font-size:%2px;padding:0 6px 2px 6px;background:transparent;")
                               .arg(ui::colors::TEXT_TERTIARY())
                               .arg(ui::fonts::px(-4)));
}

void RiskMetricsWidget::retranslateUi() {
    BaseWidget::retranslateUi();
    render();
}

} // namespace fincept::screens::widgets
