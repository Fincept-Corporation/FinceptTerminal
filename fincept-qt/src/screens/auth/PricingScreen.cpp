#include "screens/auth/PricingScreen.h"

#include "auth/AuthApi.h"
#include "auth/AuthManager.h"
#include "core/currency/Currency.h"
#include "core/logging/Logger.h"
#include "services/billing/PlanAccess.h"
#include "storage/repositories/SettingsRepository.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/ConfettiOverlay.h"
#include "ui/widgets/EnterprisePromo.h"
#include "ui/widgets/MnemonicText.h"
#include "ui/widgets/PlanHint.h"

#include <QApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QEvent>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QJsonDocument>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QShortcut>
#include <QStyle>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <utility>

namespace fincept::screens {

namespace {

constexpr const char* kPricingTag = "Pricing";
/// Last plan list the backend returned. Rendered instantly on the next visit
/// and kept on screen when the network is down.
constexpr const char* kPricingPlansCacheKey = "billing.plans_cache";
constexpr int kPricingCardMinWidth = 230;
constexpr int kPricingCardMaxWidth = 380;

QString pricing_bullet() {
    return QString::fromUtf8("\xe2\x96\xb8  "); // U+25B8 + two spaces
}

/// "$10", "$9.99", or "10 EUR" when the plan is not priced in USD.
QString pricing_format_price(double amount, const QString& currency) {
    const bool whole = std::fabs(amount - std::round(amount)) < 0.005;
    const QString number = QString::number(amount, 'f', whole ? 0 : 2);
    const QString code = currency.trimmed().toUpper();
    if (code.isEmpty() || code == QLatin1String("USD"))
        return cur::symbol_for(QStringLiteral("USD")) + number;
    return number + QLatin1Char(' ') + code;
}

void pricing_repolish(QWidget* w) {
    w->style()->unpolish(w);
    w->style()->polish(w);
}

QJsonArray pricing_extract_plans(const QJsonObject& payload) {
    if (payload.value("data").isArray())
        return payload.value("data").toArray();
    if (payload.value("data").isObject()) {
        const QJsonObject inner = payload.value("data").toObject();
        if (inner.value("data").isArray())
            return inner.value("data").toArray();
    }
    return {};
}

/// When the account's paid pass ends (ISO, naive UTC as the API sends it).
/// The subscription record is fetched last in a refresh, so it wins.
QString pricing_pass_end(const auth::SessionData& s) {
    return s.subscription.credits_expire_at.isEmpty() ? s.user_info.credits_expire_at
                                                      : s.subscription.credits_expire_at;
}

/// `a` is a later API timestamp than `b` (an empty `b` = none yet). Both are
/// read the same way, so the missing offset does not matter for the comparison.
bool pricing_is_later(const QString& a, const QString& b) {
    if (a.isEmpty())
        return false;
    if (b.isEmpty())
        return true;
    QDateTime ta = QDateTime::fromString(a, Qt::ISODateWithMs);
    if (!ta.isValid())
        ta = QDateTime::fromString(a, Qt::ISODate);
    QDateTime tb = QDateTime::fromString(b, Qt::ISODateWithMs);
    if (!tb.isValid())
        tb = QDateTime::fromString(b, Qt::ISODate);
    if (ta.isValid() && tb.isValid())
        return ta > tb;
    return a != b;
}

/// Height of a plan card's badge row: 20px at the default font size, more for
/// a larger one.
int pricing_badge_height() {
    return qMax(20, QFontMetrics(ui::fonts::font(-4)).height() + 4);
}

/// The whole screen is styled by this one sheet on the root widget — cards,
/// badges and buttons select on objectName + dynamic properties (variant,
/// tone, planAction), so rendering a card costs no stylesheet parse.
QString pricing_qss() {
    QString ss = QString(
        "#pricingRoot{background:{BG_BASE};}"
        "QScrollArea#pricingScroll{background:transparent;border:none;}"
        "#pricingContent{background:transparent;}"
        "QScrollBar:vertical{background:transparent;width:6px;}"
        "QScrollBar::handle:vertical{background:{BORDER_MED};}"
        "QScrollBar::handle:vertical:hover{background:{BORDER_BRIGHT};}"
        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}"
        "QLabel{background:transparent;border:none;}"
        "#pricingTitle{color:{AMBER};font-size:" + fincept::ui::fonts::px(6) + "px;font-weight:700;letter-spacing:2px;}"
        "#pricingSubtitle{color:{TEXT_SECONDARY};font-size:" + fincept::ui::fonts::px(-1) + "px;}"
        "#pricingAccount{color:{TEXT_TERTIARY};font-size:" + fincept::ui::fonts::px(-2) + "px;}"
        "#pricingStatus{color:{TEXT_SECONDARY};font-size:" + fincept::ui::fonts::px(-2) + "px;}"
        "#pricingStatus[tone=\"error\"]{color:{NEGATIVE};}"
        "#pricingStatus[tone=\"success\"]{color:{POSITIVE};}"
        "QPushButton#pricingRetry{background:transparent;color:{AMBER};border:1px solid {AMBER_DIM};"
        "  padding:3px 12px;font-weight:700;}"
        "QPushButton#pricingRetry:hover{color:{TEXT_PRIMARY};border-color:{AMBER};}"
        "QFrame#planCard{background:{BG_SURFACE};border:1px solid {BORDER_DIM};}"
        "QFrame#planCard[variant=\"recommended\"]{border:1px solid {AMBER_DIM};border-top:2px solid {AMBER};}"
        "QFrame#planCard[variant=\"current\"]{border-top:2px solid {POSITIVE};}"
        "QFrame#planCard[variant=\"enterprise\"]{background:{BG_RAISED};border:1px solid {BORDER_MED};"
        "  border-top:2px solid {AMBER_DIM};}"
        // The enterprise card sits on BG_RAISED, where the muted text tones are too faint.
        "QFrame#planCard[variant=\"enterprise\"] QLabel#planDesc,"
        "QFrame#planCard[variant=\"enterprise\"] QLabel#planNote{color:{TEXT_SECONDARY};}"
        "#planBadge{font-size:" + fincept::ui::fonts::px(-4) + "px;font-weight:700;letter-spacing:1px;padding:0 8px;}"
        "#planBadge[tone=\"recommended\"]{color:{AMBER};border:1px solid {AMBER_DIM};}"
        "#planBadge[tone=\"current\"]{color:{POSITIVE};border:1px solid {POSITIVE};}"
        "#planBadge[tone=\"enterprise\"]{color:{TEXT_SECONDARY};border:1px solid {BORDER_MED};}"
        "#planName{color:{TEXT_PRIMARY};font-size:" + fincept::ui::fonts::px(1) + "px;font-weight:700;letter-spacing:1.5px;}"
        "#planName[accent=\"true\"]{color:{AMBER};}"
        "#planDesc{color:{TEXT_TERTIARY};font-size:" + fincept::ui::fonts::px(-2) + "px;}"
        "#planPrice{color:{TEXT_PRIMARY};font-size:" + fincept::ui::fonts::px(16) + "px;font-weight:700;}"
        "#planPrice[accent=\"true\"]{color:{AMBER};}"
        "#planPeriod{color:{TEXT_DIM};font-size:" + fincept::ui::fonts::px(-2) + "px;}"
        "QFrame#planRule{background:{BORDER_DIM};border:none;}"
        "#planFeature{color:{TEXT_SECONDARY};font-size:" + fincept::ui::fonts::px(-2) + "px;}"
        "#planNote{color:{TEXT_DIM};font-size:" + fincept::ui::fonts::px(-3) + "px;}"
        "QPushButton#planPriceLink{background:transparent;border:none;padding:0;color:{AMBER};font-size:" + fincept::ui::fonts::px(2) + "px;"
        "  font-weight:700;text-align:left;}"
        "QPushButton#planPriceLink:hover{color:{TEXT_PRIMARY};}"
        "QPushButton#planBtn{min-height:34px;padding:0 14px;font-weight:700;letter-spacing:0.5px;"
        "  background:transparent;color:{TEXT_SECONDARY};border:1px solid {BORDER_MED};}"
        "QPushButton#planBtn:hover{color:{TEXT_PRIMARY};border-color:{TEXT_SECONDARY};}"
        "QPushButton#planBtn[planAction=\"select\"]{background:{AMBER};color:{TEXT_ON_ACCENT};border-color:{AMBER};}"
        "QPushButton#planBtn[planAction=\"select\"]:hover{border-color:{TEXT_PRIMARY};}"
        "QPushButton#planBtn[planAction=\"select\"]:pressed{background:{AMBER_DIM};color:{TEXT_ON_ACCENT};"
        "  border-color:{AMBER_DIM};}"
        "QPushButton#planBtn[planAction=\"current\"]:disabled{color:{POSITIVE};border-color:{POSITIVE};}"
        "QPushButton#planBtn[planAction=\"included\"]:disabled{color:{TEXT_DIM};border-color:{BORDER_DIM};}"
        "QPushButton#planBtn[planAction=\"processing\"]:disabled{color:{TEXT_TERTIARY};border-color:{BORDER_DIM};"
        "  background:{BG_RAISED};}"
        "QPushButton#planEntPrimary{min-height:34px;padding:0 14px;font-weight:700;letter-spacing:0.5px;"
        "  background:transparent;color:{AMBER};border:1px solid {AMBER};}"
        "QPushButton#planEntPrimary:hover{background:{AMBER};color:{TEXT_ON_ACCENT};}"
        "QPushButton#planEntSecondary{min-height:30px;padding:0 14px;font-weight:700;letter-spacing:0.5px;"
        "  background:transparent;color:{TEXT_SECONDARY};border:1px solid {BORDER_MED};}"
        "QPushButton#planEntSecondary:hover{color:{AMBER};border-color:{AMBER};}"
        "#pricingFootnote{color:{TEXT_DIM};font-size:" + fincept::ui::fonts::px(-3) + "px;}"
        "QPushButton#pricingBack{background:transparent;border:none;color:{TEXT_SECONDARY};font-size:" + fincept::ui::fonts::px(-2) + "px;"
        "  font-weight:700;letter-spacing:0.5px;padding:6px 10px;}"
        "QPushButton#pricingBack:hover{color:{AMBER};}");
    const std::pair<const char*, const char*> tokens[] = {
        {"{BG_BASE}", ui::colors::BG_BASE()},
        {"{BG_SURFACE}", ui::colors::BG_SURFACE()},
        {"{BG_RAISED}", ui::colors::BG_RAISED()},
        {"{BORDER_DIM}", ui::colors::BORDER_DIM()},
        {"{BORDER_MED}", ui::colors::BORDER_MED()},
        {"{BORDER_BRIGHT}", ui::colors::BORDER_BRIGHT()},
        {"{TEXT_PRIMARY}", ui::colors::TEXT_PRIMARY()},
        {"{TEXT_SECONDARY}", ui::colors::TEXT_SECONDARY()},
        {"{TEXT_TERTIARY}", ui::colors::TEXT_TERTIARY()},
        {"{TEXT_DIM}", ui::colors::TEXT_DIM()},
        {"{AMBER}", ui::colors::AMBER()},
        {"{AMBER_DIM}", ui::colors::AMBER_DIM()},
        {"{TEXT_ON_ACCENT}", ui::colors::TEXT_ON_ACCENT()},
        {"{POSITIVE}", ui::colors::POSITIVE()},
        {"{NEGATIVE}", ui::colors::NEGATIVE()},
    };
    for (const auto& [token, value] : tokens)
        ss.replace(QLatin1String(token), QLatin1String(value));
    return ss;
}

} // namespace

