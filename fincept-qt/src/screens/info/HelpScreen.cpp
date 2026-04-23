#include "screens/info/HelpScreen.h"

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

static const char* MF = "font-family:'Consolas','Courier New',monospace;";

// ── Collapsible FAQ item ──────────────────────────────────────────────────────

static QWidget* make_faq(const QString& question, const QString& answer, const QString& icon = "?") {
    auto* container = new QWidget(nullptr);
    container->setStyleSheet("background: transparent;");
    auto* vl = new QVBoxLayout(container);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    auto* q_btn = new QPushButton(QString("  %1  %2").arg(icon, question));
    q_btn->setCursor(Qt::PointingHandCursor);
    q_btn->setStyleSheet(QString("QPushButton { color: %1; background: %2; border: 1px solid %3;"
                                 " padding: 11px 14px; text-align: left;"
                                 " font-size: 12px; font-weight: 600; %4 }"
                                 "QPushButton:hover { background: %5; border-color: %6; color: %7; }")
                             .arg(colors::TEXT_PRIMARY(), colors::BG_SURFACE(), colors::BORDER_DIM(), MF, colors::BG_RAISED(),
                                  colors::AMBER(), colors::AMBER()));

    auto* a_lbl = new QLabel(answer);
    a_lbl->setWordWrap(true);
    a_lbl->setStyleSheet(QString("color: %1; font-size: 12px; background: %2;"
                                 " border: 1px solid %3; border-top: none;"
                                 " border-left: 3px solid %4;"
                                 " padding: 12px 16px 12px 18px; %5")
                             .arg(colors::TEXT_SECONDARY(), colors::BG_SURFACE(), colors::BORDER_DIM(), colors::AMBER(), MF));
    a_lbl->setVisible(false);

    QObject::connect(q_btn, &QPushButton::clicked, a_lbl, [a_lbl, q_btn, question, icon]() {
        bool show = !a_lbl->isVisible();
        a_lbl->setVisible(show);
        QString arrow = show ? "▾" : "▸";
        q_btn->setText(QString("  %1  %2").arg(icon, question));
        // tint open state
        q_btn->setStyleSheet(
            QString("QPushButton { color: %1; background: %2; border: 1px solid %3;"
                    " border-left: 3px solid %3;"
                    " padding: 11px 14px; text-align: left;"
                    " font-size: 12px; font-weight: 600; font-family:'Consolas','Courier New',monospace; }"
                    "QPushButton:hover { background: %4; }")
                .arg(show ? colors::AMBER() : colors::TEXT_PRIMARY(), show ? colors::BG_RAISED() : colors::BG_SURFACE(),
                     show ? colors::AMBER() : colors::BORDER_DIM(), colors::BG_RAISED()));
        Q_UNUSED(arrow);
    });

    vl->addWidget(q_btn);
    vl->addWidget(a_lbl);
    return container;
}

// ── Section header ────────────────────────────────────────────────────────────

static QWidget* section_header(const QString& title, const QString& subtitle = {}) {
    auto* w = new QWidget(nullptr);
    w->setStyleSheet("background: transparent;");
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(3);

    auto* t = new QLabel(title);
    t->setStyleSheet(QString("color: %1; font-size: 11px; font-weight: bold; letter-spacing: 1px;"
                             " background: transparent; %2")
                         .arg(colors::AMBER(), MF));
    vl->addWidget(t);

    if (!subtitle.isEmpty()) {
        auto* s = new QLabel(subtitle);
        s->setStyleSheet(
            QString("color: %1; font-size: 11px; background: transparent; %2").arg(colors::TEXT_TERTIARY(), MF));
        vl->addWidget(s);
    }
    return w;
}

// ── Constructor ───────────────────────────────────────────────────────────────

