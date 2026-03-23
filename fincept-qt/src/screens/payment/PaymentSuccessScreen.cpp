#include "screens/payment/PaymentSuccessScreen.h"

#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QVBoxLayout>

namespace fincept::screens {

using namespace fincept::ui;

static const char* MF = "font-family:'Consolas','Courier New',monospace;";

PaymentSuccessScreen::PaymentSuccessScreen(QWidget* parent) : QWidget(parent) {
    build_ui();

    redirect_timer_ = new QTimer(this);
    redirect_timer_->setInterval(1000);
    connect(redirect_timer_, &QTimer::timeout, this, [this]() {
        countdown_--;
        if (countdown_ <= 0) {
            redirect_timer_->stop();
            emit navigate_dashboard();
        } else {
            countdown_label_->setText(QString("Redirecting to dashboard in %1s...").arg(countdown_));
        }
    });
}

void PaymentSuccessScreen::build_ui() {
    setStyleSheet(QString("QWidget#PaySuccessRoot { background: %1; }").arg(colors::BG_BASE));
    setObjectName("PaySuccessRoot");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addStretch(2);

    auto* center = new QWidget(this);
    center->setFixedWidth(480);
    auto* cl = new QVBoxLayout(center);
    cl->setSpacing(12);

    // Success icon
    auto* icon = new QLabel("[OK]", center);
    icon->setAlignment(Qt::AlignCenter);
    icon->setStyleSheet(QString("color: %1; font-size: 32px; font-weight: 700; %2")
                            .arg(colors::POSITIVE, MF));
    cl->addWidget(icon);

    // Title
    title_label_ = new QLabel("PAYMENT SUCCESSFUL", center);
    title_label_->setAlignment(Qt::AlignCenter);
    title_label_->setStyleSheet(
        QString("color: %1; font-size: 20px; font-weight: 700; letter-spacing: 1px; %2")
            .arg(colors::POSITIVE, MF));
    cl->addWidget(title_label_);

    cl->addSpacing(8);

    // Details panel
    auto* panel = new QWidget(center);
    panel->setStyleSheet("background: #0a0a0a; border: 1px solid #16a34a; border-radius: 2px;");
    auto* pvl = new QVBoxLayout(panel);
    pvl->setContentsMargins(20, 16, 20, 16);
    pvl->setSpacing(8);

    plan_label_ = new QLabel("Plan: —", panel);
    plan_label_->setAlignment(Qt::AlignCenter);
    plan_label_->setStyleSheet(QString("color: #e5e5e5; font-size: 14px; font-weight: 600; "
                                       "background: transparent; %1")
                                   .arg(MF));
    pvl->addWidget(plan_label_);

    amount_label_ = new QLabel("Amount: —", panel);
    amount_label_->setAlignment(Qt::AlignCenter);
    amount_label_->setStyleSheet(QString("color: %1; font-size: 13px; background: transparent; %2")
                                     .arg(colors::AMBER, MF));
    pvl->addWidget(amount_label_);

    validity_label_ = new QLabel("Validity: —", panel);
    validity_label_->setAlignment(Qt::AlignCenter);
    validity_label_->setStyleSheet(QString("color: %1; font-size: 12px; background: transparent; %2")
                                       .arg(colors::TEXT_SECONDARY, MF));
    pvl->addWidget(validity_label_);

    cl->addWidget(panel);

    cl->addSpacing(8);

    // Countdown
    countdown_label_ = new QLabel("Redirecting to dashboard in 5s...", center);
    countdown_label_->setAlignment(Qt::AlignCenter);
    countdown_label_->setStyleSheet(QString("color: %1; font-size: 11px; %2")
                                        .arg(colors::TEXT_TERTIARY, MF));
    cl->addWidget(countdown_label_);

    // Dashboard button
    dashboard_btn_ = new QPushButton("GO TO DASHBOARD", center);
    dashboard_btn_->setFixedHeight(40);
    dashboard_btn_->setCursor(Qt::PointingHandCursor);
    dashboard_btn_->setStyleSheet(
        QString("QPushButton { background: %1; color: %2; border: none; "
                "font-size: 13px; font-weight: 700; letter-spacing: 1px; %3 }"
                "QPushButton:hover { background: #15803d; }")
            .arg(colors::POSITIVE, colors::TEXT_PRIMARY, MF));
    connect(dashboard_btn_, &QPushButton::clicked, this, [this]() {
        redirect_timer_->stop();
        emit navigate_dashboard();
    });
    cl->addWidget(dashboard_btn_);

    // Horizontal centering
    auto* hcenter = new QHBoxLayout();
    hcenter->addStretch();
    hcenter->addWidget(center);
    hcenter->addStretch();

    root->addLayout(hcenter);
    root->addStretch(3);
}

void PaymentSuccessScreen::show_success(const QString& plan_name, double amount, int days) {
    plan_label_->setText(QString("Plan: %1").arg(plan_name.toUpper()));
    amount_label_->setText(QString("Amount: $%1").arg(amount, 0, 'f', 2));
    validity_label_->setText(days > 0 ? QString("Validity: %1 days").arg(days) : "Validity: Forever");

    countdown_ = 5;
    countdown_label_->setText("Redirecting to dashboard in 5s...");
    redirect_timer_->start();
}

} // namespace fincept::screens
