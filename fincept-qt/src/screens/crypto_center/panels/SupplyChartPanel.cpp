#include "screens/crypto_center/panels/SupplyChartPanel.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "services/wallet/WalletTypes.h"
#include "ui/theme/Theme.h"

#include <QChart>
#include <QChartView>
#include <QColor>
#include <QDateTime>
#include <QDateTimeAxis>
#include <QFrame>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QLabel>
#include <QLineSeries>
#include <QPainter>
#include <QPen>
#include <QShowEvent>
#include <QStyle>
#include <QValueAxis>
#include <QVBoxLayout>

#include <cmath>

namespace fincept::screens::panels {

namespace {

constexpr const char* kTopicSupplyHistory = "treasury:supply_history";

QString font_stack() {
    return QStringLiteral(
        "'Consolas','Cascadia Mono','JetBrains Mono','SF Mono',monospace");
}

double atomic_to_billions(const QString& raw, int decimals) {
    if (raw.isEmpty()) return 0.0;
    bool ok = false;
    const auto units = raw.toULongLong(&ok);
    if (!ok) return 0.0;
    return (static_cast<double>(units) / std::pow(10.0, std::max(0, decimals))) / 1e9;
}

} // namespace

SupplyChartPanel::SupplyChartPanel(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("supplyChartPanel"));
    build_ui();
    apply_theme();

    auto& hub = fincept::datahub::DataHub::instance();
    connect(&hub, &fincept::datahub::DataHub::topic_error, this,
            &SupplyChartPanel::on_topic_error);
}

SupplyChartPanel::~SupplyChartPanel() = default;

void SupplyChartPanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Head
    auto* head = new QWidget(this);
    head->setObjectName(QStringLiteral("supplyChartHead"));
    head->setFixedHeight(34);
    auto* hl = new QHBoxLayout(head);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(8);
    auto* title = new QLabel(QStringLiteral("SUPPLY CHART · 12 MONTHS"), head);
    title->setObjectName(QStringLiteral("supplyChartTitle"));
    auto* legend = new QLabel(
        QStringLiteral("● TOTAL  ● CIRCULATING  ● BURNED"), head);
    legend->setObjectName(QStringLiteral("supplyChartLegend"));
    status_pill_ = new QLabel(QStringLiteral("LIVE"), head);
    status_pill_->setObjectName(QStringLiteral("supplyChartPill"));
    hl->addWidget(title);
    hl->addStretch();
    hl->addWidget(legend);
    hl->addSpacing(12);
    hl->addWidget(status_pill_);
    root->addWidget(head);

    // Body
    auto* body = new QWidget(this);
    body->setObjectName(QStringLiteral("supplyChartBody"));
    auto* bl = new QVBoxLayout(body);
    bl->setContentsMargins(8, 8, 8, 8);
    bl->setSpacing(8);

    // Chart
    chart_ = new QChart;
    chart_->setBackgroundVisible(false);
    chart_->setMargins(QMargins(0, 0, 0, 0));
    chart_->legend()->hide(); // we render our own in the head
    chart_->setBackgroundRoundness(0);

    using namespace ui::colors;
    total_series_ = new QLineSeries;
    total_series_->setName(QStringLiteral("Total"));
    total_series_->setPen(QPen(QColor(QString(BORDER_BRIGHT())), 2));

    circulating_series_ = new QLineSeries;
    circulating_series_->setName(QStringLiteral("Circulating"));
    circulating_series_->setPen(QPen(QColor(QString(TEXT_TERTIARY())), 2));

    burned_series_ = new QLineSeries;
    burned_series_->setName(QStringLiteral("Burned"));
    burned_series_->setPen(QPen(QColor(QString(AMBER())), 2));

    chart_->addSeries(total_series_);
    chart_->addSeries(circulating_series_);
    chart_->addSeries(burned_series_);

    x_axis_ = new QDateTimeAxis;
    x_axis_->setFormat(QStringLiteral("MMM yy"));
    x_axis_->setLabelsColor(QColor(QString(TEXT_TERTIARY())));
    x_axis_->setGridLineColor(QColor(QString(BORDER_DIM())));
    x_axis_->setLinePen(QPen(QColor(QString(BORDER_DIM()))));
    x_axis_->setTickCount(7);
    chart_->addAxis(x_axis_, Qt::AlignBottom);

    y_axis_ = new QValueAxis;
    y_axis_->setLabelFormat(QStringLiteral("%.1f B"));
    y_axis_->setLabelsColor(QColor(QString(TEXT_TERTIARY())));
    y_axis_->setGridLineColor(QColor(QString(BORDER_DIM())));
    y_axis_->setLinePen(QPen(QColor(QString(BORDER_DIM()))));
    y_axis_->setRange(0.0, 12.0); // placeholder; update_inputs sets real range
    chart_->addAxis(y_axis_, Qt::AlignLeft);

    total_series_->attachAxis(x_axis_);
    total_series_->attachAxis(y_axis_);
    circulating_series_->attachAxis(x_axis_);
    circulating_series_->attachAxis(y_axis_);
    burned_series_->attachAxis(x_axis_);
    burned_series_->attachAxis(y_axis_);

    chart_view_ = new QChartView(chart_, body);
    // No QPainter::Antialiasing render hint per P9 — large fills here are
    // expensive and the line series are 12 segments, so the staircase is
    // imperceptible.
    chart_view_->setMinimumHeight(220);
    bl->addWidget(chart_view_, 1);

    // Error strip
    error_strip_ = new QFrame(body);
    error_strip_->setObjectName(QStringLiteral("supplyChartErrorStrip"));
    auto* es_l = new QHBoxLayout(error_strip_);
    es_l->setContentsMargins(10, 6, 10, 6);
    es_l->setSpacing(8);
    auto* es_icon = new QLabel(QStringLiteral("!"), error_strip_);
    es_icon->setObjectName(QStringLiteral("supplyChartErrorIcon"));
    error_text_ = new QLabel(QString(), error_strip_);
    error_text_->setObjectName(QStringLiteral("supplyChartErrorText"));
    error_text_->setWordWrap(true);
    es_l->addWidget(es_icon);
    es_l->addWidget(error_text_, 1);
    error_strip_->hide();
    bl->addWidget(error_strip_);

    root->addWidget(body, 1);
}

