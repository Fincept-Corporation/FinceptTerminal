#include "ui/navigation/ToolBar.h"

#include "auth/AuthManager.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QAction>
#include <QDateTime>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QPushButton>
#include <QResizeEvent>

namespace fincept::ui {

static QString menu_ss() {
    return QString("QMenuBar{background:transparent;color:%1;border:none;spacing:0;}"
                   "QMenuBar::item{background:transparent;padding:4px 10px;}"
                   "QMenuBar::item:selected{background:%2;color:%3;}")
        .arg(colors::TEXT_SECONDARY)
        .arg(colors::BG_RAISED)
        .arg(colors::TEXT_PRIMARY);
}

static QString popup_ss() {
    return QString("QMenu{background:%1;color:%2;border:1px solid %3;padding:4px 0;}"
                   "QMenu::item{padding:5px 24px 5px 12px;}"
                   "QMenu::item:selected{background:%4;}"
                   "QMenu::item:disabled{color:%5;}"
                   "QMenu::separator{background:%3;height:1px;margin:4px 8px;}")
        .arg(colors::BG_SURFACE)
        .arg(colors::TEXT_PRIMARY)
        .arg(colors::BORDER_DIM)
        .arg(colors::BG_RAISED)
        .arg(colors::TEXT_DIM);
}

ToolBar::ToolBar(QWidget* parent) : QWidget(parent) {
    setFixedHeight(32);
    auto* hl = new QHBoxLayout(this);
    hl->setContentsMargins(4, 0, 6, 0);
    hl->setSpacing(0);

    // Menus
    menu_bar_ = new QMenuBar(this);
    menu_bar_->setStyleSheet(menu_ss());
    menu_bar_->addMenu(build_file_menu());
    menu_bar_->addMenu(build_navigate_menu());
    menu_bar_->addMenu(build_view_menu());
    menu_bar_->addMenu(build_help_menu());
    hl->addWidget(menu_bar_);

    // Command bar — shrinks gracefully
    command_bar_ = new CommandBar(this);
    command_bar_->setMinimumWidth(80);
    command_bar_->setMaximumWidth(240);
    command_bar_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    connect(command_bar_, &CommandBar::navigate_to,  this, &ToolBar::navigate_to);
    connect(command_bar_, &CommandBar::dock_command, this, &ToolBar::dock_command);
    hl->addWidget(command_bar_);

    auto sep = [&]() -> QLabel* {
        auto* s = new QLabel("|");
        s->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        hl->addWidget(s);
        separators_.append(s);
        return s;
    };

    auto mk = [](const QString& t) -> QLabel* {
        auto* l = new QLabel(t);
        l->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        return l;
    };

    sep();
    fincept_label_ = mk("FINCEPT ");
    hl->addWidget(fincept_label_);
    branding_label_ = mk("TERMINAL");
    hl->addWidget(branding_label_);
    subtitle_label_ = mk("  |  PROFESSIONAL RESEARCH DESK");
    hl->addWidget(subtitle_label_);
    hl->addWidget(mk("  "));
    live_dot_ = mk("\xe2\x97\x8f");
    hl->addWidget(live_dot_);
    live_label_ = mk(" LIVE");
    hl->addWidget(live_label_);

    hl->addStretch(1);

    clock_label_ = mk("");
    clock_label_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    hl->addWidget(clock_label_);

    hl->addStretch(1);

    user_label_ = mk("---");
    user_label_->setMaximumWidth(120);
    hl->addWidget(user_label_);
    sep();
    credits_label_ = mk("---");
    credits_label_->setMaximumWidth(100);
    hl->addWidget(credits_label_);
    sep();
    plan_btn_ = new QPushButton("---");
    plan_btn_->setCursor(Qt::PointingHandCursor);
    plan_btn_->setToolTip("View Plans & Pricing");
    plan_btn_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(plan_btn_, &QPushButton::clicked, this, &ToolBar::plan_clicked);
    hl->addWidget(plan_btn_);
    sep();

    chat_mode_btn_ = new QPushButton(QString::fromUtf8("⬡ CHAT"));
    chat_mode_btn_->setFixedHeight(20);
    chat_mode_btn_->setCursor(Qt::PointingHandCursor);
    chat_mode_btn_->setToolTip("Switch to Chat Mode (F9)");
    chat_mode_btn_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(chat_mode_btn_, &QPushButton::clicked, this, &ToolBar::chat_mode_toggled);
    hl->addWidget(chat_mode_btn_);
    sep();

    logout_btn_ = new QPushButton("LOGOUT");
    logout_btn_->setFixedHeight(20);
    logout_btn_->setCursor(Qt::PointingHandCursor);
    logout_btn_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(logout_btn_, &QPushButton::clicked, this, &ToolBar::logout_clicked);
    hl->addWidget(logout_btn_);

    clock_timer_ = new QTimer(this);
    clock_timer_->setInterval(1000);
    connect(clock_timer_, &QTimer::timeout, this, &ToolBar::update_clock);
    clock_timer_->start();
    update_clock();

    connect(&auth::AuthManager::instance(), &auth::AuthManager::auth_state_changed, this,
            &ToolBar::refresh_user_display);

    connect(&ThemeManager::instance(), &ThemeManager::theme_changed,
            this, [this](const ThemeTokens&) { refresh_theme(); });

    refresh_user_display();
    refresh_theme();
}

void ToolBar::refresh_theme() {
    // Bar background
    setStyleSheet(QString("background:%1;border-bottom:1px solid %2;")
                      .arg(colors::BG_BASE).arg(colors::BORDER_DIM));
    // Menu bar + popup menus
    if (menu_bar_) {
        menu_bar_->setStyleSheet(menu_ss());
        for (auto* action : menu_bar_->actions())
            if (action->menu()) action->menu()->setStyleSheet(popup_ss());
    }
    // Separators
    for (auto* s : separators_)
        s->setStyleSheet(QString("color:%1;background:transparent;padding:0 3px;").arg(colors::TEXT_DIM));
    // Branding labels
    auto lbl = [](QLabel* l, const QString& c, bool b = false) {
        if (l) l->setStyleSheet(QString("color:%1;%2background:transparent;").arg(c, b ? "font-weight:700;" : ""));
    };
    lbl(fincept_label_, colors::AMBER(), true);
    lbl(branding_label_, colors::TEXT_PRIMARY(), true);
    lbl(subtitle_label_, colors::TEXT_SECONDARY());
    lbl(live_dot_, colors::POSITIVE());
    lbl(live_label_, colors::POSITIVE(), true);
    lbl(clock_label_, colors::TEXT_PRIMARY());
    lbl(user_label_, colors::AMBER());
    lbl(credits_label_, colors::POSITIVE());
    // Buttons
    if (plan_btn_)
        plan_btn_->setStyleSheet(QString("QPushButton{color:%1;background:transparent;border:none;padding:0 2px;}"
                                         "QPushButton:hover{color:%2;}")
                                     .arg(colors::TEXT_PRIMARY).arg(colors::AMBER));
    if (chat_mode_btn_)
        chat_mode_btn_->setStyleSheet(
            QString("QPushButton{background:transparent;color:%1;border:1px solid %2;"
                    "padding:0 8px;font-weight:700;}"
                    "QPushButton:hover{background:%2;color:%3;border-color:%1;}")
                .arg(colors::AMBER).arg(colors::AMBER_DIM).arg(colors::TEXT_PRIMARY));
    if (logout_btn_)
        logout_btn_->setStyleSheet(
            QString("QPushButton{background:transparent;color:%1;border:1px solid %1;"
                    "padding:0 8px;font-weight:700;}"
                    "QPushButton:hover{background:%1;color:%2;border-color:%1;}")
                .arg(colors::NEGATIVE).arg(colors::TEXT_PRIMARY));
}

void ToolBar::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    apply_responsive_layout(e->size().width());
}

