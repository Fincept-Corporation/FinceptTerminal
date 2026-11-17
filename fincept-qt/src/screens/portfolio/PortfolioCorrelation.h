// src/screens/portfolio/PortfolioCorrelation.h
//
// Correlation matrix presentation shared by the main view's allocation panel and
// the ANALYTICS view: every holding (no top-N cut), ordered by average-linkage
// clustering on the distance 1 − r so co-moving groups sit together, and coloured
// so it READS for diversification — high positive correlation (concentrated risk)
// in the "down" colour, negative correlation (diversifying) in the "up" colour.
// MatrixHeatmap colours low values with the negative end of the diverging scale,
// so the matrix is handed −r and its formatter prints r back.
#pragma once

#include "ui/charts/MatrixHeatmap.h"
#include "ui/formatting/NumberFormat.h"

#include <QHash>
#include <QStringList>
#include <QVector>

#include <algorithm>
#include <cmath>
#include <limits>

namespace fincept::screens::pfcorr {

struct Matrix {
    QStringList labels;             ///< display order (clustered)
    QVector<QVector<double>> r;     ///< r[i][j] in display order; NaN = unknown
    double avg_pair = std::numeric_limits<double>::quiet_NaN(); ///< mean off-diagonal r
    QString max_pair;               ///< "A / B"
    double max_r = std::numeric_limits<double>::quiet_NaN();
    QString min_pair;
    double min_r = std::numeric_limits<double>::quiet_NaN();
};

inline double lookup(const QHash<QString, double>& pairs, const QString& a, const QString& b) {
    if (a == b)
        return 1.0;
    auto it = pairs.constFind(a + QLatin1Char('|') + b);
    if (it != pairs.constEnd())
        return *it;
    it = pairs.constFind(b + QLatin1Char('|') + a);
    return it != pairs.constEnd() ? *it : std::numeric_limits<double>::quiet_NaN();
}

/// `pairs` keyed "A|B" (either order). `symbols` = the holdings to show.
inline Matrix build(const QHash<QString, double>& pairs, const QStringList& symbols) {
    Matrix m;
    QStringList syms;
    for (const auto& s : symbols) {
        bool any = false;
        for (const auto& t : symbols)
            if (t != s && std::isfinite(lookup(pairs, s, t))) {
                any = true;
                break;
            }
        if (any)
            syms << s;
    }
    const int n = static_cast<int>(syms.size());
    if (n < 2)
        return m;
    // Distance matrix and average-linkage agglomeration.
    QVector<QVector<double>> d(n, QVector<double>(n, 0.0));
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j) {
            const double r = lookup(pairs, syms[i], syms[j]);
            d[i][j] = std::isfinite(r) ? 1.0 - r : 1.0;
        }
    QVector<QVector<int>> clusters;
    for (int i = 0; i < n; ++i)
        clusters.append(QVector<int>{i});
    while (clusters.size() > 1) {
        int ba = 0, bb = 1;
        double best = std::numeric_limits<double>::max();
        for (int a = 0; a < clusters.size(); ++a)
            for (int b = a + 1; b < clusters.size(); ++b) {
                double s = 0;
                for (int x : clusters[a])
                    for (int y : clusters[b])
                        s += d[x][y];
                s /= double(clusters[a].size() * clusters[b].size());
                if (s < best) {
                    best = s;
                    ba = a;
                    bb = b;
                }
            }
        // Join in the orientation that keeps the closest leaves adjacent.
        QVector<int> A = clusters[ba];
        QVector<int> B = clusters[bb];
        const double o1 = d[A.last()][B.first()], o2 = d[A.last()][B.last()], o3 = d[A.first()][B.first()],
                     o4 = d[A.first()][B.last()];
        const double mn = std::min({o1, o2, o3, o4});
        if (mn == o2) {
            std::reverse(B.begin(), B.end());
        } else if (mn == o3) {
            std::reverse(A.begin(), A.end());
        } else if (mn == o4) {
            std::reverse(A.begin(), A.end());
            std::reverse(B.begin(), B.end());
        }
        A += B;
        clusters[ba] = A;
        clusters.removeAt(bb);
    }
    const QVector<int> order = clusters.first();
    double sum = 0;
    int cnt = 0;
    m.r = QVector<QVector<double>>(n, QVector<double>(n, std::numeric_limits<double>::quiet_NaN()));
    for (int i = 0; i < n; ++i) {
        m.labels << syms[order[i]];
        for (int j = 0; j < n; ++j) {
            const double r = lookup(pairs, syms[order[i]], syms[order[j]]);
            m.r[i][j] = r;
            if (j > i && std::isfinite(r)) {
                sum += r;
                ++cnt;
                if (!std::isfinite(m.max_r) || r > m.max_r) {
                    m.max_r = r;
                    m.max_pair = syms[order[i]] + QStringLiteral(" / ") + syms[order[j]];
                }
                if (!std::isfinite(m.min_r) || r < m.min_r) {
                    m.min_r = r;
                    m.min_pair = syms[order[i]] + QStringLiteral(" / ") + syms[order[j]];
                }
            }
        }
    }
    if (cnt > 0)
        m.avg_pair = sum / cnt;
    return m;
}

/// Hand `m` to `w`: −r values on a fixed (−1, 1) diverging scale, r printed back.
inline void show(ui::MatrixHeatmap* w, const Matrix& m) {
    QVector<QVector<double>> neg = m.r;
    for (auto& row : neg)
        for (double& v : row)
            v = std::isfinite(v) ? -v : v;
    w->set_cluster(false); // already ordered on the real correlation
    w->set_diverging_scale(-1.0, 1.0, 0.0);
    w->set_value_formatter([](double v) { return ui::formatting::number(std::isfinite(v) ? -v : v, 2); });
    w->set_matrix(m.labels, m.labels, neg);
}

} // namespace fincept::screens::pfcorr
