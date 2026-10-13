#include "ui/widgets/EnterprisePromo.h"

#include "app/TerminalShell.h"
#include "app/WindowFrame.h"
#include "auth/AuthManager.h"
#include "auth/InactivityGuard.h"
#include "core/actions/ActionRegistry.h"
#include "core/config/AppConfig.h"
#include "core/keys/WindowCycler.h"
#include "core/logging/Logger.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QApplication>
#include <QCheckBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QLabel>
#include <QPointer>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QShowEvent>
#include <QStyle>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>

namespace fincept::ui {

namespace {

constexpr const char* kEntTag = "EnterprisePromo";

// AppConfig (QSettings) keys. The reminder used to be a permanent opt-out
// (kEntLegacySuppressKey); it is now "not until the next release".
constexpr const char* kEntLegacySuppressKey = "ui/upgrade_prompt_suppressed";
constexpr const char* kEntSuppressedVersionKey = "ui/enterprise_prompt_suppressed_version";
constexpr const char* kEntLastShownKey = "ui/enterprise_prompt_last_shown"; // unix seconds
constexpr const char* kEntLastVersionKey = "ui/enterprise_prompt_last_version";
constexpr const char* kEntDismissedPrefix = "ui/enterprise_dismissed/"; // + placement → version

constexpr qint64 kEntReminderIntervalSecs = 72LL * 60 * 60;
constexpr int kEntShowcaseRotateMs = 7000;
constexpr int kEntShowcaseFadeMs = 260;

/// A platform plugin with no window system — headless CI, --smoke-test runs
/// under offscreen. A modal there would block until the process is killed.
bool ent_headless_platform() {
    const QString p = QGuiApplication::platformName();
    return p.isEmpty() || p == QLatin1String("offscreen") || p == QLatin1String("minimal");
}

QString ent_app_version() {
    const QString v = QCoreApplication::applicationVersion();
#ifdef FINCEPT_VERSION_STRING
    return v.isEmpty() ? QStringLiteral(FINCEPT_VERSION_STRING) : v;
#else
    return v;
#endif
}

/// Process-wide single-instance guard (same pattern as OnboardingTour).
QPointer<UpgradeDialog>& ent_active_dialog() {
    static QPointer<UpgradeDialog> dlg;
    return dlg;
}

/// One-time migration of the old permanent opt-out: it now suppresses only
/// the release the user ticked it in.
void ent_migrate_legacy_suppression() {
    auto& cfg = AppConfig::instance();
    const QVariant legacy = cfg.get(kEntLegacySuppressKey);
    if (!legacy.isValid())
        return;
    if (legacy.toBool())
        cfg.set(kEntSuppressedVersionKey, ent_app_version());
    cfg.remove(kEntLegacySuppressKey);
}

void ent_record_shown() {
    auto& cfg = AppConfig::instance();
    cfg.set(kEntLastShownKey, QDateTime::currentSecsSinceEpoch());
    cfg.set(kEntLastVersionKey, ent_app_version());
}

QString ent_bullet() {
    // U+25B8 small right-pointing triangle. fromUtf8 — a raw QStringLiteral
    // would widen each UTF-8 byte into its own UTF-16 unit (mojibake).
    return QString::fromUtf8("\xe2\x96\xb8  ");
}

QString ent_cta_ss(enterprise::CtaStyle style) {
    using enterprise::CtaStyle;
    switch (style) {
        case CtaStyle::Primary:
            // Dark text on amber: white on #d97706 fails contrast.
            return QString("QPushButton{background:%1;color:%2;border:1px solid %1;padding:6px 16px;"
                           "font-weight:700;letter-spacing:0.5px;}"
                           "QPushButton:hover{border-color:%3;}"
                           "QPushButton:pressed{background:%4;border-color:%4;color:%3;}")
                .arg(colors::AMBER(), colors::BG_BASE(), colors::TEXT_PRIMARY(), colors::AMBER_DIM());
        case CtaStyle::Outline:
            return QString("QPushButton{background:transparent;color:%1;border:1px solid %2;padding:6px 16px;"
                           "font-weight:700;letter-spacing:0.5px;}"
                           "QPushButton:hover{color:%3;border-color:%1;}")
                .arg(colors::AMBER(), colors::AMBER_DIM(), colors::TEXT_PRIMARY());
        case CtaStyle::Ghost:
            return QString("QPushButton{background:transparent;color:%1;border:1px solid %2;padding:6px 16px;"
                           "font-weight:700;letter-spacing:0.5px;}"
                           "QPushButton:hover{color:%3;border-color:%3;}")
                .arg(colors::TEXT_SECONDARY(), colors::BORDER_MED(), colors::AMBER());
        case CtaStyle::Link:
            return QString("QPushButton{background:transparent;color:%1;border:none;padding:0;"
                           "font-weight:700;text-align:left;}"
                           "QPushButton:hover{color:%2;text-decoration:underline;}")
                .arg(colors::AMBER(), colors::TEXT_PRIMARY());
        case CtaStyle::Quiet:
            return QString("QPushButton{background:transparent;color:%1;border:none;padding:2px 0;"
                           "font-size:11px;text-align:left;}"
                           "QPushButton:hover{color:%2;}")
                .arg(colors::TEXT_TERTIARY(), colors::AMBER());
    }
    return {};
}

/// Dynamic property remembering a CTA button's current style, so theme
/// changes re-apply the style the button has now, not the one it was built with.
constexpr const char* kEntCtaStyleProp = "entCtaStyle";

void ent_apply_cta_style(QPushButton* btn, enterprise::CtaStyle style) {
    if (!btn)
        return;
    btn->setProperty(kEntCtaStyleProp, static_cast<int>(style));
    btn->setStyleSheet(ent_cta_ss(style));
}

void ent_apply_mention_style(QLabel* label) {
    if (label)
        label->setStyleSheet(QString("QLabel{color:%1;background:transparent;border:none;font-size:11px;}")
                                 .arg(colors::TEXT_TERTIARY()));
}

/// Re-evaluates property-based selectors after a dynamic property change.
void ent_repolish(QWidget* w) {
    if (!w)
        return;
    w->style()->unpolish(w);
    w->style()->polish(w);
}

} // namespace

// ── Canonical destinations ───────────────────────────────────────────────────

namespace enterprise {

QString site_url() {
    return QStringLiteral("https://fincept.in/enterprise");
}
QString pricing_url() {
    return QStringLiteral("https://fincept.in/pricing");
}
QString comparison_url() {
    return QStringLiteral("https://fincept.in/comparison");
}
QString signup_url() {
    return QStringLiteral("https://fincept.in/enterprise/signup");
}
QString demo_url() {
    return QStringLiteral("https://calendly.com/nikultilak/fincept-terminal-demo");
}

QString url_with_ref(const QString& base, const QString& placement) {
    if (placement.trimmed().isEmpty())
        return base;
    QUrl url(base);
    const QString host = url.host().toLower();
    if (!url.isValid() || (host != QLatin1String("fincept.in") && !host.endsWith(QLatin1String(".fincept.in"))))
        return base;
    QUrlQuery query(url);
    query.removeAllQueryItems(QStringLiteral("ref"));
    query.addQueryItem(QStringLiteral("ref"), QStringLiteral("terminal-") + placement.trimmed().toLower());
    url.setQuery(query);
    return url.toString();
}

void open_url(const QString& url) {
    if (url.isEmpty())
        return;
    if (!QDesktopServices::openUrl(QUrl(url)))
        LOG_WARN(kEntTag, QString("Could not open %1 in the browser").arg(url));
}

QPushButton* make_cta_button(const QString& text, const QString& url, CtaStyle style, QWidget* parent) {
    auto* btn = new QPushButton(text, parent);
    btn->setCursor(Qt::PointingHandCursor);
    ent_apply_cta_style(btn, style);
    if (!url.isEmpty()) {
        btn->setToolTip(url);
        QObject::connect(btn, &QPushButton::clicked, btn, [url]() { open_url(url); });
    }
    QObject::connect(&ThemeManager::instance(), &ThemeManager::theme_changed, btn, [btn](const ThemeTokens&) {
        ent_apply_cta_style(btn, static_cast<CtaStyle>(btn->property(kEntCtaStyleProp).toInt()));
    });
    return btn;
}

void set_cta_style(QPushButton* btn, CtaStyle style) {
    if (!btn)
        return;
    const QVariant current = btn->property(kEntCtaStyleProp);
    if (current.isValid() && current.toInt() == static_cast<int>(style))
        return;
    ent_apply_cta_style(btn, style);
}

QLabel* make_mention_label(QWidget* parent) {
    auto* label = new QLabel(parent);
    label->setObjectName(QStringLiteral("entMention"));
    label->setTextFormat(Qt::RichText);
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
    label->setOpenExternalLinks(false);
    ent_apply_mention_style(label);
    QObject::connect(label, &QLabel::linkActivated, label, [](const QString& link) {
        const QString scheme = QUrl(link).scheme().toLower();
        if (scheme == QLatin1String("https") || scheme == QLatin1String("http"))
            open_url(link);
    });
    QObject::connect(&ThemeManager::instance(), &ThemeManager::theme_changed, label,
                     [label](const ThemeTokens&) { ent_apply_mention_style(label); });
    return label;
}

void set_mention_text(QLabel* label, const QString& lead, const QString& link_text, const QString& url) {
    if (!label)
        return;
    const QString link = QStringLiteral("<a href=\"%1\" style=\"color:%2;text-decoration:none;\">%3</a>")
                             .arg(url.toHtmlEscaped(), QString(colors::AMBER()), link_text.toHtmlEscaped());
    label->setText(lead.isEmpty() ? link : lead.toHtmlEscaped() + QStringLiteral(" ") + link);
    label->setToolTip(url);
}

bool placement_dismissed(const QString& placement) {
    return AppConfig::instance().get(QString::fromLatin1(kEntDismissedPrefix) + placement).toString() ==
           ent_app_version();
}

void dismiss_placement(const QString& placement) {
    AppConfig::instance().set(QString::fromLatin1(kEntDismissedPrefix) + placement, ent_app_version());
}

} // namespace enterprise

// ── Plans entry point ────────────────────────────────────────────────────────

namespace billing {

void open_plans(QWidget* origin) {
    CommandContext ctx;
    ctx.shell = &TerminalShell::instance();
    if (origin)
        ctx.focused_frame = qobject_cast<WindowFrame*>(origin->window());
    if (!ctx.focused_frame)
        ctx.focused_frame = WindowCycler::instance().focused_frame();
    const auto r = ActionRegistry::instance().invoke(QStringLiteral("billing.plans"), ctx);
    if (r.is_err())
        LOG_WARN(kEntTag, QString("billing.plans: %1").arg(QString::fromStdString(r.error())));
}

} // namespace billing

// ── UpgradeDialog ────────────────────────────────────────────────────────────

UpgradeDialog::UpgradeDialog(QWidget* parent, const QString& placement)
    : QDialog(parent), placement_(placement.isEmpty() ? QStringLiteral("app") : placement) {
    setObjectName("upgradeDialog");
    setModal(true);
    setMinimumWidth(580);
    setMaximumWidth(680);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(30, 26, 30, 22);
    root->setSpacing(0);

    kicker_label_ = new QLabel;
    kicker_label_->setObjectName("upgradeKicker");
    root->addWidget(kicker_label_);
    root->addSpacing(6);

    title_label_ = new QLabel;
    title_label_->setObjectName("upgradeTitle");
    root->addWidget(title_label_);
    root->addSpacing(10);

    pitch_label_ = new QLabel;
    pitch_label_->setObjectName("upgradePitch");
    pitch_label_->setWordWrap(true);
    root->addWidget(pitch_label_);
    root->addSpacing(16);

    // Feature rows — filled by retranslateUi() in declaration order.
    for (int i = 0; i < 5; ++i) {
        auto* row = new QLabel;
        row->setObjectName("upgradeFeature");
        row->setWordWrap(true);
        feature_labels_.append(row);
        root->addWidget(row);
        root->addSpacing(7);
    }

    root->addSpacing(8);
    pricing_label_ = new QLabel;
    pricing_label_->setObjectName("upgradePricing");
    pricing_label_->setTextFormat(Qt::RichText);
    pricing_label_->setWordWrap(true);
    pricing_label_->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
    connect(pricing_label_, &QLabel::linkActivated, this, [](const QString& link) { enterprise::open_url(link); });
    root->addWidget(pricing_label_);

    root->addSpacing(6);
    oss_note_label_ = new QLabel;
    oss_note_label_->setObjectName("upgradeNote");
    oss_note_label_->setWordWrap(true);
    root->addWidget(oss_note_label_);

    root->addSpacing(20);

    auto* buttons = new QHBoxLayout;
    buttons->setContentsMargins(0, 0, 0, 0);
    buttons->setSpacing(8);

    dont_remind_check_ = new QCheckBox;
    dont_remind_check_->setObjectName("upgradeDontRemind");
    dont_remind_check_->setCursor(Qt::PointingHandCursor);
    buttons->addWidget(dont_remind_check_);
    buttons->addStretch();

    later_btn_ = new QPushButton;
    later_btn_->setObjectName("upgradeGhost");
    later_btn_->setCursor(Qt::PointingHandCursor);
    connect(later_btn_, &QPushButton::clicked, this, &QDialog::reject);
    buttons->addWidget(later_btn_);

    compare_btn_ = new QPushButton;
    compare_btn_->setObjectName("upgradeGhost");
    compare_btn_->setCursor(Qt::PointingHandCursor);
    connect(compare_btn_, &QPushButton::clicked, this, [this]() {
        enterprise::open_url(enterprise::url_with_ref(enterprise::comparison_url(), placement_));
    });
    buttons->addWidget(compare_btn_);

    primary_btn_ = new QPushButton;
    primary_btn_->setObjectName("upgradePrimary");
    primary_btn_->setCursor(Qt::PointingHandCursor);
    primary_btn_->setDefault(true);
    connect(primary_btn_, &QPushButton::clicked, this, [this]() {
        enterprise::open_url(enterprise::url_with_ref(enterprise::site_url(), placement_));
        accept();
    });
    buttons->addWidget(primary_btn_);

    root->addLayout(buttons);

    // Persist the release-scoped opt-out whichever way the dialog is dismissed.
    connect(this, &QDialog::finished, this, [this](int) {
        if (dont_remind_check_ && dont_remind_check_->isChecked())
            AppConfig::instance().set(kEntSuppressedVersionKey, ent_app_version());
    });

    connect(&ThemeManager::instance(), &ThemeManager::theme_changed, this,
            [this](const ThemeTokens&) { refresh_theme(); });
    refresh_theme(); // also fills every text via retranslateUi()
}

void UpgradeDialog::refresh_theme() {
    setStyleSheet(QString("#upgradeDialog{background:%1;border:1px solid %2;}"
                          "QLabel{background:transparent;}"
                          "#upgradeKicker{color:%3;font-weight:700;letter-spacing:1.5px;font-size:11px;}"
                          "#upgradeTitle{color:%4;font-size:20px;font-weight:700;}"
                          "#upgradePitch{color:%5;font-size:13px;}"
                          "#upgradeFeature{color:%4;font-size:12px;}"
                          "#upgradePricing{color:%5;font-size:12px;}"
                          "#upgradeNote{color:%6;font-size:11px;}"
                          "#upgradeDontRemind{color:%6;font-size:11px;background:transparent;}"
                          "QPushButton#upgradeGhost{background:transparent;color:%5;border:1px solid %7;"
                          "  padding:7px 16px;font-weight:700;}"
                          "QPushButton#upgradeGhost:hover{color:%3;border-color:%3;}"
                          "QPushButton#upgradePrimary{background:%3;color:%8;border:1px solid %3;"
                          "  padding:7px 18px;font-weight:700;}"
                          "QPushButton#upgradePrimary:hover{border-color:%4;}"
                          "QPushButton#upgradePrimary:pressed{background:%9;border-color:%9;color:%4;}")
                      .arg(colors::BG_SURFACE(), colors::BORDER_DIM(), colors::AMBER(), colors::TEXT_PRIMARY(),
                           colors::TEXT_SECONDARY(), colors::TEXT_TERTIARY(), colors::BORDER_MED(), colors::BG_BASE(),
                           colors::AMBER_DIM()));
    retranslateUi(); // the pricing link carries an inline colour
}

void UpgradeDialog::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QDialog::changeEvent(event);
}