void ToolBar::apply_responsive_layout(int w) {
    // Progressive disclosure: hide less-critical elements as width shrinks
    // >= 1200: everything visible
    // >= 1000: hide subtitle
    //  >= 800: also hide clock, LIVE label
    //  >= 650: also hide credits, chat button
    //  <  650: minimal — menus + branding + user + plan + logout

    bool show_subtitle = (w >= 1200);
    bool show_clock    = (w >= 800);
    bool show_live     = (w >= 800);
    bool show_credits  = (w >= 650);
    bool show_chat     = (w >= 650);

    if (subtitle_label_) subtitle_label_->setVisible(show_subtitle);
    if (clock_label_)    clock_label_->setVisible(show_clock);
    if (live_dot_)       live_dot_->setVisible(show_live);
    if (live_label_)     live_label_->setVisible(show_live);
    if (credits_label_)  credits_label_->setVisible(show_credits);
    if (chat_mode_btn_)  chat_mode_btn_->setVisible(show_chat);

    // Hide separators adjacent to hidden widgets — check each separator's neighbors
    // Simple approach: hide seps 3 (after credits) and 4 (after plan/before chat)
    // when their adjacent content is hidden
    if (separators_.size() >= 5) {
        separators_[2]->setVisible(show_credits); // sep before credits
        separators_[3]->setVisible(show_chat);    // sep before chat
    }
}