void SupplyChartPanel::apply_theme() {
    using namespace ui::colors;
    const QString font = font_stack();

    const QString ss = QStringLiteral(
        "QWidget#supplyChartPanel { background:%1; }"
        "QWidget#supplyChartHead { background:%2; border-bottom:1px solid %3; }"
        "QLabel#supplyChartTitle { color:%4; font-family:%5; font-size:11px;"
        "  font-weight:700; letter-spacing:1.4px; background:transparent; }"
        "QLabel#supplyChartLegend { color:%6; font-family:%5; font-size:10px;"
        "  font-weight:600; letter-spacing:0.8px; background:transparent; }"
        "QLabel#supplyChartPill { color:%7; background:%8; border:1px solid %3;"
        "  font-family:%5; font-size:9px; font-weight:700; letter-spacing:1.2px;"
        "  padding:2px 8px; }"
        "QLabel#supplyChartPillDemo { color:%4; background:rgba(217,119,6,0.10);"
        "  border:1px solid %12; font-family:%5; font-size:9px; font-weight:700;"
        "  letter-spacing:1.2px; padding:2px 8px; }"
        "QWidget#supplyChartBody { background:%1; }"

        "QFrame#supplyChartErrorStrip { background:rgba(220,38,38,0.10);"
        "  border:1px solid %9; }"
        "QLabel#supplyChartErrorIcon { color:%9; font-family:%5; font-size:13px;"
        "  font-weight:700; background:transparent; }"
        "QLabel#supplyChartErrorText { color:%9; font-family:%5; font-size:11px;"
        "  background:transparent; }"
    )
        .arg(BG_BASE(),         // %1
             BG_SURFACE(),      // %2
             BORDER_DIM(),      // %3
             AMBER(),           // %4
             font,              // %5
             TEXT_TERTIARY(),   // %6
             TEXT_PRIMARY(),    // %7
             BG_RAISED(),       // %8
             NEGATIVE())        // %9
        .arg(BG_HOVER(),                       // %10
             BORDER_BRIGHT(),                  // %11
             QStringLiteral("#78350f"));       // %12 darker amber

    setStyleSheet(ss);
}

