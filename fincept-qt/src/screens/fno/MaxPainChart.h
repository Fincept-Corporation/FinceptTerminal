#pragma once
// MaxPainChart — vertical bars showing total option-writer pain per strike,
// with the minimum (the "max pain" point) highlighted in amber.
//
// Pain at hypothetical settlement S* :
//   Σ_{K<S*} (S*−K)·CE_OI(K)  +  Σ_{K>S*} (K−S*)·PE_OI(K)
//
// The strike that minimises this sum is what option writers would prefer
// expiry to settle at. The producer (`OptionChainService`) already publishes
// just the min strike; this widget recomputes per-strike pain so we can
// render the whole curve.

#include "services/options/OptionChainTypes.h"

#include <QChartView>

class QBarCategoryAxis;
class QBarSeries;
class QBarSet;
class QValueAxis;

namespace fincept::screens::fno {

class MaxPainChart : public QChartView {
    Q_OBJECT
  public:
    explicit MaxPainChart(QWidget* parent = nullptr);

    void set_chain(const fincept::services::options::OptionChain& chain);
    void set_strike_window(int n) { strike_window_ = n > 0 ? n : 10; }

  private:
    QChart* chart_ = nullptr;
    QBarSeries* series_ = nullptr;
    QBarSet* min_set_ = nullptr;        // single-bar amber marker
    QBarSet* others_set_ = nullptr;     // dim grey for non-min strikes
    QBarCategoryAxis* axis_x_ = nullptr;
    QValueAxis* axis_y_ = nullptr;
    int strike_window_ = 10;
};

} // namespace fincept::screens::fno
