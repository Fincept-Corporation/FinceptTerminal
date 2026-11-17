#pragma once

#include "ui/charts/SeriesLayer.h"

namespace fincept::ui {

class EmaLayer : public SeriesLayer {
    Q_OBJECT
  public:
    /// `color`: explicit line colour, or invalid (default) for the theme accent.
    /// `color`: explicit line colour, or invalid (default) for the theme accent.
    explicit EmaLayer(int period = 21, const QColor& color = QColor(), QObject* parent = nullptr);

    void compute(const QVector<CandleData>& candles) override;

    int period() const { return period_; }
    void set_period(int p);

  private:
    int period_;
};

} // namespace fincept::ui