// ── Construction ─────────────────────────────────────────────────────────────

PricingScreen::PricingScreen(QWidget* parent) : QWidget(parent) {
    build_ui();
    confetti_ = new ui::ConfettiOverlay(this);

    // Plan state and credits arrive through the session. The cards are rebuilt
    // only when the plan actually changes (see render_cards()).
    connect(&auth::AuthManager::instance(), &auth::AuthManager::auth_state_changed, this,
            &PricingScreen::on_session_changed);
    // Balance moved by a billed call (no profile refresh, so no auth_state_changed).
    connect(&auth::AuthManager::instance(), &auth::AuthManager::credit_balance_changed, this, [this](double) {
        if (isVisible())
            update_account_line();
    });
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this](const ui::ThemeTokens&) { refresh_theme(); });

    payment_poll_timer_ = new QTimer(this);
    payment_poll_timer_->setInterval(kPaymentPollIntervalMs);
    connect(payment_poll_timer_, &QTimer::timeout, this, &PricingScreen::poll_payment_status);
}

void PricingScreen::build_ui() {
    setObjectName("pricingRoot");
    setAttribute(Qt::WA_StyledBackground, true);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* scroll = new QScrollArea(this);
    scroll->setObjectName("pricingScroll");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* content = new QWidget;
    content->setObjectName("pricingContent");
    auto* vl = new QVBoxLayout(content);
    vl->setContentsMargins(32, 30, 32, 26);
    vl->setSpacing(0);

    // ── Header ───────────────────────────────────────────────────────────────
    title_label_ = new QLabel(content);
    title_label_->setObjectName("pricingTitle");
    title_label_->setAlignment(Qt::AlignCenter);
    vl->addWidget(title_label_);
    vl->addSpacing(8);

    subtitle_label_ = new QLabel(content);
    subtitle_label_->setObjectName("pricingSubtitle");
    subtitle_label_->setAlignment(Qt::AlignCenter);
    subtitle_label_->setWordWrap(true);
    vl->addWidget(subtitle_label_);
    vl->addSpacing(6);

    account_label_ = new QLabel(content);
    account_label_->setObjectName("pricingAccount");
    account_label_->setAlignment(Qt::AlignCenter);
    account_label_->hide();
    vl->addWidget(account_label_);
    vl->addSpacing(14);

    // ── Status (loading / offline / error / checkout progress) ──────────────
    status_row_ = new QWidget(content);
    auto* status_hl = new QHBoxLayout(status_row_);
    status_hl->setContentsMargins(0, 0, 0, 0);
    status_hl->setSpacing(10);
    status_hl->addStretch();
    status_label_ = new QLabel(status_row_);
    status_label_->setObjectName("pricingStatus");
    status_label_->setWordWrap(true);
    status_label_->setAlignment(Qt::AlignCenter);
    status_hl->addWidget(status_label_);
    retry_btn_ = new QPushButton(status_row_);
    retry_btn_->setObjectName("pricingRetry");
    retry_btn_->setCursor(Qt::PointingHandCursor);
    retry_btn_->hide();
    connect(retry_btn_, &QPushButton::clicked, this, &PricingScreen::fetch_plans);
    status_hl->addWidget(retry_btn_);
    status_hl->addStretch();
    status_row_->hide();
    vl->addWidget(status_row_);
    vl->addSpacing(14);

    // ── Cards: FREE | PRO | ENTERPRISE, equal width, centred ────────────────
    auto* cards_row = new QHBoxLayout;
    cards_row->setContentsMargins(0, 0, 0, 0);
    cards_row->setSpacing(0);
    cards_row->addStretch(1);
    cards_host_ = new QWidget(content);
    cards_host_->setObjectName("pricingCards");
    cards_layout_ = new QHBoxLayout(cards_host_);
    cards_layout_->setContentsMargins(0, 0, 0, 0);
    cards_layout_->setSpacing(14);
    cards_row->addWidget(cards_host_, 100);
    cards_row->addStretch(1);
    vl->addLayout(cards_row);
    vl->addSpacing(20);

    // ── Plan value: what the account saved (or would save), a rollover waiting
    // for a renewal, what Pro adds and the per-plan price of every category.
    // Server-owned (PlanAccess); hides itself when the server sent no table.
    {
        auto* value_row = new QHBoxLayout;
        value_row->setContentsMargins(0, 0, 0, 0);
        value_row->setSpacing(0);
        value_row->addStretch(1);
        auto* value = new ui::PlanBenefitsView(content);
        value->set_show_upgrade_button(false); // the PRO card above is the action
        value->setMaximumWidth(kPricingCardMaxWidth * 2 + 14);
        value_row->addWidget(value, 100);
        value_row->addStretch(1);
        vl->addLayout(value_row);
        vl->addSpacing(12);
    }

    // ── Footer ───────────────────────────────────────────────────────────────
    footnote_label_ = new QLabel(content);
    footnote_label_->setObjectName("pricingFootnote");
    footnote_label_->setAlignment(Qt::AlignCenter);
    footnote_label_->setWordWrap(true);
    vl->addWidget(footnote_label_);
    vl->addSpacing(12);

    back_btn_ = new QPushButton(content);
    back_btn_->setObjectName("pricingBack");
    back_btn_->setCursor(Qt::PointingHandCursor);
    connect(back_btn_, &QPushButton::clicked, this, &PricingScreen::back_requested);
    vl->addWidget(back_btn_, 0, Qt::AlignHCenter);
    vl->addStretch();

    scroll->setWidget(content);
    root->addWidget(scroll);

    auto* esc = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    esc->setContext(Qt::WidgetWithChildrenShortcut);
    connect(esc, &QShortcut::activated, this, &PricingScreen::back_requested);

    refresh_theme();
    retranslateUi();
}

