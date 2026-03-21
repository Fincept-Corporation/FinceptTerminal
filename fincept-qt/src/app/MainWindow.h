#pragma once
#include <QMainWindow>
#include <QStackedWidget>
#include "app/ScreenRouter.h"

namespace fincept {

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    QStackedWidget* stack_  = nullptr;
    ScreenRouter*   router_ = nullptr;

    // Two stacks: auth screens and main app screens
    QStackedWidget* auth_stack_ = nullptr;
    QStackedWidget* app_stack_  = nullptr;

    void setup_auth_screens();
    void setup_app_screens();
    void setup_navigation();
    void restore_session();
    void on_auth_state_changed();

private slots:
    void show_login();
    void show_register();
    void show_forgot_password();
    void show_pricing();
};

} // namespace fincept
