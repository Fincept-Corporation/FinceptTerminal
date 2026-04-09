#include "screens/info/ContactScreen.h"

#include "ui/theme/Theme.h"

#include <QDesktopServices>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QUrl>
#include <QVBoxLayout>

namespace fincept::screens {

using namespace fincept::ui;

// ── Style constants ──────────────────────────────────────────────────────────

static QString PANEL() {
    return QString("background: %1; border: 1px solid %2; border-radius: 2px;")
        .arg(colors::BG_SURFACE, colors::BORDER_DIM);
}

static const char* MF = "font-family:'Consolas','Courier New',monospace;";

static QLabel* make_header(const QString& icon, const QString& title, const QString& icon_color) {
    auto* lbl = new QLabel(QString("%1  %2").arg(icon, title));
    lbl->setStyleSheet(QString("color: %1; font-size: 11px; font-weight: bold; letter-spacing: 0.5px; "
                               "background: %2; padding: 10px 14px; border-bottom: 1px solid %3; %4")
                           .arg(icon_color, colors::BG_RAISED, colors::BORDER_DIM, MF));
    return lbl;
}

static QWidget* make_contact_card(const QString& title, const QString& value, const QString& detail) {
    auto* card = new QWidget;
    card->setStyleSheet(PANEL());
    auto* vl = new QVBoxLayout(card);
    vl->setContentsMargins(14, 12, 14, 12);
    vl->setSpacing(4);

    auto* t = new QLabel(title);
    t->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: bold; "
                             "letter-spacing: 0.5px; background: transparent; %2")
                         .arg(colors::TEXT_SECONDARY, MF));
    vl->addWidget(t);

    auto* v = new QLabel(value);
    v->setStyleSheet(QString("color: %1; font-size: 13px; font-weight: 600; background: transparent; %2")
                         .arg(colors::TEXT_PRIMARY, MF));
    vl->addWidget(v);

    auto* d = new QLabel(detail);
    d->setStyleSheet(QString("color: %1; font-size: 11px; background: transparent; %2")
                         .arg(colors::TEXT_TERTIARY, MF));
    d->setWordWrap(true);
    vl->addWidget(d);

    return card;
}

// ── Constructor ──────────────────────────────────────────────────────────────

