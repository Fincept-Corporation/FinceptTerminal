#include "screens/auth/PricingScreen.h"

#include "auth/AuthApi.h"
#include "auth/AuthManager.h"
#include "core/currency/Currency.h"
#include "ui/theme/Theme.h"

#include <QApplication>
#include <QDesktopServices>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QScrollArea>
#include <QUrl>

#include <algorithm>

namespace fincept::screens {

// ── Obsidian Design System constants ─────────────────────────────────────────

static const char* MF = "font-family:'Consolas','Courier New',monospace;";

static QString card_normal() {
    return QString("QWidget#plan_card { background: %1; border: 1px solid %2; }"
                   "QLabel { border: none; background: transparent; }"
                   "QFrame { border: none; background: transparent; }")
        .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM());
}

static QString card_popular() {
    return QString("QWidget#plan_card { background: %1; border: 1px solid %2; }"
                   "QLabel { border: none; background: transparent; }"
                   "QFrame { border: none; background: transparent; }")
        .arg(ui::colors::BG_SURFACE(), ui::colors::AMBER_DIM());
}

static QString card_active() {
    return QString("QWidget#plan_card { background: %1; border: 1px solid %2; }"
                   "QLabel { border: none; background: transparent; }"
                   "QFrame { border: none; background: transparent; }")
        .arg(ui::colors::BG_SURFACE(), ui::colors::POSITIVE());
}

// ── Constructor ──────────────────────────────────────────────────────────────

PricingScreen::PricingScreen(QWidget* parent) : QWidget(parent) {
    build_ui();
    confetti_ = new ui::ConfettiOverlay(this);
}

void PricingScreen::build_ui() {
    setStyleSheet(QString("background: %1;").arg(ui::colors::BG_BASE()));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet(QString("QScrollArea { background: transparent; border: none; }"
                                  "QScrollBar:vertical { background: transparent; width: 6px; }"
                                  "QScrollBar::handle:vertical { background: %1; }"
                                  "QScrollBar::handle:vertical:hover { background: %2; }"
                                  "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
                              .arg(ui::colors::BORDER_MED(), ui::colors::BORDER_BRIGHT()));

    auto* content = new QWidget(this);
    content->setStyleSheet("background: transparent;");
    auto* vl = new QVBoxLayout(content);
    vl->setContentsMargins(14, 14, 14, 14);
    vl->setSpacing(10);

    // ── Header ───────────────────────────────────────────────────────────────
    title_label_ = new QLabel;
    title_label_->setAlignment(Qt::AlignCenter);
    title_label_->setStyleSheet(QString("color: %1; font-size: 20px; font-weight: 700; "
                                        "letter-spacing: 1px; background: transparent; %2")
                                    .arg(ui::colors::AMBER())
                                    .arg(MF));
    vl->addWidget(title_label_);

    subtitle_label_ = new QLabel;
    subtitle_label_->setAlignment(Qt::AlignCenter);
    subtitle_label_->setStyleSheet(
        QString("color: %1; font-size: 13px; background: transparent; %2").arg(ui::colors::TEXT_TERTIARY()).arg(MF));
    vl->addWidget(subtitle_label_);

    user_info_label_ = new QLabel;
    user_info_label_->setAlignment(Qt::AlignCenter);
    user_info_label_->setStyleSheet(
        QString("color: %1; font-size: 12px; background: transparent; %2").arg(ui::colors::TEXT_SECONDARY()).arg(MF));
    vl->addWidget(user_info_label_);

    // ── Loading ──────────────────────────────────────────────────────────────
    loading_label_ = new QLabel;
    loading_label_->setAlignment(Qt::AlignCenter);
    loading_label_->setStyleSheet(
        QString("color: %1; font-size: 13px; background: transparent; %2").arg(ui::colors::TEXT_DIM()).arg(MF));
    loading_label_->hide();
    vl->addWidget(loading_label_);

    // ── Error ────────────────────────────────────────────────────────────────
    error_label_ = new QLabel;
    error_label_->setWordWrap(true);
    error_label_->setAlignment(Qt::AlignCenter);
    error_label_->setStyleSheet(QString("color: %1; font-size: 12px; background: rgba(220,38,38,0.1); "
                                        "border: 1px solid #7f1d1d; padding: 6px 12px; %2")
                                    .arg(ui::colors::NEGATIVE())
                                    .arg(MF));
    error_label_->hide();
    vl->addWidget(error_label_);

    // ── Cards container ──────────────────────────────────────────────────────
    cards_container_ = new QWidget(this);
    cards_container_->setStyleSheet("background: transparent;");
    cards_layout_ = new QVBoxLayout(cards_container_);
    cards_layout_->setContentsMargins(0, 0, 0, 0);
    vl->addWidget(cards_container_);

    // ── Footer ───────────────────────────────────────────────────────────────
    footer_widget_ = new QWidget(this);
    footer_widget_->setStyleSheet("background: transparent;");
    auto* footer_vl = new QVBoxLayout(footer_widget_);
    footer_vl->setContentsMargins(0, 10, 0, 0);
    footer_vl->setSpacing(6);

    auto* sep = new QFrame;
    sep->setFixedHeight(1);
    sep->setStyleSheet(QString("background: %1; border: none;").arg(ui::colors::BORDER_DIM()));
    footer_vl->addWidget(sep);

    footer_vl->addSpacing(4);
    vl->addWidget(footer_widget_);

    vl->addStretch();
    scroll->setWidget(content);
    root->addWidget(scroll);

    retranslateUi();
}

void PricingScreen::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
        // Per-card buttons + footer carry state-dependent labels — easier to
        // rebuild than to thread retranslation through every dynamic widget.
        if (!plans_.empty())
            render_plan_cards();
        update_footer();
    }
    QWidget::changeEvent(event);
}

