#pragma once
#include "app/ScreenRouter.h"
#include "ai_chat/AiChatBubble.h"

#include <QMainWindow>
#include <QShortcut>
#include <QStackedWidget>

namespace fincept {

class MainWindow : public QMainWindow {
    Q_OBJECT
  public:
    explicit MainWindow(QWidget* parent = nullptr);

  protected:
    void closeEvent(QCloseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

  private:
    QStackedWidget* stack_ = nullptr;
    ScreenRouter* router_ = nullptr;

    // Two stacks: auth screens and main app screens
    QStackedWidget* auth_stack_ = nullptr;
    QStackedWidget* app_stack_ = nullptr;

    // View state
    bool focus_mode_ = false;
    bool always_on_top_ = false;
    QWidget* tab_bar_widget_ = nullptr; // kept to hide/show in focus mode
    QWidget* status_bar_widget_ = nullptr;
    AiChatBubble* chat_bubble_ = nullptr;

    void setup_auth_screens();
    void setup_app_screens();
    void setup_navigation();
    void restore_session();
    void on_auth_state_changed();

    // Info screens stack (Contact, Terms, Privacy, Trademarks, Help)
    QStackedWidget* info_stack_ = nullptr;

  private slots:
    void show_login();
    void show_register();
    void show_forgot_password();
    void show_pricing();
    void show_payment_processing();
    void show_info_contact();
    void show_info_terms();
    void show_info_privacy();
    void show_info_trademarks();
    void show_info_help();
};

} // namespace fincept
