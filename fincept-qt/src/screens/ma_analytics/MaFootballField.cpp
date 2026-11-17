#include "screens/ma_analytics/MaFootballField.h"

#include "ui/theme/Theme.h"

#include <QFontMetrics>
#include <QGridLayout>
#include <QLabel>
#include <QVBoxLayout>

#include <algorithm>

namespace fincept::screens {

MaFootballField::MaFootballField(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("maFootballField"));
    // One selector stylesheet, set before any child exists.
    setStyleSheet(QString("QLabel#maFfName { color:%1; font-size:%2px; font-weight:600; background:transparent; }"
                          "QLabel#maFfNote { color:%3; font-size:%4px; background:transparent; }")
                      .arg(ui::colors::TEXT_PRIMARY())
                      .arg(ui::fonts::px(-2))
                      .arg(ui::colors::TEXT_TERTIARY())
                      .arg(ui::fonts::px(-3)));
    grid_ = new QGridLayout(this);
    grid_->setContentsMargins(0, 0, 0, 0);
    grid_->setHorizontalSpacing(12);
    grid_->setVerticalSpacing(6);
    grid_->setColumnStretch(1, 1);
}

void MaFootballField::set_rows(const QVector<ma_ff::Range>& rows, double axis_low, double axis_high,
                               const QVector<ui::RangeMarker>& markers, const std::function<QString(double)>& format) {
    while (QLayoutItem* item = grid_->takeAt(0)) {
        if (QWidget* w = item->widget())
            w->deleteLater();
        delete item;
    }
    // Names never wrap into the bars: the column is as wide as the longest name (within reason).
    const QFontMetrics fm(ui::fonts::font(-2, QFont::DemiBold));
    int name_w = fm.horizontalAdvance(QStringLiteral("Trading comps · EV/Revenue"));
    for (const ma_ff::Range& r : rows)
        name_w = std::max(name_w, std::min(fm.horizontalAdvance(r.label), fm.averageCharWidth() * 34));
    int row = 0;
    for (const ma_ff::Range& r : rows) {
        if (!r.valid())
            continue;
        auto* names = new QWidget(this);
        auto* nl = new QVBoxLayout(names);
        nl->setContentsMargins(0, 0, 0, 0);
        nl->setSpacing(0);
        auto* name = new QLabel(r.label, names);
        name->setObjectName(QStringLiteral("maFfName"));
        auto* note = new QLabel(format(r.low) + QStringLiteral(" – ") + format(r.high), names);
        note->setObjectName(QStringLiteral("maFfNote"));
        names->setToolTip(r.note);
        nl->addWidget(name);
        nl->addWidget(note);
        names->setFixedWidth(name_w + 8);

        ui::RangeBarData d;
        d.low = axis_low;
        d.high = axis_high;
        d.fill_to_current = false;
        ui::RangeBand band;
        band.from = r.low;
        band.to = r.high;
        band.tone = ui::RangeBand::Tone::Accent;
        band.label = r.label + (r.note.isEmpty() ? QString() : QStringLiteral(": ") + r.note);
        d.bands = {band};
        if (std::isfinite(r.mid)) {
            ui::RangeMarker m;
            m.value = r.mid;
            m.label = tr("Mid");
            m.role = ui::RangeMarker::Role::Mean;
            d.markers << m;
        }
        d.markers += markers;
        auto* bar = new ui::RangeBar(this);
        bar->set_data(d);
        bar->set_value_formatter(format);
        bar->setMinimumHeight(52);
        bar->setToolTip(r.note);
        grid_->addWidget(names, row, 0, Qt::AlignVCenter);
        grid_->addWidget(bar, row, 1);
        ++row;
    }
}

} // namespace fincept::screens