void PricingScreen::retranslateUi() {
    if (title_label_)    title_label_->setText(tr("PLANS & PRICING"));
    if (subtitle_label_) subtitle_label_->setText(tr("Unlock the full power of Fincept Terminal"));
    if (loading_label_)
        loading_label_->setText(loading_is_refresh_ ? tr("Updating plan status...")
                                                    : tr("Loading plans..."));
}

void PricingScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);

    auto& auth = auth::AuthManager::instance();
    if (auth.is_authenticated()) {
        // Show loading while we fetch fresh data
        loading_is_refresh_ = true;
        loading_label_->setText(tr("Updating plan status..."));
        loading_label_->show();
        user_info_label_->hide();

        // Fetch fresh user data from API, then render once with accurate data
        auth.refresh_user_data();
        auto* conn = new QMetaObject::Connection;
        *conn = connect(&auth, &auth::AuthManager::auth_state_changed, this, [this, conn]() {
            disconnect(*conn);
            delete conn;

            loading_label_->hide();

            auto& mgr = auth::AuthManager::instance();
            if (mgr.is_authenticated()) {
                auto& s = mgr.session();
                QString info = (s.user_info.email.isEmpty() ? s.user_info.username : s.user_info.email);
                info += QString("  |  %1 CR").arg(s.user_info.credit_balance, 0, 'f', 0);
                user_info_label_->setText(info);
                user_info_label_->show();
            }

            // Render cards once with fresh data
            fetched_ = false;
            fetch_plans();
            update_footer();
        });
    } else {
        user_info_label_->hide();
        fetched_ = false;
        fetch_plans();
        update_footer();
    }
}

// ── Fetch plans ──────────────────────────────────────────────────────────────

