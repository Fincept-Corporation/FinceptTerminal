#pragma once
// IntradayOIChart — minute-resolution OI history for a chosen strike's CE
// and PE legs. Subscribes to two `oi:history:<broker>:<token>:<window>`
// topics and re-requests every 60 s while visible (OISnapshotter flushes
// once per minute, so faster polling is wasted work).
//
// Data shape per topic: `QVector<OISample>` ordered ascending by ts_minute.
// `OISample` carries oi, ltp, vol, iv — this chart shows oi only; the LTP
// could be added as a secondary axis in a later phase.

#include "services/options/OptionChainTypes.h"

#include <QChartView>
#include <QPointer>
#include <QString>
#include <QTimer>

class QDateTimeAxis;
class QLineSeries;
class QValueAxis;

namespace fincept::screens::fno {

class IntradayOIChart : public QChartView {
    Q_OBJECT
  public:
    explicit IntradayOIChart(QWidget* parent = nullptr);
    ~IntradayOIChart() override;

    /// Switch the chart to a new (broker, ce_token, pe_token) target.
    /// Unsubscribes any prior topics, kicks off a forced refresh, and
    /// subsequent OISnapshotter publishes feed the curve.
    void set_subscription(const QString& broker_id, qint64 ce_token, qint64 pe_token,
                          const QString& window = QStringLiteral("1d"));

    /// Detach all subscriptions and clear the chart.
    void clear_subscription();

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

  private:
    void on_history(qint64 token, const QVariant& v);
    void replot();
    void poll_refresh();

    QChart* chart_ = nullptr;
    QLineSeries* ce_series_ = nullptr;
    QLineSeries* pe_series_ = nullptr;
    QDateTimeAxis* axis_x_ = nullptr;
    QValueAxis* axis_y_ = nullptr;

    QString broker_id_;
    qint64 ce_token_ = 0;
    qint64 pe_token_ = 0;
    QString window_ = QStringLiteral("1d");
    QString ce_topic_, pe_topic_;

    QVector<fincept::services::options::OISample> ce_samples_;
    QVector<fincept::services::options::OISample> pe_samples_;

    QTimer poll_timer_;
};

} // namespace fincept::screens::fno
