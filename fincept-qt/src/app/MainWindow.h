#pragma once
#include "app/ScreenRouter.h"
#include "ai_chat/AiChatBubble.h"

#include <QMainWindow>
#include <QShortcut>
#include <QStackedWidget>
#include <QTimer>

namespace ads { class CDockManager; }
namespace fincept { class DockScreenRouter; }
namespace fincept::ui { class DockToolBar; class DockStatusBar; class TabBar; }
namespace fincept::chat_mode { class ChatModeScreen; }

namespace fincept {

class MainWindow : public QMainWindow {
    Q_OBJECT
  public:
    /// @param window_id  Unique id for this window instance (0 = primary, 1+ = secondary).
    ///                   Primary restores saved geometry; secondary uses smart placement.
    explicit MainWindow(int window_id = 0, QWidget* parent = nullptr);

    int window_id() const { return window_id_; }

    /// Returns the next unique window ID (thread-safe via Qt UI thread only).
    static int next_window_id() { static int s_id = 1; return s_id++; }

  protected:
    void closeEvent(QCloseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

  private:
    QStackedWidget* stack_ = nullptr;
    QStackedWidget* auth_stack_ = nullptr;

    int window_id_ = 0;

    // ADS dock system
    ads::CDockManager*      dock_manager_     = nullptr;
    DockScreenRouter*       dock_router_      = nullptr;
    ui::DockToolBar*        dock_toolbar_     = nullptr;
    ui::DockStatusBar*      dock_status_bar_  = nullptr;
    ui::TabBar*             tab_bar_          = nullptr;

    // View state
    bool focus_mode_  = false;
    bool chat_mode_   = false;
    bool always_on_top_ = false;
    AiChatBubble* chat_bubble_  = nullptr;
    QTimer* user_refresh_timer_ = nullptr;

    // Chat mode
    chat_mode::ChatModeScreen* chat_mode_screen_ = nullptr;

    void setup_auth_screens();
    void setup_app_screens();
    void setup_docking_mode();
    void setup_dock_screens();
    void setup_navigation();
    void on_auth_state_changed();
    void toggle_chat_mode();

    // Info screens stack (Contact, Terms, Privacy, Trademarks, Help)
    QStackedWidget* info_stack_ = nullptr;

  private slots:
    void show_login();
    void show_register();
    void show_forgot_password();
    void show_pricing();
    void show_info_contact();
    void show_info_terms();
    void show_info_privacy();
    void show_info_trademarks();
    void show_info_help();
};

} // namespace fincept