void PricingScreen::fetch_plans() {
    if (loading_)
        return;
    loading_ = true;
    fetched_ = true;
    loading_is_refresh_ = false;
    loading_label_->setText(tr("Loading plans..."));
    loading_label_->show();
    error_label_->hide();

    auth::AuthApi::instance().get_subscription_plans([this](auth::ApiResponse r) {
        loading_ = false;
        loading_label_->hide();

        if (!r.success) {
            error_label_->setText(r.error.isEmpty() ? tr("Failed to load plans") : r.error);
            error_label_->show();
            return;
        }

        QJsonObject payload = r.data;
        QJsonArray plans_array;
        if (payload.contains("data") && payload["data"].isArray()) {
            plans_array = payload["data"].toArray();
        } else if (payload.contains("data") && payload["data"].isObject()) {
            auto inner = payload["data"].toObject();
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
        auto* empty = new QLabel(tr("No plans available."));
        empty->setAlignment(Qt::AlignCenter);
        empty->setStyleSheet(
            QString("color: %1; font-size: 13px; background: transparent; %2").arg(ui::colors::TEXT_DIM()).arg(MF));
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
        const auto& s = auth_mgr.session();
        QString user_plan = s.account_type().toLower();
        is_current = (user_plan == plan.plan_id.toLower());
    }

    auto* card = new QWidget(this);
    card->setObjectName("plan_card");
    card->setFixedWidth(260);
    card->setMinimumHeight(400);
    card->setStyleSheet(is_current ? card_active() : (is_popular ? card_popular() : card_normal()));

    auto* vl = new QVBoxLayout(card);
    vl->setContentsMargins(12, 12, 12, 12);
    vl->setSpacing(4);

    // Popular badge
    if (is_popular) {
        auto* badge = new QLabel(tr("RECOMMENDED"));
        badge->setAlignment(Qt::AlignCenter);
        badge->setFixedHeight(20);
        badge->setStyleSheet(QString("color: %1; background: rgba(217,119,6,0.1); "
                                     "border: 1px solid %2; padding: 0 8px; "
                                     "font-size: 11px; font-weight: 700; letter-spacing: 0.5px; %3")
                                 .arg(ui::colors::AMBER(), ui::colors::AMBER_DIM())
                                 .arg(MF));
        vl->addWidget(badge, 0, Qt::AlignCenter);
        vl->addSpacing(2);
    }

    // Plan name
    auto* name = new QLabel(plan.name.toUpper());
    name->setAlignment(Qt::AlignCenter);
    name->setStyleSheet(QString("color: %1; font-size: 16px; font-weight: 700; "
                                "letter-spacing: 0.5px; background: transparent; %2")
                            .arg(is_popular ? ui::colors::AMBER() : ui::colors::TEXT_PRIMARY())
                            .arg(MF));
    vl->addWidget(name);

    // Description
    if (!plan.description.isEmpty()) {
        auto* desc = new QLabel(plan.description);
        desc->setWordWrap(true);
        desc->setAlignment(Qt::AlignCenter);
        desc->setStyleSheet(
            QString("color: %1; font-size: 12px; background: transparent; %2").arg(ui::colors::TEXT_TERTIARY()).arg(MF));
        vl->addWidget(desc);
    }

    vl->addSpacing(6);

    // Price
    if (plan.is_free) {
        auto* price = new QLabel(tr("FREE"));
        price->setAlignment(Qt::AlignCenter);
        price->setStyleSheet(QString("color: %1; font-size: 28px; font-weight: 700; "
                                     "background: transparent; %2")
                                 .arg(ui::colors::TEXT_PRIMARY())
                                 .arg(MF));
        vl->addWidget(price);
    } else {
        // Plan prices are intrinsically USD (not FX-converted), so pin the symbol
        // to USD rather than following the global currency preference. Preserve the
        // whole-number format used for plan prices.
        auto* price = new QLabel(cur::symbol_for("USD") + QString::number(plan.price_usd, 'f', 0));
        price->setAlignment(Qt::AlignCenter);
        price->setStyleSheet(QString("color: %1; font-size: 28px; font-weight: 700; "
                                     "background: transparent; %2")
                                 .arg(ui::colors::AMBER())
                                 .arg(MF));
        vl->addWidget(price);

        auto* period = new QLabel(tr("/ %1 days").arg(plan.validity_days));
        period->setAlignment(Qt::AlignCenter);
        period->setStyleSheet(
            QString("color: %1; font-size: 12px; background: transparent; %2").arg(ui::colors::TEXT_DIM()).arg(MF));
        vl->addWidget(period);
    }

    vl->addSpacing(2);

    // Credits
    auto* credits = new QLabel(tr("%1 credits").arg(plan.credits));
    credits->setAlignment(Qt::AlignCenter);
    credits->setStyleSheet(QString("color: %1; font-size: 13px; font-weight: 700; "
                                   "background: transparent; %2")
                               .arg(ui::colors::CYAN())
                               .arg(MF));
    vl->addWidget(credits);

    // Support — keep the support_type token (e.g. "Standard", "Priority") as
    // returned by the API; localise only the surrounding word.
    auto* support = new QLabel(tr("%1 support").arg(plan.support_type));
    support->setAlignment(Qt::AlignCenter);
    support->setStyleSheet(
        QString("color: %1; font-size: 12px; background: transparent; %2").arg(ui::colors::TEXT_TERTIARY()).arg(MF));
    vl->addWidget(support);

    // Separator
    auto* sep = new QFrame;
    sep->setFixedHeight(1);
    sep->setStyleSheet(QString("background: %1;").arg(ui::colors::BORDER_DIM()));
    vl->addWidget(sep);

    vl->addSpacing(2);

    // Features
    for (const auto& feature : plan.features) {
        auto* feat = new QLabel(QString("  %1").arg(feature));
        feat->setWordWrap(true);
        feat->setStyleSheet(
            QString("color: %1; font-size: 12px; background: transparent; %2").arg(ui::colors::TEXT_SECONDARY()).arg(MF));
        vl->addWidget(feat);
    }

    vl->addStretch();

    // ── Action button ────────────────────────────────────────────────────────
    auto* btn = new QPushButton;
    btn->setFixedHeight(26);
    btn->setCursor(Qt::PointingHandCursor);

    if (is_current) {
        btn->setText(tr("ACTIVE"));
        btn->setEnabled(false);
        btn->setStyleSheet(QString("QPushButton { background: rgba(22,163,74,0.1); color: %1; "
                                   "border: 1px solid %1; font-size: 11px; font-weight: 700; %2 }"
                                   "QPushButton:disabled { color: %1; }")
                               .arg(ui::colors::POSITIVE())
                               .arg(MF));
    } else if (plan.is_free) {
        bool user_has_paid = auth_mgr.is_authenticated() && auth_mgr.session().has_paid_plan();
        if (user_has_paid) {
            btn->setText(tr("FREE TIER"));
            btn->setEnabled(false);
            btn->setStyleSheet(QString("QPushButton { background: %1; color: %2; "
                                       "border: 1px solid %3; font-size: 11px; font-weight: 700; %4 }")
                                   .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_DIM(), ui::colors::BORDER_DIM())
                                   .arg(MF));
        } else {
            btn->setText(tr("CONTINUE FREE"));
            btn->setStyleSheet(QString("QPushButton { background: %1; color: %2; "
                                       "border: 1px solid %3; font-size: 11px; font-weight: 700; %4 }"
                                       "QPushButton:hover { color: %5; background: %6; }")
                                   .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_DIM())
                                   .arg(MF)
                                   .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_HOVER()));
            connect(btn, &QPushButton::clicked, this, &PricingScreen::navigate_dashboard);
        }
    } else {
        btn->setText(tr("SELECT PLAN"));
        btn->setStyleSheet(
            QString("QPushButton { background: rgba(217,119,6,0.1); color: %1; "
                    "border: 1px solid %2; font-size: 11px; font-weight: 700; %3 }"
                    "QPushButton:hover { background: %1; color: %4; }"
                    "QPushButton:disabled { background: %5; color: %6; border-color: %7; }")
                .arg(ui::colors::AMBER(), ui::colors::AMBER_DIM())
                .arg(MF)
                .arg(ui::colors::BG_BASE(), ui::colors::BG_RAISED(), ui::colors::TEXT_DIM(), ui::colors::BORDER_DIM()));
        QString pid = plan.plan_id;
        connect(btn, &QPushButton::clicked, this, [this, pid]() { on_select_plan(pid); });
    }

    vl->addWidget(btn);

    return card;
}

