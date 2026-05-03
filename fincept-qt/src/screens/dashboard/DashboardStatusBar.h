#pragma once
#include <QElapsedTimer>
#include <QHideEvent>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QShowEvent>
#include <QTimer>
#include <QWidget>

// Forward declarations — avoids pulling notification headers into every TU
// that includes DashboardStatusBar.h
namespace fincept::ui {
class NotifBell;
class NotifPanel;
} // namespace fincept::ui

namespace fincept::screens {

/// Bottom status bar — session uptime, feed indicators, system status,
/// and notification bell with unread badge (right side).
class DashboardStatusBar : public QWidget {
    Q_OBJECT
  public:
    explicit DashboardStatusBar(QWidget* parent = nullptr);

    void set_widget_count(int count);
    void set_connected(bool connected);

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

  private:
    void refresh_theme();
    void update_uptime();
    void update_memory();
    void ping_api();
    void set_latency(int ms); // -1 = timeout/error
    void toggle_notif_panel();

    QLabel* version_label_ = nullptr;
    QLabel* uptime_label_ = nullptr;
    QLabel* layout_label_ = nullptr;
    QLabel* feeds_label_ = nullptr;
    QLabel* latency_label_ = nullptr;
    QLabel* mem_label_ = nullptr;

    fincept::ui::NotifBell* notif_bell_ = nullptr;
    fincept::ui::NotifPanel* notif_panel_ = nullptr;

    QTimer uptime_timer_;
    QTimer ping_timer_;
    QTimer mem_timer_;
    QNetworkAccessManager* nam_ = nullptr;
    QElapsedTimer ping_elapsed_;

    qint64 start_time_ = 0;
};

} // namespace fincept::screens
