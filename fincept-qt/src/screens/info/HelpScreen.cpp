#include "screens/info/HelpScreen.h"

#include "ui/theme/Theme.h"

#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace fincept::screens {

using namespace fincept::ui;

static const char* MF = "font-family:'Consolas','Courier New',monospace;";

/// Collapsible FAQ item built with a toggle button and answer label.
static QWidget* make_faq(const QString& question, const QString& answer) {
    auto* container = new QWidget;
    container->setStyleSheet("background: transparent;");
    auto* vl = new QVBoxLayout(container);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(2);

    auto* q_btn = new QPushButton(QString("> %1").arg(question));
    q_btn->setCursor(Qt::PointingHandCursor);
    q_btn->setStyleSheet(QString("QPushButton { color: %1; background: %2; border: 1px solid %3; "
                                 "border-radius: 2px; padding: 10px 14px; text-align: left; "
                                 "font-size: 12px; font-weight: 600; %4 }"
                                 "QPushButton:hover { background: %5; }")
                             .arg(colors::TEXT_PRIMARY, colors::BG_SURFACE, colors::BORDER_DIM,
                                  MF, colors::BG_RAISED));

    auto* a_lbl = new QLabel(answer);
    a_lbl->setWordWrap(true);
    a_lbl->setStyleSheet(QString("color: %1; font-size: 12px; background: %2; "
                                 "border: 1px solid %3; border-top: none; "
                                 "padding: 10px 14px; %4")
                             .arg(colors::TEXT_SECONDARY, colors::BG_SURFACE, colors::BORDER_DIM, MF));
    a_lbl->setVisible(false);

    QObject::connect(q_btn, &QPushButton::clicked, a_lbl, [a_lbl, q_btn, question]() {
        bool show = !a_lbl->isVisible();
        a_lbl->setVisible(show);
        q_btn->setText(show ? QString("v %1").arg(question) : QString("> %1").arg(question));
    });

    vl->addWidget(q_btn);
    vl->addWidget(a_lbl);
    return container;
}