void UpgradeDialog::retranslateUi() {
    setWindowTitle(tr("Fincept Terminal Enterprise"));
    if (kicker_label_)
        kicker_label_->setText(tr("PRIVATE EDITION"));
    if (title_label_)
        title_label_->setText(tr("Fincept Terminal Enterprise"));
    if (pitch_label_)
        pitch_label_->setText(tr("You are running the free, open-source edition. Enterprise is the private, "
                                 "closed-source edition the team develops daily — built for funds, family "
                                 "offices and research desks."));

    if (feature_labels_.size() == 5) {
        const QString b = ent_bullet();
        feature_labels_[0]->setText(b + tr("Proprietary and private datasets with deeper point-in-time history"));
        feature_labels_[1]->setText(b + tr("Multi-agent AI research with AI credits included, plus a private dataroom"));
        feature_labels_[2]->setText(b + tr("41 modules across six research desks"));
        feature_labels_[3]->setText(b + tr("SSO/SAML, audit logs and role-based access control"));
        feature_labels_[4]->setText(b + tr("SLA-backed priority support"));
    }

    if (pricing_label_) {
        const QString url = enterprise::url_with_ref(enterprise::pricing_url(), placement_);
        pricing_label_->setText(tr("Plans for individuals and teams are published on fincept.in.") +
                                QStringLiteral(" <a href=\"%1\" style=\"color:%2;text-decoration:none;\">%3</a>")
                                    .arg(url.toHtmlEscaped(), QString(colors::AMBER()),
                                         tr("See plans & pricing ↗").toHtmlEscaped()));
    }
    if (oss_note_label_)
        oss_note_label_->setText(tr("This open-source edition stays free under AGPL-3.0. Enterprise is a separate "
                                    "product with its own account — a Fincept login from this terminal does not "
                                    "sign in to it."));

    if (dont_remind_check_)
        dont_remind_check_->setText(tr("Don't remind me until the next release"));
    if (later_btn_)
        later_btn_->setText(tr("Not now"));
    if (compare_btn_)
        compare_btn_->setText(tr("Compare editions"));
    if (primary_btn_)
        primary_btn_->setText(tr("See Enterprise ↗"));
}

