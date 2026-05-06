#pragma once
// FiiDiiChart — twin-bar visualisation of daily FII vs DII net flows.
//
// One QBarSet per institutional category (FII, DII). Categorical X axis
// keyed on dates (DD-MMM short form). Negative values render below the
// zero baseline; positive above. The default Qt6 Charts QBarSeries handles
// signed values cleanly so we don't need any tinting trickery.

#include "services/options/FiiDiiTypes.h"

#include <QChartView>

class QBarCategoryAxis;
class QBarSeries;
class QBarSet;
class QValueAxis;

namespace fincept::screens::fno {

class FiiDiiChart : public QChartView {
    Q_OBJECT
  public:
    explicit FiiDiiChart(QWidget* parent = nullptr);

    /// Replace the bars. `days` is expected ascending by date_iso.
    void set_data(const QVector<fincept::services::options::FiiDiiDay>& days);

  private:
    QChart* chart_ = nullptr;
    QBarSeries* series_ = nullptr;
    QBarSet* fii_set_ = nullptr;
    QBarSet* dii_set_ = nullptr;
    QBarCategoryAxis* axis_x_ = nullptr;
    QValueAxis* axis_y_ = nullptr;
};

} // namespace fincept::screens::fno
