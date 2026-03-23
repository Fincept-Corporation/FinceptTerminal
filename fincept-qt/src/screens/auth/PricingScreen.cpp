#include "screens/auth/PricingScreen.h"

#include "auth/AuthApi.h"
#include "auth/AuthManager.h"
#include "ui/theme/Theme.h"

#include <QDesktopServices>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QScrollArea>
#include <QUrl>

#include <algorithm>

namespace fincept::screens {

// ── Obsidian Design System constants ─────────────────────────────────────────

static const char* MF = "font-family:'Consolas','Courier New',monospace;";

static const char* CARD_NORMAL = "QWidget#plan_card { background: #0a0a0a; border: 1px solid #1a1a1a; }"
                                 "QLabel { border: none; background: transparent; }"
                                 "QFrame { border: none; background: transparent; }";

static const char* CARD_POPULAR = "QWidget#plan_card { background: #0a0a0a; border: 1px solid #78350f; }"
                                  "QLabel { border: none; background: transparent; }"
                                  "QFrame { border: none; background: transparent; }";

static const char* CARD_ACTIVE = "QWidget#plan_card { background: #0a0a0a; border: 1px solid #16a34a; }"
                                 "QLabel { border: none; background: transparent; }"
                                 "QFrame { border: none; background: transparent; }";

// ── Constructor ──────────────────────────────────────────────────────────────

PricingScreen::PricingScreen(QWidget* parent) : QWidget(parent) {
    build_ui();
}

void PricingScreen::build_ui() {
    setStyleSheet("background: #080808;");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("QScrollArea { background: transparent; border: none; }"
                          "QScrollBar:vertical { background: transparent; width: 6px; }"
                          "QScrollBar::handle:vertical { background: #222222; }"
                          "QScrollBar::handle:vertical:hover { background: #333333; }"
                          "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }");

    auto* content = new QWidget;
    content->setStyleSheet("background: transparent;");
    auto* vl = new QVBoxLayout(content);
    vl->setContentsMargins(14, 14, 14, 14);
    vl->setSpacing(10);

    // ── Header ───────────────────────────────────────────────────────────────
    auto* title = new QLabel("PLANS & PRICING");
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(QString("color: #d97706; font-size: 20px; font-weight: 700; "
                                 "letter-spacing: 1px; background: transparent; %1")
                             .arg(MF));
    vl->addWidget(title);

    auto* subtitle = new QLabel("Unlock the full power of Fincept Terminal");
    subtitle->setAlignment(Qt::AlignCenter);
    subtitle->setStyleSheet(QString("color: #525252; font-size: 13px; background: transparent; %1").arg(MF));
    vl->addWidget(subtitle);

    user_info_label_ = new QLabel;
    user_info_label_->setAlignment(Qt::AlignCenter);
    user_info_label_->setStyleSheet(QString("color: #808080; font-size: 12px; background: transparent; %1").arg(MF));
    vl->addWidget(user_info_label_);

    // ── Loading ──────────────────────────────────────────────────────────────
    loading_label_ = new QLabel("Loading plans...");
    loading_label_->setAlignment(Qt::AlignCenter);
    loading_label_->setStyleSheet(QString("color: #404040; font-size: 13px; background: transparent; %1").arg(MF));
    loading_label_->hide();
    vl->addWidget(loading_label_);

    // ── Error ────────────────────────────────────────────────────────────────
    error_label_ = new QLabel;
    error_label_->setWordWrap(true);
    error_label_->setAlignment(Qt::AlignCenter);
    error_label_->setStyleSheet(QString("color: #dc2626; font-size: 12px; background: rgba(220,38,38,0.1); "
                                        "border: 1px solid #7f1d1d; padding: 6px 12px; %1")
                                    .arg(MF));
    error_label_->hide();
    vl->addWidget(error_label_);

    // ── Cards container ──────────────────────────────────────────────────────
    cards_container_ = new QWidget;
    cards_container_->setStyleSheet("background: transparent;");
    cards_layout_ = new QVBoxLayout(cards_container_);
    cards_layout_->setContentsMargins(0, 0, 0, 0);
    vl->addWidget(cards_container_);

    // ── Footer ───────────────────────────────────────────────────────────────
    footer_widget_ = new QWidget;
    footer_widget_->setStyleSheet("background: transparent;");
    auto* footer_vl = new QVBoxLayout(footer_widget_);
    footer_vl->setContentsMargins(0, 10, 0, 0);
    footer_vl->setSpacing(6);

    auto* sep = new QFrame;
    sep->setFixedHeight(1);
    sep->setStyleSheet("background: #1a1a1a; border: none;");
    footer_vl->addWidget(sep);

    footer_vl->addSpacing(4);
    vl->addWidget(footer_widget_);

    vl->addStretch();
    scroll->setWidget(content);
    root->addWidget(scroll);
}

void PricingScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    update_footer();