void UpgradeDialog::show_now(QWidget* parent, const QString& placement) {
    if (ent_headless_platform())
        return;
    if (UpgradeDialog* open = ent_active_dialog().data()) {
        open->raise();
        open->activateWindow();
        return;
    }
    auto* dlg = new UpgradeDialog(parent, placement);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    ent_active_dialog() = dlg;
    // Someone who just looked at Enterprise shouldn't get the reminder too.
    ent_record_shown();
    // open() rather than exec(): callers run inside timer callbacks and signal
    // handlers, where exec() would spin a nested event loop.
    dlg->open();
}

void UpgradeDialog::maybe_show(QWidget* parent) {
    if (ent_headless_platform())
        return;
    ent_migrate_legacy_suppression();

    auto& cfg = AppConfig::instance();
    const QString version = ent_app_version();
    if (cfg.get(kEntSuppressedVersionKey).toString() == version)
        return; // "Don't remind me until the next release"
    if (ent_active_dialog() || QApplication::activeModalWidget())
        return;

    QWidget* win = parent ? parent->window() : nullptr;
    if (!win || !win->isVisible() || win->isMinimized() || !win->isActiveWindow())
        return; // never steal focus from another app or a hidden window
    if (auto* frame = qobject_cast<WindowFrame*>(win); frame && frame->is_locked())
        return;
    if (!auth::AuthManager::instance().is_authenticated() || auth::InactivityGuard::instance().is_terminal_locked())
        return;

    const bool new_release = cfg.get(kEntLastVersionKey).toString() != version;
    const qint64 last_shown = cfg.get(kEntLastShownKey, 0).toLongLong();
    const bool interval_elapsed =
        last_shown <= 0 || QDateTime::currentSecsSinceEpoch() - last_shown >= kEntReminderIntervalSecs;
    if (!new_release && !interval_elapsed)
        return;

    LOG_INFO(kEntTag, new_release ? QStringLiteral("Showing Enterprise reminder (first session of this release)")
                                  : QStringLiteral("Showing Enterprise reminder (72 h since last shown)"));
    show_now(parent, QStringLiteral("popup"));
}