// ── Payment ──────────────────────────────────────────────────────────────────

void PricingScreen::on_select_plan(const QString& plan_id) {
    const QString select_text = tr("SELECT PLAN");
    const QString processing_text = tr("PROCESSING...");
    for (auto* btn : cards_container_->findChildren<QPushButton*>()) {
        if (btn->text() == select_text) {
            btn->setEnabled(false);
            btn->setText(processing_text);
        }
    }
    error_label_->hide();

    auth::AuthApi::instance().generate_checkout_token(plan_id, [this, plan_id, processing_text, select_text](auth::ApiResponse r) {
        for (auto* btn : cards_container_->findChildren<QPushButton*>()) {
            if (btn->text() == processing_text) {
                btn->setEnabled(true);
                btn->setText(select_text);
            }
        }

        if (!r.success) {
            error_label_->setText(r.error.isEmpty() ? tr("Failed to generate checkout token") : r.error);
            error_label_->show();
            return;
        }

        QJsonObject payload = r.data;
        if (payload.contains("data") && payload["data"].isObject())
            payload = payload["data"].toObject();

        QString token = payload["token"].toString();
        if (token.isEmpty()) {
            error_label_->setText(tr("No checkout token received from server"));
            error_label_->show();
            return;
        }

        QString url = QString("https://fincept.in/checkout?token=%1&plan=%2")
                          .arg(QUrl::toPercentEncoding(token), QUrl::toPercentEncoding(plan_id));
        QDesktopServices::openUrl(QUrl(url));

        // Store plan before payment so we can detect changes
        awaiting_payment_ = true;
        awaiting_plan_id_ = plan_id;
        pre_payment_plan_ = auth::AuthManager::instance().session().account_type().toLower();

        // Start polling every 5 seconds to detect plan change
        if (!payment_poll_timer_) {
            payment_poll_timer_ = new QTimer(this);
            payment_poll_timer_->setInterval(1000);
            connect(payment_poll_timer_, &QTimer::timeout, this, &PricingScreen::poll_payment_status);
        }
        payment_poll_timer_->start();

        // Also check on focus return for faster detection
        if (focus_connection_)
            disconnect(focus_connection_);
        focus_connection_ =
            connect(qApp, &QApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
                if (state == Qt::ApplicationActive && awaiting_payment_)
                    poll_payment_status();
            });
    });
}

