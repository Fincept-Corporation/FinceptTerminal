#pragma once
#include "ai_chat/AiChatBubble.h"
#include "app/ScreenRouter.h"

#include <QMainWindow>
#include <QStackedWidget>
#include <QTimer>

namespace ads {
class CDockManager;
}
namespace fincept {
class DockScreenRouter;
}
namespace fincept::ui {
class DockToolBar;
class DockStatusBar;
class TabBar;
} // namespace fincept::ui
namespace fincept::chat_mode {
class ChatModeScreen;
}
namespace fincept::screens {
class LockScreen;
}

namespace fincept {

class MainWindow : public QMainWindow {
    Q_OBJECT
  public:
    /// @param window_id  Unique id for this window instance (0 = primary, 1+ = secondary).
    ///                   Primary restores saved geometry; secondary uses smart placement.
    explicit MainWindow(int window_id = 0, QWidget* parent = nullptr);

    int window_id() const { return window_id_; }

    /// Returns the next unique window ID (thread-safe via Qt UI thread only).
    /// The allocator is seeded from saved state on first call so that IDs
    /// never collide with orphaned saved layouts from a previous session.
    static int next_window_id();

  protected:
    void closeEvent(QCloseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

  private:
    QStackedWidget* stack_ = nullptr;
    QStackedWidget* auth_stack_ = nullptr;

    int window_id_ = 0;

    // ADS dock system
    ads::CDockManager* dock_manager_ = nullptr;
    DockScreenRouter* dock_router_ = nullptr;
    ui::DockToolBar* dock_toolbar_ = nullptr;
    ui::DockStatusBar* dock_status_bar_ = nullptr;
    ui::TabBar* tab_bar_ = nullptr;

    // View state
    bool focus_mode_ = false;
    bool chat_mode_ = false;
    bool always_on_top_ = false;
    bool locked_ = false; ///< True while lock/PIN screen is active — blocks navigation.
    bool pin_gate_cleared_ = false; ///< Set once the user has passed the PIN gate this session.
                                    ///< Prevents subsequent auth_state_changed events (profile
                                    ///< refresh, subscription fetch, focus refresh) from
                                    ///< re-locking the terminal. Reset on lock / logout.
    AiChatBubble* chat_bubble_ = nullptr;
    QTimer* user_refresh_timer_ = nullptr;

    // Debounced persistence of dock layout on add/replace/remove via command bar.
    // Without this, layout changes only survive clean shutdown (closeEvent),
    // so a crash or kill loses the user's dock setup.
    QTimer* dock_layout_save_timer_ = nullptr;
    void schedule_dock_layout_save();

    // Chat mode
    chat_mode::ChatModeScreen* chat_mode_screen_ = nullptr;

    // Lock/PIN screen
    screens::LockScreen* lock_screen_ = nullptr;

    void setup_auth_screens();
    void setup_app_screens();
    void setup_docking_mode();
    void setup_dock_screens();
    void setup_navigation();
    void on_auth_state_changed();
    void toggle_chat_mode();
    void show_lock_screen();
    void on_terminal_unlocked();
    void update_window_title();
    /// Show or hide the toolbar/status bar shell (hidden during auth screens).
    void set_shell_visible(bool visible);
    /// Show the auth stack at the given index and hide the privileged shell.
    /// All login/register/forgot/pricing/info transitions go through here.
    void enter_auth_stack(int auth_index);

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