// ── EnterpriseShowcase ───────────────────────────────────────────────────────

EnterpriseShowcase::EnterpriseShowcase(QWidget* parent) : QWidget(parent) {
    setObjectName("entShowcase");
    setAttribute(Qt::WA_StyledBackground, true);
    setMinimumWidth(300);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(28, 26, 28, 22);
    root->setSpacing(0);

    header_ = new QLabel(this);
    header_->setObjectName("entShowcaseHeader");
    header_->setWordWrap(true);
    root->addWidget(header_);
    root->addSpacing(12);

    auto* rule = new QFrame(this);
    rule->setObjectName("entShowcaseRule");
    rule->setFixedHeight(1);
    root->addWidget(rule);
    root->addSpacing(24);

    // Slide — fixed minimum height so slides of different length never make
    // the buttons below jump.
    slide_box_ = new QWidget(this);
    slide_box_->setObjectName("entShowcaseSlide");
    slide_box_->setMinimumHeight(200);
    auto* sl = new QVBoxLayout(slide_box_);
    sl->setContentsMargins(0, 0, 0, 0);
    sl->setSpacing(0);
    kicker_ = new QLabel(slide_box_);
    kicker_->setObjectName("entShowcaseKicker");
    sl->addWidget(kicker_);
    sl->addSpacing(10);
    title_ = new QLabel(slide_box_);
    title_->setObjectName("entShowcaseTitle");
    title_->setWordWrap(true);
    sl->addWidget(title_);
    sl->addSpacing(12);
    body_ = new QLabel(slide_box_);
    body_->setObjectName("entShowcaseBody");
    body_->setWordWrap(true);
    sl->addWidget(body_);
    sl->addStretch();
    root->addWidget(slide_box_, 1);

    fade_ = new QGraphicsOpacityEffect(slide_box_);
    fade_->setOpacity(1.0);
    fade_->setEnabled(false); // only while animating — no offscreen pass at rest
    slide_box_->setGraphicsEffect(fade_);
    fade_anim_ = new QPropertyAnimation(fade_, "opacity", this);
    fade_anim_->setDuration(kEntShowcaseFadeMs);
    fade_anim_->setStartValue(0.0);
    fade_anim_->setEndValue(1.0);
    connect(fade_anim_, &QPropertyAnimation::finished, this, [this]() { fade_->setEnabled(false); });

    root->addSpacing(16);
    auto* nav = new QHBoxLayout;
    nav->setContentsMargins(0, 0, 0, 0);
    nav->setSpacing(6);
    const int slide_count = static_cast<int>(slides().size());
    for (int i = 0; i < slide_count; ++i) {
        auto* dot = new QPushButton(this);
        dot->setObjectName("entShowcaseDot");
        dot->setCursor(Qt::PointingHandCursor);
        dot->setFocusPolicy(Qt::NoFocus);
        dot->setProperty("active", false);
        connect(dot, &QPushButton::clicked, this, [this, i]() {
            show_slide(i, true);
            if (isVisible())
                rotate_timer_->start(); // restart the 7 s dwell after a manual pick
        });
        dots_.append(dot);
        nav->addWidget(dot, 0, Qt::AlignVCenter);
    }
    nav->addStretch();
    index_ = new QLabel(this);
    index_->setObjectName("entShowcaseIndex");
    nav->addWidget(index_);
    root->addLayout(nav);

    root->addSpacing(22);
    auto* actions = new QHBoxLayout;
    actions->setContentsMargins(0, 0, 0, 0);
    actions->setSpacing(8);
    primary_btn_ = new QPushButton(this);
    primary_btn_->setObjectName("entShowcasePrimary");
    primary_btn_->setCursor(Qt::PointingHandCursor);
    connect(primary_btn_, &QPushButton::clicked, this,
            [this]() { enterprise::open_url(enterprise::url_with_ref(enterprise::site_url(), placement_)); });
    actions->addWidget(primary_btn_, 1);
    demo_btn_ = new QPushButton(this);
    demo_btn_->setObjectName("entShowcaseSecondary");
    demo_btn_->setCursor(Qt::PointingHandCursor);
    connect(demo_btn_, &QPushButton::clicked, this, []() { enterprise::open_url(enterprise::demo_url()); });
    actions->addWidget(demo_btn_, 1);
    root->addLayout(actions);

    root->addSpacing(12);
    note_ = new QLabel(this);
    note_->setObjectName("entShowcaseNote");
    note_->setWordWrap(true);
    root->addWidget(note_);

    // §P3: interval and connection here; started in showEvent, stopped in hideEvent.
    rotate_timer_ = new QTimer(this);
    rotate_timer_->setInterval(kEntShowcaseRotateMs);
    connect(rotate_timer_, &QTimer::timeout, this, [this]() { show_slide(current_ + 1, true); });

    connect(&ThemeManager::instance(), &ThemeManager::theme_changed, this,
            [this](const ThemeTokens&) { refresh_theme(); });
    refresh_theme();
    retranslateUi();
}

