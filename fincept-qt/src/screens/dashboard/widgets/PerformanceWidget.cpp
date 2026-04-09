#include "screens/dashboard/widgets/PerformanceWidget.h"

#include "ui/theme/Theme.h"

#include <QFrame>

#include <cmath>

namespace fincept::screens::widgets {

PerformanceWidget::PerformanceWidget(QWidget* parent)
    : BaseWidget("PERFORMANCE TRACKER", parent, ui::colors::POSITIVE()) {
    auto* vl = content_layout();

    // We'll show metrics derived from real benchmark ETF data
    QStringList labels = {"S&P 500 Daily",         "NASDAQ Daily",         "DOW Daily", "Russell 2000 Daily",
                          "S&P 500 vs DOW Spread", "NASDAQ vs S&P Spread", "VIX Level", "Gold Daily"};

    for (const auto& label : labels) {
        auto* row = new QWidget(this);
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(8, 4, 8, 4);

        MetricRow mr;
        mr.row_widget = row;
        mr.label = new QLabel(label);
        rl->addWidget(mr.label);
        rl->addStretch();

        mr.period = new QLabel("TODAY");
        rl->addWidget(mr.period);

        mr.value = new QLabel("--");
        rl->addWidget(mr.value);

        rows_.append(mr);
        vl->addWidget(row);
    }
    vl->addStretch();

    connect(this, &BaseWidget::refresh_requested, this, &PerformanceWidget::refresh_data);

    apply_styles();
    set_loading(true);
    refresh_data();
}

void PerformanceWidget::apply_styles() {
    for (const auto& mr : rows_) {
        mr.row_widget->setStyleSheet(QString("border-bottom: 1px solid %1;").arg(ui::colors::BORDER_DIM()));
        mr.label->setStyleSheet(
            QString("color: %1; font-size: 11px; background: transparent;").arg(ui::colors::TEXT_SECONDARY()));
        mr.period->setStyleSheet(
            QString("color: %1; font-size: 9px; background: transparent;").arg(ui::colors::TEXT_TERTIARY()));
        mr.value->setStyleSheet(QString("color: %1; font-size: 11px; font-weight: bold; background: transparent;")
                                    .arg(ui::colors::TEXT_PRIMARY()));
    }
}

void PerformanceWidget::on_theme_changed() {
    apply_styles();
}

void PerformanceWidget::refresh_data() {
    set_loading(true);

    // Fetch benchmark ETFs + VIX + Gold
    QStringList syms = {"^GSPC", "^IXIC", "^DJI", "^RUT", "^VIX", "GC=F"};

    services::MarketDataService::instance().fetch_quotes(syms, [this](bool ok, QVector<services::QuoteData> quotes) {
        set_loading(false);
        if (!ok || quotes.isEmpty())
            return;
        populate(quotes);
    });
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
        QString color = val > 0   ? ui::colors::POSITIVE()
                        : val < 0 ? ui::colors::NEGATIVE()
                                  : ui::colors::TEXT_PRIMARY();
        rows_[idx].value->setStyleSheet(
            QString("color: %1; font-size: 11px; font-weight: bold; background: transparent;").arg(color));
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
            QString color = vix > 25   ? ui::colors::NEGATIVE()
                            : vix > 18 ? ui::colors::WARNING()
                                       : ui::colors::POSITIVE();
            rows_[6].value->setStyleSheet(
                QString("color: %1; font-size: 11px; font-weight: bold; background: transparent;").arg(color));
        }
    }

    // Gold daily
    if (map.contains("GC=F"))
        set_row(7, map["GC=F"]->change_pct);
}

} // namespace fincept::screens::widgets
