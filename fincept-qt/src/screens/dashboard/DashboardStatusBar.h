#pragma once
#include <QElapsedTimer>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QTimer>
#include <QWidget>

namespace fincept::screens {

/// Bottom status bar — session uptime, feed indicators, system status.
class DashboardStatusBar : public QWidget {
    Q_OBJECT
  public:
    explicit DashboardStatusBar(QWidget* parent = nullptr);

    void set_widget_count(int count);
    void set_connected(bool connected);

  private:
    void update_uptime();
    void ping_api();
    void set_latency(int ms);   // -1 = timeout/error

    QLabel* uptime_label_  = nullptr;
    QLabel* layout_label_  = nullptr;
    QLabel* feeds_label_   = nullptr;
    QLabel* latency_label_ = nullptr;

    QTimer  uptime_timer_;
    QTimer  ping_timer_;
    QNetworkAccessManager* nam_ = nullptr;
    QElapsedTimer ping_elapsed_;

    qint64 start_time_ = 0;
    bool   connected_  = true;
};

} // namespace fincept::screens