void EnterpriseShowcase::set_placement(const QString& placement) {
    placement_ = placement.trimmed().isEmpty() ? QStringLiteral("setup") : placement.trimmed();
}

QVector<EnterpriseShowcase::Slide> EnterpriseShowcase::slides() const {
    return {
        {tr("DATA"), tr("Proprietary and private datasets"),
         tr("Deeper point-in-time history and data you won't find in public feeds — research you can defend.")},
        {tr("MULTI-AGENT RESEARCH"), tr("Agents that plan, delegate and cite"),
         tr("Multi-agent AI research with AI credits included, plus a private dataroom for your own documents.")},
        {tr("RESEARCH DESKS"), tr("41 modules across six desks"),
         tr("Research, quant, portfolio, markets and macro workflows, organised the way a firm actually works.")},
        {tr("CONTROLS"), tr("SSO, audit logs and an SLA"),
         tr("SSO/SAML, role-based access and full audit trails, backed by SLA priority support.")},
        {tr("BUILT FOR FIRMS"), tr("The private edition, developed daily"),
         tr("Closed-source and built for funds, family offices and research desks — a separate product with its "
            "own accounts.")},
    };
}

void EnterpriseShowcase::show_slide(int index, bool animate) {
    const QVector<Slide> all = slides();
    if (all.isEmpty())
        return;
    const int n = static_cast<int>(all.size());
    current_ = ((index % n) + n) % n;
    const Slide& s = all.at(current_);
    kicker_->setText(s.kicker);
    title_->setText(s.title);
    body_->setText(s.body);
    index_->setText(QStringLiteral("%1 / %2").arg(current_ + 1, 2, 10, QLatin1Char('0')).arg(n, 2, 10,
                                                                                               QLatin1Char('0')));
    for (int i = 0; i < dots_.size(); ++i) {
        const bool active = (i == current_);
        if (dots_[i]->property("active").toBool() != active) {
            dots_[i]->setProperty("active", active);
            ent_repolish(dots_[i]);
        }
        dots_[i]->setAccessibleName(tr("Show slide %1: %2").arg(i + 1).arg(all.at(i).title));
    }
    if (animate && isVisible()) {
        fade_anim_->stop();
        fade_->setEnabled(true);
        fade_->setOpacity(0.0);
        fade_anim_->start();
    }
}

