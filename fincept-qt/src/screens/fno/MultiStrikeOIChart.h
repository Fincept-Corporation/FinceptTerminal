#pragma once
// MultiStrikeOIChart — Sensibull's signature horizontal OI bar visual.
//
//   ┌──────────────┬─┬──────────────┐
//   │              │ │              │
//   │  ◀── CE OI   │S│   PE OI ──▶  │  one row per strike
//   │              │ │              │
//   └──────────────┴─┴──────────────┘
//
// CE bars extend leftward (green tint), PE bars rightward (red tint). The
// ATM strike row gets an amber category label. The chart uses two
// QHorizontalBarSeries with mirrored signs — CE values are negated so they
// render to the left of zero on a single value axis.
//
// Limited to ±10 strikes around ATM by default to keep the chart readable
// (a deep NIFTY chain can have 80+ strikes). Caller can override the
// window via `set_strike_window()`.

#include "services/options/OptionChainTypes.h"

#include <QChartView>

class QBarSet;
class QHorizontalBarSeries;
class QBarCategoryAxis;
class QValueAxis;

namespace fincept::screens::fno {

class MultiStrikeOIChart : public QChartView {
    Q_OBJECT
  public:
    explicit MultiStrikeOIChart(QWidget* parent = nullptr);

    /// Replace the chart contents. Strikes outside the configured window
    /// around ATM are dropped from the visual.
    void set_chain(const fincept::services::options::OptionChain& chain);

    /// Window of strikes shown around ATM (default 10 → ±10 strikes).
    void set_strike_window(int n) { strike_window_ = n > 0 ? n : 10; }

  private:
    QChart* chart_ = nullptr;
    QHorizontalBarSeries* ce_series_ = nullptr;
    QHorizontalBarSeries* pe_series_ = nullptr;
    QBarSet* ce_set_ = nullptr;
    QBarSet* pe_set_ = nullptr;
    QBarCategoryAxis* axis_y_ = nullptr;
    QValueAxis* axis_x_ = nullptr;
    int strike_window_ = 10;
};

} // namespace fincept::screens::fno