void PricingScreen::poll_payment_status() {
    if (!awaiting_payment_)
        return;

    auto& auth = auth::AuthManager::instance();
    auth.refresh_user_data();

    auto* conn = new QMetaObject::Connection;
    *conn = connect(&auth, &auth::AuthManager::auth_state_changed, this, [this, conn]() {
        disconnect(*conn);
        delete conn;

        auto& mgr = auth::AuthManager::instance();

        // Update user info label
        if (mgr.is_authenticated()) {
            auto& s = mgr.session();
            QString info = (s.user_info.email.isEmpty() ? s.user_info.username : s.user_info.email);
            info += QString("  |  %1 CR").arg(s.user_info.credit_balance, 0, 'f', 0);
            user_info_label_->setText(info);
        }

        QString new_plan = mgr.session().account_type().toLower();

        if (new_plan != pre_payment_plan_) {
            // Plan changed — stop polling, re-render, show confetti
            awaiting_payment_ = false;
            if (payment_poll_timer_)
                payment_poll_timer_->stop();
            if (focus_connection_)
                disconnect(focus_connection_);

            fetched_ = false;
            fetch_plans();
            update_footer();

            confetti_->setGeometry(0, 0, width(), height());
            confetti_->show_confetti();
        }
    });
}

void PricingScreen::on_app_focus_returned() {
    poll_payment_status();
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
        auto* back_btn = new QPushButton(tr("Back to Dashboard"));
        back_btn->setCursor(Qt::PointingHandCursor);
        back_btn->setStyleSheet(QString("QPushButton { color: %1; background: transparent; border: none; "
                                        "font-size: 12px; %2 }"
                                        "QPushButton:hover { color: %3; }")
                                    .arg(ui::colors::TEXT_SECONDARY())
                                    .arg(MF)
                                    .arg(ui::colors::TEXT_PRIMARY()));
        connect(back_btn, &QPushButton::clicked, this, &PricingScreen::navigate_dashboard);
        fl->addWidget(back_btn);
        static_cast<QVBoxLayout*>(fl)->setAlignment(back_btn, Qt::AlignCenter);
    } else {
        auto* row = new QWidget(this);
        row->setStyleSheet("background: transparent;");
        auto* hl = new QHBoxLayout(row);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->setAlignment(Qt::AlignCenter);
        hl->setSpacing(6);

        auto* explore = new QLabel(tr("Want to explore first?"));
        explore->setStyleSheet(
            QString("color: %1; font-size: 12px; background: transparent; %2").arg(ui::colors::TEXT_DIM()).arg(MF));
        hl->addWidget(explore);

        auto* free_btn = new QPushButton(tr("Continue with Free Plan"));
        free_btn->setCursor(Qt::PointingHandCursor);
        free_btn->setStyleSheet(QString("QPushButton { color: %1; background: transparent; border: none; "
                                        "font-size: 12px; %2 }"
                                        "QPushButton:hover { color: %3; }")
                                    .arg(ui::colors::TEXT_SECONDARY())
                                    .arg(MF)
                                    .arg(ui::colors::TEXT_PRIMARY()));
        connect(free_btn, &QPushButton::clicked, this, &PricingScreen::navigate_dashboard);
        hl->addWidget(free_btn);

        fl->addWidget(row);
    }
}

} // namespace fincept::screens
