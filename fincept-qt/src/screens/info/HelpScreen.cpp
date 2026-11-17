#include "screens/info/HelpScreen.h"

#include "core/events/EventBus.h"
#include "screens/info/ContactScreen.h"
#include "screens/info/PrivacyScreen.h"
#include "screens/launchpad/OnboardingTour.h"
#include "ui/theme/Theme.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMetaMethod>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QStackedWidget>
#include <QUrl>
#include <QVBoxLayout>

#include <functional>
#include <utility>

namespace fincept::screens {

using namespace fincept::ui;

namespace {

// Addresses are shown verbatim, never translated.
constexpr QLatin1String kSupportEmail("support@fincept.in");
constexpr QLatin1String kDiscordUrl("https://discord.gg/ae87a8ygbN");
// The in-app Docs panel needs a signed-in terminal; before sign-in, the same guides online.
constexpr QLatin1String kOnlineDocsUrl("https://github.com/Fincept-Corporation/FinceptTerminal/tree/main/docs");

using Action = std::function<void()>;

QLabel* make_label(const char* object_name, const QString& text, bool wrap = true) {
    auto* label = new QLabel(text);
    label->setObjectName(QLatin1String(object_name));
    label->setTextFormat(Qt::PlainText);
    label->setWordWrap(wrap);
    return label;
}

QPushButton* make_button(const char* object_name, const QString& text, const Action& action) {
    auto* button = new QPushButton(text);
    button->setObjectName(QLatin1String(object_name));
    button->setCursor(Qt::PointingHandCursor);
    QObject::connect(button, &QPushButton::clicked, button, [action]() { action(); });
    return button;
}

QFrame* make_card() {
    auto* card = new QFrame;
    card->setObjectName(QStringLiteral("helpCard"));
    return card;
}

Action open_url(const QString& url) {
    return [url]() { QDesktopServices::openUrl(QUrl(url)); };
}

/// Signed in: opens the screen as a dock panel in the frame this page lives in.
Action open_panel(const char* screen_id) {
    return [screen_id]() {
        EventBus::instance().publish(QStringLiteral("nav.switch_screen"),
                                     QVariantMap{{QStringLiteral("screen_id"), QString::fromLatin1(screen_id)}});
    };
}

/// Before sign-in: Privacy and Contact are pages of the same info stack as this
/// one (their BACK returns to sign-in). Empty when the page is not there.
template <typename Screen>
Action open_info_sibling(QWidget* self) {
    QPointer<QStackedWidget> stack = qobject_cast<QStackedWidget*>(self->parentWidget());
    QPointer<Screen> target = stack ? stack->findChild<Screen*>(QString(), Qt::FindDirectChildrenOnly) : nullptr;
    if (!target)
        return {};
    return [stack, target]() {
        if (stack && target)
            stack->setCurrentWidget(target);
    };
}

} // namespace

HelpScreen::HelpScreen(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("helpRoot"));
    setAttribute(Qt::WA_StyledBackground, true);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    scroll_ = new QScrollArea;
    scroll_->setObjectName(QStringLiteral("helpScroll"));
    scroll_->setWidgetResizable(true);
    scroll_->setFrameShape(QFrame::NoFrame);
    root->addWidget(scroll_, 1);
    // The page is built on first show — see showEvent(). This screen is also
    // constructed eagerly for the pre-login info stack.

    refresh_theme();
    // Never inside theme_changed (restyling there stalls a theme switch): once the
    // appearance has settled, and only when the sheet's text changed.
    connect(&ThemeManager::instance(), &ThemeManager::appearance_settled, this, [this]() { refresh_theme(); });
}

