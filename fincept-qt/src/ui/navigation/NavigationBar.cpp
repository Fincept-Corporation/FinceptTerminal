#include "ui/navigation/NavigationBar.h"

#include "auth/AuthManager.h"
#include "ui/theme/Theme.h"

#include <QDateTime>

namespace fincept::ui {

NavigationBar::NavigationBar(QWidget* parent) : QWidget(parent) {
    setFixedHeight(38);
    setStyleSheet("background:#0a0a0a;border-bottom:1px solid #1a1a1a;");
    auto* hl = new QHBoxLayout(this);
    hl->setContentsMargins(14, 0, 14, 0);
    hl->setSpacing(0);

    auto mk = [](const QString& t, const QString& c, int sz = 14, bool b = false) {
        auto* l = new QLabel(t);
        l->setStyleSheet(QString("color:%1;font-size:%2px;%3background:transparent;"
                                 "font-family:'Consolas',monospace;")
                             .arg(c)
                             .arg(sz)
                             .arg(b ? "font-weight:700;" : ""));
        return l;
    };

    hl->addWidget(mk("FINCEPT", "#d97706", 16, true));
    hl->addWidget(mk("TERMINAL", "#ffffff", 14));
    hl->addWidget(mk("   ", "#000"));
    hl->addWidget(mk("\xe2\x97\x8f", "#16a34a", 10));
    hl->addWidget(mk(" LIVE", "#16a34a", 13, true));
    hl->addStretch();
    clock_label_ = mk("", "#404040", 13);
    hl->addWidget(clock_label_);
    hl->addStretch();
    user_label_ = mk("---", "#d97706", 13);
    hl->addWidget(user_label_);
    hl->addWidget(mk("  |  ", "#1a1a1a", 13));
    credits_label_ = mk("---", "#16a34a", 13);
    hl->addWidget(credits_label_);
    hl->addWidget(mk("  |  ", "#1a1a1a", 13));
    plan_label_ = mk("---", "#ffffff", 13);
    hl->addWidget(plan_label_);
    hl->addWidget(mk("  |  ", "#1a1a1a", 13));

    auto* logout_btn = new QPushButton("LOGOUT");
    logout_btn->setFixedHeight(24);
    logout_btn->setCursor(Qt::PointingHandCursor);
    logout_btn->setStyleSheet("QPushButton{background:transparent;color:#dc2626;border:1px solid #7f1d1d;"
                              "padding:0 10px;font-size:12px;font-weight:700;font-family:'Consolas',monospace;}"
                              "QPushButton:hover{background:#dc2626;color:#e5e5e5;border-color:#dc2626;}");
    connect(logout_btn, &QPushButton::clicked, this, &NavigationBar::logout_clicked);
    hl->addWidget(logout_btn);

    clock_timer_ = new QTimer(this);
    clock_timer_->setInterval(1000);
    connect(clock_timer_, &QTimer::timeout, this, &NavigationBar::update_clock);
    clock_timer_->start();
    update_clock();

    connect(&auth::AuthManager::instance(), &auth::AuthManager::auth_state_changed, this,
            &NavigationBar::refresh_user_display);
    refresh_user_display();
}

void NavigationBar::update_clock() {
    clock_label_->setText(QDateTime::currentDateTime().toString("yyyy-MM-dd  HH:mm:ss"));
}

void NavigationBar::refresh_user_display() {
    const auto& s = auth::AuthManager::instance().session();
    if (!s.authenticated) {
        user_label_->setText("---");
        credits_label_->setText("---");
        plan_label_->setText("---");
        return;
    }
    user_label_->setText(s.user_info.username.isEmpty() ? s.user_info.email : s.user_info.username);
    credits_label_->setText(QString("%1 CR").arg(s.user_info.credit_balance, 0, 'f', 2));
    credits_label_->setStyleSheet(
        QString("color:%1;font-size:13px;background:transparent;font-family:'Consolas',monospace;")
            .arg(s.user_info.credit_balance > 0 ? "#16a34a" : "#dc2626"));
    plan_label_->setText(s.user_info.account_type.toUpper());
}

} // namespace fincept::ui
