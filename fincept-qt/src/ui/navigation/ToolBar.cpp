#include "ui/navigation/ToolBar.h"

#include "auth/AuthManager.h"

#include <QAction>
#include <QDateTime>
#include <QHBoxLayout>
#include <QPushButton>

namespace fincept::ui {

static const char* MENU_SS = "QMenuBar{background:transparent;color:#808080;border:none;font-size:13px;"
                             "font-family:'Consolas',monospace;spacing:0;}"
                             "QMenuBar::item{background:transparent;padding:4px 10px;}"
                             "QMenuBar::item:selected{background:#1a1a1a;color:#e5e5e5;}";

static const char* POPUP_SS = "QMenu{background:#0a0a0a;color:#e5e5e5;border:1px solid #1a1a1a;"
                              "font-size:13px;font-family:'Consolas',monospace;padding:4px 0;}"
                              "QMenu::item{padding:5px 24px 5px 12px;}"
                              "QMenu::item:selected{background:#1a1a1a;}"
                              "QMenu::item:disabled{color:#404040;}"
                              "QMenu::separator{background:#1a1a1a;height:1px;margin:4px 8px;}";

static const char* MF = "font-family:'Consolas',monospace;";

ToolBar::ToolBar(QWidget* parent) : QWidget(parent) {
    setFixedHeight(32);
    setStyleSheet("background:#0a0a0a;border-bottom:1px solid #1a1a1a;");
    auto* hl = new QHBoxLayout(this);
    hl->setContentsMargins(4, 0, 10, 0);
    hl->setSpacing(0);

    // Menus
    menu_bar_ = new QMenuBar(this);
    menu_bar_->setStyleSheet(MENU_SS);
    menu_bar_->addMenu(build_file_menu());
    menu_bar_->addMenu(build_navigate_menu());
    menu_bar_->addMenu(build_view_menu());
    menu_bar_->addMenu(build_help_menu());
    hl->addWidget(menu_bar_);

    auto sep = [&]() {
        auto* s = new QLabel("|");
        s->setStyleSheet("color:#1a1a1a;font-size:13px;background:transparent;padding:0 6px;");
        hl->addWidget(s);
    };

    auto mk = [](const QString& t, const QString& c, int sz = 13, bool b = false) -> QLabel* {
        auto* l = new QLabel(t);
        l->setStyleSheet(QString("color:%1;font-size:%2px;%3background:transparent;%4")
                             .arg(c)
                             .arg(sz)
                             .arg(b ? "font-weight:700;" : "")
                             .arg(MF));
        return l;
    };

    sep();
    hl->addWidget(mk("FINCEPT", "#d97706", 14, true));
    hl->addWidget(mk(" TERMINAL", "#404040", 12));
    hl->addWidget(mk("  ", "#000"));
    hl->addWidget(mk("\xe2\x97\x8f", "#16a34a", 9));
    hl->addWidget(mk(" LIVE", "#16a34a", 11, true));

    hl->addStretch();
    clock_label_ = mk("", "#404040", 12);
    hl->addWidget(clock_label_);
    hl->addStretch();

    user_label_ = mk("---", "#d97706", 12);
    hl->addWidget(user_label_);
    sep();
    credits_label_ = mk("---", "#16a34a", 12);
    hl->addWidget(credits_label_);
    sep();
    plan_btn_ = new QPushButton("---");
    plan_btn_->setCursor(Qt::PointingHandCursor);
    plan_btn_->setToolTip("View Plans & Pricing");
    plan_btn_->setStyleSheet("QPushButton{color:#525252;font-size:12px;background:transparent;border:none;"
                             "font-family:'Consolas',monospace;padding:0 2px;}"
                             "QPushButton:hover{color:#d97706;}");
    connect(plan_btn_, &QPushButton::clicked, this, &ToolBar::plan_clicked);
    hl->addWidget(plan_btn_);
    sep();

    auto* logout_btn = new QPushButton("LOGOUT");
    logout_btn->setFixedHeight(20);
    logout_btn->setCursor(Qt::PointingHandCursor);
    logout_btn->setStyleSheet("QPushButton{background:transparent;color:#dc2626;border:1px solid #7f1d1d;"
                              "padding:0 8px;font-size:11px;font-weight:700;font-family:'Consolas',monospace;}"
                              "QPushButton:hover{background:#dc2626;color:#e5e5e5;border-color:#dc2626;}");
    connect(logout_btn, &QPushButton::clicked, this, &ToolBar::logout_clicked);
    hl->addWidget(logout_btn);

    clock_timer_ = new QTimer(this);
    clock_timer_->setInterval(1000);
    connect(clock_timer_, &QTimer::timeout, this, &ToolBar::update_clock);
    clock_timer_->start();
    update_clock();

    connect(&auth::AuthManager::instance(), &auth::AuthManager::auth_state_changed, this,
            &ToolBar::refresh_user_display);
    refresh_user_display();
}

void ToolBar::update_clock() {
    clock_label_->setText(QDateTime::currentDateTime().toString("yyyy-MM-dd  HH:mm:ss"));
}

void ToolBar::refresh_user_display() {
    const auto& s = auth::AuthManager::instance().session();
    if (!s.authenticated) {
        user_label_->setText("---");
        credits_label_->setText("---");
        plan_btn_->setText("---");
        return;
    }
    user_label_->setText(s.user_info.username.isEmpty() ? s.user_info.email : s.user_info.username);
    credits_label_->setText(QString("%1 CR").arg(s.user_info.credit_balance, 0, 'f', 2));
    credits_label_->setStyleSheet(QString("color:%1;font-size:12px;background:transparent;%2")
                                      .arg(s.user_info.credit_balance > 0 ? "#16a34a" : "#dc2626")
                                      .arg(MF));

    QString plan_text = s.user_info.account_type.toUpper();
    if (!plan_text.isEmpty())
        plan_text += " Plan";
    plan_btn_->setText(plan_text.isEmpty() ? "FREE" : plan_text);
}

QMenu* ToolBar::build_file_menu() {
    auto* m = new QMenu("File", this);
    m->setStyleSheet(POPUP_SS);
    m->addAction("New Workspace", this, [this]() { emit action_triggered("new_workspace"); });
    m->addAction("Open Workspace", this, [this]() { emit action_triggered("open_workspace"); });
    m->addAction("Save Workspace", this, [this]() { emit action_triggered("save_workspace"); });
    m->addSeparator();
    m->addAction("Import Data", this, [this]() { emit action_triggered("import_data"); });
    m->addAction("Export Data", this, [this]() { emit action_triggered("export_data"); });
    m->addSeparator();
    m->addAction("Refresh All", this, [this]() { emit action_triggered("refresh"); });
    return m;
}

QMenu* ToolBar::build_navigate_menu() {
    auto* m = new QMenu("Navigate", this);
    m->setStyleSheet("QMenu{background:#0a0a0a;color:#e5e5e5;border:1px solid #1a1a1a;"
                     "font-size:13px;font-family:'Consolas',monospace;padding:2px 0;}"
                     "QMenu::item{padding:3px 20px 3px 10px;}"
                     "QMenu::item:selected{background:#1a1a1a;}"
                     "QMenu::item:disabled{color:#d97706;font-weight:700;padding:6px 10px 2px 10px;}"
                     "QMenu::separator{background:#1a1a1a;height:1px;margin:2px 6px;}"
                     "QMenu::scroller{background:#0a0a0a;height:14px;}"
                     "QMenu::scroller:hover{background:#1a1a1a;}");

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
    m->setStyleSheet(POPUP_SS);
    m->addAction("Fullscreen\tF11", this, [this]() { emit action_triggered("fullscreen"); });
    m->addSeparator();
    m->addAction("Focus Mode\tF10", this, [this]() { emit action_triggered("focus_mode"); });
    m->addAction("Always on Top", this, [this]() { emit action_triggered("always_on_top"); });
    m->addSeparator();
    m->addAction("Refresh Screen\tF5", this, [this]() { emit action_triggered("refresh"); });
    m->addAction("Take Screenshot\tCtrl+P", this, [this]() { emit action_triggered("screenshot"); });
    return m;
}

QMenu* ToolBar::build_help_menu() {
    auto* m = new QMenu("Help", this);
    m->setStyleSheet(POPUP_SS);
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
