#pragma once
#include "ui/navigation/CommandBar.h"

#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QTimer>
#include <QWidget>

class QToolButton;

namespace fincept::ui {

class PushpinBar;

/// Combined toolbar: File/Navigate/View/Help menus + branding + clock + user
/// info (credits, plan badge → Plans & Pricing) + ENTERPRISE + chat + logout.
///
/// Internationalised: all menus and labels flow through tr(). Menus are
/// rebuilt on QEvent::LanguageChange so action labels pick up the new
/// translator; static labels and buttons retranslate via setText().
class ToolBar : public QWidget {
    Q_OBJECT
  public:
    explicit ToolBar(QWidget* parent = nullptr);

    void refresh_user_display();

    PushpinBar* pushpin_bar() const { return pushpin_bar_; }

  signals:
    void navigate_to(const QString& tab_id);
    void dock_command(const QString& action, const QString& primary, const QString& secondary);
    void action_triggered(const QString& action);
    void logout_clicked();
    void plan_clicked();
    void upgrade_clicked();
    void chat_mode_toggled();

  protected:
    void resizeEvent(QResizeEvent* e) override;
    void changeEvent(QEvent* e) override;
    // §P3: the 1 s clock ticks only while the toolbar is visible.
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

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
    QLabel* live_gap_ = nullptr; ///< the blank gap before the LIVE dot (collapses with it)
    /// Plan badge ("FREE" / "PRO" / legacy tier) → Plans & Pricing. Carries a
    /// "tier" property (free / paid) that the stylesheet outlines for free accounts.
    QPushButton* plan_btn_ = nullptr;
    /// Always-visible ENTERPRISE CTA for Fincept Terminal Enterprise (the private edition).
    QPushButton* upgrade_btn_ = nullptr;
    QPushButton* chat_mode_btn_ = nullptr;
    /// Next to CHAT: agent runs in progress (INFO) and runs waiting for the user's
    /// answer / approval (WARNING), from TaskRunService::attention_changed. Hidden at 0.
    QLabel* chat_runs_badge_ = nullptr;
    QLabel* chat_attention_badge_ = nullptr;
    int chat_active_runs_ = 0;
    int chat_attention_runs_ = 0;
    void update_chat_badge();
    /// AI activity pill, right after LIVE: a filled, pulsing "AI WORKING · <step>" while a
    /// request is in flight (it names the step when the AI is operating the screens), then
    /// "AI FINISHED" for a few seconds, then hidden. From LlmService::activity_changed.
    QLabel* ai_status_ = nullptr;
    QTimer* ai_pulse_timer_ = nullptr;
    QTimer* ai_done_timer_ = nullptr;
    int ai_running_ = 0;
    bool ai_finished_ = false; ///< showing the "finished" state
    bool ai_pulse_on_ = true;
    QString ai_step_;
    void update_ai_status();
    QPushButton* logout_btn_ = nullptr;
    /// "More" button at the right end, shown when the row is too narrow for
    /// every item: its menu offers the actions (and account info) that were
    /// collapsed out of the row.
    QToolButton* overflow_btn_ = nullptr;
    bool in_responsive_layout_ = false;
    QTimer* clock_timer_ = nullptr;
    QVector<QLabel*> separators_;
    QLabel* fincept_label_ = nullptr;

    CommandBar* command_bar_ = nullptr;
    PushpinBar* pushpin_bar_ = nullptr;

    void refresh_theme();
    void retranslateUi();
    /// Tear down and recreate every menu on the menu bar so action labels
    /// pick up the active QTranslator. Called from retranslateUi() so a
    /// LanguageChange event rebuilds menus alongside static labels.
    void rebuild_menus();
    /// Fit the row to `width` from the items' own size hints (font metrics):
    /// lower-priority items collapse first, actions go into the overflow menu.
    void apply_responsive_layout(int width);
    void fill_overflow_menu(QMenu* menu);

    QMenu* build_file_menu();
    QMenu* build_navigate_menu();
    QMenu* build_view_menu();
    QMenu* build_help_menu();
};

} // namespace fincept::ui
