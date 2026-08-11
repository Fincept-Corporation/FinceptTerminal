#include "ui/widgets/EnterprisePromo.h"

#include "core/config/AppConfig.h"
#include "ui/theme/Theme.h"

#include <QCheckBox>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

namespace fincept::ui {

namespace {

/// AppConfig key for the "Don't show this again" opt-out.
constexpr const char* kEntSuppressKey = "ui/upgrade_prompt_suppressed";

/// A platform plugin with no window system — headless CI, --smoke-test runs
/// under offscreen. A modal there would block until the process is killed.
bool ent_headless_platform() {
    const QString p = QGuiApplication::platformName();
    return p.isEmpty() || p == QLatin1String("offscreen") || p == QLatin1String("minimal");
}

QString ent_accent_button_ss() {
    return QString("QPushButton{background:%1;color:%2;border:1px solid %1;"
                   "padding:7px 18px;font-weight:700;}"
                   "QPushButton:hover{background:%3;border-color:%3;}")
        .arg(colors::AMBER())
        .arg(colors::BG_BASE())
        .arg(colors::AMBER_DIM());
}

QString ent_ghost_button_ss() {
    return QString("QPushButton{background:transparent;color:%1;border:1px solid %2;"
                   "padding:7px 18px;font-weight:700;}"
                   "QPushButton:hover{color:%3;border-color:%3;}")
        .arg(colors::TEXT_SECONDARY())
        .arg(colors::BORDER_DIM())
        .arg(colors::AMBER());
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

void open_url(const QString& url) {
    QDesktopServices::openUrl(QUrl(url));
}

} // namespace enterprise

// ── UpgradeDialog ────────────────────────────────────────────────────────────

UpgradeDialog::UpgradeDialog(QWidget* parent) : QDialog(parent) {
    setObjectName("upgradeDialog");
    setModal(true);
    setMinimumWidth(560);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(28, 24, 28, 20);
    root->setSpacing(0);

    kicker_label_ = new QLabel;
    kicker_label_->setObjectName("upgradeKicker");
    root->addWidget(kicker_label_);
    root->addSpacing(4);

    title_label_ = new QLabel;
    title_label_->setObjectName("upgradeTitle");
    root->addWidget(title_label_);
    root->addSpacing(10);

    pitch_label_ = new QLabel;
    pitch_label_->setObjectName("upgradePitch");
    pitch_label_->setWordWrap(true);
    root->addWidget(pitch_label_);
    root->addSpacing(16);

    // Feature list — four rows, kept in the same order as retranslateUi().
    for (int i = 0; i < 4; ++i) {
        auto* row = new QLabel;
        row->setObjectName("upgradeFeature");
        row->setWordWrap(true);
        feature_labels_.append(row);
        root->addWidget(row);
        root->addSpacing(6);
    }

    root->addSpacing(10);
    price_label_ = new QLabel;
    price_label_->setObjectName("upgradePrice");
    price_label_->setWordWrap(true);
    root->addWidget(price_label_);

    root->addSpacing(4);
    oss_note_label_ = new QLabel;
    oss_note_label_->setObjectName("upgradeNote");
    oss_note_label_->setWordWrap(true);
    root->addWidget(oss_note_label_);

    root->addSpacing(20);

    auto* buttons = new QHBoxLayout;
    buttons->setContentsMargins(0, 0, 0, 0);
    buttons->setSpacing(8);

    dont_show_check_ = new QCheckBox;
    dont_show_check_->setObjectName("upgradeDontShow");
    dont_show_check_->setCursor(Qt::PointingHandCursor);
    buttons->addWidget(dont_show_check_);
    buttons->addStretch();

    later_btn_ = new QPushButton;
    later_btn_->setCursor(Qt::PointingHandCursor);
    later_btn_->setStyleSheet(ent_ghost_button_ss());
    connect(later_btn_, &QPushButton::clicked, this, &QDialog::reject);
    buttons->addWidget(later_btn_);

    compare_btn_ = new QPushButton;
    compare_btn_->setCursor(Qt::PointingHandCursor);
    compare_btn_->setStyleSheet(ent_ghost_button_ss());
    // No capture — open_url/comparison_url are free functions in the
    // enterprise namespace. `this` stays as connect()'s context object (3rd
    // arg) for lifetime, but capturing it would be unused and Clang builds
    // with -Werror=unused-lambda-capture.
    connect(compare_btn_, &QPushButton::clicked, this,
            []() { enterprise::open_url(enterprise::comparison_url()); });
    buttons->addWidget(compare_btn_);

    primary_btn_ = new QPushButton;
    primary_btn_->setCursor(Qt::PointingHandCursor);
    primary_btn_->setDefault(true);
    primary_btn_->setStyleSheet(ent_accent_button_ss());
    connect(primary_btn_, &QPushButton::clicked, this, [this]() {
        enterprise::open_url(enterprise::site_url());
        accept();
    });
    buttons->addWidget(primary_btn_);

    root->addLayout(buttons);

    // Persist the opt-out whichever way the dialog is dismissed.
    connect(this, &QDialog::finished, this, [this](int) {
        if (dont_show_check_ && dont_show_check_->isChecked())
            set_startup_prompt_enabled(false);
    });

    setStyleSheet(QString("#upgradeDialog{background:%1;border:1px solid %2;}"
                          "#upgradeKicker{color:%3;font-weight:700;letter-spacing:1px;font-size:11px;"
                          "  background:transparent;}"
                          "#upgradeTitle{color:%4;font-size:20px;font-weight:700;background:transparent;}"
                          "#upgradePitch{color:%5;font-size:13px;background:transparent;}"
                          "#upgradeFeature{color:%4;font-size:12px;background:transparent;}"
                          "#upgradePrice{color:%3;font-size:13px;font-weight:700;background:transparent;}"
                          "#upgradeNote{color:%6;font-size:11px;background:transparent;}"
                          "#upgradeDontShow{color:%6;font-size:11px;background:transparent;}")
                      .arg(colors::BG_SURFACE())     // %1
                      .arg(colors::BORDER_DIM())     // %2
                      .arg(colors::AMBER())          // %3
                      .arg(colors::TEXT_PRIMARY())   // %4
                      .arg(colors::TEXT_SECONDARY()) // %5
                      .arg(colors::TEXT_TERTIARY())); // %6

    retranslateUi();
}

void UpgradeDialog::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QDialog::changeEvent(event);
}

void UpgradeDialog::retranslateUi() {
    setWindowTitle(tr("Upgrade to Fincept Terminal Enterprise"));
    if (kicker_label_)
        kicker_label_->setText(tr("PRIVATE EDITION"));
    if (title_label_)
        title_label_->setText(tr("Fincept Terminal Enterprise"));
    if (pitch_label_)
        pitch_label_->setText(tr("You are running the free open-source build. Enterprise is the private edition "
                                 "the team develops daily — built for funds, family offices and research desks."));

    if (feature_labels_.size() == 4) {
        // U+2192 arrow used as a bullet glyph — decoded from UTF-8 so it does
        // not widen into mojibake the way a raw QStringLiteral would.
        const QString bullet = QString::fromUtf8("\xe2\x96\xb8 ");
        feature_labels_[0]->setText(bullet + tr("41 modules across 6 desks, on private and proprietary datasets"));
        feature_labels_[1]->setText(bullet +
                                    tr("Multi-agent research that plans and delegates, plus a private dataroom"));
        feature_labels_[2]->setText(bullet + tr("Live broker routing, live algo deployment, point-in-time backtests"));
        feature_labels_[3]->setText(bullet + tr("SSO/SAML, audit logs, role-based access and SLA-backed support"));
    }

    if (price_label_)
        price_label_->setText(tr("From $99/user/month — no annual lock-in, no seat minimum."));
    if (oss_note_label_)
        oss_note_label_->setText(tr("This open-source edition stays free and public under AGPL-3.0, with one "
                                    "release a month. Enterprise needs a separate account."));

    if (dont_show_check_)
        dont_show_check_->setText(tr("Don't show this again"));
    if (later_btn_)
        later_btn_->setText(tr("Maybe later"));
    if (compare_btn_)
        compare_btn_->setText(tr("Compare editions"));
    if (primary_btn_)
        primary_btn_->setText(tr("See Enterprise"));
}

bool UpgradeDialog::startup_prompt_enabled() {
    return !AppConfig::instance().get(kEntSuppressKey, false).toBool();
}

void UpgradeDialog::set_startup_prompt_enabled(bool enabled) {
    AppConfig::instance().set(kEntSuppressKey, !enabled);
}

void UpgradeDialog::show_now(QWidget* parent) {
    if (ent_headless_platform())
        return;
    auto* dlg = new UpgradeDialog(parent);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    // open() rather than exec(): the startup call site runs inside a timer
    // callback, and exec() there would spin a nested event loop.
    dlg->open();
}

void UpgradeDialog::maybe_show_at_startup(QWidget* parent) {
    if (!startup_prompt_enabled())
        return;
    show_now(parent);
}

// ── Inline banner ────────────────────────────────────────────────────────────

QWidget* make_enterprise_banner(QWidget* parent) {
    auto* panel = new QWidget(parent);
    panel->setObjectName("entBanner");

    auto* hl = new QHBoxLayout(panel);
    hl->setContentsMargins(18, 14, 18, 14);
    hl->setSpacing(14);

    auto* text_col = new QVBoxLayout;
    text_col->setContentsMargins(0, 0, 0, 0);
    text_col->setSpacing(3);

    auto* head = new QLabel(QObject::tr("FINCEPT TERMINAL ENTERPRISE"), panel);
    head->setObjectName("entBannerHead");
    text_col->addWidget(head);

    auto* sub = new QLabel(QObject::tr("The private edition — 41 modules, proprietary data, multi-agent research, "
                                       "live broker routing, SSO and an SLA. From $99/user/month."),
                           panel);
    sub->setObjectName("entBannerSub");
    sub->setWordWrap(true);
    text_col->addWidget(sub);

    // Enterprise runs on a different backend with its own user records, so a
    // subscription bought here grants nothing there. Say so on the banner —
    // sitting next to a list of purchasable plans, it would otherwise read as
    // just another tier of the same product.
    auto* note = new QLabel(QObject::tr("Separate product, separate account. Enterprise is billed and signed in "
                                        "separately — plans on this page do not apply to it, and a Fincept "
                                        "account from this terminal will not sign in to Enterprise."),
                            panel);
    note->setObjectName("entBannerNote");
    note->setWordWrap(true);
    text_col->addWidget(note);

    hl->addLayout(text_col, 1);

    auto* btn = new QPushButton(QObject::tr("See Enterprise"), panel);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setStyleSheet(ent_accent_button_ss());
    QObject::connect(btn, &QPushButton::clicked, panel, []() { enterprise::open_url(enterprise::site_url()); });
    hl->addWidget(btn, 0, Qt::AlignVCenter);

    panel->setStyleSheet(QString("#entBanner{background:%1;border:1px solid %2;}"
                                 "#entBannerHead{color:%2;font-weight:700;letter-spacing:1px;font-size:12px;"
                                 "  background:transparent;}"
                                 "#entBannerSub{color:%3;font-size:12px;background:transparent;}"
                                 "#entBannerNote{color:%4;font-size:11px;background:transparent;}")
                             .arg(colors::BG_SURFACE())
                             .arg(colors::AMBER())
                             .arg(colors::TEXT_SECONDARY())
                             .arg(colors::TEXT_TERTIARY()));
    return panel;
}

} // namespace fincept::ui
