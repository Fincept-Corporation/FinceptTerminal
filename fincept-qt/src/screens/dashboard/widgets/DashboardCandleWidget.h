#pragma once
#include "screens/dashboard/widgets/BaseWidget.h"
#include "services/markets/MarketDataService.h"

#include <QPixmap>
#include <QVector>

namespace fincept::screens::widgets {

/// Lightweight QPainter candlestick canvas — candles + price axis + time axis.
/// Ported from ResearchCandleCanvas (equity_research) but consumes
/// `services::HistoryPoint` directly and lives in the dashboard widgets
/// namespace so DashboardCandleWidget stays self-contained. Caches its render
/// to a QPixmap and only rebuilds on set_candles()/resize (CLAUDE.md P9).
class CandleCanvas : public QWidget {
    Q_OBJECT
  public:
    explicit CandleCanvas(QWidget* parent = nullptr);
    void set_candles(const QVector<services::HistoryPoint>& candles);
    void clear();

  protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void changeEvent(QEvent* event) override;

  private:
    void rebuild_cache();

    QVector<services::HistoryPoint> candles_;
    QPixmap cache_;
    bool dirty_ = true;

    static constexpr int MAX_VISIBLE = 260;
    static constexpr int PRICE_AXIS_W = 64;
    static constexpr int TIME_AXIS_H = 20;
};

/// Dashboard tile showing a candlestick chart for a single ticker.
///
/// Pure DataHub consumer: subscribes to
/// `market:history:<symbol_>:1y:1d` (produced by MarketDataService). Symbol is
/// user-configurable via the gear-icon dialog and persists with the layout.
/// Visibility-driven subscribe/unsubscribe per CLAUDE.md P3 / D3.
class DashboardCandleWidget : public BaseWidget {
    Q_OBJECT
  public:
    explicit DashboardCandleWidget(const QJsonObject& cfg = {}, QWidget* parent = nullptr);

    QJsonObject config() const override;
    void apply_config(const QJsonObject& cfg) override;

  protected:
    QDialog* make_config_dialog(QWidget* parent) override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void retranslateUi() override;

  private:
    QString history_topic() const;
    void hub_resubscribe();
    void refresh_data();

    QString symbol_;
    CandleCanvas* canvas_ = nullptr;
    bool hub_active_ = false;
};

} // namespace fincept::screens::widgets