    auto& auth = auth::AuthManager::instance();
    if (auth.is_authenticated()) {
        auto& s = auth.session();
        QString info = (s.user_info.email.isEmpty() ? s.user_info.username : s.user_info.email);
        info += QString("  |  %1 CR").arg(s.user_info.credit_balance, 0, 'f', 0);
        user_info_label_->setText(info);
        user_info_label_->show();
    } else {
        user_info_label_->hide();
    }

    if (!fetched_) {
        fetch_plans();
    }
}

// ── Fetch plans ──────────────────────────────────────────────────────────────

void PricingScreen::fetch_plans() {
    if (loading_)
        return;
    loading_ = true;
    fetched_ = true;
    loading_label_->show();
    error_label_->hide();

    auth::AuthApi::instance().get_subscription_plans([this](auth::ApiResponse r) {
        loading_ = false;
        loading_label_->hide();

        if (!r.success) {
            error_label_->setText(r.error.isEmpty() ? "Failed to load plans" : r.error);
            error_label_->show();
            return;
        }

        QJsonObject data = r.data;
        QJsonArray plans_array;
        if (data.contains("data") && data["data"].isArray()) {
            plans_array = data["data"].toArray();
        } else if (data.contains("data") && data["data"].isObject()) {
            auto inner = data["data"].toObject();
            if (inner.contains("data") && inner["data"].isArray()) {
                plans_array = inner["data"].toArray();
            }
        }

        plans_.clear();
        for (const auto& p : plans_array) {
            if (p.isObject())
                plans_.push_back(auth::SubscriptionPlan::from_json(p.toObject()));
        }

        std::sort(plans_.begin(), plans_.end(), [](const auth::SubscriptionPlan& a, const auth::SubscriptionPlan& b) {
            return a.display_order < b.display_order;
        });

        render_plan_cards();
    });
}

void PricingScreen::refresh_plans() {
    fetched_ = false;
    fetch_plans();
}

// ── Render plan cards ────────────────────────────────────────────────────────

void PricingScreen::render_plan_cards() {
    if (cards_layout_) {
        QLayoutItem* item;
        while ((item = cards_layout_->takeAt(0)) != nullptr) {
            if (item->widget())
                item->widget()->deleteLater();
            if (item->layout()) {
                QLayoutItem* sub;
                while ((sub = item->layout()->takeAt(0)) != nullptr) {
                    if (sub->widget())
                        sub->widget()->deleteLater();
                    delete sub;
                }
            }
            delete item;
        }
        delete cards_layout_;
    }

    auto* hbox = new QHBoxLayout;
    hbox->setSpacing(10);
    hbox->setContentsMargins(0, 0, 0, 0);
    hbox->addStretch();

    for (int i = 0; i < (int)plans_.size(); ++i) {
        hbox->addWidget(create_plan_card(plans_[i], i));
    }

    hbox->addStretch();

    cards_layout_ = new QVBoxLayout;
    cards_layout_->setContentsMargins(0, 0, 0, 0);
    cards_layout_->addLayout(hbox);
    cards_container_->setLayout(cards_layout_);

    if (plans_.empty()) {
        auto* empty = new QLabel("No plans available.");
        empty->setAlignment(Qt::AlignCenter);
        empty->setStyleSheet(QString("color: #404040; font-size: 13px; background: transparent; %1").arg(MF));
        cards_layout_->addWidget(empty);
    }

    update_footer();
}

