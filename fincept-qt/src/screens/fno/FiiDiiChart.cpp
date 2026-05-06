#include "screens/fno/FiiDiiChart.h"

#include "ui/theme/Theme.h"

#include <QBarCategoryAxis>
#include <QBarSeries>
#include <QBarSet>
#include <QChart>
#include <QDate>
#include <QValueAxis>

#include <algorithm>
#include <cmath>

namespace fincept::screens::fno {

using fincept::services::options::FiiDiiDay;
using namespace fincept::ui;

namespace {

QString short_date(const QString& iso) {
    QDate d = QDate::fromString(iso, "yyyy-MM-dd");
    if (!d.isValid())
        return iso;
    return d.toString("dd-MMM");
}

QColor with_alpha(const QString& hex, int alpha) {
    QColor c(hex);
    c.setAlpha(alpha);
    return c;
}

}  // namespace

FiiDiiChart::FiiDiiChart(QWidget* parent) : QChartView(parent) {
    setRenderHint(QPainter::Antialiasing, true);

    chart_ = new QChart();
    chart_->setBackgroundBrush(QColor(colors::BG_BASE()));
    chart_->setPlotAreaBackgroundBrush(QColor(colors::BG_BASE()));
    chart_->setPlotAreaBackgroundVisible(true);
    chart_->setMargins(QMargins(0, 4, 0, 0));
    chart_->setTitle(QStringLiteral("Daily Net Flows (₹ Cr)"));
    chart_->setTitleBrush(QColor(colors::TEXT_SECONDARY()));
    QFont title_font = chart_->titleFont();
    title_font.setPointSize(9);
    title_font.setBold(true);
    chart_->setTitleFont(title_font);
    chart_->legend()->setVisible(true);
    chart_->legend()->setAlignment(Qt::AlignTop);
    chart_->legend()->setLabelColor(QColor(colors::TEXT_SECONDARY()));
    setChart(chart_);

    fii_set_ = new QBarSet(QStringLiteral("FII"));
    dii_set_ = new QBarSet(QStringLiteral("DII"));
    fii_set_->setColor(with_alpha(colors::AMBER(), 200));
    dii_set_->setColor(with_alpha(colors::POSITIVE(), 200));
    fii_set_->setBorderColor(Qt::transparent);
    dii_set_->setBorderColor(Qt::transparent);

    series_ = new QBarSeries();
    series_->append(fii_set_);
    series_->append(dii_set_);
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

void FiiDiiChart::set_data(const QVector<FiiDiiDay>& days) {
    fii_set_->remove(0, fii_set_->count());
    dii_set_->remove(0, dii_set_->count());
    if (days.isEmpty()) {
        axis_x_->clear();
        axis_y_->setRange(-1, 1);
        return;
    }

    QStringList categories;
    categories.reserve(days.size());
    double y_max = 0;
    double y_min = 0;
    for (const auto& d : days) {
        categories.append(short_date(d.date_iso));
        fii_set_->append(d.fii_net);
        dii_set_->append(d.dii_net);
        y_max = std::max({y_max, d.fii_net, d.dii_net});
        y_min = std::min({y_min, d.fii_net, d.dii_net});
    }
    axis_x_->clear();
    axis_x_->append(categories);
    const double pad = std::max(std::abs(y_max - y_min) * 0.05, 1.0);
    axis_y_->setRange(y_min - pad, y_max + pad);
}

} // namespace fincept::screens::fno
