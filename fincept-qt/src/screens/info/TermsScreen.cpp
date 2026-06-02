#include "screens/info/TermsScreen.h"

#include "ui/theme/Theme.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace fincept::screens {

using namespace fincept::ui;

static const char* MF = "font-family:'Consolas','Courier New',monospace;";

static QLabel* section_heading(const QString& number, const QString& title) {
    auto* lbl = new QLabel(QString("%1. %2").arg(number, title));
    lbl->setStyleSheet(QString("color: %1; font-size: 14px; font-weight: 700; "
                               "background: transparent; margin-top: 8px; %2")
                           .arg(colors::AMBER(), MF));
    return lbl;
}

static QLabel* body_text(const QString& text) {
    auto* lbl = new QLabel(text);
    lbl->setWordWrap(true);
    lbl->setStyleSheet(QString("color: %1; font-size: 12px; line-height: 1.5; background: transparent; %2")
                           .arg(colors::TEXT_PRIMARY(), MF));
    return lbl;
}

static QLabel* bullet(const QString& text) {
    auto* lbl = new QLabel(QString("  > %1").arg(text));
    lbl->setWordWrap(true);
    lbl->setStyleSheet(
        QString("color: %1; font-size: 12px; background: transparent; %2").arg(colors::TEXT_SECONDARY(), MF));
    return lbl;
}

TermsScreen::TermsScreen(QWidget* parent) : QWidget(parent) {
    setStyleSheet(QString("QWidget#TermsRoot { background: %1; }").arg(colors::BG_BASE()));
    setObjectName("TermsRoot");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    scroll_ = new QScrollArea;
    scroll_->setWidgetResizable(true);
    scroll_->setStyleSheet("QScrollArea { border: none; background: transparent; }");
    scroll_->setWidget(build_page());
    root->addWidget(scroll_, 1);
}

// ── Re-translation ────────────────────────────────────────────────────────────
// Static legal text — rebuild the page on language change rather than caching
// every label as a member. QScrollArea::setWidget() deletes the old content.

void TermsScreen::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange && scroll_) {
        scroll_->setWidget(build_page());
    }
    QWidget::changeEvent(event);
}

// ── Page builder ──────────────────────────────────────────────────────────────

