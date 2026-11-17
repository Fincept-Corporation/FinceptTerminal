#include "screens/fno/FiiDiiChart.h"

#include "screens/fno/FnoFormat.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
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

/// "Daily Net Flows (₹ Cr)": the flows are published in crores of rupees
/// (FiiDiiTypes.h); the unit follows the user's symbol style.
QString fiichart_title() {
    return fincept::ui::formatting::header_scaled(FiiDiiChart::tr("Daily Net Flows"), fno_ccy(),
                                                  fincept::ui::formatting::Scale::Crore);
}

} // namespace

FiiDiiChart::FiiDiiChart(QWidget* parent) : QChartView(parent) {
    setRenderHint(QPainter::Antialiasing, true);

    chart_ = new QChart();
    chart_->setPlotAreaBackgroundVisible(true);
    chart_->setMargins(QMargins(0, 4, 0, 0));
    chart_->setTitle(fiichart_title());
    chart_->legend()->setVisible(true);
    chart_->legend()->setAlignment(Qt::AlignTop);
    setChart(chart_);

    fii_set_ = new QBarSet(QStringLiteral("FII"));
    dii_set_ = new QBarSet(QStringLiteral("DII"));
    fii_set_->setBorderColor(Qt::transparent);
    dii_set_->setBorderColor(Qt::transparent);

    series_ = new QBarSeries();
    series_->append(fii_set_);
    series_->append(dii_set_);
    series_->setLabelsVisible(false);
    chart_->addSeries(series_);

    axis_x_ = new QBarCategoryAxis();
    axis_y_ = new QValueAxis();
    axis_y_->setLabelFormat("%.0f");

    chart_->addAxis(axis_x_, Qt::AlignBottom);
    chart_->addAxis(axis_y_, Qt::AlignLeft);
    series_->attachAxis(axis_x_);
    series_->attachAxis(axis_y_);

    refresh_theme();
    connect(&ThemeManager::instance(), &ThemeManager::theme_changed, this,
            [this](const ThemeTokens&) { refresh_theme(); });
    // The title carries the currency unit; the bars are crores and need nothing.
    fincept::ui::formatting::on_format_changed(this, [this]() { retranslateUi(); });
}

void FiiDiiChart::refresh_theme() {
    chart_->setBackgroundBrush(QColor(colors::BG_BASE()));
    chart_->setPlotAreaBackgroundBrush(QColor(colors::BG_BASE()));
    chart_->setTitleBrush(QColor(colors::TEXT_SECONDARY()));
    chart_->setTitleFont(fonts::font(-2, QFont::Bold));
    chart_->legend()->setLabelColor(QColor(colors::TEXT_SECONDARY()));
    chart_->legend()->setFont(fonts::font(-3));

    fii_set_->setColor(with_alpha(colors::AMBER(), 200));
    dii_set_->setColor(with_alpha(colors::POSITIVE(), 200));

    const QFont label_font = fonts::font(-3);
    for (QAbstractAxis* axis : {static_cast<QAbstractAxis*>(axis_x_), static_cast<QAbstractAxis*>(axis_y_)}) {
        axis->setLabelsColor(QColor(colors::TEXT_SECONDARY()));
        axis->setLabelsFont(label_font);
        axis->setGridLineColor(QColor(colors::BORDER_DIM()));
        axis->setLinePen(QPen(QColor(colors::BORDER_DIM()), 1));
    }
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

void FiiDiiChart::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QChartView::changeEvent(event);
}

void FiiDiiChart::retranslateUi() {
    if (chart_)
        chart_->setTitle(fiichart_title());
}

} // namespace fincept::screens::fno