HelpScreen::HelpScreen(QWidget* parent) : QWidget(parent) {
    setStyleSheet(QString("QWidget#HelpRoot { background: %1; }").arg(colors::BG_BASE));
    setObjectName("HelpRoot");

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
    back_btn->setStyleSheet(QString("QPushButton { color: %1; background: transparent; border: none; "
                                    "font-size: 12px; %2 } QPushButton:hover { color: %3; }")
                                .arg(colors::TEXT_SECONDARY, MF, colors::TEXT_PRIMARY));
    connect(back_btn, &QPushButton::clicked, this, &HelpScreen::navigate_back);
    vl->addWidget(back_btn, 0, Qt::AlignLeft);

    auto* title = new QLabel("HELP CENTER");
    title->setStyleSheet(QString("color: %1; font-size: 20px; font-weight: 700; letter-spacing: 1px; "
                                 "background: transparent; %2")
                             .arg(colors::AMBER, MF));
    vl->addWidget(title);

    auto* subtitle = new QLabel("Frequently asked questions and quick actions");
    subtitle->setStyleSheet(
        QString("color: %1; font-size: 13px; background: transparent; %2").arg(colors::TEXT_TERTIARY, MF));
    vl->addWidget(subtitle);
    vl->addSpacing(12);

    // ── FAQ Section ──────────────────────────────────────────────────────────
    {
        auto* header = new QLabel("FREQUENTLY ASKED QUESTIONS");
        header->setStyleSheet(QString("color: %1; font-size: 11px; font-weight: bold; letter-spacing: 0.5px; "
                                      "background: transparent; %2")
                                  .arg(colors::TEXT_SECONDARY, MF));
        vl->addWidget(header);
        vl->addSpacing(4);

        vl->addWidget(make_faq("How do I reset my password?",
                               "Click \"Forgot Password\" on the login screen. Enter your email address and "
                               "we'll send you a reset link. The link expires in 24 hours."));

        vl->addWidget(make_faq("What is Guest Access?",
                               "Guest access lets you explore the terminal without creating an account. "
                               "Some features like trading and portfolio management require a registered account."));

        vl->addWidget(make_faq("How do I contact support?",
                               "Email support@fincept.in or join our Discord community at discord.gg/ae87a8ygbN. "
                               "Support hours are Mon-Fri 9AM-6PM EST."));

        vl->addWidget(
            make_faq("What is a Credit?",
                     "Credits are the virtual currency used within Fincept Terminal. They are consumed "
                     "when using premium features like AI analysis, advanced analytics, and certain data feeds. "
                     "Free accounts receive a limited number of credits."));

        vl->addWidget(make_faq("What are the system requirements?",
                               "Windows 10+ (x64), macOS 12+, or Linux (glibc 2.31+). 8GB RAM recommended. "
                               "Internet connection required for data feeds. Python 3.12 is installed automatically "
                               "during first-time setup."));
    }

    vl->addSpacing(16);

    // ── Quick Actions ────────────────────────────────────────────────────────
    {
        auto* header = new QLabel("QUICK ACTIONS");
        header->setStyleSheet(QString("color: %1; font-size: 11px; font-weight: bold; letter-spacing: 0.5px; "
                                      "background: transparent; %2")
                                  .arg(colors::TEXT_SECONDARY, MF));
        vl->addWidget(header);
        vl->addSpacing(4);

        auto* grid = new QGridLayout;
        grid->setSpacing(8);

        auto make_action = [](const QString& text) {
            auto* btn = new QPushButton(text);
            btn->setFixedHeight(40);
            btn->setCursor(Qt::PointingHandCursor);
            btn->setStyleSheet(QString("QPushButton { background: %1; color: %2; border: 1px solid %3; "
                                       "border-radius: 2px; font-size: 12px; font-weight: 600; "
                                       "font-family:'Consolas','Courier New',monospace; }"
                                       "QPushButton:hover { background: %4; color: #38bdf8; }")
                                   .arg(colors::BG_SURFACE, colors::CYAN, colors::BORDER_DIM, colors::BG_RAISED));
            return btn;
        };

        auto* create_btn = make_action("Create Account");
        connect(create_btn, &QPushButton::clicked, this, &HelpScreen::navigate_register);
        grid->addWidget(create_btn, 0, 0);

        auto* reset_btn = make_action("Reset Password");
        connect(reset_btn, &QPushButton::clicked, this, &HelpScreen::navigate_forgot_password);
        grid->addWidget(reset_btn, 0, 1);

        vl->addLayout(grid);
    }

    vl->addSpacing(16);

    // ── Contact Information ──────────────────────────────────────────────────
    {
        auto* panel = new QWidget;
        panel->setStyleSheet(QString("background: %1; border: 1px solid %2; border-radius: 2px;")
                                 .arg(colors::BG_SURFACE, colors::BORDER_DIM));
        auto* pvl = new QVBoxLayout(panel);
        pvl->setContentsMargins(14, 12, 14, 12);
        pvl->setSpacing(6);

        auto* header = new QLabel("CONTACT INFORMATION");
        header->setStyleSheet(QString("color: %1; font-size: 11px; font-weight: bold; "
                                      "letter-spacing: 0.5px; background: transparent; %2")
                                  .arg(colors::AMBER, MF));
        pvl->addWidget(header);

        auto info = [](const QString& label, const QString& value) {
            auto* lbl = new QLabel(QString("%1:  %2").arg(label, value));
            lbl->setStyleSheet(QString("color: %1; font-size: 12px; background: transparent; "
                                       "font-family:'Consolas','Courier New',monospace;").arg(colors::TEXT_SECONDARY));
            return lbl;
        };

        pvl->addWidget(info("Email", "support@fincept.in"));
        pvl->addWidget(info("Discord", "discord.gg/ae87a8ygbN"));
        pvl->addWidget(info("Hours", "Mon-Fri 9AM-6PM EST"));

        vl->addWidget(panel);
    }

    vl->addStretch();
    scroll->setWidget(page);
    root->addWidget(scroll, 1);
}

} // namespace fincept::screens
