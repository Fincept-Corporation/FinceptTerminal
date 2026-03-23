#include "screens/info/TermsScreen.h"

#include "ui/theme/Theme.h"

#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace fincept::screens {

using namespace fincept::ui;

static const char* MF = "font-family:'Consolas','Courier New',monospace;";

static QLabel* section_heading(const QString& number, const QString& title) {
    auto* lbl = new QLabel(QString("%1. %2").arg(number, title));
    lbl->setStyleSheet(QString("color: #d97706; font-size: 14px; font-weight: 700; "
                               "background: transparent; margin-top: 8px; %1")
                           .arg(MF));
    return lbl;
}

static QLabel* body_text(const QString& text) {
    auto* lbl = new QLabel(text);
    lbl->setWordWrap(true);
    lbl->setStyleSheet(QString("color: #e5e5e5; font-size: 12px; line-height: 1.5; background: transparent; %1").arg(MF));
    return lbl;
}

static QLabel* bullet(const QString& text) {
    auto* lbl = new QLabel(QString("  > %1").arg(text));
    lbl->setWordWrap(true);
    lbl->setStyleSheet(QString("color: #808080; font-size: 12px; background: transparent; %1").arg(MF));
    return lbl;
}

TermsScreen::TermsScreen(QWidget* parent) : QWidget(parent) {
    setStyleSheet(QString("QWidget#TermsRoot { background: %1; }").arg(colors::BG_BASE));
    setObjectName("TermsRoot");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea { border: none; background: transparent; }");

    auto* page = new QWidget;
    page->setStyleSheet(QString("background: %1;").arg(colors::BG_BASE));
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(24, 24, 24, 24);
    vl->setSpacing(6);

    // Back button
    auto* back_btn = new QPushButton("< BACK");
    back_btn->setCursor(Qt::PointingHandCursor);
    back_btn->setStyleSheet(QString("QPushButton { color: #808080; background: transparent; border: none; "
                                    "font-size: 12px; %1 } QPushButton:hover { color: #e5e5e5; }")
                                .arg(MF));
    connect(back_btn, &QPushButton::clicked, this, &TermsScreen::navigate_back);
    vl->addWidget(back_btn, 0, Qt::AlignLeft);

    // Title
    auto* title = new QLabel("TERMS OF SERVICE");
    title->setStyleSheet(QString("color: %1; font-size: 20px; font-weight: 700; letter-spacing: 1px; "
                                 "background: transparent; %2")
                             .arg(colors::AMBER, MF));
    vl->addWidget(title);

    auto* updated = new QLabel("Last updated: January 1, 2026");
    updated->setStyleSheet(QString("color: %1; font-size: 11px; background: transparent; %2")
                               .arg(colors::TEXT_TERTIARY, MF));
    vl->addWidget(updated);
    vl->addSpacing(12);

    // Panel container
    auto* panel = new QWidget;
    panel->setStyleSheet("background: #0a0a0a; border: 1px solid #1a1a1a; border-radius: 2px;");
    auto* pvl = new QVBoxLayout(panel);
    pvl->setContentsMargins(20, 16, 20, 16);
    pvl->setSpacing(6);

    // Section 1
    pvl->addWidget(section_heading("1", "ACCEPTANCE OF TERMS"));
    pvl->addWidget(body_text(
        "By accessing or using Fincept Terminal (\"the Service\"), you agree to be bound by these "
        "Terms of Service. If you do not agree to these terms, do not use the Service."));

    // Section 2
    pvl->addWidget(section_heading("2", "DESCRIPTION OF SERVICE"));
    pvl->addWidget(body_text(
        "Fincept Terminal is a desktop financial intelligence terminal providing market data, "
        "analytics, trading tools, and AI-powered research capabilities."));

    // Section 3
    pvl->addWidget(section_heading("3", "USER ACCOUNTS AND REGISTRATION"));
    pvl->addWidget(body_text(
        "To access certain features, you must create an account. You are responsible for maintaining "
        "the confidentiality of your account credentials and for all activities under your account."));

    // Section 4
    pvl->addWidget(section_heading("4", "ACCEPTABLE USE POLICY"));
    pvl->addWidget(body_text("You agree not to:"));
    pvl->addWidget(bullet("Use the Service for any unlawful purpose"));
    pvl->addWidget(bullet("Attempt to gain unauthorized access to any part of the Service"));
    pvl->addWidget(bullet("Interfere with or disrupt the Service or its servers"));
    pvl->addWidget(bullet("Reverse engineer, decompile, or disassemble any part of the Service"));
    pvl->addWidget(bullet("Use automated means to access the Service without permission"));

    // Section 5
    pvl->addWidget(section_heading("5", "DATA AND PRIVACY"));
    pvl->addWidget(body_text(
        "Your use of the Service is also governed by our Privacy Policy. By using the Service, "
        "you consent to the collection and use of information as described therein."));

    // Section 6
    pvl->addWidget(section_heading("6", "SUBSCRIPTION AND BILLING"));
    pvl->addWidget(body_text(
        "Certain features require a paid subscription. Subscriptions are billed in advance. "
        "Refunds are handled according to our refund policy. Credits expire according to plan terms."));

    // Section 7
    pvl->addWidget(section_heading("7", "DISCLAIMERS AND LIMITATIONS"));
    pvl->addWidget(body_text(
        "The Service is provided \"as is\" without warranty of any kind. Fincept Corporation shall not "
        "be liable for any indirect, incidental, special, or consequential damages. Financial data "
        "and analytics are for informational purposes only and do not constitute investment advice."));

    // Section 8
    pvl->addWidget(section_heading("8", "TERMINATION"));
    pvl->addWidget(body_text(
        "We may terminate or suspend your account at any time for violation of these terms. "
        "Upon termination, your right to use the Service will immediately cease."));

    // Section 9
    pvl->addWidget(section_heading("9", "CHANGES TO TERMS"));
    pvl->addWidget(body_text(
        "We reserve the right to modify these terms at any time. Continued use of the Service "
        "after changes constitutes acceptance of the modified terms."));

    // Section 10
    pvl->addWidget(section_heading("10", "CONTACT INFORMATION"));
    pvl->addWidget(body_text("For questions about these Terms, contact us at support@fincept.in"));

    vl->addWidget(panel);

    // Footer navigation
    auto* footer = new QWidget;
    footer->setStyleSheet("background: transparent;");
    auto* fhl = new QHBoxLayout(footer);
    fhl->setContentsMargins(0, 12, 0, 0);

    auto make_link = [](const QString& text) {
        auto* btn = new QPushButton(text);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QString("QPushButton { color: #0891b2; background: transparent; border: none; "
                                   "font-size: 12px; font-family:'Consolas','Courier New',monospace; }"
                                   "QPushButton:hover { color: #38bdf8; }"));
        return btn;
    };

    auto* privacy_link = make_link("Privacy Policy");
    connect(privacy_link, &QPushButton::clicked, this, &TermsScreen::navigate_privacy);
    fhl->addWidget(privacy_link);

    fhl->addStretch();

    auto* contact_link = make_link("Contact Us");
    connect(contact_link, &QPushButton::clicked, this, &TermsScreen::navigate_contact);
    fhl->addWidget(contact_link);

    vl->addWidget(footer);
    vl->addStretch();

    scroll->setWidget(page);
    root->addWidget(scroll, 1);
}

} // namespace fincept::screens