void ToolBar::update_clock() {
    auto dt = QDateTime::currentDateTime();
    clock_label_->setText(dt.toString("dd MMM yy").toUpper() + " " + dt.toString("HH:mm:ss"));
}

void ToolBar::refresh_user_display() {
    const auto& s = auth::AuthManager::instance().session();
    if (!s.authenticated) {
        user_label_->setText("---");
        credits_label_->setText("---");
        plan_btn_->setText("---");
        return;
    }

    // Elide username/email to fit within maxWidth
    QString name = s.user_info.username.isEmpty() ? s.user_info.email : s.user_info.username;
    QFontMetrics fm(user_label_->font());
    user_label_->setText(fm.elidedText(name, Qt::ElideRight, user_label_->maximumWidth() - 4));
    user_label_->setToolTip(name);

    // Credits — compact format
    int credits = static_cast<int>(s.user_info.credit_balance);
    credits_label_->setText(QString("%1 CR").arg(credits));
    credits_label_->setStyleSheet(QString("color:%1;background:transparent;")
                                      .arg(s.user_info.credit_balance > 0
                                               ? colors::POSITIVE.get()
                                               : colors::NEGATIVE.get()));

    QString plan_text = s.account_type().toUpper();
    plan_btn_->setText(plan_text.isEmpty() ? "FREE" : plan_text);
}

QMenu* ToolBar::build_file_menu() {
    auto* m = new QMenu("File", this);
    m->setStyleSheet(popup_ss());
    m->addAction("New Window",        this, [this]() { emit action_triggered("new_window"); });
    m->addSeparator();
    m->addAction("New Workspace",     this, [this]() { emit action_triggered("new_workspace"); });
    m->addAction("Open Workspace",    this, [this]() { emit action_triggered("open_workspace"); });
    m->addAction("Save Workspace",    this, [this]() { emit action_triggered("save_workspace"); });
    m->addAction("Save Workspace As", this, [this]() { emit action_triggered("save_workspace_as"); });
    m->addSeparator();
    m->addAction("Import Workspace", this, [this]() { emit action_triggered("import_data"); });
    m->addAction("Export Workspace", this, [this]() { emit action_triggered("export_data"); });
    m->addSeparator();
    m->addAction("File Manager", this, [this]() { emit navigate_to("file_manager"); });
    m->addSeparator();
    m->addAction("Refresh All", this, [this]() { emit action_triggered("refresh"); });
    return m;
}