QWidget* PricingScreen::create_plan_card(const auth::SubscriptionPlan& plan, int index) {
    Q_UNUSED(index);
    auto& auth_mgr = auth::AuthManager::instance();

    bool is_popular = (plan.name == "Standard" || plan.name == "Pro");
    bool is_current = false;
    if (auth_mgr.is_authenticated()) {
        QString user_plan = auth_mgr.session().user_info.account_type.toLower();
        is_current = (user_plan == plan.plan_id.toLower());
    }

    auto* card = new QWidget;
    card->setObjectName("plan_card");
    card->setFixedWidth(260);
    card->setMinimumHeight(400);
    card->setStyleSheet(is_current ? CARD_ACTIVE : (is_popular ? CARD_POPULAR : CARD_NORMAL));

    auto* vl = new QVBoxLayout(card);
    vl->setContentsMargins(12, 12, 12, 12);
    vl->setSpacing(4);

    // Popular badge
    if (is_popular) {
        auto* badge = new QLabel("RECOMMENDED");
        badge->setAlignment(Qt::AlignCenter);
        badge->setFixedHeight(20);
        badge->setStyleSheet(QString("color: #d97706; background: rgba(217,119,6,0.1); "
                                     "border: 1px solid #78350f; padding: 0 8px; "
                                     "font-size: 11px; font-weight: 700; letter-spacing: 0.5px; %1")
                                 .arg(MF));
        vl->addWidget(badge, 0, Qt::AlignCenter);
        vl->addSpacing(2);
    }

    // Plan name
    auto* name = new QLabel(plan.name.toUpper());
    name->setAlignment(Qt::AlignCenter);
    name->setStyleSheet(QString("color: %1; font-size: 16px; font-weight: 700; "
                                "letter-spacing: 0.5px; background: transparent; %2")
                            .arg(is_popular ? "#d97706" : "#e5e5e5")
                            .arg(MF));
    vl->addWidget(name);

    // Description
    if (!plan.description.isEmpty()) {
        auto* desc = new QLabel(plan.description);
        desc->setWordWrap(true);
        desc->setAlignment(Qt::AlignCenter);
        desc->setStyleSheet(QString("color: #525252; font-size: 12px; background: transparent; %1").arg(MF));
        vl->addWidget(desc);
    }

    vl->addSpacing(6);

    // Price
    if (plan.is_free) {
        auto* price = new QLabel("FREE");
        price->setAlignment(Qt::AlignCenter);
        price->setStyleSheet(QString("color: #e5e5e5; font-size: 28px; font-weight: 700; "
                                     "background: transparent; %1")
                                 .arg(MF));
        vl->addWidget(price);
    } else {
        auto* price = new QLabel(QString("$%1").arg(plan.price_usd, 0, 'f', 0));
        price->setAlignment(Qt::AlignCenter);
        price->setStyleSheet(QString("color: #d97706; font-size: 28px; font-weight: 700; "
                                     "background: transparent; %1")
                                 .arg(MF));
        vl->addWidget(price);

        auto* period = new QLabel(QString("/ %1 days").arg(plan.validity_days));
        period->setAlignment(Qt::AlignCenter);
        period->setStyleSheet(QString("color: #404040; font-size: 12px; background: transparent; %1").arg(MF));
        vl->addWidget(period);
    }

    vl->addSpacing(2);

    // Credits
    auto* credits = new QLabel(QString("%1 credits").arg(plan.credits));
    credits->setAlignment(Qt::AlignCenter);
    credits->setStyleSheet(QString("color: #0891b2; font-size: 13px; font-weight: 700; "
                                   "background: transparent; %1")
                               .arg(MF));
    vl->addWidget(credits);

    // Support
    auto* support = new QLabel(QString("%1 support").arg(plan.support_type));
    support->setAlignment(Qt::AlignCenter);
    support->setStyleSheet(QString("color: #525252; font-size: 12px; background: transparent; %1").arg(MF));
    vl->addWidget(support);

    // Separator
    auto* sep = new QFrame;
    sep->setFixedHeight(1);
    sep->setStyleSheet("background: #1a1a1a;");
    vl->addWidget(sep);

    vl->addSpacing(2);

    // Features
    for (const auto& feature : plan.features) {
        auto* feat = new QLabel(QString("  %1").arg(feature));
        feat->setWordWrap(true);
        feat->setStyleSheet(QString("color: #808080; font-size: 12px; background: transparent; %1").arg(MF));
        vl->addWidget(feat);
    }

    vl->addStretch();

    // ── Action button ────────────────────────────────────────────────────────
    auto* btn = new QPushButton;
    btn->setFixedHeight(26);
    btn->setCursor(Qt::PointingHandCursor);

    if (is_current) {
        btn->setText("ACTIVE");
        btn->setEnabled(false);
        btn->setStyleSheet(QString("QPushButton { background: rgba(22,163,74,0.1); color: #16a34a; "
                                   "border: 1px solid #16a34a; font-size: 11px; font-weight: 700; %1 }"
                                   "QPushButton:disabled { color: #16a34a; }")
                               .arg(MF));
    } else if (plan.is_free) {
        bool user_has_paid = auth_mgr.is_authenticated() && auth_mgr.session().has_paid_plan();
        if (user_has_paid) {
            btn->setText("FREE TIER");
            btn->setEnabled(false);
            btn->setStyleSheet(QString("QPushButton { background: #111111; color: #404040; "
                                       "border: 1px solid #1a1a1a; font-size: 11px; font-weight: 700; %1 }")
                                   .arg(MF));
        } else {
            btn->setText("CONTINUE FREE");
            btn->setStyleSheet(QString("QPushButton { background: #111111; color: #808080; "
                                       "border: 1px solid #1a1a1a; font-size: 11px; font-weight: 700; %1 }"
                                       "QPushButton:hover { color: #e5e5e5; background: #161616; }")
                                   .arg(MF));
            connect(btn, &QPushButton::clicked, this, &PricingScreen::navigate_dashboard);
        }
    } else {
        btn->setText("SELECT PLAN");
        btn->setStyleSheet(
            QString("QPushButton { background: rgba(217,119,6,0.1); color: #d97706; "
                    "border: 1px solid #78350f; font-size: 11px; font-weight: 700; %1 }"
                    "QPushButton:hover { background: #d97706; color: #080808; }"
                    "QPushButton:disabled { background: #111111; color: #404040; border-color: #1a1a1a; }")
                .arg(MF));
        QString pid = plan.plan_id;
        connect(btn, &QPushButton::clicked, this, [this, pid]() { on_select_plan(pid); });
    }

    vl->addWidget(btn);

    return card;
}