void PricingScreen::refresh_theme() {
    setStyleSheet(pricing_qss());
}

void PricingScreen::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
        update_account_line();
        rendered_for_.clear(); // card labels carry translated text — rebuild them
        render_cards();
    }
    QWidget::changeEvent(event);
}

void PricingScreen::retranslateUi() {
    title_label_->setText(tr("PLANS & PRICING"));
    subtitle_label_->setText(tr("Credits power cloud analytics, AI and data in this terminal. Start free — "
                                "move to Pro when you need more."));
    footnote_label_->setText(tr("Checkout runs securely on fincept.in and your plan updates here automatically. "
                                "Fincept Terminal Enterprise is a separate product with its own accounts and "
                                "billing."));
    back_btn_->setText(tr("← BACK TO TERMINAL"));
    retry_btn_->setText(tr("RETRY"));
}

void PricingScreen::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (confetti_)
        confetti_->setGeometry(rect());
}

// ── Show / hide ──────────────────────────────────────────────────────────────

void PricingScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    load_cached_plans(); // first visit: last-known-good plans render instantly
    update_account_line();
    // The catalogue changes with a release, not between visits: re-ask only
    // when it is missing or old (RETRY always asks).
    if (plans_.empty() || QDateTime::currentMSecsSinceEpoch() - plans_fetched_ms_ > kPlansFreshMs)
        fetch_plans(); // refresh in the background; cached cards stay up meanwhile
    render_cards();
    billing::PlanAccess::instance().refresh(); // plan value section (reuses a recent table)
    // Plan + credits for the account line and the cards' current-plan state,
    // re-fetched only if the session's copy is more than a couple of minutes
    // old (the balance itself follows billed calls); the reply lands in
    // on_session_changed().
    auto& auth = auth::AuthManager::instance();
    if (auth.is_authenticated())
        auth.refresh_user_data(auth::AuthManager::Refresh::OnView);
}

