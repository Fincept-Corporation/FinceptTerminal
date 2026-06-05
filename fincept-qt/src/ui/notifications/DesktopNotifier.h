// src/ui/notifications/DesktopNotifier.h
#pragma once
#include <QObject>
#include <QString>

class QSystemTrayIcon;
class QWidget;
class QLabel;
class QTimer;

namespace fincept::ui {

/// Cross-platform native desktop notifications, backed by QSystemTrayIcon.
/// Qt maps showMessage() to the platform's native channel:
///   Windows → toast / balloon, macOS → Notification Center, Linux → libnotify.
/// Listens to ToastService::posted so every in-app toast (alerts, errors, …)
/// also raises a real OS notification — this is the connection that was missing,
/// which is why fired alerts were silent. Singleton; GUI thread only.
class DesktopNotifier : public QObject {
    Q_OBJECT
  public:
    static DesktopNotifier& instance();

    /// Create the tray icon + wire ToastService. Call once at startup on the GUI
    /// thread, after the app/window icon is set. No-op if already initialised or
    /// no system tray is available.
    void init();

    /// Raise a native OS notification immediately. `severity` is a
    /// ToastService::Severity cast to int.
    void notify(const QString& title, const QString& body, int severity = 0);

  private:
    DesktopNotifier() = default;
    Q_DISABLE_COPY(DesktopNotifier)

    /// Self-rendered in-app toast that stays visible ~20s (native OS banners
    /// auto-dismiss in a few seconds and ignore the duration hint on macOS).
    void show_inapp(const QString& title, const QString& body, int severity);

    QSystemTrayIcon* tray_ = nullptr;
    QWidget* toast_ = nullptr;
    QLabel*  toast_lbl_ = nullptr;
    QTimer*  toast_timer_ = nullptr;
};

} // namespace fincept::ui
