#include "screens/fno/MultiStrikeOIChart.h"

#include "ui/theme/Theme.h"

#include <QBarCategoryAxis>
#include <QBarSet>
#include <QChart>
#include <QHorizontalBarSeries>
#include <QValueAxis>

#include <algorithm>
#include <cmath>

namespace fincept::screens::fno {

using fincept::services::options::OptionChain;
using fincept::services::options::OptionChainRow;
using namespace fincept::ui;

namespace {

constexpr int kBarAlpha = 180;

QColor with_alpha(const QString& hex, int alpha) {
    QColor c(hex);
    c.setAlpha(alpha);
    return c;
}

}  // namespace

MultiStrikeOIChart::MultiStrikeOIChart(QWidget* parent) : QChartView(parent) {
    setRenderHint(QPainter::Antialiasing, true);

    chart_ = new QChart();
    chart_->setBackgroundBrush(QColor(colors::BG_BASE()));
    chart_->setPlotAreaBackgroundBrush(QColor(colors::BG_BASE()));
    chart_->setPlotAreaBackgroundVisible(true);
    chart_->setMargins(QMargins(0, 4, 0, 0));
    chart_->setTitle(QStringLiteral("Open Interest by Strike"));
    chart_->setTitleBrush(QColor(colors::TEXT_SECONDARY()));
    QFont title_font = chart_->titleFont();
    title_font.setPointSize(9);
    title_font.setBold(true);
    chart_->setTitleFont(title_font);
    chart_->legend()->setVisible(false);
    setChart(chart_);

    ce_set_ = new QBarSet(QStringLiteral("CE OI"));
    pe_set_ = new QBarSet(QStringLiteral("PE OI"));
    ce_set_->setColor(with_alpha(colors::POSITIVE(), kBarAlpha));
    pe_set_->setColor(with_alpha(colors::NEGATIVE(), kBarAlpha));
    ce_set_->setBorderColor(Qt::transparent);
    pe_set_->setBorderColor(Qt::transparent);

    ce_series_ = new QHorizontalBarSeries();
    pe_series_ = new QHorizontalBarSeries();
    ce_series_->append(ce_set_);
    pe_series_->append(pe_set_);
    ce_series_->setLabelsVisible(false);
    pe_series_->setLabelsVisible(false);

    chart_->addSeries(ce_series_);
    chart_->addSeries(pe_series_);

    axis_y_ = new QBarCategoryAxis();
    axis_y_->setLabelsColor(QColor(colors::TEXT_SECONDARY()));
    axis_y_->setGridLineColor(QColor(colors::BORDER_DIM()));
    axis_y_->setLinePen(QPen(QColor(colors::BORDER_DIM()), 1));

    axis_x_ = new QValueAxis();
    axis_x_->setLabelFormat("%.0f");
    axis_x_->setLabelsColor(QColor(colors::TEXT_SECONDARY()));
    axis_x_->setGridLineColor(QColor(colors::BORDER_DIM()));
    axis_x_->setLinePen(QPen(QColor(colors::BORDER_DIM()), 1));

    chart_->addAxis(axis_y_, Qt::AlignLeft);
    chart_->addAxis(axis_x_, Qt::AlignBottom);
    ce_series_->attachAxis(axis_y_);
    ce_series_->attachAxis(axis_x_);
    pe_series_->attachAxis(axis_y_);
    pe_series_->attachAxis(axis_x_);
}

void MultiStrikeOIChart::set_chain(const OptionChain& chain) {
    ce_set_->remove(0, ce_set_->count());
    pe_set_->remove(0, pe_set_->count());
    if (chain.rows.isEmpty())
        return;

    // Find ATM index and clip to strike_window_ either side.
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
    categories.reserve(hi - lo + 1);
    double max_oi = 0;
    // Walk top-down so the highest strike sits at the top of the chart
    // (Sensibull convention).
    for (int i = hi; i >= lo; --i) {
        const OptionChainRow& row = chain.rows[i];
        const QString cat = QString::number(row.strike, 'f', row.strike < 100 ? 2 : 0);
        categories.append(cat);
        const double ce = double(row.ce_quote.oi);
        const double pe = double(row.pe_quote.oi);
        // Negate CE so it renders to the left of zero.
        ce_set_->append(-ce);
        pe_set_->append(pe);
        max_oi = std::max({max_oi, ce, pe});
    }
    axis_y_->clear();
    axis_y_->append(categories);
    if (max_oi > 0) {
        const double pad = max_oi * 0.05;
        axis_x_->setRange(-(max_oi + pad), max_oi + pad);
    } else {
        axis_x_->setRange(-1, 1);
    }
}

} // namespace fincept::screens::fno
