#pragma once
#include <QPushButton>

namespace fincept::ui {

/// Bell button with unread badge — lives in DashboardStatusBar.
/// Connects to NotificationService::unread_count_changed to update badge.
class NotifBell : public QPushButton {
    Q_OBJECT
  public:
    explicit NotifBell(QWidget* parent = nullptr);

    void set_unread(int count);

  signals:
    void bell_clicked();

  private:
    void update_label();
    int unread_{0};
};

} // namespace fincept::ui
