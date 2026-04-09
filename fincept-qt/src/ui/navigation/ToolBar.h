#pragma once
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QTimer>
#include <QWidget>

#include "ui/navigation/CommandBar.h"

namespace fincept::ui {

/// Combined toolbar: File/Navigate/View/Help menus + branding + clock + user info + logout.
/// Replaces both ToolBar and NavigationBar in a single row.
class ToolBar : public QWidget {
    Q_OBJECT
  public:
    explicit ToolBar(QWidget* parent = nullptr);

    void refresh_user_display();

  signals:
    void navigate_to(const QString& tab_id);
    void dock_command(const QString& action, const QString& primary, const QString& secondary);
    void action_triggered(const QString& action);
    void logout_clicked();
    void plan_clicked();
    void chat_mode_toggled();

  protected:
    void resizeEvent(QResizeEvent* e) override;

  private slots:
    void update_clock();

  private:
    QMenuBar* menu_bar_ = nullptr;
    QLabel* clock_label_ = nullptr;
    QLabel* user_label_ = nullptr;
    QLabel* credits_label_ = nullptr;
    QLabel* subtitle_label_ = nullptr;
    QLabel* branding_label_ = nullptr;
    QLabel* live_dot_ = nullptr;
    QLabel* live_label_ = nullptr;
    QPushButton* plan_btn_ = nullptr;
    QPushButton* chat_mode_btn_ = nullptr;
    QPushButton* logout_btn_ = nullptr;
    QTimer* clock_timer_ = nullptr;
    QVector<QLabel*> separators_;
    QLabel* fincept_label_ = nullptr;

    CommandBar* command_bar_ = nullptr;

    void refresh_theme();
    void apply_responsive_layout(int width);

    QMenu* build_file_menu();
    QMenu* build_navigate_menu();
    QMenu* build_view_menu();
    QMenu* build_help_menu();
};

} // namespace fincept::ui
