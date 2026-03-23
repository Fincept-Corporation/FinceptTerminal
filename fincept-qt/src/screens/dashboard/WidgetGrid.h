#pragma once
#include <QGridLayout>
#include <QScrollArea>
#include <QWidget>

namespace fincept::screens {

/// Main dashboard grid — 9 default widget cards using individual widget classes.
/// All widgets source real data from yfinance via MarketDataService.
class WidgetGrid : public QWidget {
    Q_OBJECT
  public:
    explicit WidgetGrid(QWidget* parent = nullptr);

    int widget_count() const { return widget_count_; }

  private:
    QGridLayout* grid_ = nullptr;
    int widget_count_ = 0;
};

} // namespace fincept::screens