void EnterpriseShowcase::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    rotate_timer_->start();
}

void EnterpriseShowcase::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    rotate_timer_->stop();
    fade_anim_->stop();
    fade_->setEnabled(false);
}

void EnterpriseShowcase::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void EnterpriseShowcase::retranslateUi() {
    header_->setText(tr("FINCEPT TERMINAL ENTERPRISE · PRIVATE EDITION"));
    primary_btn_->setText(tr("SEE ENTERPRISE ↗"));
    primary_btn_->setToolTip(enterprise::site_url());
    demo_btn_->setText(tr("BOOK A DEMO"));
    demo_btn_->setToolTip(enterprise::demo_url());
    note_->setText(tr("Separate product · separate account. This open-source edition stays free under AGPL-3.0."));
    show_slide(current_, false);
}

void EnterpriseShowcase::refresh_theme() {
    setStyleSheet(QString("#entShowcase{background:%1;border:1px solid %2;border-left:2px solid %3;}"
                          "QLabel{background:transparent;border:none;}"
                          "#entShowcaseSlide{background:transparent;}"
                          "#entShowcaseHeader{color:%3;font-size:11px;font-weight:700;letter-spacing:1.5px;}"
                          "#entShowcaseRule{background:%2;border:none;}"
                          "#entShowcaseKicker{color:%3;font-size:11px;font-weight:700;letter-spacing:1px;}"
                          "#entShowcaseTitle{color:%4;font-size:20px;font-weight:700;}"
                          "#entShowcaseBody{color:%5;font-size:13px;}"
                          "#entShowcaseIndex{color:%6;font-size:11px;letter-spacing:1px;}"
                          "#entShowcaseNote{color:%6;font-size:11px;}"
                          "QPushButton#entShowcaseDot{background:%7;border:none;padding:0;"
                          "  min-width:8px;max-width:8px;min-height:4px;max-height:4px;}"
                          "QPushButton#entShowcaseDot:hover{background:%5;}"
                          "QPushButton#entShowcaseDot[active=\"true\"]{background:%3;min-width:22px;max-width:22px;}"
                          "QPushButton#entShowcasePrimary{background:%3;color:%8;border:1px solid %3;"
                          "  padding:8px 12px;font-weight:700;letter-spacing:0.5px;}"
                          "QPushButton#entShowcasePrimary:hover{border-color:%4;}"
                          "QPushButton#entShowcasePrimary:pressed{background:%9;border-color:%9;color:%4;}"
                          "QPushButton#entShowcaseSecondary{background:transparent;color:%5;border:1px solid %7;"
                          "  padding:8px 12px;font-weight:700;letter-spacing:0.5px;}"
                          "QPushButton#entShowcaseSecondary:hover{color:%3;border-color:%3;}")
                      .arg(colors::BG_SURFACE(), colors::BORDER_DIM(), colors::AMBER(), colors::TEXT_PRIMARY(),
                           colors::TEXT_SECONDARY(), colors::TEXT_DIM(), colors::BORDER_MED(), colors::BG_BASE(),
                           colors::AMBER_DIM()));
}

QWidget* make_enterprise_showcase(QWidget* parent) {
    return new EnterpriseShowcase(parent);
}

} // namespace fincept::ui
