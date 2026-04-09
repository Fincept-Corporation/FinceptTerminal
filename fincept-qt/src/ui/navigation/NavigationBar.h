#pragma once
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QWidget>

namespace fincept::ui {

/// Top menu bar matching Tauri's header — branding, time, version, user, logout.
class NavigationBar : public QWidget {
    Q_OBJECT
  public:
    explicit NavigationBar(QWidget* parent = nullptr);

    void refresh_user_display();

  signals:
    void navigate_profile();
    void logout_clicked();

  private slots:
    void update_clock();

  private:
    void refresh_theme();

    QLabel*      clock_label_   = nullptr;
    QLabel*      user_label_    = nullptr;
    QLabel*      credits_label_ = nullptr;
    QLabel*      plan_label_    = nullptr;
    QPushButton* logout_btn_    = nullptr;
    QTimer*      clock_timer_   = nullptr;
};

} // namespace fincept::ui
