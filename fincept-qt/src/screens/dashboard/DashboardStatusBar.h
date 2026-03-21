#pragma once
#include <QWidget>
#include <QLabel>
#include <QTimer>

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
    QWidget* build_feed_indicator(const QString& label, const QString& color);

    QLabel* uptime_label_   = nullptr;
    QLabel* layout_label_   = nullptr;
    QLabel* feeds_label_    = nullptr;
    QTimer  uptime_timer_;
    qint64  start_time_     = 0;
    bool    connected_      = true;
};

} // namespace fincept::screens