void HelpScreen::refresh_theme() {
    // One stylesheet for the whole page; every widget is matched by objectName.
    QString ss = QStringLiteral(
        "#helpRoot, #helpPage, QScrollArea#helpScroll { background:{BG}; border:none; }"
        "#helpPage QLabel { background:transparent; }"
        "QLabel#helpTitle { color:{ACCENT}; font-size:{FS_TITLE}px; font-weight:700; letter-spacing:1px; }"
        "QLabel#helpLead { color:{TEXT2}; font-size:{FS_BODY}px; }"
        "QLabel#helpSection { color:{ACCENT}; font-size:{FS_TINY}px; font-weight:700; letter-spacing:1px; }"
        "QFrame#helpCard { background:{SURFACE}; border:1px solid {BORDER}; }"
        "QLabel#helpRowTitle { color:{TEXT}; font-size:{FS_BODY}px; font-weight:700; }"
        "QLabel#helpText { color:{TEXT2}; font-size:{FS_BODY}px; }"
        "QPushButton#helpAction { background:{RAISED}; color:{TEXT}; border:1px solid {BORDER_MED}; }"
        "QPushButton#helpAction { padding:4px 14px; font-size:{FS_SMALL}px; font-weight:600; }"
        "QPushButton#helpAction:hover, QPushButton#helpAction:focus { color:{ACCENT}; border-color:{ACCENT}; }"
        "QPushButton#helpQ { background:transparent; border:none; border-top:1px solid {BORDER}; padding:0; }"
        "QPushButton#helpQ[first=\"true\"] { border-top:none; }"
        "QPushButton#helpQ:hover, QPushButton#helpQ:focus, QPushButton#helpQ:checked { background:{RAISED}; }"
        "QLabel#helpQText { color:{TEXT}; font-size:{FS_BODY}px; font-weight:600; }"
        "QLabel#helpAnswer { background:{RAISED}; color:{TEXT2}; font-size:{FS_BODY}px; padding:0 12px 10px 30px; }"
        "QLabel#helpFoot { color:{TEXT3}; font-size:{FS_SMALL}px; }"
        "QPushButton#helpLink { background:transparent; color:{ACCENT}; border:none; padding:0; }"
        "QPushButton#helpLink { font-size:{FS_SMALL}px; text-align:left; }"
        "QPushButton#helpLink:hover, QPushButton#helpLink:focus { color:{TEXT}; text-decoration:underline; }");
    const auto color = [](const ColorToken& token) { return QString::fromLatin1(token.get()); };
    const std::pair<const char*, QString> values[] = {
        {"{BG}", color(colors::BG_BASE)},          {"{SURFACE}", color(colors::BG_SURFACE)},
        {"{RAISED}", color(colors::BG_RAISED)},     {"{BORDER}", color(colors::BORDER_DIM)},
        {"{BORDER_MED}", color(colors::BORDER_MED)}, {"{ACCENT}", color(colors::AMBER)},
        {"{TEXT}", color(colors::TEXT_PRIMARY)},    {"{TEXT2}", color(colors::TEXT_SECONDARY)},
        {"{TEXT3}", color(colors::TEXT_TERTIARY)},  {"{FS_TITLE}", fonts::px(6)},
        {"{FS_BODY}", fonts::px(-1)},               {"{FS_SMALL}", fonts::px(-2)},
        {"{FS_TINY}", fonts::px(-3)},
    };
    for (const auto& [key, value] : values)
        ss.replace(QLatin1String(key), value);
    // setStyleSheet() re-styles the whole page even for an identical sheet.
    if (styleSheet() != ss)
        setStyleSheet(ss);
}

void HelpScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    // Built here rather than in the constructor so build_page() can tell which
    // host it is in: by first show, WindowFrame has already connected the
    // pre-login navigation signals (or, for a docked copy, never will).
    if (!page_built_) {
        page_built_ = true;
        scroll_->setWidget(build_page());
    }
}

void HelpScreen::changeEvent(QEvent* event) {
    // Static text — rebuild the page rather than keeping every label as a member.
    // QScrollArea::setWidget() deletes the previous page.
    if (event->type() == QEvent::LanguageChange && scroll_ && page_built_)
        scroll_->setWidget(build_page());
    QWidget::changeEvent(event);
}

