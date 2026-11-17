#include "screens/fno/IntradayOIChart.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "services/options/OISnapshotter.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/LoadingSkeleton.h"

#include <QChart>
#include <QDateTime>
#include <QDateTimeAxis>
#include <QHideEvent>
#include <QLineSeries>
#include <QPointer>
#include <QResizeEvent>
#include <QShowEvent>
#include <QTimer>
#include <QValueAxis>

#include <algorithm>
#include <limits>

namespace fincept::screens::fno {

using fincept::services::options::OISample;
using fincept::services::options::OISnapshotter;
using namespace fincept::ui;

IntradayOIChart::IntradayOIChart(QWidget* parent) : QChartView(parent) {
    setRenderHint(QPainter::Antialiasing, true);

    chart_ = new QChart();
    chart_->setPlotAreaBackgroundVisible(true);
    chart_->setMargins(QMargins(0, 4, 0, 0));
    chart_->setTitle(tr("Intraday OI"));
    chart_->legend()->setVisible(true);
    chart_->legend()->setAlignment(Qt::AlignTop);
    setChart(chart_);

    ce_series_ = new QLineSeries();
    ce_series_->setName(tr("CE OI"));

    pe_series_ = new QLineSeries();
    pe_series_->setName(tr("PE OI"));

    chart_->addSeries(ce_series_);
    chart_->addSeries(pe_series_);

    axis_x_ = new QDateTimeAxis();
    axis_x_->setFormat("HH:mm");

    axis_y_ = new QValueAxis();
    axis_y_->setLabelFormat("%.0f");

    chart_->addAxis(axis_x_, Qt::AlignBottom);
    chart_->addAxis(axis_y_, Qt::AlignLeft);
    ce_series_->attachAxis(axis_x_);
    ce_series_->attachAxis(axis_y_);
    pe_series_->attachAxis(axis_x_);
    pe_series_->attachAxis(axis_y_);

    refresh_theme();
    connect(&ThemeManager::instance(), &ThemeManager::theme_changed, this,
            [this](const ThemeTokens&) { refresh_theme(); });
}

void IntradayOIChart::refresh_theme() {
    chart_->setBackgroundBrush(QColor(colors::BG_BASE()));
    chart_->setPlotAreaBackgroundBrush(QColor(colors::BG_BASE()));
    chart_->setTitleBrush(QColor(colors::TEXT_SECONDARY()));
    chart_->setTitleFont(fonts::font(-2, QFont::Bold));
    chart_->legend()->setLabelColor(QColor(colors::TEXT_SECONDARY()));
    chart_->legend()->setFont(fonts::font(-3));

    // Brace-init dodges the vexing-parse on single-arg QColor temporaries.
    QPen ce_pen{QColor(colors::POSITIVE())};
    ce_pen.setWidth(2);
    ce_series_->setPen(ce_pen);
    QPen pe_pen{QColor(colors::NEGATIVE())};
    pe_pen.setWidth(2);
    pe_series_->setPen(pe_pen);

    const QFont label_font = fonts::font(-3);
    for (QAbstractAxis* axis : {static_cast<QAbstractAxis*>(axis_x_), static_cast<QAbstractAxis*>(axis_y_)}) {
        axis->setLabelsColor(QColor(colors::TEXT_SECONDARY()));
        axis->setLabelsFont(label_font);
        axis->setGridLineColor(QColor(colors::BORDER_DIM()));
        axis->setLinePen(QPen(QColor(colors::BORDER_DIM()), 1));
    }
}

IntradayOIChart::~IntradayOIChart() {
    fincept::datahub::DataHub::instance().unsubscribe(this);
}

void IntradayOIChart::set_subscription(const QString& broker_id, qint64 ce_token, qint64 pe_token,
                                       const QString& window) {
    // Drop prior subscriptions.
    detach_topics();

    broker_id_ = broker_id;
    ce_token_ = ce_token;
    pe_token_ = pe_token;
    window_ = window;
    ce_samples_.clear();
    pe_samples_.clear();
    ce_received_ = false;
    pe_received_ = false;

    if (broker_id.isEmpty() || (ce_token == 0 && pe_token == 0)) {
        ce_topic_.clear();
        pe_topic_.clear();
        if (wait_timer_)
            wait_timer_->stop();
        set_cover(Cover::None);
        replot();
        return;
    }

    ce_topic_ = ce_token != 0 ? OISnapshotter::history_topic(broker_id, ce_token, window) : QString();
    pe_topic_ = pe_token != 0 ? OISnapshotter::history_topic(broker_id, pe_token, window) : QString();
    replot();
    // Loading until the first history publish (or the wait runs out: "nothing recorded yet").
    set_cover(Cover::Loading);
    // Hidden (e.g. the OI tab was just built behind the Chain tab): showEvent attaches.
    if (isVisible()) {
        attach_topics();
        if (wait_timer_)
            wait_timer_->start();
    }
}

void IntradayOIChart::set_unavailable(const fincept::ui::EmptyState::Spec& page) {
    detach_topics();
    ce_topic_.clear();
    pe_topic_.clear();
    ce_token_ = pe_token_ = 0;
    ce_samples_.clear();
    pe_samples_.clear();
    if (wait_timer_)
        wait_timer_->stop();
    replot();
    set_cover(Cover::Page);
    page_is_no_history_ = false;
    page_->apply(page);
}

void IntradayOIChart::set_cover(Cover cover) {
    if (cover != Cover::None && !loading_) {
        // Built on first use, as children of the view so they sit over the plot.
        loading_ = new fincept::ui::LoadingSkeleton(fincept::ui::LoadingSkeleton::Shape::Chart, this);
        loading_->set_message(tr("Loading intraday open interest…"));
        loading_->hide();
        page_ = new fincept::ui::EmptyState(this);
        page_->set_compact(true);
        page_->set_opaque(true, fincept::ui::EmptyState::Backdrop::Base);
        page_->hide();
        connect(page_, &fincept::ui::EmptyState::primary_clicked, this, [this]() {
            if (page_is_no_history_) {
                set_subscription(broker_id_, ce_token_, pe_token_, window_); // ask again
                return;
            }
            emit page_primary_clicked();
        });
        connect(page_, &fincept::ui::EmptyState::secondary_clicked, this, &IntradayOIChart::page_secondary_clicked);
        wait_timer_ = new QTimer(this);
        wait_timer_->setSingleShot(true);
        wait_timer_->setInterval(10'000);
        connect(wait_timer_, &QTimer::timeout, this, [this]() {
            if (ce_samples_.isEmpty() && pe_samples_.isEmpty())
                show_no_history();
        });
    }
    cover_ = cover;
    if (!loading_)
        return;
    loading_->setGeometry(rect());
    page_->setGeometry(rect());
    loading_->setVisible(cover == Cover::Loading);
    page_->setVisible(cover == Cover::Page);
    if (cover == Cover::Loading)
        loading_->raise();
    else if (cover == Cover::Page)
        page_->raise();
}

void IntradayOIChart::show_no_history() {
    if (wait_timer_)
        wait_timer_->stop();
    set_cover(Cover::Page);
    fincept::ui::EmptyState::Spec s;
    s.glyph = QStringLiteral("○");
    s.glyph_tone = fincept::ui::EmptyState::Tone::Neutral;
    s.title = tr("No intraday OI recorded yet");
    s.message = tr("Open interest for this strike is sampled once a minute while the chain is open; the lines "
                   "fill in as the session trades. Outside market hours there may be nothing to draw.");
    s.primary = tr("REFRESH");
    s.illustration = true;
    s.illustration_shape = fincept::ui::LoadingSkeleton::Shape::Chart;
    page_->apply(s);
    page_is_no_history_ = true;
}

void IntradayOIChart::resizeEvent(QResizeEvent* e) {
    QChartView::resizeEvent(e);
    if (loading_)
        loading_->setGeometry(rect());
    if (page_)
        page_->setGeometry(rect());
}

void IntradayOIChart::attach_topics() {
    if (attached_)
        return;
    auto& hub = fincept::datahub::DataHub::instance();
    QPointer<IntradayOIChart> self = this;
    if (!ce_topic_.isEmpty()) {
        const qint64 ce_token = ce_token_;
        hub.subscribe(this, ce_topic_, [self, ce_token](const QVariant& v) {
            if (self)
                self->on_history(ce_token, v);
        });
        hub.request(ce_topic_, /*force*/ true); // cold start
    }
    if (!pe_topic_.isEmpty()) {
        const qint64 pe_token = pe_token_;
        hub.subscribe(this, pe_topic_, [self, pe_token](const QVariant& v) {
            if (self)
                self->on_history(pe_token, v);
        });
        hub.request(pe_topic_, /*force*/ true);
    }
    attached_ = true;
}

void IntradayOIChart::detach_topics() {
    if (!attached_)
        return;
    auto& hub = fincept::datahub::DataHub::instance();
    if (!ce_topic_.isEmpty())
        hub.unsubscribe(this, ce_topic_);
    if (!pe_topic_.isEmpty())
        hub.unsubscribe(this, pe_topic_);
    attached_ = false;
}

void IntradayOIChart::clear_subscription() {
    set_subscription({}, 0, 0);
}

void IntradayOIChart::on_history(qint64 token, const QVariant& v) {
    if (!v.canConvert<QVector<OISample>>())
        return;
    const auto samples = v.value<QVector<OISample>>();
    if (token == ce_token_) {
        ce_samples_ = samples;
        ce_received_ = true;
    } else if (token == pe_token_) {
        pe_samples_ = samples;
        pe_received_ = true;
    }
    replot();
    if (!ce_samples_.isEmpty() || !pe_samples_.isEmpty()) {
        if (wait_timer_)
            wait_timer_->stop();
        if (cover_ != Cover::None)
            set_cover(Cover::None);
        return;
    }
    // Every topic answered and none has a sample: nothing recorded yet.
    if ((ce_topic_.isEmpty() || ce_received_) && (pe_topic_.isEmpty() || pe_received_))
        show_no_history();
}

void IntradayOIChart::replot() {
    qint64 ts_min = std::numeric_limits<qint64>::max();
    qint64 ts_max = 0;
    double oi_max = 0;

    // Build each series' points first and hand them over with ONE replace():
    // clear() + a per-sample append() re-laid-out the whole line for every
    // sample, on every OI history publish.
    QList<QPointF> ce_points;
    ce_points.reserve(ce_samples_.size());
    for (const auto& s : ce_samples_) {
        const qint64 ms = s.ts_minute * 1000;
        ce_points.append(QPointF(double(ms), double(s.oi)));
        ts_min = std::min(ts_min, ms);
        ts_max = std::max(ts_max, ms);
        oi_max = std::max(oi_max, double(s.oi));
    }
    QList<QPointF> pe_points;
    pe_points.reserve(pe_samples_.size());
    for (const auto& s : pe_samples_) {
        const qint64 ms = s.ts_minute * 1000;
        pe_points.append(QPointF(double(ms), double(s.oi)));
        ts_min = std::min(ts_min, ms);
        ts_max = std::max(ts_max, ms);
        oi_max = std::max(oi_max, double(s.oi));
    }
    ce_series_->replace(ce_points);
    pe_series_->replace(pe_points);

    if (ts_min < ts_max) {
        axis_x_->setRange(QDateTime::fromMSecsSinceEpoch(ts_min), QDateTime::fromMSecsSinceEpoch(ts_max));
    } else {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        axis_x_->setRange(QDateTime::fromMSecsSinceEpoch(now - 3'600'000), QDateTime::fromMSecsSinceEpoch(now));
    }
    axis_y_->setRange(0, oi_max > 0 ? oi_max * 1.1 : 1);
}

void IntradayOIChart::showEvent(QShowEvent* e) {
    QChartView::showEvent(e);
    attach_topics();
    if (cover_ == Cover::Loading && wait_timer_)
        wait_timer_->start();
}

void IntradayOIChart::hideEvent(QHideEvent* e) {
    QChartView::hideEvent(e);
    detach_topics();
    if (wait_timer_)
        wait_timer_->stop(); // no timer runs while hidden
}

void IntradayOIChart::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QChartView::changeEvent(event);
}

void IntradayOIChart::retranslateUi() {
    if (chart_)
        chart_->setTitle(tr("Intraday OI"));
    if (ce_series_)
        ce_series_->setName(tr("CE OI"));
    if (pe_series_)
        pe_series_->setName(tr("PE OI"));
}

} // namespace fincept::screens::fno
