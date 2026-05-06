#include "screens/fno/IntradayOIChart.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "services/options/OISnapshotter.h"
#include "ui/theme/Theme.h"

#include <QChart>
#include <QDateTime>
#include <QDateTimeAxis>
#include <QHideEvent>
#include <QLineSeries>
#include <QShowEvent>
#include <QValueAxis>

#include <algorithm>
#include <limits>

namespace fincept::screens::fno {

using fincept::services::options::OISample;
using fincept::services::options::OISnapshotter;
using namespace fincept::ui;

namespace {

constexpr int kPollIntervalMs = 60'000;  // OISnapshotter flush cadence.

}  // namespace

IntradayOIChart::IntradayOIChart(QWidget* parent) : QChartView(parent) {
    setRenderHint(QPainter::Antialiasing, true);

    chart_ = new QChart();
    chart_->setBackgroundBrush(QColor(colors::BG_BASE()));
    chart_->setPlotAreaBackgroundBrush(QColor(colors::BG_BASE()));
    chart_->setPlotAreaBackgroundVisible(true);
    chart_->setMargins(QMargins(0, 4, 0, 0));
    chart_->setTitle(QStringLiteral("Intraday OI"));
    chart_->setTitleBrush(QColor(colors::TEXT_SECONDARY()));
    QFont title_font = chart_->titleFont();
    title_font.setPointSize(9);
    title_font.setBold(true);
    chart_->setTitleFont(title_font);
    chart_->legend()->setVisible(true);
    chart_->legend()->setAlignment(Qt::AlignTop);
    chart_->legend()->setLabelColor(QColor(colors::TEXT_SECONDARY()));
    setChart(chart_);

    ce_series_ = new QLineSeries();
    ce_series_->setName(QStringLiteral("CE OI"));
    // Brace-init dodges the vexing-parse on single-arg QColor temporaries.
    QPen ce_pen{QColor(colors::POSITIVE())};
    ce_pen.setWidth(2);
    ce_series_->setPen(ce_pen);

    pe_series_ = new QLineSeries();
    pe_series_->setName(QStringLiteral("PE OI"));
    QPen pe_pen{QColor(colors::NEGATIVE())};
    pe_pen.setWidth(2);
    pe_series_->setPen(pe_pen);

    chart_->addSeries(ce_series_);
    chart_->addSeries(pe_series_);

    axis_x_ = new QDateTimeAxis();
    axis_x_->setFormat("HH:mm");
    axis_x_->setLabelsColor(QColor(colors::TEXT_SECONDARY()));
    axis_x_->setGridLineColor(QColor(colors::BORDER_DIM()));
    axis_x_->setLinePen(QPen(QColor(colors::BORDER_DIM()), 1));

    axis_y_ = new QValueAxis();
    axis_y_->setLabelFormat("%.0f");
    axis_y_->setLabelsColor(QColor(colors::TEXT_SECONDARY()));
    axis_y_->setGridLineColor(QColor(colors::BORDER_DIM()));
    axis_y_->setLinePen(QPen(QColor(colors::BORDER_DIM()), 1));

    chart_->addAxis(axis_x_, Qt::AlignBottom);
    chart_->addAxis(axis_y_, Qt::AlignLeft);
    ce_series_->attachAxis(axis_x_);
    ce_series_->attachAxis(axis_y_);
    pe_series_->attachAxis(axis_x_);
    pe_series_->attachAxis(axis_y_);

    poll_timer_.setInterval(kPollIntervalMs);
    poll_timer_.setSingleShot(false);
    connect(&poll_timer_, &QTimer::timeout, this, &IntradayOIChart::poll_refresh);
}

IntradayOIChart::~IntradayOIChart() {
    fincept::datahub::DataHub::instance().unsubscribe(this);
}

void IntradayOIChart::set_subscription(const QString& broker_id, qint64 ce_token, qint64 pe_token,
                                       const QString& window) {
    auto& hub = fincept::datahub::DataHub::instance();
    // Drop prior subscriptions.
    if (!ce_topic_.isEmpty())
        hub.unsubscribe(this, ce_topic_);
    if (!pe_topic_.isEmpty())
        hub.unsubscribe(this, pe_topic_);

    broker_id_ = broker_id;
    ce_token_ = ce_token;
    pe_token_ = pe_token;
    window_ = window;
    ce_samples_.clear();
    pe_samples_.clear();

    if (broker_id.isEmpty() || (ce_token == 0 && pe_token == 0)) {
        ce_topic_.clear();
        pe_topic_.clear();
        replot();
        return;
    }

    if (ce_token != 0) {
        ce_topic_ = OISnapshotter::history_topic(broker_id, ce_token, window);
        hub.subscribe(this, ce_topic_, [this, ce_token](const QVariant& v) { on_history(ce_token, v); });
    } else {
        ce_topic_.clear();
    }
    if (pe_token != 0) {
        pe_topic_ = OISnapshotter::history_topic(broker_id, pe_token, window);
        hub.subscribe(this, pe_topic_, [this, pe_token](const QVariant& v) { on_history(pe_token, v); });
    } else {
        pe_topic_.clear();
    }
    poll_refresh();
}

void IntradayOIChart::clear_subscription() {
    set_subscription({}, 0, 0);
}

void IntradayOIChart::on_history(qint64 token, const QVariant& v) {
    if (!v.canConvert<QVector<OISample>>())
        return;
    const auto samples = v.value<QVector<OISample>>();
    if (token == ce_token_)
        ce_samples_ = samples;
    else if (token == pe_token_)
        pe_samples_ = samples;
    replot();
}

void IntradayOIChart::replot() {
    ce_series_->clear();
    pe_series_->clear();

    qint64 ts_min = std::numeric_limits<qint64>::max();
    qint64 ts_max = 0;
    double oi_max = 0;

    for (const auto& s : ce_samples_) {
        const qint64 ms = s.ts_minute * 1000;
        ce_series_->append(double(ms), double(s.oi));
        ts_min = std::min(ts_min, ms);
        ts_max = std::max(ts_max, ms);
        oi_max = std::max(oi_max, double(s.oi));
    }
    for (const auto& s : pe_samples_) {
        const qint64 ms = s.ts_minute * 1000;
        pe_series_->append(double(ms), double(s.oi));
        ts_min = std::min(ts_min, ms);
        ts_max = std::max(ts_max, ms);
        oi_max = std::max(oi_max, double(s.oi));
    }

    if (ts_min < ts_max) {
        axis_x_->setRange(QDateTime::fromMSecsSinceEpoch(ts_min),
                          QDateTime::fromMSecsSinceEpoch(ts_max));
    } else {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        axis_x_->setRange(QDateTime::fromMSecsSinceEpoch(now - 3'600'000), QDateTime::fromMSecsSinceEpoch(now));
    }
    axis_y_->setRange(0, oi_max > 0 ? oi_max * 1.1 : 1);
}

void IntradayOIChart::poll_refresh() {
    auto& hub = fincept::datahub::DataHub::instance();
    if (!ce_topic_.isEmpty())
        hub.request(ce_topic_, /*force*/ true);
    if (!pe_topic_.isEmpty())
        hub.request(pe_topic_, /*force*/ true);
}

void IntradayOIChart::showEvent(QShowEvent* e) {
    QChartView::showEvent(e);
    poll_timer_.start();
}

void IntradayOIChart::hideEvent(QHideEvent* e) {
    QChartView::hideEvent(e);
    poll_timer_.stop();
}

} // namespace fincept::screens::fno