QMenu* ToolBar::build_navigate_menu() {
    auto* m = new QMenu("Navigate", this);
    m->setStyleSheet(QString(
        "QMenu{background:%1;color:%2;border:1px solid %3;padding:2px 0;}"
        "QMenu::item{padding:3px 20px 3px 10px;}"
        "QMenu::item:selected{background:%4;}"
        "QMenu::item:disabled{color:%5;font-weight:700;padding:6px 10px 2px 10px;}"
        "QMenu::separator{background:%3;height:1px;margin:2px 6px;}"
        "QMenu::scroller{background:%1;height:14px;}"
        "QMenu::scroller:hover{background:%4;}")
        .arg(colors::BG_SURFACE)
        .arg(colors::TEXT_PRIMARY)
        .arg(colors::BORDER_DIM)
        .arg(colors::BG_RAISED)
        .arg(colors::AMBER));

    // Use submenus for each group — cleaner and no scroll needed
    auto add_sub = [&](const QString& title) -> QMenu* {
        auto* sub = m->addMenu(title);
        sub->setStyleSheet(m->styleSheet());
        return sub;
    };

    auto nav = [this](QMenu* menu, const QString& label, const QString& id) {
        menu->addAction(label, this, [this, id]() { emit navigate_to(id); });
    };

    // Markets & Data
    auto* mkt = add_sub("Markets & Data");
    nav(mkt, "Economics", "economics");
    nav(mkt, "GOVT Data", "gov_data");
    nav(mkt, "DBnomics", "dbnomics");
    nav(mkt, "AKShare Data", "akshare");
    nav(mkt, "Asia Markets", "asia_markets");
    nav(mkt, "Relationship Map", "relationship_map");

    // Trading & Portfolio
    auto* trd = add_sub("Trading & Portfolio");
    nav(trd, "Equity Trading", "equity_trading");
    nav(trd, "Alpha Arena", "alpha_arena");
    nav(trd, "Polymarket", "polymarket");
    nav(trd, "Derivatives", "derivatives");
    nav(trd, "Watchlist", "watchlist");

    // Research & Intelligence
    auto* res = add_sub("Research & Intelligence");
    nav(res, "Equity Research", "equity_research");
    nav(res, "M&A Analytics", "ma_analytics");
    nav(res, "Alt. Investments", "alt_investments");
    nav(res, "Geopolitics", "geopolitics");
    nav(res, "Maritime", "maritime");
    nav(res, "Surface Analytics", "surface_analytics");

    // Tools
    auto* tools = add_sub("Tools");
    nav(tools, "Agent Config", "agent_config");
    nav(tools, "MCP Servers", "mcp_servers");
    nav(tools, "Data Mapping", "data_mapping");
    nav(tools, "Data Sources", "data_sources");
    nav(tools, "Report Builder", "report_builder");
    nav(tools, "Excel", "excel");
    nav(tools, "Trade Viz", "trade_viz");
    nav(tools, "File Manager", "file_manager");
    nav(tools, "Notes", "notes");

    m->addSeparator();

    // Direct items (no submenu)
    nav(m, "Forum", "forum");
    nav(m, "Docs", "docs");
    nav(m, "Support", "support");
    nav(m, "About", "about");

    return m;
}