void PricingScreen::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    // §P3: the poll timer must not outlive the screen leaving the stack. A
    // minimise (spontaneous hide) keeps polling — the user is paying in the
    // browser and will come back.
    if (event && event->spontaneous())
        return;
    if (awaiting_payment_) {
        LOG_INFO(kPricingTag, "Pricing screen hidden — stopping payment polling");
        stop_payment_polling();
    }
}

// ── Status line ──────────────────────────────────────────────────────────────

void PricingScreen::set_status(StatusTone tone, const QString& text, bool show_retry) {
    const char* tone_name = tone == StatusTone::Error ? "error" : (tone == StatusTone::Success ? "success" : "info");
    if (status_label_->property("tone").toString() != QLatin1String(tone_name)) {
        status_label_->setProperty("tone", QString::fromLatin1(tone_name));
        pricing_repolish(status_label_);
    }
    status_label_->setText(text);
    retry_btn_->setVisible(show_retry);
    status_row_->show();
}

void PricingScreen::clear_status() {
    status_row_->hide();
    retry_btn_->hide();
}

void PricingScreen::update_account_line() {
    auto& mgr = auth::AuthManager::instance();
    if (!mgr.is_authenticated()) {
        account_label_->hide();
        return;
    }
    const auto& s = mgr.session();
    const QString who = s.user_info.email.isEmpty() ? s.user_info.username : s.user_info.email;
    account_label_->setText(tr("%1  ·  %2 plan  ·  %3 credits")
                                .arg(who, s.plan_label(), auth::format_credits(s.user_info.credit_balance)));
    account_label_->show();
}

// ── Plans ────────────────────────────────────────────────────────────────────

void PricingScreen::load_cached_plans() {
    if (cache_checked_)
        return;
    cache_checked_ = true;
    if (!plans_.empty())
        return;
    auto r = SettingsRepository::instance().get(kPricingPlansCacheKey);
    if (r.is_err() || r.value().isEmpty())
        return;
    const QJsonDocument doc = QJsonDocument::fromJson(r.value().toUtf8());
    if (doc.isArray())
        apply_plans_json(doc.array());
}

