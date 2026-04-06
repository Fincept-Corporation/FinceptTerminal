#pragma once
#include "services/notifications/NotificationService.h"

#include <QLabel>
#include <QPropertyAnimation>
#include <QTimer>
#include <QWidget>

namespace fincept::ui {

/// Slide-in toast notification overlay.
/// Anchored to the top-right of its parent widget.
/// Shows for ~4 seconds then auto-dismisses.
///
/// Usage:
///   auto* toast = new NotifToast(main_window);
///   connect(&NotificationService::instance(),
///           &NotificationService::notification_received,
///           toast, &NotifToast::show_notification);
class NotifToast : public QWidget {
    Q_OBJECT
  public:
    explicit NotifToast(QWidget* parent = nullptr);

  public slots:
    void show_notification(const fincept::notifications::NotificationRecord& record);

  protected:
    void paintEvent(QPaintEvent* e) override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;

  private:
    void reposition();
    void dismiss();

    QLabel*             level_dot_   = nullptr;
    QLabel*             title_lbl_   = nullptr;
    QLabel*             msg_lbl_     = nullptr;
    QLabel*             time_lbl_    = nullptr;
    QTimer*             auto_dismiss_= nullptr;
    QPropertyAnimation* slide_anim_  = nullptr;

    fincept::notifications::NotifLevel current_level_{
        fincept::notifications::NotifLevel::Info};

    static constexpr int TOAST_W        = 320;
    static constexpr int TOAST_H        = 72;
    static constexpr int MARGIN         = 12;
    static constexpr int AUTO_DISMISS_MS= 4000;
    static constexpr int ANIM_MS        = 250;
};

} // namespace fincept::ui
