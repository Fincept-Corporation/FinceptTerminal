#include "ui/charts/layers/SupportResistanceLayer.h"

#include <algorithm>
#include <cmath>

namespace fincept::ui {

SupportResistanceLayer::SupportResistanceLayer(QObject* parent)
    : HorizontalLineLayer("sr_auto", "Auto S/R", parent) {}

void SupportResistanceLayer::compute(const QVector<CandleData>& candles) {
    const auto clusters = find_clusters(candles);

    QVector<HorizontalLevel> levels;
    for (const auto& c : clusters) {
        HorizontalLevel level;
        level.price = c.price;
        level.label = QStringLiteral("S/R %1 (%2x)")
                          .arg(c.price, 0, 'f', 2)
                          .arg(c.touches);
        level.color = QColor("#808080");
        level.style = Qt::DashLine;
        levels.append(level);
    }
    set_levels(levels);
}

QVector<SupportResistanceLayer::Cluster>
SupportResistanceLayer::find_clusters(const QVector<CandleData>& candles, double tolerance_pct) {
    if (candles.size() < 5)
        return {};

    QVector<double> extremes;
    extremes.reserve(candles.size() * 2);
    for (const auto& c : candles) {
        extremes.append(c.high);
        extremes.append(c.low);
    }

    std::sort(extremes.begin(), extremes.end());

    QVector<Cluster> clusters;
    int i = 0;
    while (i < extremes.size()) {
        const double ref = extremes[i];
        const double tolerance = ref * tolerance_pct / 100.0;
        double sum = 0;
        int count = 0;
        int j = i;
        while (j < extremes.size() && extremes[j] <= ref + tolerance) {
            sum += extremes[j];
            ++count;
            ++j;
        }
        if (count >= 3) {
            const double recency = 1.0;
            clusters.append({sum / count, count, count * recency});
        }
        i = j;
    }

    std::sort(clusters.begin(), clusters.end(),
              [](const Cluster& a, const Cluster& b) { return a.weight > b.weight; });

    const int max_levels = 6;
    if (clusters.size() > max_levels)
        clusters.resize(max_levels);

    return clusters;
}

} // namespace fincept::ui