bool PricingScreen::apply_plans_json(const QJsonArray& plans) {
    std::vector<auth::SubscriptionPlan> parsed;
    for (const auto& v : plans) {
        if (!v.isObject())
            continue;
        auto plan = auth::SubscriptionPlan::from_json(v.toObject());
        if (!plan.plan_id.isEmpty())
            parsed.push_back(std::move(plan));
    }
    if (parsed.empty())
        return false;
    std::stable_sort(parsed.begin(), parsed.end(),
                     [](const auth::SubscriptionPlan& a, const auth::SubscriptionPlan& b) {
                         return a.display_order < b.display_order;
                     });
    plans_ = std::move(parsed);
    rendered_for_.clear();
    render_cards();
    return true;
}

void PricingScreen::fetch_plans() {
    if (loading_)
        return;
    loading_ = true;
    if (plans_.empty())
        set_status(StatusTone::Info, tr("Loading plans…"));

    auth::AuthApi::instance().get_subscription_plans(
        [this](auth::ApiResponse r) {
            loading_ = false;
            if (!r.success) {
                const bool offline = r.status_code == 0;
                LOG_WARN(kPricingTag, QString("Plan fetch failed (HTTP %1)").arg(r.status_code));
                if (!plans_.empty()) {
                    if (!awaiting_payment_)
                        set_status(StatusTone::Info,
                                   offline ? tr("Offline — showing the plans from your last visit.")
                                           : tr("Couldn't refresh plans — showing the last known list."),
                                   true);
                } else {
                    set_status(StatusTone::Error,
                               offline ? tr("Can't reach Fincept right now. Check your connection and retry.")
                                       : (r.error.isEmpty() ? tr("Plans could not be loaded.") : r.error),
                               true);
                    rendered_for_.clear();
                    render_cards(); // the Enterprise card still renders without the backend
                }
                return;
            }

            const QJsonArray arr = pricing_extract_plans(r.data);
            if (!apply_plans_json(arr)) {
                if (plans_.empty()) {
                    set_status(StatusTone::Error, tr("No plans are available right now."), true);
                    rendered_for_.clear();
                    render_cards();
                }
                return;
            }
            if (!awaiting_payment_)
                clear_status();
            plans_fetched_ms_ = QDateTime::currentMSecsSinceEpoch();
            const auto saved = SettingsRepository::instance().set(
                kPricingPlansCacheKey, QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)),
                QStringLiteral("billing"));
            if (saved.is_err())
                LOG_DEBUG(kPricingTag, "Could not cache the plan list");
        },
        this); // context: the reply handler is dropped if the screen is destroyed first
}

// ── Cards ────────────────────────────────────────────────────────────────────

void PricingScreen::render_cards() {
    auto& mgr = auth::AuthManager::instance();
    const bool signed_in = mgr.is_authenticated();
    const QString state = plans_.empty() ? (loading_ ? QStringLiteral("loading") : QStringLiteral("failed"))
                                         : QStringLiteral("ready");
    // Plan, plan in force (a lapsed pass) and pass end: a renewal changes only
    // the last one, and the current card's "valid until" note must follow it.
    const auto& s = mgr.session();
    QString account = QStringLiteral("-");
    if (signed_in)
        account = s.account_type().toLower() + QLatin1Char('|') + s.effective_plan() + QLatin1Char('|') +
                  s.user_info.credits_expire_at;
    const QString key = state + QLatin1Char('|') + account;
    if (key == rendered_for_)
        return;
    rendered_for_ = key;

    while (QLayoutItem* item = cards_layout_->takeAt(0)) {
        if (QWidget* w = item->widget())
            w->deleteLater();
        delete item;
    }
    if (plans_.empty() && loading_)
        return; // the status line says "Loading plans…"

    // Zero-stretch spacers soak up width only once every card is at its
    // maximum — e.g. the Enterprise card alone after a failed first load —
    // so the row stays centred instead of hugging the left edge.
    cards_layout_->addStretch(0);
    for (const auto& plan : plans_)
        cards_layout_->addWidget(build_plan_card(plan), 1);
    cards_layout_->addWidget(build_enterprise_card(), 1);
    cards_layout_->addStretch(0);
}

void PricingScreen::set_plan_action(QPushButton* btn, PlanAction action) {
    const char* name = "select";
    switch (action) {
        case PlanAction::Current:
            name = "current";
            break;
        case PlanAction::Included:
            name = "included";
            break;
        case PlanAction::ContinueFree:
            name = "continue";
            break;
        case PlanAction::Select:
            name = "select";
            break;
        case PlanAction::Processing:
            name = "processing";
            break;
    }
    btn->setEnabled(action == PlanAction::ContinueFree || action == PlanAction::Select);
    if (btn->property("planAction").toString() != QLatin1String(name)) {
        btn->setProperty("planAction", QString::fromLatin1(name));
        pricing_repolish(btn);
    }
}

