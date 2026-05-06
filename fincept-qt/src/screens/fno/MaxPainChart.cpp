#include "screens/fno/MaxPainChart.h"

#include "ui/theme/Theme.h"

#include <QBarCategoryAxis>
#include <QBarSeries>
#include <QBarSet>
#include <QChart>
#include <QValueAxis>

#include <algorithm>
#include <limits>

namespace fincept::screens::fno {

using fincept::services::options::OptionChain;
using fincept::services::options::OptionChainRow;
using namespace fincept::ui;

namespace {

QColor with_alpha(const QString& hex, int alpha) {
    QColor c(hex);
    c.setAlpha(alpha);
    return c;
}

}  // namespace

MaxPainChart::MaxPainChart(QWidget* parent) : QChartView(parent) {
    setRenderHint(QPainter::Antialiasing, true);

    chart_ = new QChart();
    chart_->setBackgroundBrush(QColor(colors::BG_BASE()));
    chart_->setPlotAreaBackgroundBrush(QColor(colors::BG_BASE()));
    chart_->setPlotAreaBackgroundVisible(true);
    chart_->setMargins(QMargins(0, 4, 0, 0));
    chart_->setTitle(QStringLiteral("Max Pain Profile"));
    chart_->setTitleBrush(QColor(colors::TEXT_SECONDARY()));
    QFont title_font = chart_->titleFont();
    title_font.setPointSize(9);
    title_font.setBold(true);
    chart_->setTitleFont(title_font);
    chart_->legend()->setVisible(false);
    setChart(chart_);

    others_set_ = new QBarSet(QStringLiteral("Pain"));
    min_set_ = new QBarSet(QStringLiteral("Max Pain"));
    others_set_->setColor(with_alpha(colors::TEXT_TERTIARY(), 140));
    min_set_->setColor(with_alpha(colors::AMBER(), 220));
    others_set_->setBorderColor(Qt::transparent);
    min_set_->setBorderColor(Qt::transparent);

    series_ = new QBarSeries();
    series_->append(others_set_);
    series_->append(min_set_);
    series_->setLabelsVisible(false);
    chart_->addSeries(series_);

    axis_x_ = new QBarCategoryAxis();
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
    series_->attachAxis(axis_x_);
    series_->attachAxis(axis_y_);
}

void MaxPainChart::set_chain(const OptionChain& chain) {
    others_set_->remove(0, others_set_->count());
    min_set_->remove(0, min_set_->count());
    if (chain.rows.isEmpty())
        return;

    // Find ATM index and clip to strike_window_ either side — same windowing
    // rule as MultiStrikeOIChart for visual consistency.
    int atm = 0;
    for (int i = 0; i < chain.rows.size(); ++i) {
        if (chain.rows[i].is_atm) {
            atm = i;
            break;
        }
    }
    const int lo = std::max(0, atm - strike_window_);
    const int hi = std::min<int>(chain.rows.size() - 1, atm + strike_window_);

    QStringList categories;
    QVector<double> pain_vals;
    categories.reserve(hi - lo + 1);
    pain_vals.reserve(hi - lo + 1);

    // Per-strike pain — O(N²) on the windowed slice. Cheap for 20 strikes.
    double min_pain = std::numeric_limits<double>::infinity();
    int min_idx = 0;
    for (int i = lo; i <= hi; ++i) {
        const double S = chain.rows[i].strike;
        double pain = 0;
        for (const auto& r : chain.rows) {
            if (r.strike < S)
                pain += (S - r.strike) * double(r.ce_quote.oi);
            else if (r.strike > S)
                pain += (r.strike - S) * double(r.pe_quote.oi);
        }
        pain_vals.append(pain);
        categories.append(QString::number(S, 'f', S < 100 ? 2 : 0));
        if (pain < min_pain) {
            min_pain = pain;
            min_idx = i - lo;
        }
    }

    // Stack the two sets so each strike has a value in exactly one of them.
    for (int i = 0; i < pain_vals.size(); ++i) {
        if (i == min_idx) {
            others_set_->append(0.0);
            min_set_->append(pain_vals[i]);
        } else {
            others_set_->append(pain_vals[i]);
            min_set_->append(0.0);
        }
    }

    axis_x_->clear();
    axis_x_->append(categories);
    double max_pain = 0;
    for (double v : pain_vals)
        max_pain = std::max(max_pain, v);
    axis_y_->setRange(0, max_pain * 1.05);
}

} // namespace fincept::screens::fno