HelpScreen::HelpScreen(QWidget* parent) : QWidget(parent) {
    setStyleSheet(QString("QWidget#HelpRoot { background: %1; }").arg(colors::BG_BASE()));
    setObjectName("HelpRoot");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea { border: none; background: transparent; }");

    auto* page = new QWidget(this);
    page->setStyleSheet(QString("background: %1;").arg(colors::BG_BASE()));
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(28, 24, 28, 32);
    vl->setSpacing(0);

    // ── Hero banner ───────────────────────────────────────────────────────────
    {
        auto* hero = new QWidget(this);
        hero->setStyleSheet(QString("background: %1; border: 1px solid %2; border-left: 4px solid %3;")
                                .arg(colors::BG_SURFACE(), colors::BORDER_DIM(), colors::AMBER()));
        auto* hl = new QHBoxLayout(hero);
        hl->setContentsMargins(20, 18, 20, 18);
        hl->setSpacing(16);

        auto* text_vl = new QVBoxLayout;
        text_vl->setSpacing(5);

        auto* title = new QLabel("HELP CENTER");
        title->setStyleSheet(QString("color: %1; font-size: 22px; font-weight: 700; letter-spacing: 2px;"
                                     " background: transparent; %2")
                                 .arg(colors::AMBER(), MF));
        text_vl->addWidget(title);

        auto* sub = new QLabel("Find answers, get support, and connect with the Fincept community.");
        sub->setStyleSheet(
            QString("color: %1; font-size: 12px; background: transparent; %2").arg(colors::TEXT_SECONDARY(), MF));
        text_vl->addWidget(sub);

        hl->addLayout(text_vl, 1);

        // Contact chips on the right
        auto* chips_vl = new QVBoxLayout;
        chips_vl->setSpacing(5);

        auto make_chip = [](const QString& icon, const QString& text, const QString& color,
                            const QString& url = {}) -> QWidget* {
            if (url.isEmpty()) {
                auto* chip = new QLabel(QString("%1  %2").arg(icon, text));
                chip->setStyleSheet(QString("color: %1; font-size: 11px; background: transparent;"
                                            " font-family:'Consolas','Courier New',monospace;")
                                        .arg(color));
                return chip;
            }
            auto* chip = new QPushButton(QString("%1  %2").arg(icon, text));
            chip->setFlat(true);
            chip->setCursor(Qt::PointingHandCursor);
            chip->setStyleSheet(QString("QPushButton { color: %1; font-size: 11px; background: transparent;"
                                        " border: none; text-align: left; padding: 0;"
                                        " font-family:'Consolas','Courier New',monospace; }"
                                        "QPushButton:hover { color: %2; }")
                                    .arg(color, colors::AMBER()));
            QObject::connect(chip, &QPushButton::clicked, chip, [url]() { QDesktopServices::openUrl(QUrl(url)); });
            return chip;
        };
        chips_vl->addWidget(make_chip("✉", "support@fincept.in", colors::CYAN, "mailto:support@fincept.in"));
        chips_vl->addWidget(
            make_chip("💬", "discord.gg/ae87a8ygbN", colors::POSITIVE, "https://discord.gg/ae87a8ygbN"));
        chips_vl->addWidget(make_chip("🕐", "Mon-Fri  9AM–6PM EST", colors::TEXT_TERTIARY));
        hl->addLayout(chips_vl);

        vl->addWidget(hero);
    }

    vl->addSpacing(20);

    // ── Quick Actions ─────────────────────────────────────────────────────────
    {
        vl->addWidget(section_header("QUICK ACTIONS", "Common tasks you can do right now"));
        vl->addSpacing(8);

        auto* grid = new QGridLayout;
        grid->setSpacing(8);

        struct Action {
            const char* icon;
            const char* label;
            const char* desc;
        };
        const Action actions[] = {
            {"👤", "Create Account", "Register for full access"},
            {"🔑", "Reset Password", "Recover your account"},
            {"📖", "Documentation", "Guides, tutorials & API ref"},
            {"🐛", "Report a Bug", "Open a bug report ticket"},
            {"💬", "Join Discord", "Community & live support"},
            {"🎟", "Support Tickets", "View or open a support ticket"},
        };

        int col = 0, row = 0;
        for (const auto& a : actions) {
            auto* btn = new QPushButton;
            btn->setCursor(Qt::PointingHandCursor);
            btn->setFixedHeight(60);
            btn->setStyleSheet(QString("QPushButton { background: %1; color: %2; border: 1px solid %3;"
                                       " text-align: left; padding: 0; }"
                                       "QPushButton:hover { background: %4; border-color: %5; }")
                                   .arg(colors::BG_SURFACE(), colors::TEXT_PRIMARY(), colors::BORDER_DIM(), colors::BG_RAISED(),
                                        colors::AMBER()));

            auto* bl = new QVBoxLayout(btn);
            bl->setContentsMargins(12, 8, 12, 8);
            bl->setSpacing(3);

            auto* top = new QHBoxLayout;
            top->setSpacing(7);
            auto* icon_lbl = new QLabel(QString::fromUtf8(a.icon));
            icon_lbl->setStyleSheet("background: transparent; font-size: 14px;");
            auto* name_lbl = new QLabel(a.label);
            name_lbl->setStyleSheet(QString("background: transparent; color: %1; font-size: 12px;"
                                            " font-weight: bold; %2")
                                        .arg(colors::TEXT_PRIMARY(), MF));
            top->addWidget(icon_lbl);
            top->addWidget(name_lbl);
            top->addStretch();
            bl->addLayout(top);

            auto* desc_lbl = new QLabel(a.desc);
            desc_lbl->setStyleSheet(
                QString("background: transparent; color: %1; font-size: 10px; %2").arg(colors::TEXT_TERTIARY(), MF));
            bl->addWidget(desc_lbl);

            if (col == 3) {
                col = 0;
                ++row;
            }
            grid->addWidget(btn, row, col++);

            // Wire known actions
            if (QString(a.label) == "Create Account")
                connect(btn, &QPushButton::clicked, this, &HelpScreen::navigate_register);
            if (QString(a.label) == "Reset Password")
                connect(btn, &QPushButton::clicked, this, &HelpScreen::navigate_forgot_password);
        }

        vl->addLayout(grid);
    }

    vl->addSpacing(24);

    // ── FAQ ───────────────────────────────────────────────────────────────────
    {
        vl->addWidget(section_header("FREQUENTLY ASKED QUESTIONS", "Click a question to expand the answer"));
        vl->addSpacing(8);

        struct FAQ {
            const char* icon;
            const char* q;
            const char* a;
        };
        const FAQ faqs[] = {
            {"🔑", "How do I reset my password?",
             "Click \"Forgot Password\" on the login screen. Enter your email address and we'll "
             "send you a reset link. The link expires in 24 hours."},

            {"👤", "What is Guest Access?",
             "Guest access lets you explore the terminal without creating an account. "
             "Features like trading, portfolio management, and AI analytics require a "
             "registered account."},

            {"💳", "What is a Credit?",
             "Credits are the in-app currency used for premium features such as AI analysis, "
             "advanced data feeds, and quantitative analytics. Free accounts receive a limited "
             "number of credits on signup. Additional credits can be purchased in Settings → Billing."},

            {"📊", "How do I connect a broker?",
             "Navigate to Settings → Brokers, select your broker from the list, and enter your "
             "API key and secret. Fincept supports 18+ brokers including Zerodha, Angel One, "
             "Upstox, Interactive Brokers, and more."},

            {"🐍", "Why does Python install at first launch?",
             "Fincept embeds Python for 1300+ analytics scripts covering CFA-level equity, "
             "portfolio, derivatives, and quant analysis. The one-time install is ~150 MB and "
             "happens automatically in the background."},

            {"💻", "What are the system requirements?",
             "Windows 10+ (x64), macOS 12+, or Linux (glibc 2.31+). 8 GB RAM recommended. "
             "Active internet required for data feeds. Python 3.11.9 is installed automatically "
             "during first-time setup."},

            {"🔒", "Is my data secure?",
             "Credentials are stored encrypted via SecureStorage (OS keychain on each platform). "
             "API keys are never logged or sent to Fincept servers — they are used only for "
             "direct broker connections from your machine."},

            {"🐛", "How do I report a bug?",
             "Open a support ticket with category \"bug report\" (Help → Support Tickets → "
             "+ New Ticket). Include your OS, version, steps to reproduce, and any error "
             "messages you see. Screenshots are helpful."},
        };

        for (const auto& f : faqs)
            vl->addWidget(make_faq(f.q, f.a, QString::fromUtf8(f.icon)));

        vl->addSpacing(24);
    }

    // ── Getting Started ────────────────────────────────────────────────────────
    {
        vl->addWidget(section_header("GETTING STARTED", "New to Fincept? Start here"));
        vl->addSpacing(8);

        struct Step {
            const char* num;
            const char* title;
            const char* detail;
        };
        const Step steps[] = {
            {"1", "Create an account", "Register at fincept.in or use the in-app sign-up."},
            {"2", "Complete setup", "The setup wizard installs Python and configures your paths."},
            {"3", "Connect a data source", "Add a broker or enable free data feeds in Data Sources."},
            {"4", "Explore the terminal", "Browse Markets, Research, AI Chat, and QuantLib tabs."},
        };

        auto* steps_widget = new QWidget(this);
        steps_widget->setStyleSheet(
            QString("background: %1; border: 1px solid %2;").arg(colors::BG_SURFACE(), colors::BORDER_DIM()));
        auto* swl = new QVBoxLayout(steps_widget);
        swl->setContentsMargins(0, 0, 0, 0);
        swl->setSpacing(0);

        for (int i = 0; i < 4; ++i) {
            const auto& s = steps[i];
            auto* row = new QWidget(this);
            bool last = (i == 3);
            row->setStyleSheet(QString("background: transparent; border-bottom: %1;")
                                   .arg(last ? "none" : QString("1px solid %1;").arg(colors::BORDER_DIM())));
            auto* rl = new QHBoxLayout(row);
            rl->setContentsMargins(16, 12, 16, 12);
            rl->setSpacing(14);

            auto* num = new QLabel(s.num);
            num->setFixedSize(26, 26);
            num->setAlignment(Qt::AlignCenter);
            num->setStyleSheet(QString("background: %1; color: %2; font-size: 12px; font-weight: bold;"
                                       " border-radius: 13px; %3")
                                   .arg(colors::AMBER(), colors::BG_BASE(), MF));
            rl->addWidget(num);

            auto* txt_vl = new QVBoxLayout;
            txt_vl->setSpacing(2);
            auto* ttl = new QLabel(s.title);
            ttl->setStyleSheet(QString("color: %1; font-size: 12px; font-weight: bold;"
                                       " background: transparent; %2")
                                   .arg(colors::TEXT_PRIMARY(), MF));
            auto* det = new QLabel(s.detail);
            det->setStyleSheet(
                QString("color: %1; font-size: 11px; background: transparent; %2").arg(colors::TEXT_SECONDARY(), MF));
            det->setWordWrap(true);
            txt_vl->addWidget(ttl);
            txt_vl->addWidget(det);
            rl->addLayout(txt_vl, 1);

            swl->addWidget(row);
        }

        vl->addWidget(steps_widget);
        vl->addSpacing(24);
    }

    // ── Contact & Resources ───────────────────────────────────────────────────
    {
        vl->addWidget(section_header("CONTACT & RESOURCES"));
        vl->addSpacing(8);

        auto* grid2 = new QGridLayout;
        grid2->setSpacing(8);

        struct Contact {
            const char* icon;
            const char* label;
            const char* value;
            const char* url;
        };
        const Contact contacts[] = {
            {"✉", "Email Support", "support@fincept.in", "mailto:support@fincept.in"},
            {"💬", "Discord Server", "discord.gg/ae87a8ygbN", "https://discord.gg/ae87a8ygbN"},
            {"🌐", "Website", "fincept.in", "https://fincept.in"},
            {"📦", "GitHub", "github.com/Fincept-Corporation/FinceptTerminal",
             "https://github.com/Fincept-Corporation/FinceptTerminal"},
        };

        int ci = 0;
        for (const auto& c : contacts) {
            auto* card = new QWidget(this);
            card->setStyleSheet(
                QString("background: %1; border: 1px solid %2;").arg(colors::BG_SURFACE(), colors::BORDER_DIM()));
            auto* cl = new QHBoxLayout(card);
            cl->setContentsMargins(14, 12, 14, 12);
            cl->setSpacing(10);

            auto* ico = new QLabel(QString::fromUtf8(c.icon));
            ico->setStyleSheet("background: transparent; font-size: 18px;");
            cl->addWidget(ico);

            auto* tvl = new QVBoxLayout;
            tvl->setSpacing(2);
            auto* lbl = new QLabel(c.label);
            lbl->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: bold; background: transparent; %2")
                                   .arg(colors::TEXT_TERTIARY(), MF));

            auto* val = new QPushButton(c.value);
            val->setFlat(true);
            val->setCursor(Qt::PointingHandCursor);
            val->setStyleSheet(QString("QPushButton { color: %1; font-size: 11px; background: transparent;"
                                       " border: none; text-align: left; padding: 0; %2 }"
                                       "QPushButton:hover { color: %3; text-decoration: underline; }")
                                   .arg(colors::CYAN(), MF, colors::AMBER()));
            const QString link(c.url);
            QObject::connect(val, &QPushButton::clicked, val, [link]() { QDesktopServices::openUrl(QUrl(link)); });

            tvl->addWidget(lbl);
            tvl->addWidget(val);
            cl->addLayout(tvl, 1);

            grid2->addWidget(card, ci / 2, ci % 2);
            ++ci;
        }

        vl->addLayout(grid2);
    }

    vl->addStretch();
    scroll->setWidget(page);
    root->addWidget(scroll, 1);

    // ── Theme wiring ──────────────────────────────────────────────────────────
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this, [this](const ui::ThemeTokens&) {
        setStyleSheet(QString("QWidget#HelpRoot { background: %1; }").arg(colors::BG_BASE()));
    });
}

} // namespace fincept::screens
