#pragma once
#include "auth/AuthTypes.h"

#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
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
    void payment_initiated(const QString& order_id, const QString& plan_name);

  protected:
    void showEvent(QShowEvent* event) override;

  private:
    QVBoxLayout* cards_layout_ = nullptr;
    QLabel* error_label_ = nullptr;
    QLabel* loading_label_ = nullptr;
    QLabel* user_info_label_ = nullptr;
    QWidget* cards_container_ = nullptr;
    QWidget* footer_widget_ = nullptr;

    std::vector<auth::SubscriptionPlan> plans_;
    bool fetched_ = false;
    bool loading_ = false;

    void build_ui();
    void fetch_plans();
    void render_plan_cards();
    QWidget* create_plan_card(const auth::SubscriptionPlan& plan, int index);
    void on_select_plan(const QString& plan_id);
    void update_footer();
};

} // namespace fincept::screens