QWidget* PricingScreen::build_plan_card(const auth::SubscriptionPlan& plan) {
    auto& mgr = auth::AuthManager::instance();
    const bool signed_in = mgr.is_authenticated();
    // Paid = the plan in force right now (effective_plan): a Pro pass that has
    // lapsed is on Free, even though account_type still says "pro" until the
    // server settles it.
    const bool user_paid = signed_in && mgr.session().has_paid_plan();
    const bool lapsed = signed_in && mgr.session().pass_lapsed();
    const QString user_plan = signed_in ? mgr.session().account_type().trimmed().toLower() : QString();
    const bool own_plan = user_plan == plan.plan_id.toLower();
    // A free account is on the free plan even if its account_type is empty.
    const bool is_current = signed_in && (plan.is_free ? !user_paid : (user_paid && own_plan));

    auto* card = new QFrame(cards_host_);
    card->setObjectName("planCard");
    card->setProperty("variant", is_current ? "current" : (plan.is_recommended ? "recommended" : "standard"));
    card->setMinimumWidth(kPricingCardMinWidth);
    card->setMaximumWidth(kPricingCardMaxWidth);
    card->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred); // equal widths by stretch

    auto* vl = new QVBoxLayout(card);
    vl->setContentsMargins(22, 18, 22, 18);
    vl->setSpacing(0);

    auto* badge_row = new QHBoxLayout;
    badge_row->setContentsMargins(0, 0, 0, 0);
    auto* badge = new QLabel(card);
    badge->setObjectName("planBadge");
    badge->setFixedHeight(pricing_badge_height()); // same on every card, so the rows line up
    if (is_current) {
        badge->setText(tr("CURRENT PLAN"));
        badge->setProperty("tone", "current");
    } else if (plan.is_recommended) {
        badge->setText(tr("RECOMMENDED"));
        badge->setProperty("tone", "recommended");
    } else {
        badge->setProperty("tone", "none"); // keeps the row height so cards line up
    }
    badge_row->addWidget(badge);
    badge_row->addStretch();
    vl->addLayout(badge_row);
    vl->addSpacing(14);

    auto* name = new QLabel(plan.name.isEmpty() ? plan.plan_id.toUpper() : plan.name.toUpper(), card);
    name->setObjectName("planName");
    name->setProperty("accent", plan.is_recommended);
    vl->addWidget(name);
    vl->addSpacing(6);

    auto* desc = new QLabel(plan.description, card);
    desc->setObjectName("planDesc");
    desc->setWordWrap(true);
    desc->setMinimumHeight(34); // two lines — keeps the price rows aligned across cards
    desc->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    vl->addWidget(desc);
    vl->addSpacing(12);

    auto* price_row = new QHBoxLayout;
    price_row->setContentsMargins(0, 0, 0, 0);
    price_row->setSpacing(8);
    auto* price = new QLabel(plan.is_free ? pricing_format_price(0.0, plan.currency)
                                          : pricing_format_price(plan.price_usd, plan.currency),
                             card);
    price->setObjectName("planPrice");
    price->setProperty("accent", plan.is_recommended && !plan.is_free);
    price_row->addWidget(price, 0, Qt::AlignBottom);
    QString period;
    if (plan.is_free)
        period = tr("no card required");
    else if (plan.validity_days == 30)
        period = tr("/ month");
    else if (plan.validity_days > 0)
        period = tr("/ %1 days").arg(plan.validity_days);
    auto* period_lbl = new QLabel(period, card);
    period_lbl->setObjectName("planPeriod");
    price_row->addWidget(period_lbl, 0, Qt::AlignBottom);
    price_row->addStretch();
    vl->addLayout(price_row);
    vl->addSpacing(16);

    auto* rule = new QFrame(card);
    rule->setObjectName("planRule");
    rule->setFixedHeight(1);
    vl->addWidget(rule);
    vl->addSpacing(14);

    // Features are backend-owned copy. Older payloads without a list fall back
    // to the structured fields.
    QStringList features = plan.features;
    if (features.isEmpty()) {
        if (plan.credits > 0)
            features << tr("%1 credits").arg(auth::format_credits(plan.credits));
        if (!plan.support_type.isEmpty())
            features << tr("%1 support").arg(plan.support_type);
    }
    for (const QString& f : features) {
        auto* feat = new QLabel(pricing_bullet() + f, card);
        feat->setObjectName("planFeature");
        feat->setWordWrap(true);
        vl->addWidget(feat);
        vl->addSpacing(8);
    }
    vl->addStretch(1);
    vl->addSpacing(12);

    auto* btn = new QPushButton(card);
    btn->setObjectName("planBtn");
    btn->setCursor(Qt::PointingHandCursor);
    QString note;
    const QString plan_name = plan.name.isEmpty() ? plan.plan_id.toUpper() : plan.name.toUpper();
    if (plan.is_free) {
        if (user_paid) {
            btn->setText(tr("FREE TIER"));
            set_plan_action(btn, PlanAction::Included);
        } else {
            btn->setText(tr("CONTINUE WITH FREE"));
            set_plan_action(btn, PlanAction::ContinueFree);
            connect(btn, &QPushButton::clicked, this, &PricingScreen::back_requested);
        }
    } else if (is_current) {
        // The server sells a pass repeatedly — each purchase adds credits and
        // extends it — so the plan you are on is still something you can buy:
        // the only way to top up a Pro account out of credits. The badge above
        // says CURRENT PLAN.
        btn->setText(tr("RENEW / ADD CREDITS"));
        set_plan_action(btn, PlanAction::Select);
        const QString expires_at = mgr.session().user_info.credits_expire_at;
        // Local date, as Profile shows it (the raw UTC date could be a day off).
        note = expires_at.isEmpty()
                   ? tr("Renewing adds credits and extends your pass.")
                   : tr("Credits valid until %1. Renewing adds credits and extends your pass.")
                         .arg(auth::format_local_stamp(expires_at, false));
        const QString pid = plan.plan_id;
        connect(btn, &QPushButton::clicked, this, [this, pid]() { on_select_plan(pid); });
    } else {
        if (lapsed && own_plan)
            btn->setText(tr("RENEW %1").arg(plan_name)); // the pass ran out
        else
            btn->setText(user_paid ? tr("SWITCH TO %1").arg(plan_name) : tr("UPGRADE TO %1").arg(plan_name));
        set_plan_action(btn, PlanAction::Select);
        note = tr("Secure checkout on fincept.in — opens in your browser");
        const QString pid = plan.plan_id;
        connect(btn, &QPushButton::clicked, this, [this, pid]() { on_select_plan(pid); });
    }
    btn->setProperty("idleText", btn->text());
    vl->addWidget(btn);
    if (!note.isEmpty()) {
        vl->addSpacing(8);
        auto* note_lbl = new QLabel(note, card);
        note_lbl->setObjectName("planNote");
        note_lbl->setWordWrap(true);
        vl->addWidget(note_lbl);
    }
    return card;
}

