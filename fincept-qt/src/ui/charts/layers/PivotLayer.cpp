#include "ui/charts/layers/PivotLayer.h"

namespace fincept::ui {

PivotLayer::PivotLayer(PivotType type, QObject* parent)
    : HorizontalLineLayer("pivot_std", "Pivot Points", parent), type_(type) {}

void PivotLayer::compute(const QVector<CandleData>& candles) {
    if (candles.size() < 2)
        return;

    double day_high = 0, day_low = 1e18, day_close = 0;
    const int64_t last_ts = candles.last().timestamp;
    const int64_t one_day_ms = 86400000LL;

    for (const auto& c : candles) {
        if (c.timestamp < last_ts - one_day_ms)
            continue;
        if (c.timestamp >= last_ts)
            break;
        if (c.high > day_high) day_high = c.high;
        if (c.low < day_low) day_low = c.low;
        day_close = c.close;
    }

    if (day_high <= 0 || day_low >= 1e18)
        return;

    const auto r = indicators::pivot_points(day_high, day_low, day_close, type_);

    QVector<HorizontalLevel> levels;
    auto add = [&](double price, const QString& label, const QColor& color) {
        levels.append({price, label, color, Qt::DotLine});
    };

    add(r.pp, "PP", QColor("#e5e5e5"));
    add(r.r1, "R1", QColor("#16a34a"));
    add(r.r2, "R2", QColor("#16a34a"));
    add(r.r3, "R3", QColor("#16a34a"));
    add(r.s1, "S1", QColor("#dc2626"));
    add(r.s2, "S2", QColor("#dc2626"));
    add(r.s3, "S3", QColor("#dc2626"));

    set_levels(levels);
}

void PivotLayer::set_pivot_type(PivotType t) {
    type_ = t;
}

} // namespace fincept::ui