QWidget* HelpScreen::build_page() {
    // The pre-login info stack connects navigate_back; a docked copy has no listener.
    const bool before_sign_in = isSignalConnected(QMetaMethod::fromSignal(&HelpScreen::navigate_back));

    auto* page = new QWidget;
    page->setObjectName(QStringLiteral("helpPage"));
    auto* vl = new QVBoxLayout(page);
    vl->setContentsMargins(24, 20, 24, 24);
    vl->setSpacing(6);

    const auto section = [vl](const QString& title) {
        vl->addSpacing(14);
        vl->addWidget(make_label("helpSection", title));
    };

    // ── Header ────────────────────────────────────────────────────────────────
    if (before_sign_in)
        vl->addWidget(make_button("helpLink", tr("< BACK"), [this]() { emit navigate_back(); }), 0, Qt::AlignLeft);
    vl->addWidget(make_label("helpTitle", tr("HELP CENTER")));
    vl->addWidget(
        make_label("helpLead", tr("Where to get help, and short answers to the problems people run into most.")));

    // ── Get help ──────────────────────────────────────────────────────────────
    section(tr("GET HELP"));
    {
        struct Row {
            QString title;
            QString text;
            QString button;
            Action action;
        };
        const QString email(kSupportEmail);
        QList<Row> rows;
        if (before_sign_in) {
            rows = {
                {tr("Documentation"), tr("Guides for every screen, in your browser."), tr("Open Docs"),
                 open_url(kOnlineDocsUrl)},
                {tr("Report a problem"), tr("E-mail %1. Once signed in you can also open a support ticket.").arg(email),
                 tr("Send E-mail"), open_url(QStringLiteral("mailto:") + email)},
                {tr("Can't sign in?"), tr("Reset your password with a code sent to your e-mail."), tr("Reset Password"),
                 [this]() { emit navigate_forgot_password(); }},
            };
        } else {
            rows = {
                {tr("Documentation"), tr("Guides for every screen, including the keyboard shortcuts."),
                 tr("Open Docs"), open_panel("docs")},
                {tr("Report a problem"), tr("Open a ticket and follow the replies in Support, or e-mail %1.").arg(email),
                 tr("Open Support"), open_panel("support")},
                {tr("Welcome tour"), tr("A 30-second walkthrough of the command bar, panel links and settings."),
                 tr("Start Tour"),
                 [this]() {
                     // Same as the help.replay_tour action: re-arm the first-run flag, show it now.
                     OnboardingTour::reset_seen();
                     OnboardingTour::show_for(window());
                 }},
            };
        }
        rows.append({tr("Community"), tr("Questions and discussion with other users on Discord."), tr("Join Discord"),
                     open_url(kDiscordUrl)});

        auto* card = make_card();
        auto* grid = new QGridLayout(card);
        grid->setContentsMargins(14, 10, 14, 10);
        grid->setHorizontalSpacing(16);
        grid->setVerticalSpacing(8);
        grid->setColumnStretch(1, 1);
        for (int i = 0; i < rows.size(); ++i) {
            const Row& row = rows.at(i);
            auto* button = make_button("helpAction", row.button, row.action);
            button->setAccessibleDescription(row.text);
            grid->addWidget(make_label("helpRowTitle", row.title, false), i, 0, Qt::AlignLeft | Qt::AlignVCenter);
            grid->addWidget(make_label("helpText", row.text), i, 1);
            grid->addWidget(button, i, 2, Qt::AlignVCenter);
        }
        vl->addWidget(card);
    }

    // ── Fix common problems ───────────────────────────────────────────────────
    // Every answer names what the user sees on screen; keep them true to the code.
    section(tr("FIX COMMON PROBLEMS"));
    {
        const std::pair<QString, QString> faqs[] = {
            {tr("Market data, charts or analytics stay empty"),
             tr("They need Python libraries that install in the background; the PYTHON ENV chip in the status bar "
                "shows the progress. If it reads PYTHON ENV · SET UP or PYTHON ENV FAILED · RETRY, click it — "
                "Settings ▸ Python Env lists what is installed and adds the full library set some features need. "
                "Sources that need an API key read it from Settings ▸ Credentials.")},
            {tr("Signed out, or told you are logged in on another device"),
             tr("Your account has one active session at a time: signing in on another computer ends the session on "
                "this one, and features that use it then say you were logged in from another device. To continue "
                "here, log out and sign in again — if the other session is still active, the SESSION CONFLICT page "
                "lets you log it out. A forgotten password is reset with FORGOT PASSWORD? on the sign-in screen.")},
            {tr("How do I update the terminal?"),
             tr("The terminal checks for a new version each time it starts and every few hours while it is open, and "
                "offers it in an update window. Update now downloads and verifies it, installs it over this "
                "installation — your settings and data are kept — and restarts the terminal. To check yourself, use "
                "Help ▸ Check for Updates; an installation that cannot update itself gets a link to the download "
                "page instead.")},
            {tr("Change the theme or the text size"),
             tr("Settings ▸ Appearance: Colour Theme (Dark, Light or System), Font Size, Font Family and Content "
                "Density. Changes apply at once; press Save Settings to keep them for the next start.")},
            {tr("Something crashed or misbehaves — what should I send?"),
             tr("About ▸ Diagnostics: Copy System Info puts your version and operating system on the clipboard, and "
                "Open Folder opens the crash dumps. Logs are in Settings ▸ Logging ▸ Open Log Folder. Paste the "
                "system info and the steps that led to the problem into a support ticket, and e-mail the files if "
                "support asks for them.")},
            {tr("Connect a broker or add an API key"),
             tr("For a broker, open Equity Trading, click ACCOUNTS, add the account, enter its API key and secret (or "
                "follow the broker's sign-in) and press CONNECT. Keys for data providers and other services go in "
                "Settings ▸ Credentials. Both are stored encrypted on this computer.")},
            {tr("What uses credits, and what happens when they run out?"),
             tr("Fincept AI requests and the cloud analytics modules use credits, and some modules need a Pro plan. "
                "Your balance is shown in the toolbar, and Profile ▸ Usage lists what used credits over the last 30 "
                "days. When the balance runs out those requests are refused until you top up from Plans & Pricing "
                "(click the plan badge in the toolbar); AI chat can also run on your own provider key from "
                "Settings ▸ LLM Config.")},
            {tr("Reset the terminal or remove it"),
             tr("Settings ▸ Storage & Cache: Reset UI State restores window and panel layouts at the next start, "
                "Clear All Cache removes temporary data, and Delete All (Danger Zone) erases all local data except "
                "the encrypted API keys. To remove the terminal, uninstall it like any other app on your system — "
                "run Delete All first if you want your local data gone too.")},
        };

        auto* card = make_card();
        auto* cl = new QVBoxLayout(card);
        cl->setContentsMargins(0, 0, 0, 0);
        cl->setSpacing(0);
        bool first = true;
        for (const auto& faq : faqs) {
            const QString& question = faq.first;
            // The question is a label inside a checkable button, so a long
            // question wraps (at any font size) instead of widening the page.
            auto* toggle = new QPushButton;
            toggle->setObjectName(QStringLiteral("helpQ"));
            toggle->setProperty("first", first);
            toggle->setCheckable(true);
            toggle->setCursor(Qt::PointingHandCursor);
            toggle->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
            toggle->setAccessibleName(question);
            auto* q_text = make_label("helpQText", QString());
            q_text->setAttribute(Qt::WA_TransparentForMouseEvents);
            auto* tl = new QVBoxLayout(toggle);
            tl->setContentsMargins(12, 8, 12, 8);
            tl->addWidget(q_text);

            auto* a_text = make_label("helpAnswer", faq.second);
            a_text->setTextInteractionFlags(Qt::TextSelectableByMouse);

            const auto show = [q_text, a_text, question](bool open) {
                q_text->setText((open ? QStringLiteral("▾  ") : QStringLiteral("▸  ")) + question);
                a_text->setVisible(open);
            };
            show(false);
            connect(toggle, &QPushButton::toggled, a_text, show);
            cl->addWidget(toggle);
            cl->addWidget(a_text);
            first = false;
        }
        vl->addWidget(card);
    }

    // ── What data the terminal collects ───────────────────────────────────────
    section(tr("WHAT DATA THE TERMINAL COLLECTS"));
    {
        auto* card = make_card();
        auto* hl = new QHBoxLayout(card);
        hl->setContentsMargins(14, 10, 14, 10);
        hl->setSpacing(16);
        hl->addWidget(make_label("helpText",
                                 tr("Broker and data-provider keys you save in the terminal are stored encrypted in "
                                    "its local database on this computer. The terminal "
                                    "sends usage information to Fincept to improve the product — app version, "
                                    "operating system and device type, how long and which screens are used, and "
                                    "error and performance reports — as described in the Privacy Policy.")),
                      1);
        const Action privacy = before_sign_in ? open_info_sibling<PrivacyScreen>(this) : open_panel("privacy");
        if (privacy)
            hl->addWidget(make_button("helpAction", tr("Privacy Policy"), privacy), 0, Qt::AlignTop);
        vl->addWidget(card);
    }

    // ── Footer ────────────────────────────────────────────────────────────────
    vl->addSpacing(14);
    {
        auto* foot = new QHBoxLayout;
        foot->setSpacing(16);
        foot->addWidget(
            make_label("helpFoot", tr("Fincept Terminal %1").arg(QCoreApplication::applicationVersion()), false));
        if (!before_sign_in)
            foot->addWidget(make_button("helpLink", tr("About and diagnostics"), open_panel("about")));
        const Action contact = before_sign_in ? open_info_sibling<ContactScreen>(this) : open_panel("contact");
        if (contact)
            foot->addWidget(make_button("helpLink", tr("Contact"), contact));
        foot->addStretch();
        vl->addLayout(foot);
    }

    vl->addStretch();
    return page;
}

} // namespace fincept::screens
