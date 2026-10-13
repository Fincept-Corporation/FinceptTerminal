#pragma once
#include "auth/AuthTypes.h"

#include <QJsonArray>
#include <QMetaObject>
#include <QString>
#include <QWidget>

#include <vector>

class QHBoxLayout;
class QLabel;
class QPushButton;
class QTimer;

namespace fincept::ui {
class ConfettiOverlay;
}

namespace fincept::screens {

/// PLANS & PRICING — three equal cards: FREE and PRO (fetched from
/// /cashfree/plans; content such as credits and features is backend-owned) and
/// a static Fincept Terminal Enterprise card (separate product, separate
/// account). Reached from the toolbar plan badge, Profile → Billing, low-credit
/// prompts and the "billing.plans" action; free accounts also see it once per
/// release after signing in (WindowFrame_Auth.cpp).
///
/// Checkout: POST /user/generate-checkout-token {plan_id} → opens
/// https://fincept.in/checkout?token=…&plan=… and polls the profile every 5 s
/// (10 min max) until the account type changes.
class PricingScreen : public QWidget {
    Q_OBJECT
  public:
    explicit PricingScreen(QWidget* parent = nullptr);

  signals:
    /// Leave the screen (CONTINUE WITH FREE / BACK TO TERMINAL / Esc). The
    /// window returns to wherever the user was — dock UI or chat mode.
    void back_requested();

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void changeEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

  private:
    /// What a plan card's button does. Kept on the button as the "planAction"
    /// property so state changes never depend on matching translated text.
    enum class PlanAction { Current, Included, ContinueFree, Select, Processing };

    /// Tone of the status line under the header.
    enum class StatusTone { Info, Error, Success };

    void build_ui();
    void refresh_theme();
    void retranslateUi();

    void update_account_line();
    void set_status(StatusTone tone, const QString& text, bool show_retry = false);
    void clear_status();

    void fetch_plans();
    void load_cached_plans();
    bool apply_plans_json(const QJsonArray& plans);
    void render_cards();
    QWidget* build_plan_card(const auth::SubscriptionPlan& plan);
    QWidget* build_enterprise_card();
    static void set_plan_action(QPushButton* btn, PlanAction action);

    void on_select_plan(const QString& plan_id);
    void on_session_changed();
    void stop_payment_polling();
    void poll_payment_status();

    // ── Widgets ──────────────────────────────────────────────────────────────
    QLabel* title_label_ = nullptr;
    QLabel* subtitle_label_ = nullptr;
    QLabel* account_label_ = nullptr;
    QLabel* status_label_ = nullptr;
    QPushButton* retry_btn_ = nullptr;
    QWidget* status_row_ = nullptr;
    QWidget* cards_host_ = nullptr;
    QHBoxLayout* cards_layout_ = nullptr;
    QLabel* footnote_label_ = nullptr;
    QPushButton* back_btn_ = nullptr;
    ui::ConfettiOverlay* confetti_ = nullptr;

    // ── State ────────────────────────────────────────────────────────────────
    std::vector<auth::SubscriptionPlan> plans_;
    bool loading_ = false;
    bool cache_checked_ = false;
    /// Account type (and load state) the cards were last rendered for — the
    /// cards are rebuilt only when it or the plan list changes, not on every
    /// profile refresh.
    QString rendered_for_;

    bool awaiting_payment_ = false;
    /// True between issuing a refresh_user_data() poll and the next
    /// auth_state_changed. One request at a time — a 10-minute checkout must
    /// not stack 120 in-flight profile requests.
    bool payment_poll_in_flight_ = false;
    qint64 payment_poll_issued_ms_ = 0;
    int payment_poll_ticks_ = 0;
    static constexpr int kPaymentPollIntervalMs = 5000;
    static constexpr int kMaxPaymentPolls = 120; // 5 s × 120 = 10 minutes
    QString awaiting_plan_id_;
    QString pre_payment_plan_;
    QMetaObject::Connection focus_connection_;
    QTimer* payment_poll_timer_ = nullptr;
};

} // namespace fincept::screens
