#pragma once
#include "auth/AuthTypes.h"
#include "ui/widgets/ConfettiOverlay.h"

#include <QLabel>
#include <QMetaObject>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <vector>

namespace fincept::screens {

class PricingScreen : public QWidget {
    Q_OBJECT
  public:
    explicit PricingScreen(QWidget* parent = nullptr);

    void refresh_plans();

  signals:
    void navigate_dashboard();

  protected:
    void showEvent(QShowEvent* event) override;
    void changeEvent(QEvent* event) override;

  private:
    QVBoxLayout* cards_layout_ = nullptr;
    QLabel* title_label_ = nullptr;
    QLabel* subtitle_label_ = nullptr;
    QLabel* error_label_ = nullptr;
    QLabel* loading_label_ = nullptr;
    QLabel* user_info_label_ = nullptr;
    QWidget* cards_container_ = nullptr;
    QWidget* footer_widget_ = nullptr;

    /// Tracks whether the last loading state was the "refresh" variant
    /// (showEvent's "Updating plan status...") vs the generic "Loading plans...".
    /// Used by retranslateUi() to pick the correct localised string for the
    /// currently-displayed loading message.
    bool loading_is_refresh_ = false;
    void retranslateUi();

    std::vector<auth::SubscriptionPlan> plans_;
    bool fetched_ = false;
    bool loading_ = false;
    bool awaiting_payment_ = false;
    QString awaiting_plan_id_;
    QString pre_payment_plan_;
    QMetaObject::Connection focus_connection_;
    QTimer* payment_poll_timer_ = nullptr;
    ui::ConfettiOverlay* confetti_ = nullptr;

    void build_ui();
    void fetch_plans();
    void render_plan_cards();
    QWidget* create_plan_card(const auth::SubscriptionPlan& plan, int index);
    void on_select_plan(const QString& plan_id);
    void update_footer();
    void on_app_focus_returned();
    void poll_payment_status();
};

} // namespace fincept::screens
