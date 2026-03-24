#include "screens/info/PrivacyScreen.h"

#include "ui/theme/Theme.h"

#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace fincept::screens {

using namespace fincept::ui;

static const char* MF = "font-family:'Consolas','Courier New',monospace;";

static QLabel* section_heading(const QString& icon, const QString& title) {
    auto* lbl = new QLabel(QString("%1  %2").arg(icon, title));
    lbl->setStyleSheet(QString("color: #d97706; font-size: 14px; font-weight: 700; "
                               "background: transparent; margin-top: 8px; %1")
                           .arg(MF));
    return lbl;
}

static QLabel* body_text(const QString& text) {
    auto* lbl = new QLabel(text);
    lbl->setWordWrap(true);
    lbl->setStyleSheet(QString("color: #e5e5e5; font-size: 12px; background: transparent; %1").arg(MF));
    return lbl;
}

static QLabel* bullet(const QString& text) {
    auto* lbl = new QLabel(QString("  > %1").arg(text));
    lbl->setWordWrap(true);
    lbl->setStyleSheet(QString("color: #808080; font-size: 12px; background: transparent; %1").arg(MF));
    return lbl;
}

static QWidget* info_card(const QString& title, const QString& desc) {
    auto* card = new QWidget;
    card->setStyleSheet("background: #0a0a0a; border: 1px solid #1a1a1a; border-radius: 2px;");
    auto* vl = new QVBoxLayout(card);
    vl->setContentsMargins(12, 10, 12, 10);
    vl->setSpacing(4);

    auto* t = new QLabel(title);
    t->setStyleSheet(QString("color: #d97706; font-size: 11px; font-weight: 700; background: transparent; %1").arg(MF));
    vl->addWidget(t);

    auto* d = new QLabel(desc);
    d->setWordWrap(true);
    d->setStyleSheet(QString("color: #808080; font-size: 11px; background: transparent; %1").arg(MF));
    vl->addWidget(d);

    return card;
}

