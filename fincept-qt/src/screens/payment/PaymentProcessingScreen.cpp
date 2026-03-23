#include "screens/payment/PaymentProcessingScreen.h"

#include "auth/AuthApi.h"
#include "auth/AuthManager.h"
#include "core/logging/Logger.h"
#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QJsonObject>
#include <QVBoxLayout>

namespace fincept::screens {

using namespace fincept::ui;

static const char* MF = "font-family:'Consolas','Courier New',monospace;";

// ── Constructor ──────────────────────────────────────────────────────────────

PaymentProcessingScreen::PaymentProcessingScreen(QWidget* parent) : QWidget(parent) {
    build_ui();

    poll_timer_ = new QTimer(this);
    poll_timer_->setInterval(POLL_INTERVAL_MS);
    connect(poll_timer_, &QTimer::timeout, this, &PaymentProcessingScreen::poll_status);
}

void PaymentProcessingScreen::build_ui() {
    setStyleSheet(QString("QWidget#PayProcRoot { background: %1; }").arg(colors::BG_BASE));
    setObjectName("PayProcRoot");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addStretch(2);

    auto* center = new QWidget(this);
    center->setFixedWidth(500);
    auto* cl = new QVBoxLayout(center);
    cl->setSpacing(14);

    // Title
    title_label_ = new QLabel("PAYMENT PROCESSING", center);
    title_label_->setAlignment(Qt::AlignCenter);
    title_label_->setStyleSheet(
        QString("color: %1; font-size: 20px; font-weight: 700; letter-spacing: 1px; %2")
            .arg(colors::AMBER, MF));
    cl->addWidget(title_label_);

    // Status indicator panel
    auto* status_panel = new QWidget(center);
    status_panel->setStyleSheet("background: #0a0a0a; border: 1px solid #1a1a1a; border-radius: 2px;");
    auto* sp_vl = new QVBoxLayout(status_panel);
    sp_vl->setContentsMargins(20, 16, 20, 16);
    sp_vl->setSpacing(10);

    status_label_ = new QLabel("Waiting for payment confirmation...", status_panel);
    status_label_->setAlignment(Qt::AlignCenter);
    status_label_->setStyleSheet(QString("color: %1; font-size: 14px; font-weight: 600; "
                                         "background: transparent; %2")
                                     .arg(colors::AMBER, MF));
    sp_vl->addWidget(status_label_);

    detail_label_ = new QLabel("Your payment is being processed. Please complete the checkout "
                               "in your browser. This page will update automatically.",
                               status_panel);
    detail_label_->setAlignment(Qt::AlignCenter);
    detail_label_->setWordWrap(true);
    detail_label_->setStyleSheet(QString("color: %1; font-size: 12px; background: transparent; %2")
                                     .arg(colors::TEXT_SECONDARY, MF));
    sp_vl->addWidget(detail_label_);

    // Stats row
    auto* stats_row = new QWidget(status_panel);
    stats_row->setStyleSheet("background: transparent;");
    auto* stats_hl = new QHBoxLayout(stats_row);
    stats_hl->setContentsMargins(0, 8, 0, 0);

    elapsed_label_ = new QLabel("Elapsed: 0s", stats_row);
    elapsed_label_->setStyleSheet(QString("color: %1; font-size: 11px; background: transparent; %2")
                                      .arg(colors::TEXT_TERTIARY, MF));
    stats_hl->addWidget(elapsed_label_);

    stats_hl->addStretch();

    check_count_label_ = new QLabel("Checks: 0/60", stats_row);
    check_count_label_->setStyleSheet(QString("color: %1; font-size: 11px; background: transparent; %2")
                                          .arg(colors::TEXT_TERTIARY, MF));
    stats_hl->addWidget(check_count_label_);

    sp_vl->addWidget(stats_row);
    cl->addWidget(status_panel);

    // Security note
    auto* security = new QLabel("Secure payment via Cashfree. Your details are encrypted end-to-end.");
    security->setAlignment(Qt::AlignCenter);
    security->setStyleSheet(QString("color: %1; font-size: 10px; %2").arg(colors::TEXT_DIM, MF));
    cl->addWidget(security);

    cl->addSpacing(8);

    // Buttons
    auto* btn_row = new QWidget(center);
    btn_row->setStyleSheet("background: transparent;");
    auto* bhl = new QHBoxLayout(btn_row);
    bhl->setContentsMargins(0, 0, 0, 0);
    bhl->setSpacing(10);

    refresh_btn_ = new QPushButton("REFRESH STATUS", btn_row);
    refresh_btn_->setFixedHeight(36);
    refresh_btn_->setCursor(Qt::PointingHandCursor);
    refresh_btn_->setStyleSheet(
        QString("QPushButton { background: rgba(217,119,6,0.1); color: %1; border: 1px solid #78350f; "
                "font-size: 12px; font-weight: 700; %2 }"
                "QPushButton:hover { background: %1; color: #080808; }")
            .arg(colors::AMBER, MF));
    connect(refresh_btn_, &QPushButton::clicked, this, &PaymentProcessingScreen::poll_status);
    bhl->addWidget(refresh_btn_);

    back_btn_ = new QPushButton("BACK TO PRICING", btn_row);
    back_btn_->setFixedHeight(36);
    back_btn_->setCursor(Qt::PointingHandCursor);
    back_btn_->setStyleSheet(
        QString("QPushButton { background: transparent; color: %1; border: 1px solid #1a1a1a; "
                "font-size: 12px; font-weight: 700; %2 }"
                "QPushButton:hover { color: #e5e5e5; background: #111111; }")
            .arg(colors::TEXT_SECONDARY, MF));
    connect(back_btn_, &QPushButton::clicked, this, [this]() {
        stop_polling();
        emit navigate_back();
    });
    bhl->addWidget(back_btn_);

    cl->addWidget(btn_row);

    // Horizontal centering
    auto* hcenter = new QHBoxLayout();
    hcenter->addStretch();
    hcenter->addWidget(center);
    hcenter->addStretch();

    root->addLayout(hcenter);
    root->addStretch(3);
}

// ── Polling ──────────────────────────────────────────────────────────────────

void PaymentProcessingScreen::start_polling(const QString& order_id, const QString& plan_name) {
    order_id_ = order_id;
    plan_name_ = plan_name;
    check_count_ = 0;
    elapsed_seconds_ = 0;

    set_status("Waiting for payment confirmation...", colors::AMBER);
    detail_label_->setText("Your payment is being processed. Please complete the checkout "
                           "in your browser. This page will update automatically.");
    elapsed_label_->setText("Elapsed: 0s");
    check_count_label_->setText("Checks: 0/60");

    LOG_INFO("Payment", QString("Started polling for order: %1").arg(order_id));

    // Initial delay before first check
    QTimer::singleShot(INITIAL_DELAY_MS, this, [this]() {
        poll_status();
        poll_timer_->start();
    });
}

void PaymentProcessingScreen::stop_polling() {
    poll_timer_->stop();
}

void PaymentProcessingScreen::poll_status() {
    if (order_id_.isEmpty())
        return;

    check_count_++;
    elapsed_seconds_ += POLL_INTERVAL_MS / 1000;
    elapsed_label_->setText(QString("Elapsed: %1s").arg(elapsed_seconds_));
    check_count_label_->setText(QString("Checks: %1/%2").arg(check_count_).arg(MAX_CHECKS));

    if (check_count_ >= MAX_CHECKS) {
        stop_polling();
        set_status("TIMEOUT", colors::NEGATIVE);
        detail_label_->setText("Payment verification timed out. If you completed the payment, "
                               "it may take a few minutes to process. Please check your email "
                               "or contact support@fincept.in");
        return;
    }

    auth::AuthApi::instance().get_transaction_status(order_id_, [this](auth::ApiResponse r) {
        if (!r.success)
            return; // Silently retry on next poll

        QJsonObject data = r.data;
        if (data.contains("data") && data["data"].isObject())
            data = data["data"].toObject();

        QString status = data["order_status"].toString().toUpper();

        if (status == "PAID" || status == "COMPLETED" || status == "SUCCESS") {
            stop_polling();
            set_status("PAYMENT SUCCESSFUL", colors::POSITIVE);
            detail_label_->setText(QString("Your %1 plan is now active!").arg(plan_name_));
            LOG_INFO("Payment", "Payment confirmed for order: " + order_id_);

            // Refresh user data then emit completion
            auth::AuthManager::instance().refresh_user_data();
            QTimer::singleShot(2000, this, [this]() { emit payment_completed(); });

        } else if (status == "FAILED" || status == "CANCELLED" || status == "USER_DROPPED" || status == "EXPIRED") {
            stop_polling();
            set_status("PAYMENT " + status, colors::NEGATIVE);
            detail_label_->setText("Payment was not completed. You can try again from the pricing page.");
            LOG_WARN("Payment", "Payment " + status + " for order: " + order_id_);
        }
        // Otherwise keep polling (ACTIVE/PENDING status)
    });
}

void PaymentProcessingScreen::set_status(const QString& status, const QString& color) {
    status_label_->setText(status);
    status_label_->setStyleSheet(
        QString("color: %1; font-size: 14px; font-weight: 600; background: transparent; %2")
            .arg(color, MF));
}

} // namespace fincept::screens