// ── Payment ──────────────────────────────────────────────────────────────────

void PricingScreen::on_select_plan(const QString& plan_id) {
    for (auto* btn : cards_container_->findChildren<QPushButton*>()) {
        if (btn->text() == "SELECT PLAN") {
            btn->setEnabled(false);
            btn->setText("PROCESSING...");
        }
    }
    error_label_->hide();

    auth::AuthApi::instance().create_payment_order(plan_id, [this, plan_id](auth::ApiResponse r) {
        for (auto* btn : cards_container_->findChildren<QPushButton*>()) {
            if (btn->text() == "PROCESSING...") {
                btn->setEnabled(true);
                btn->setText("SELECT PLAN");
            }
        }

        if (!r.success) {
            error_label_->setText(r.error.isEmpty() ? "Failed to create payment session" : r.error);
            error_label_->show();
            return;
        }

        QJsonObject data = r.data;
        if (data.contains("data") && data["data"].isObject())
            data = data["data"].toObject();

        QString checkout_url;
        if (data.contains("payment_session_id")) {
            QString sid = data["payment_session_id"].toString();
            checkout_url = "https://fincept.in/checkout.html?session=" + sid;
        }
        if (checkout_url.isEmpty() && data.contains("checkout_url"))
            checkout_url = data["checkout_url"].toString();
        if (checkout_url.isEmpty() && data.contains("url"))
            checkout_url = data["url"].toString();

        QString order_id = data["order_id"].toString();

        if (!checkout_url.isEmpty()) {
            QDesktopServices::openUrl(QUrl(checkout_url));
            // Navigate to payment processing screen to poll for confirmation
            if (!order_id.isEmpty())
                emit payment_initiated(order_id, plan_id);
        } else {
            error_label_->setText("No checkout URL received from server");
            error_label_->show();
        }
    });
}

// ── Footer ───────────────────────────────────────────────────────────────────

void PricingScreen::update_footer() {
    auto* fl = footer_widget_->layout();
    if (!fl)
        return;

    while (fl->count() > 2) {
        auto* item = fl->takeAt(2);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    auto& auth_mgr = auth::AuthManager::instance();
    bool user_has_paid = auth_mgr.is_authenticated() && auth_mgr.session().has_paid_plan();

    if (user_has_paid) {
        auto* back_btn = new QPushButton("Back to Dashboard");
        back_btn->setCursor(Qt::PointingHandCursor);
        back_btn->setStyleSheet(QString("QPushButton { color: #808080; background: transparent; border: none; "
                                        "font-size: 12px; %1 }"
                                        "QPushButton:hover { color: #e5e5e5; }")
                                    .arg(MF));
        connect(back_btn, &QPushButton::clicked, this, &PricingScreen::navigate_dashboard);
        fl->addWidget(back_btn);
        static_cast<QVBoxLayout*>(fl)->setAlignment(back_btn, Qt::AlignCenter);
    } else {
        auto* row = new QWidget;
        row->setStyleSheet("background: transparent;");
        auto* hl = new QHBoxLayout(row);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->setAlignment(Qt::AlignCenter);
        hl->setSpacing(6);

        auto* explore = new QLabel("Want to explore first?");
        explore->setStyleSheet(QString("color: #404040; font-size: 12px; background: transparent; %1").arg(MF));
        hl->addWidget(explore);

        auto* free_btn = new QPushButton("Continue with Free Plan");
        free_btn->setCursor(Qt::PointingHandCursor);
        free_btn->setStyleSheet(QString("QPushButton { color: #808080; background: transparent; border: none; "
                                        "font-size: 12px; %1 }"
                                        "QPushButton:hover { color: #e5e5e5; }")
                                    .arg(MF));
        connect(free_btn, &QPushButton::clicked, this, &PricingScreen::navigate_dashboard);
        hl->addWidget(free_btn);

        fl->addWidget(row);
    }
}

} // namespace fincept::screens
