#pragma once
// MaFootballField — the M&A football field: one ui::RangeBar row per valuation method on one shared axis, with the
// current share price / offer drawn on every row. The ranges come from MaFootballFieldMath.h; this widget only
// draws them. Also used by the range check (Fairness module) to show its methods' ranges against the offer.

#include "screens/ma_analytics/MaFootballFieldMath.h"
#include "ui/charts/RangeBar.h"

#include <QCoreApplication>
#include <QVector>
#include <QWidget>

#include <functional>

class QGridLayout;

namespace fincept::screens {

class MaFootballField : public QWidget {
    Q_DECLARE_TR_FUNCTIONS(MaFootballField)

  public:
    explicit MaFootballField(QWidget* parent = nullptr);

    /// Rows (only valid ones are drawn) on the axis [axis_low, axis_high]; `markers` (price, offer) go on every row.
    void set_rows(const QVector<ma_ff::Range>& rows, double axis_low, double axis_high,
                  const QVector<fincept::ui::RangeMarker>& markers, const std::function<QString(double)>& format);

  private:
    QGridLayout* grid_ = nullptr;
};

} // namespace fincept::screens