PrivacyScreen::PrivacyScreen(QWidget* parent) : QWidget(parent) {
    setStyleSheet(QString("QWidget#PrivacyRoot { background: %1; }").arg(colors::BG_BASE));
    setObjectName("PrivacyRoot");

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

    // Back
    auto* back_btn = new QPushButton("< BACK");
    back_btn->setCursor(Qt::PointingHandCursor);
    back_btn->setStyleSheet(QString("QPushButton { color: #808080; background: transparent; border: none; "
                                    "font-size: 12px; %1 } QPushButton:hover { color: #e5e5e5; }")
                                .arg(MF));
    connect(back_btn, &QPushButton::clicked, this, &PrivacyScreen::navigate_back);
    vl->addWidget(back_btn, 0, Qt::AlignLeft);

    auto* title = new QLabel("PRIVACY POLICY");
    title->setStyleSheet(QString("color: %1; font-size: 20px; font-weight: 700; letter-spacing: 1px; "
                                 "background: transparent; %2")
                             .arg(colors::AMBER, MF));
    vl->addWidget(title);

    auto* updated = new QLabel("Last updated: January 1, 2026");
    updated->setStyleSheet(
        QString("color: %1; font-size: 11px; background: transparent; %2").arg(colors::TEXT_TERTIARY, MF));
    vl->addWidget(updated);
    vl->addSpacing(12);

    // Main panel
    auto* panel = new QWidget;
    panel->setStyleSheet("background: #0a0a0a; border: 1px solid #1a1a1a; border-radius: 2px;");
    auto* pvl = new QVBoxLayout(panel);
    pvl->setContentsMargins(20, 16, 20, 16);
    pvl->setSpacing(6);

    // 1 — Commitment
    pvl->addWidget(section_heading("#", "OUR COMMITMENT TO PRIVACY"));
    pvl->addWidget(
        body_text("At Fincept Corporation, we are committed to protecting your privacy. This policy describes "
                  "how we collect, use, and safeguard your personal information when you use Fincept Terminal."));

    // 2 — Information We Collect
    pvl->addWidget(section_heading("@", "INFORMATION WE COLLECT"));
    pvl->addWidget(body_text("Personal Information:"));
    pvl->addWidget(bullet("Name and email address"));
    pvl->addWidget(bullet("Account credentials (encrypted)"));
    pvl->addWidget(bullet("Payment information (processed by third-party providers)"));
    pvl->addWidget(bullet("Phone number (optional)"));
    pvl->addWidget(bullet("Country and region"));

    pvl->addSpacing(4);
    pvl->addWidget(body_text("Usage Information:"));
    pvl->addWidget(bullet("Feature usage and navigation patterns"));
    pvl->addWidget(bullet("Device and browser information"));
    pvl->addWidget(bullet("IP address and approximate location"));
    pvl->addWidget(bullet("Error logs and performance metrics"));
    pvl->addWidget(bullet("Session duration and frequency"));

    // 3 — How We Use
    pvl->addWidget(section_heading("*", "HOW WE USE YOUR INFORMATION"));
    {
        auto* grid = new QGridLayout;
        grid->setSpacing(8);
        grid->addWidget(
            info_card("SERVICE DELIVERY",
                      "Provide and maintain terminal features, process transactions, and deliver data feeds"),
            0, 0);
        grid->addWidget(
            info_card("SECURITY",
                      "Protect accounts, detect fraud, enforce terms of service, and ensure platform integrity"),
            0, 1);
        grid->addWidget(info_card("COMMUNICATION",
                                  "Send service updates, security alerts, support responses, and optional marketing"),
                        1, 0);
        grid->addWidget(
            info_card("IMPROVEMENT", "Analyze usage to improve features, fix bugs, and develop new capabilities"), 1,
            1);
        pvl->addLayout(grid);
    }

    // 4 — Sharing
    pvl->addWidget(section_heading("~", "INFORMATION SHARING"));
    pvl->addWidget(body_text("We may share your information with:"));
    pvl->addWidget(bullet("Service Providers — third-party services that help operate the platform"));
    pvl->addWidget(bullet("Legal Requirements — when required by law or to protect our rights"));
    pvl->addWidget(bullet("Business Transfer — in connection with a merger, acquisition, or sale"));
    pvl->addWidget(bullet("With Your Consent — when you explicitly authorize sharing"));

    // 5 — Security
    pvl->addWidget(section_heading("!", "DATA SECURITY"));
    pvl->addWidget(body_text("We implement industry-standard security measures:"));
    pvl->addWidget(bullet("End-to-end encryption for sensitive data"));
    pvl->addWidget(bullet("Secure credential storage (encrypted at rest)"));
    pvl->addWidget(bullet("Regular security audits and penetration testing"));
    pvl->addWidget(bullet("Access controls and authentication requirements"));
    pvl->addWidget(bullet("Automatic session expiry and logout"));
    pvl->addWidget(bullet("HTTPS/TLS for all data transmission"));

    // 6 — Rights
    pvl->addWidget(section_heading("=", "YOUR RIGHTS"));
    pvl->addWidget(body_text("You have the right to:"));
    pvl->addWidget(bullet("Access — Request a copy of your personal data"));
    pvl->addWidget(bullet("Correction — Update inaccurate or incomplete data"));
    pvl->addWidget(bullet("Deletion — Request deletion of your account and data"));
    pvl->addWidget(bullet("Portability — Export your data in a machine-readable format"));
    pvl->addWidget(bullet("Opt-out — Unsubscribe from marketing communications"));

    // 7 — Contact
    pvl->addWidget(section_heading("@", "CONTACT US"));
    pvl->addWidget(body_text("Privacy Officer: support@fincept.in"));
    pvl->addWidget(body_text("For privacy-related inquiries, write to the address above."));

    vl->addWidget(panel);

    // Footer navigation
    auto* footer = new QWidget;
    footer->setStyleSheet("background: transparent;");
    auto* fhl = new QHBoxLayout(footer);
    fhl->setContentsMargins(0, 12, 0, 0);

    auto make_link = [](const QString& text) {
        auto* btn = new QPushButton(text);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet("QPushButton { color: #0891b2; background: transparent; border: none; "
                           "font-size: 12px; font-family:'Consolas','Courier New',monospace; }"
                           "QPushButton:hover { color: #38bdf8; }");
        return btn;
    };

    auto* terms_link = make_link("Terms of Service");
    connect(terms_link, &QPushButton::clicked, this, &PrivacyScreen::navigate_terms);
    fhl->addWidget(terms_link);

    fhl->addStretch();

    auto* contact_link = make_link("Contact Us");
    connect(contact_link, &QPushButton::clicked, this, &PrivacyScreen::navigate_contact);
    fhl->addWidget(contact_link);

    vl->addWidget(footer);
    vl->addStretch();

    scroll->setWidget(page);
    root->addWidget(scroll, 1);
}

} // namespace fincept::screens
