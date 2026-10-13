#include "screens/dashboard/widgets/PerformanceWidget.h"

#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "ui/theme/Theme.h"

#include <QFrame>
#include <QStyle>

#include <cmath>

namespace {
inline const QStringList kPerfSymbols = {"^GSPC", "^IXIC", "^DJI", "^RUT", "^VIX", "GC=F"};

// A value label's colour is a "tone" property matched in the rows host's
// stylesheet. populate() runs on every hub delivery burst and used to call
// setStyleSheet() on every row (a CSS re-parse each); the colour only changes
// when the tone does, so re-polish only then (CLAUDE.md P7).
void perf_set_tone(QLabel* label, const char* tone) {
    if (label->property("tone").toString() == QLatin1String(tone))
        return;
    label->setProperty("tone", QString::fromLatin1(tone));
    label->style()->unpolish(label);
    label->style()->polish(label);
}
} // namespace

namespace fincept::screens::widgets {

PerformanceWidget::PerformanceWidget(QWidget* parent)
    : BaseWidget(tr("PERFORMANCE TRACKER"), parent, ui::colors::POSITIVE()) {
    auto* vl = content_layout();

    // One host carries the stylesheet for every row — see apply_styles().
    rows_host_ = new QWidget(this);
    rows_host_->setObjectName("perfRows");
    auto* host_layout = new QVBoxLayout(rows_host_);
    host_layout->setContentsMargins(0, 0, 0, 0);
    host_layout->setSpacing(vl->spacing());

    // We'll show metrics derived from real benchmark ETF data
    QStringList labels = {
        tr("S&P 500 Daily"),         tr("NASDAQ Daily"),         tr("DOW Daily"), tr("Russell 2000 Daily"),
        tr("S&P 500 vs DOW Spread"), tr("NASDAQ vs S&P Spread"), tr("VIX Level"), tr("Gold Daily")};

    for (const auto& label : labels) {
        auto* row = new QWidget(rows_host_);
        row->setObjectName("perfRow");
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(8, 4, 8, 4);

        MetricRow mr;
        mr.label = new QLabel(label);
        mr.label->setObjectName("perfMetric");
        rl->addWidget(mr.label);
        rl->addStretch();

        mr.period = new QLabel(tr("TODAY"));
        mr.period->setObjectName("perfPeriod");
        rl->addWidget(mr.period);

        mr.value = new QLabel("--");
        mr.value->setObjectName("perfValue");
        rl->addWidget(mr.value);

        rows_.append(mr);
        host_layout->addWidget(row);
    }
    vl->addWidget(rows_host_);
    vl->addStretch();

    connect(this, &BaseWidget::refresh_requested, this, &PerformanceWidget::refresh_data);

    apply_styles();
    set_loading(true);
}

void PerformanceWidget::apply_styles() {
    // Each row used to carry a selector-less "border-bottom" sheet, which Qt
    // applies to the row AND every label inside it — the second selector keeps
    // that look. A value label with no "tone" yet ("--") stays TEXT_PRIMARY.
    rows_host_->setStyleSheet(
        QString("QWidget#perfRow{border-bottom:1px solid %1;}"
                "QWidget#perfRow QLabel{border-bottom:1px solid %1;}"
                "QLabel#perfMetric{color:%2;font-size:11px;background:transparent;}"
                "QLabel#perfPeriod{color:%3;font-size:9px;background:transparent;}"
                "QLabel#perfValue{color:%4;font-size:11px;font-weight:bold;background:transparent;}"
                "QLabel#perfValue[tone=\"pos\"]{color:%5;}"
                "QLabel#perfValue[tone=\"neg\"]{color:%6;}"
                "QLabel#perfValue[tone=\"warn\"]{color:%7;}")
            .arg(ui::colors::BORDER_DIM(), ui::colors::TEXT_SECONDARY(), ui::colors::TEXT_TERTIARY(),
                 ui::colors::TEXT_PRIMARY(), ui::colors::POSITIVE(), ui::colors::NEGATIVE(), ui::colors::WARNING()));
}

void PerformanceWidget::on_theme_changed() {
    apply_styles();
}

void PerformanceWidget::showEvent(QShowEvent* e) {
    BaseWidget::showEvent(e);
    if (!hub_active_)
        hub_subscribe_all();
}

void PerformanceWidget::hideEvent(QHideEvent* e) {
    BaseWidget::hideEvent(e);
    if (hub_active_)
        hub_unsubscribe_all();
}

void PerformanceWidget::refresh_data() {
    auto& hub = datahub::DataHub::instance();
    QStringList topics;
    topics.reserve(kPerfSymbols.size());
    for (const auto& sym : kPerfSymbols)
        topics.append(QStringLiteral("market:quote:") + sym);
    hub.request(topics, /*force=*/true); // user-triggered: bypass min_interval
}

void PerformanceWidget::hub_subscribe_all() {
    auto& hub = datahub::DataHub::instance();
    set_loading_progress(row_cache_.size(), kPerfSymbols.size());
    for (const auto& sym : kPerfSymbols) {
        const QString topic = QStringLiteral("market:quote:") + sym;
        hub.subscribe(this, topic, [this, sym](const QVariant& v) {
            if (!v.canConvert<services::QuoteData>())
                return;
            row_cache_.insert(sym, v.value<services::QuoteData>());
            set_loading_progress(row_cache_.size(), kPerfSymbols.size());
            // One redraw per delivery burst, not one per symbol.
            schedule_render([this]() { rebuild_from_cache(); });
        });
    }
    hub_active_ = true;
}

void PerformanceWidget::hub_unsubscribe_all() {
    datahub::DataHub::instance().unsubscribe(this);
    hub_active_ = false;
}

void PerformanceWidget::rebuild_from_cache() {
    QVector<services::QuoteData> quotes;
    quotes.reserve(row_cache_.size());
    for (const auto& sym : kPerfSymbols) {
        auto it = row_cache_.constFind(sym);
        if (it != row_cache_.constEnd())
            quotes.append(it.value());
    }
    if (!quotes.isEmpty())
        populate(quotes);
}

void PerformanceWidget::populate(const QVector<services::QuoteData>& quotes) {
    // Build lookup by symbol
    QMap<QString, const services::QuoteData*> map;
    for (const auto& q : quotes)
        map[q.symbol] = &q;

    auto fmt_pct = [](double v) -> QString { return QString("%1%2%").arg(v >= 0 ? "+" : "").arg(v, 0, 'f', 2); };
    auto set_row = [&](int idx, double val) {
        if (idx >= rows_.size())
            return;
        rows_[idx].value->setText(fmt_pct(val));
        perf_set_tone(rows_[idx].value, val > 0 ? "pos" : (val < 0 ? "neg" : "flat"));
    };

    // S&P 500 daily
    if (map.contains("^GSPC"))
        set_row(0, map["^GSPC"]->change_pct);
    // NASDAQ daily
    if (map.contains("^IXIC"))
        set_row(1, map["^IXIC"]->change_pct);
    // DOW daily
    if (map.contains("^DJI"))
        set_row(2, map["^DJI"]->change_pct);
    // Russell daily
    if (map.contains("^RUT"))
        set_row(3, map["^RUT"]->change_pct);

    // Spreads
    if (map.contains("^GSPC") && map.contains("^DJI"))
        set_row(4, map["^GSPC"]->change_pct - map["^DJI"]->change_pct);
    if (map.contains("^IXIC") && map.contains("^GSPC"))
        set_row(5, map["^IXIC"]->change_pct - map["^GSPC"]->change_pct);

    // VIX — show absolute value, not change
    if (map.contains("^VIX")) {
        double vix = map["^VIX"]->price;
        if (6 < rows_.size()) {
            rows_[6].value->setText(QString::number(vix, 'f', 2));
            perf_set_tone(rows_[6].value, vix > 25 ? "neg" : (vix > 18 ? "warn" : "pos"));
        }
    }

    // Gold daily
    if (map.contains("GC=F"))
        set_row(7, map["GC=F"]->change_pct);
}

void PerformanceWidget::retranslateUi() {
    BaseWidget::retranslateUi();
    set_title(tr("PERFORMANCE TRACKER"));
    rebuild_from_cache(); // re-derives metric labels from cached quotes
}

} // namespace fincept::screens::widgets