ContactScreen::ContactScreen(QWidget* parent) : QWidget(parent) {
    setStyleSheet(QString("QWidget#ContactRoot { background: %1; }").arg(colors::BG_BASE));
    setObjectName("ContactRoot");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea { border: none; background: transparent; }");

    auto* page = new QWidget;
    page->setStyleSheet(QString("background: %1;").arg(colors::BG_BASE));
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(24, 24, 24, 24);
    vl->setSpacing(12);

    // ── Header ───────────────────────────────────────────────────────────────
    auto* back_btn = new QPushButton("< BACK");
    back_btn->setCursor(Qt::PointingHandCursor);
    back_btn->setStyleSheet(QString("QPushButton { color: %1; background: transparent; border: none; "
                                    "font-size: 12px; %2 } QPushButton:hover { color: %3; }")
                                .arg(colors::TEXT_SECONDARY, MF, colors::TEXT_PRIMARY));
    connect(back_btn, &QPushButton::clicked, this, &ContactScreen::navigate_back);
    vl->addWidget(back_btn, 0, Qt::AlignLeft);

    auto* title = new QLabel("CONTACT US");
    title->setStyleSheet(QString("color: %1; font-size: 20px; font-weight: 700; letter-spacing: 1px; "
                                 "background: transparent; %2")
                             .arg(colors::AMBER, MF));
    vl->addWidget(title);

    auto* subtitle = new QLabel("Get in touch with our team");
    subtitle->setStyleSheet(
        QString("color: %1; font-size: 13px; background: transparent; %2").arg(colors::TEXT_TERTIARY, MF));
    vl->addWidget(subtitle);

    vl->addSpacing(8);

    // ── Contact Information ──────────────────────────────────────────────────
    {
        auto* panel = new QWidget;
        panel->setStyleSheet(PANEL());
        auto* pvl = new QVBoxLayout(panel);
        pvl->setContentsMargins(0, 0, 0, 0);
        pvl->setSpacing(0);

        pvl->addWidget(make_header("@", "CONTACT INFORMATION", colors::AMBER));

        auto* body = new QWidget;
        body->setStyleSheet("background: transparent;");
        auto* grid = new QGridLayout(body);
        grid->setContentsMargins(14, 12, 14, 12);
        grid->setSpacing(10);

        grid->addWidget(make_contact_card("EMAIL SUPPORT", "support@fincept.in", "Response within 4-6 hours"), 0, 0);
        grid->addWidget(make_contact_card("PHONE SUPPORT", "+1-800-FINCEPT", "Mon-Fri, 9AM-6PM EST"), 0, 1);
        grid->addWidget(make_contact_card("SUPPORT HOURS", "Mon-Fri 9AM-6PM EST", "Saturday 10AM-4PM EST"), 1, 0);
        grid->addWidget(make_contact_card("OFFICE", "Fincept Corporation", "New York, United States"), 1, 1);

        pvl->addWidget(body);
        vl->addWidget(panel);
    }

    // ── Quick Actions ────────────────────────────────────────────────────────
    {
        auto* panel = new QWidget;
        panel->setStyleSheet(PANEL());
        auto* pvl = new QVBoxLayout(panel);
        pvl->setContentsMargins(0, 0, 0, 0);
        pvl->setSpacing(0);

        pvl->addWidget(make_header(">>", "QUICK ACTIONS", colors::CYAN));

        auto* body = new QWidget;
        body->setStyleSheet("background: transparent;");
        auto* hl = new QHBoxLayout(body);
        hl->setContentsMargins(14, 12, 14, 12);
        hl->setSpacing(10);

        auto make_action = [](const QString& text) {
            auto* btn = new QPushButton(text);
            btn->setFixedHeight(36);
            btn->setCursor(Qt::PointingHandCursor);
            btn->setStyleSheet(QString("QPushButton { background: %1; color: %2; border: 1px solid %3; "
                                       "border-radius: 2px; padding: 8px 16px; font-size: 12px; "
                                       "font-family:'Consolas','Courier New',monospace; }"
                                       "QPushButton:hover { background: %4; color: #38bdf8; }")
                                   .arg(colors::BG_RAISED, colors::CYAN, colors::BORDER_DIM, colors::BG_HOVER));
            return btn;
        };

        auto* email_btn = make_action("Send Email");
        connect(email_btn, &QPushButton::clicked, this,
                []() { QDesktopServices::openUrl(QUrl("mailto:support@fincept.in")); });
        hl->addWidget(email_btn);

        auto* discord_btn = make_action("Join Discord");
        connect(discord_btn, &QPushButton::clicked, this,
                []() { QDesktopServices::openUrl(QUrl("https://discord.gg/ae87a8ygbN")); });
        hl->addWidget(discord_btn);

        auto* github_btn = make_action("GitHub Issues");
        connect(github_btn, &QPushButton::clicked, this, []() {
            QDesktopServices::openUrl(QUrl("https://github.com/Fincept-Corporation/FinceptTerminal/issues"));
        });
        hl->addWidget(github_btn);

        hl->addStretch();
        pvl->addWidget(body);
        vl->addWidget(panel);
    }

    // ── Common Issues ────────────────────────────────────────────────────────
    {
        auto* panel = new QWidget;
        panel->setStyleSheet(PANEL());
        auto* pvl = new QVBoxLayout(panel);
        pvl->setContentsMargins(0, 0, 0, 0);
        pvl->setSpacing(0);

        pvl->addWidget(make_header("?", "COMMON ISSUES", colors::AMBER));

        auto* body = new QWidget;
        body->setStyleSheet("background: transparent;");
        auto* bvl = new QVBoxLayout(body);
        bvl->setContentsMargins(14, 10, 14, 12);
        bvl->setSpacing(6);

        struct Issue {
            QString q;
            QString a;
        };
        const Issue issues[] = {
            {"Cannot log in or forgot password",
             "Use the Forgot Password option on the login screen, or contact support@fincept.in"},
            {"Python setup fails or times out",
             "Ensure you have a stable internet connection. Retry setup or check firewall settings."},
            {"Data not loading or showing stale",
             "Check your internet connection. Try refreshing the screen or restarting the terminal."},
        };

        for (const auto& issue : issues) {
            auto* q = new QLabel(QString("> %1").arg(issue.q));
            q->setStyleSheet(
                QString("color: %1; font-size: 12px; font-weight: 600; background: transparent; %2")
                    .arg(colors::TEXT_PRIMARY, MF));
            bvl->addWidget(q);

            auto* a = new QLabel(QString("  %1").arg(issue.a));
            a->setWordWrap(true);
            a->setStyleSheet(QString("color: %1; font-size: 11px; background: transparent; %2")
                                 .arg(colors::TEXT_TERTIARY, MF));
            bvl->addWidget(a);
            bvl->addSpacing(4);
        }

        pvl->addWidget(body);
        vl->addWidget(panel);
    }

    vl->addStretch();
    scroll->setWidget(page);
    root->addWidget(scroll, 1);
}

} // namespace fincept::screens