QWidget* TermsScreen::build_page() {
    auto* page = new QWidget(this);
    page->setStyleSheet(QString("background: %1;").arg(colors::BG_BASE()));
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(24, 24, 24, 24);
    vl->setSpacing(6);

    // Back button
    auto* back_btn = new QPushButton(tr("< BACK"));
    back_btn->setCursor(Qt::PointingHandCursor);
    back_btn->setStyleSheet(QString("QPushButton { color: %1; background: transparent; border: none; "
                                    "font-size: 12px; %2 } QPushButton:hover { color: %3; }")
                                .arg(colors::TEXT_SECONDARY(), MF, colors::TEXT_PRIMARY()));
    connect(back_btn, &QPushButton::clicked, this, &TermsScreen::navigate_back);
    vl->addWidget(back_btn, 0, Qt::AlignLeft);

    // Title
    auto* title = new QLabel(tr("TERMS OF SERVICE"));
    title->setStyleSheet(QString("color: %1; font-size: 20px; font-weight: 700; letter-spacing: 1px; "
                                 "background: transparent; %2")
                             .arg(colors::AMBER(), MF));
    vl->addWidget(title);

    auto* updated = new QLabel(tr("Last updated: January 1, 2026"));
    updated->setStyleSheet(
        QString("color: %1; font-size: 11px; background: transparent; %2").arg(colors::TEXT_TERTIARY(), MF));
    vl->addWidget(updated);
    vl->addSpacing(12);

    // Panel container
    auto* panel = new QWidget(this);
    panel->setStyleSheet(QString("background: %1; border: 1px solid %2; border-radius: 2px;")
                             .arg(colors::BG_SURFACE(), colors::BORDER_DIM()));
    auto* pvl = new QVBoxLayout(panel);
    pvl->setContentsMargins(20, 16, 20, 16);
    pvl->setSpacing(6);

    // Section 1
    pvl->addWidget(section_heading("1", tr("ACCEPTANCE OF TERMS")));
    pvl->addWidget(body_text(tr("By accessing or using Fincept Terminal (\"the Service\"), you agree to be bound by these "
                                "Terms of Service. If you do not agree to these terms, do not use the Service.")));

    // Section 2
    pvl->addWidget(section_heading("2", tr("DESCRIPTION OF SERVICE")));
    pvl->addWidget(body_text(tr("Fincept Terminal is a desktop financial intelligence terminal providing market data, "
                                "analytics, trading tools, and AI-powered research capabilities.")));

    // Section 3
    pvl->addWidget(section_heading("3", tr("USER ACCOUNTS AND REGISTRATION")));
    pvl->addWidget(
        body_text(tr("To access certain features, you must create an account. You are responsible for maintaining "
                     "the confidentiality of your account credentials and for all activities under your account.")));

    // Section 4
    pvl->addWidget(section_heading("4", tr("ACCEPTABLE USE POLICY")));
    pvl->addWidget(body_text(tr("You agree not to:")));
    pvl->addWidget(bullet(tr("Use the Service for any unlawful purpose")));
    pvl->addWidget(bullet(tr("Attempt to gain unauthorized access to any part of the Service")));
    pvl->addWidget(bullet(tr("Interfere with or disrupt the Service or its servers")));
    pvl->addWidget(bullet(tr("Reverse engineer, decompile, or disassemble any part of the Service")));
    pvl->addWidget(bullet(tr("Use automated means to access the Service without permission")));

    // Section 5
    pvl->addWidget(section_heading("5", tr("DATA AND PRIVACY")));
    pvl->addWidget(body_text(tr("Your use of the Service is also governed by our Privacy Policy. By using the Service, "
                                "you consent to the collection and use of information as described therein.")));

    // Section 6
    pvl->addWidget(section_heading("6", tr("SUBSCRIPTION AND BILLING")));
    pvl->addWidget(
        body_text(tr("Certain features require a paid subscription. Subscriptions are billed in advance. "
                     "Refunds are handled according to our refund policy. Credits expire according to plan terms.")));

    // Section 7
    pvl->addWidget(section_heading("7", tr("DISCLAIMERS AND LIMITATIONS")));
    pvl->addWidget(
        body_text(tr("The Service is provided \"as is\" without warranty of any kind. Fincept Corporation shall not "
                     "be liable for any indirect, incidental, special, or consequential damages. Financial data "
                     "and analytics are for informational purposes only and do not constitute investment advice.")));

    // Section 8
    pvl->addWidget(section_heading("8", tr("TERMINATION")));
    pvl->addWidget(body_text(tr("We may terminate or suspend your account at any time for violation of these terms. "
                                "Upon termination, your right to use the Service will immediately cease.")));

    // Section 9
    pvl->addWidget(section_heading("9", tr("CHANGES TO TERMS")));
    pvl->addWidget(body_text(tr("We reserve the right to modify these terms at any time. Continued use of the Service "
                                "after changes constitutes acceptance of the modified terms.")));

    // Section 10
    pvl->addWidget(section_heading("10", tr("CONTACT INFORMATION")));
    pvl->addWidget(body_text(tr("For questions about these Terms, contact us at support@fincept.in")));

    vl->addWidget(panel);

    // Footer navigation
    auto* footer = new QWidget(this);
    footer->setStyleSheet("background: transparent;");
    auto* fhl = new QHBoxLayout(footer);
    fhl->setContentsMargins(0, 12, 0, 0);

    auto make_link = [](const QString& text) {
        auto* btn = new QPushButton(text);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QString("QPushButton { color: %1; background: transparent; border: none; "
                                   "font-size: 12px; font-family:'Consolas','Courier New',monospace; }"
                                   "QPushButton:hover { color: #38bdf8; }")
                               .arg(colors::CYAN()));
        return btn;
    };

    auto* privacy_link = make_link(tr("Privacy Policy"));
    connect(privacy_link, &QPushButton::clicked, this, &TermsScreen::navigate_privacy);
    fhl->addWidget(privacy_link);

    fhl->addStretch();

    auto* contact_link = make_link(tr("Contact Us"));
    connect(contact_link, &QPushButton::clicked, this, &TermsScreen::navigate_contact);
    fhl->addWidget(contact_link);

    vl->addWidget(footer);
    vl->addStretch();

    return page;
}

} // namespace fincept::screens