QWidget* PricingScreen::build_enterprise_card() {
    namespace ent = ui::enterprise;

    auto* card = new QFrame(cards_host_);
    card->setObjectName("planCard");
    card->setProperty("variant", "enterprise");
    card->setMinimumWidth(kPricingCardMinWidth);
    card->setMaximumWidth(kPricingCardMaxWidth);
    card->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

    auto* vl = new QVBoxLayout(card);
    vl->setContentsMargins(22, 18, 22, 18);
    vl->setSpacing(0);

    auto* badge_row = new QHBoxLayout;
    badge_row->setContentsMargins(0, 0, 0, 0);
    auto* badge = new QLabel(tr("PRIVATE EDITION"), card);
    badge->setObjectName("planBadge");
    badge->setProperty("tone", "enterprise");
    badge->setFixedHeight(pricing_badge_height());
    badge_row->addWidget(badge);
    badge_row->addStretch();
    vl->addLayout(badge_row);
    vl->addSpacing(14);

    auto* name = new QLabel(tr("ENTERPRISE"), card);
    name->setObjectName("planName");
    vl->addWidget(name);
    vl->addSpacing(6);

    auto* desc = new QLabel(tr("Fincept Terminal Enterprise — the private edition for funds, family offices and "
                               "research desks."),
                            card);
    desc->setObjectName("planDesc");
    desc->setWordWrap(true);
    desc->setMinimumHeight(34);
    desc->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    vl->addWidget(desc);
    vl->addSpacing(12);

    // No price on purpose: Enterprise prices live on the website, and this
    // build ships monthly. The price slot links there instead.
    auto* price_link = new QPushButton(fincept::ui::literal_ampersands(tr("See plans & pricing ↗")), card);
    price_link->setObjectName("planPriceLink");
    price_link->setCursor(Qt::PointingHandCursor);
    price_link->setMinimumHeight(36);
    connect(price_link, &QPushButton::clicked, this,
            []() { ui::enterprise::open_url(ui::enterprise::url_with_ref(ui::enterprise::pricing_url(), "pricing")); });
    vl->addWidget(price_link, 0, Qt::AlignLeft);
    vl->addSpacing(16);

    auto* rule = new QFrame(card);
    rule->setObjectName("planRule");
    rule->setFixedHeight(1);
    vl->addWidget(rule);
    vl->addSpacing(14);

    const QStringList features = {
        tr("Proprietary and private datasets with deeper point-in-time history"),
        tr("Multi-agent AI research — AI credits included, private dataroom"),
        tr("41 modules across six research desks"),
        tr("SSO/SAML, audit logs and role-based access"),
        tr("SLA-backed priority support"),
    };
    for (const QString& f : features) {
        auto* feat = new QLabel(pricing_bullet() + f, card);
        feat->setObjectName("planFeature");
        feat->setWordWrap(true);
        vl->addWidget(feat);
        vl->addSpacing(8);
    }
    vl->addStretch(1);
    vl->addSpacing(12);

    auto* primary = new QPushButton(tr("SEE ENTERPRISE ↗"), card);
    primary->setObjectName("planEntPrimary");
    primary->setCursor(Qt::PointingHandCursor);
    primary->setToolTip(ent::site_url());
    connect(primary, &QPushButton::clicked, this,
            []() { ui::enterprise::open_url(ui::enterprise::url_with_ref(ui::enterprise::site_url(), "pricing")); });
    vl->addWidget(primary);
    vl->addSpacing(6);

    // A meeting about custom requirements or a partnership — the team gives no
    // product demos. The compact label takes over when a large font leaves the
    // card too narrow for the full one.
    auto* meeting = new QPushButton(card);
    meeting->setObjectName("planEntSecondary");
    meeting->setCursor(Qt::PointingHandCursor);
    ent::set_fitting_text(meeting, tr("DISCUSS CUSTOM REQUIREMENTS"), tr("CUSTOM REQUIREMENTS"));
    meeting->setToolTip(tr("Book a meeting to talk through custom requirements or a partnership"));
    meeting->setAccessibleName(tr("Discuss custom requirements: book a meeting (opens in your browser)"));
    connect(meeting, &QPushButton::clicked, this, []() { ui::enterprise::open_url(ui::enterprise::meeting_url()); });
    vl->addWidget(meeting);
    vl->addSpacing(6);
    vl->addWidget(ent::make_partnership_link(card), 0, Qt::AlignLeft);
    vl->addSpacing(8);

    auto* note = new QLabel(tr("Separate product · separate account"), card);
    note->setObjectName("planNote");
    note->setWordWrap(true);
    vl->addWidget(note);
    return card;
}

// ── Checkout ─────────────────────────────────────────────────────────────────