QMenu* ToolBar::build_view_menu() {
    auto* m = new QMenu("View", this);
    m->setStyleSheet(popup_ss());
    m->addAction("Fullscreen\tF11", this, [this]() { emit action_triggered("fullscreen"); });
    m->addSeparator();
    m->addAction("Focus Mode\tF10", this, [this]() { emit action_triggered("focus_mode"); });
    m->addAction("Always on Top", this, [this]() { emit action_triggered("always_on_top"); });
    m->addSeparator();

    // Float any screen as a separate window on another monitor
    auto* panels = m->addMenu("Float Panel");
    panels->setStyleSheet(popup_ss());
    panels->addAction("Dashboard",       this, [this]() { emit action_triggered("panel_dashboard"); });
    panels->addAction("Watchlist",       this, [this]() { emit action_triggered("panel_watchlist"); });
    panels->addAction("News Feed",       this, [this]() { emit action_triggered("panel_news"); });
    panels->addAction("Portfolio",       this, [this]() { emit action_triggered("panel_portfolio"); });
    panels->addAction("Markets",         this, [this]() { emit action_triggered("panel_markets"); });
    panels->addSeparator();
    panels->addAction("Crypto Trading",  this, [this]() { emit action_triggered("panel_crypto"); });
    panels->addAction("Equity Trading",  this, [this]() { emit action_triggered("panel_equity"); });
    panels->addAction("Algo Trading",    this, [this]() { emit action_triggered("panel_algo"); });
    panels->addSeparator();
    panels->addAction("Equity Research", this, [this]() { emit action_triggered("panel_research"); });
    panels->addAction("Economics",       this, [this]() { emit action_triggered("panel_economics"); });
    panels->addAction("Geopolitics",     this, [this]() { emit action_triggered("panel_geopolitics"); });
    panels->addAction("AI Chat",         this, [this]() { emit action_triggered("panel_ai_chat"); });
    m->addSeparator();

    // Quick Switch — jump to a preset screen layout
    auto* persp = m->addMenu("Quick Switch");
    persp->setStyleSheet(popup_ss());
    persp->addAction("Save Workspace", this, [this]() { emit action_triggered("perspective_save"); });
    persp->addSeparator();

    // Trading
    auto* qs_trading = persp->addMenu("Trading");
    qs_trading->setStyleSheet(popup_ss());
    qs_trading->addAction("Crypto Trading",  this, [this]() { emit action_triggered("perspective_trading"); });
    qs_trading->addAction("Equity Trading",  this, [this]() { emit action_triggered("perspective_equity"); });
    qs_trading->addAction("Algo Trading",    this, [this]() { emit action_triggered("perspective_algo"); });

    // Research
    auto* qs_research = persp->addMenu("Research");
    qs_research->setStyleSheet(popup_ss());
    qs_research->addAction("Equity Research", this, [this]() { emit action_triggered("perspective_research"); });
    qs_research->addAction("Derivatives",     this, [this]() { emit action_triggered("perspective_derivatives"); });
    qs_research->addAction("M&&A Analytics",  this, [this]() { emit action_triggered("perspective_ma"); });

    // Portfolio & Markets
    persp->addAction("Portfolio View",  this, [this]() { emit action_triggered("perspective_portfolio"); });
    persp->addAction("Markets View",    this, [this]() { emit action_triggered("perspective_markets"); });
    persp->addAction("News View",       this, [this]() { emit action_triggered("perspective_news"); });

    // Economics & Data
    auto* qs_econ = persp->addMenu("Economics && Data");
    qs_econ->setStyleSheet(popup_ss());
    qs_econ->addAction("Economics",    this, [this]() { emit action_triggered("perspective_economics"); });
    qs_econ->addAction("Data Sources", this, [this]() { emit action_triggered("perspective_data"); });

    // Geopolitics
    persp->addAction("Geopolitics View", this, [this]() { emit action_triggered("perspective_geopolitics"); });

    // AI & Quant
    auto* qs_ai = persp->addMenu("AI && Quant");
    qs_ai->setStyleSheet(popup_ss());
    qs_ai->addAction("Quant Lab",  this, [this]() { emit action_triggered("perspective_quant"); });
    qs_ai->addAction("AI Chat",    this, [this]() { emit action_triggered("perspective_ai"); });

    // Tools
    persp->addAction("Tools View", this, [this]() { emit action_triggered("perspective_tools"); });
    m->addSeparator();

    m->addAction("Refresh Screen\tF5", this, [this]() { emit action_triggered("refresh"); });
    m->addAction("Take Screenshot\tCtrl+P", this, [this]() { emit action_triggered("screenshot"); });
    return m;
}

QMenu* ToolBar::build_help_menu() {
    auto* m = new QMenu("Help", this);
    m->setStyleSheet(popup_ss());
    m->addAction("About Fincept", this, [this]() { emit navigate_to("about"); });
    m->addAction("Help Center", this, [this]() { emit navigate_to("help"); });
    m->addSeparator();
    m->addAction("Contact Us", this, [this]() { emit navigate_to("contact"); });
    m->addAction("Terms of Service", this, [this]() { emit navigate_to("terms"); });
    m->addAction("Privacy Policy", this, [this]() { emit navigate_to("privacy"); });
    m->addAction("Trademarks", this, [this]() { emit navigate_to("trademarks"); });
    m->addSeparator();
    m->addAction("Check for Updates", this, [this]() { emit action_triggered("check_updates"); });
    m->addSeparator();
    m->addAction("Logout", this, [this]() { emit action_triggered("logout"); });
    return m;
}

} // namespace fincept::ui