// ── Lifecycle ──────────────────────────────────────────────────────────────

void SupplyChartPanel::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    auto& hub = fincept::datahub::DataHub::instance();
    hub.subscribe(this, QString::fromLatin1(kTopicSupplyHistory),
                  [this](const QVariant& v) { on_supply_history_update(v); });
    hub.request(QString::fromLatin1(kTopicSupplyHistory), /*force=*/false);
}

void SupplyChartPanel::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    fincept::datahub::DataHub::instance().unsubscribe(this);
}

// ── Updates ────────────────────────────────────────────────────────────────

void SupplyChartPanel::on_supply_history_update(const QVariant& v) {
    if (!v.canConvert<QVector<fincept::wallet::SupplyHistoryPoint>>()) return;
    const auto pts = v.value<QVector<fincept::wallet::SupplyHistoryPoint>>();
    if (pts.isEmpty()) return;

    // Mock detection: the supply-history vector itself carries no flag, but
    // both treasury:* topics share a producer (BuybackBurnService) and a
    // refresh path. Peek treasury:buyback_epoch — the canonical mock signal
    // — and mirror its flag onto our pill so all three panels read DEMO
    // together when the worker endpoint is unconfigured.
    bool is_mock = false;
    const auto epoch_v = fincept::datahub::DataHub::instance().peek(
        QStringLiteral("treasury:buyback_epoch"));
    if (epoch_v.canConvert<fincept::wallet::BuybackEpoch>()) {
        is_mock = epoch_v.value<fincept::wallet::BuybackEpoch>().is_mock;
    }
    update_demo_chip(is_mock);

    total_series_->clear();
    circulating_series_->clear();
    burned_series_->clear();

    double y_min = std::numeric_limits<double>::max();
    double y_max = std::numeric_limits<double>::lowest();
    qint64 x_min = std::numeric_limits<qint64>::max();
    qint64 x_max = std::numeric_limits<qint64>::lowest();

    for (const auto& p : pts) {
        const double t = atomic_to_billions(p.total_raw, p.decimals);
        const double c = atomic_to_billions(p.circulating_raw, p.decimals);
        const double b = atomic_to_billions(p.burned_raw, p.decimals);
        total_series_->append(static_cast<double>(p.ts_ms), t);
        circulating_series_->append(static_cast<double>(p.ts_ms), c);
        burned_series_->append(static_cast<double>(p.ts_ms), b);
        y_min = std::min({y_min, t, c, b});
        y_max = std::max({y_max, t, c, b});
        x_min = std::min(x_min, p.ts_ms);
        x_max = std::max(x_max, p.ts_ms);
    }

    if (x_min == std::numeric_limits<qint64>::max()) return; // nothing valid
    if (y_min == y_max) y_max = y_min + 1.0; // avoid degenerate axis

    x_axis_->setRange(QDateTime::fromMSecsSinceEpoch(x_min),
                      QDateTime::fromMSecsSinceEpoch(x_max));
    const double pad = (y_max - y_min) * 0.05;
    y_axis_->setRange(std::max(0.0, y_min - pad), y_max + pad);
    clear_error_strip();
}

void SupplyChartPanel::on_topic_error(const QString& topic, const QString& error) {
    if (topic != QLatin1String(kTopicSupplyHistory)) return;
    show_error_strip(tr("Supply history feed error: %1").arg(error));
}

void SupplyChartPanel::show_error_strip(const QString& msg) {
    if (!error_strip_) return;
    error_text_->setText(msg);
    error_strip_->show();
}

void SupplyChartPanel::clear_error_strip() {
    if (error_strip_ && error_strip_->isVisible()) {
        error_strip_->hide();
        error_text_->clear();
    }
}

void SupplyChartPanel::update_demo_chip(bool is_mock) {
    if (is_mock) {
        status_pill_->setText(QStringLiteral("DEMO"));
        status_pill_->setObjectName(QStringLiteral("supplyChartPillDemo"));
    } else {
        status_pill_->setText(QStringLiteral("LIVE"));
        status_pill_->setObjectName(QStringLiteral("supplyChartPill"));
    }
    status_pill_->style()->unpolish(status_pill_);
    status_pill_->style()->polish(status_pill_);
}

} // namespace fincept::screens::panels