void PricingScreen::on_select_plan(const QString& plan_id) {
    const auto buttons = cards_host_->findChildren<QPushButton*>(QStringLiteral("planBtn"));
    for (auto* btn : buttons) {
        if (btn->property("planAction").toString() == QLatin1String("select")) {
            set_plan_action(btn, PlanAction::Processing);
            btn->setText(tr("OPENING CHECKOUT…"));
        }
    }
    clear_status();

    auth::AuthApi::instance().generate_checkout_token(
        plan_id,
        [this, plan_id](auth::ApiResponse r) {
            for (auto* btn : cards_host_->findChildren<QPushButton*>(QStringLiteral("planBtn"))) {
                if (btn->property("planAction").toString() == QLatin1String("processing")) {
                    set_plan_action(btn, PlanAction::Select);
                    btn->setText(btn->property("idleText").toString());
                }
            }

            if (!r.success) {
                set_status(StatusTone::Error,
                           r.error.isEmpty() ? tr("Could not start checkout. Please try again.") : r.error);
                return;
            }

            QJsonObject payload = r.data;
            if (payload.contains("data") && payload["data"].isObject())
                payload = payload["data"].toObject();

            const QString token = payload["token"].toString();
            if (token.isEmpty()) {
                set_status(StatusTone::Error, tr("No checkout token received from the server. Please try again."));
                return;
            }

            // The website depends on this exact path and these parameters.
            const QString url = QString("https://fincept.in/checkout?token=%1&plan=%2")
                                    .arg(QUrl::toPercentEncoding(token), QUrl::toPercentEncoding(plan_id));
            QDesktopServices::openUrl(QUrl(url));

            awaiting_payment_ = true;
            awaiting_plan_id_ = plan_id;
            // What a completed purchase changes — see on_session_changed().
            const auto& s = auth::AuthManager::instance().session();
            pre_payment_plan_ = s.account_type().toLower();
            pre_payment_purchase_at_ = s.subscription.last_credit_purchase_at;
            pre_payment_expires_at_ = pricing_pass_end(s);
            payment_poll_ticks_ = 0;
            payment_poll_in_flight_ = false;
            set_status(StatusTone::Info, tr("Checkout opened in your browser. Complete the payment there — this "
                                            "screen updates automatically."));
            payment_poll_timer_->start();

            // Also check the moment the user comes back from the browser.
            if (focus_connection_)
                disconnect(focus_connection_);
            focus_connection_ =
                connect(qApp, &QApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
                    if (state == Qt::ApplicationActive && awaiting_payment_)
                        poll_payment_status();
                });
        },
        this); // context: this call spans a payment-provider round trip — see AuthApi::request()
}

void PricingScreen::stop_payment_polling() {
    awaiting_payment_ = false;
    payment_poll_in_flight_ = false;
    payment_poll_ticks_ = 0;
    if (payment_poll_timer_)
        payment_poll_timer_->stop();
    if (focus_connection_) {
        disconnect(focus_connection_);
        focus_connection_ = {};
    }
}

void PricingScreen::poll_payment_status() {
    if (!awaiting_payment_)
        return;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    // One request at a time; a reply that never arrives frees the slot after 30 s.
    if (payment_poll_in_flight_ && now - payment_poll_issued_ms_ < 30000)
        return;
    if (++payment_poll_ticks_ > kMaxPaymentPolls) {
        LOG_INFO(kPricingTag, "Payment polling gave up after the maximum window");
        stop_payment_polling();
        set_status(StatusTone::Info, tr("Checkout not completed yet. Your plan updates as soon as the payment "
                                        "clears — reopen Plans & Pricing any time to check."));
        return;
    }
    auto& auth = auth::AuthManager::instance();
    // refresh_user_data() is a no-op for a signed-out session, so nothing
    // would ever answer the poll — and there is nothing to poll for.
    if (!auth.is_authenticated()) {
        stop_payment_polling();
        return;
    }
    payment_poll_in_flight_ = true;
    payment_poll_issued_ms_ = now;
    auth.refresh_user_data(auth::AuthManager::Refresh::Now); // answered by on_session_changed()
}

void PricingScreen::on_session_changed() {
    payment_poll_in_flight_ = false;
    if (!isVisible())
        return;
    update_account_line();

    auto& mgr = auth::AuthManager::instance();
    if (awaiting_payment_ && mgr.is_authenticated()) {
        const auto& s = mgr.session();
        // Every purchase stamps last_credit_purchase_at and extends the pass.
        // Watching the plan name alone missed a renewal (Pro → Pro) and polled
        // to the 10-minute give-up. A plan change still counts, but only INTO a
        // paid plan: the server settling a lapsed pass (Pro → Free) mid-checkout
        // is not a payment. The balance is no signal — settling refills it.
        const QString purchase_at = s.subscription.last_credit_purchase_at;
        const bool new_purchase = !purchase_at.isEmpty() && purchase_at != pre_payment_purchase_at_;
        const bool pass_extended = pricing_is_later(pricing_pass_end(s), pre_payment_expires_at_);
        const bool upgraded = s.account_type().toLower() != pre_payment_plan_ && s.has_paid_plan();
        if (new_purchase || pass_extended || upgraded) {
            LOG_INFO(kPricingTag, QString("Purchase recorded after checkout (%1)").arg(awaiting_plan_id_));
            stop_payment_polling();
            set_status(StatusTone::Success,
                       tr("Payment received — your %1 plan is active.").arg(mgr.session().plan_label()));
            confetti_->setGeometry(rect());
            confetti_->raise();
            confetti_->show_confetti();
        }
    }
    render_cards(); // no-op unless the plan changed
}

} // namespace fincept::screens
