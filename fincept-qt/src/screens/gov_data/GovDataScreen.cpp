// src/screens/gov_data/GovDataScreen.cpp
#include "screens/gov_data/GovDataScreen.h"

#include "core/logging/Logger.h"
#include "screens/gov_data/GovDataCongressPanel.h"
#include "screens/gov_data/GovDataProviderPanel.h"
#include "screens/gov_data/GovDataTreasuryPanel.h"
#include "services/gov_data/GovDataService.h"
#include "ui/theme/Theme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QSplitter>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace fincept::screens {

using namespace fincept::ui;

// ── Shared stylesheet ────────────────────────────────────────────────────────

static const char* kScreenStyle =
    "#govScreen { background:#080808; }"

    // Toolbar
    "#govToolbar { background:#111111; border-bottom:1px solid #1a1a1a; }"
    "#govToolbarTitle { color:#e5e5e5; font-size:13px; font-weight:700;"
    "  letter-spacing:1px; background:transparent; }"
    "#govToolbarSub { color:#808080; font-size:10px; background:transparent; }"

    // Sidebar
    "#govSidebar { background:#0a0a0a; border-right:1px solid #1a1a1a; }"
    "#govSidebarHeader { background:#111111; border-bottom:1px solid #1a1a1a; }"
    "#govSidebarTitle { color:#808080; font-size:9px; font-weight:700;"
    "  letter-spacing:1px; background:transparent; }"
    "#govProviderList { background:transparent; border:none; outline:none; }"
    "#govProviderList::item { color:#808080; padding:10px 14px;"
    "  border-bottom:1px solid #1a1a1a; font-size:11px; }"
    "#govProviderList::item:hover { color:#e5e5e5; background:#161616; }"
    "#govProviderList::item:selected { color:#d97706; background:rgba(217,119,6,0.08);"
    "  border-left:2px solid #d97706; padding-left:12px; font-weight:700; }"

    // Status bar
    "#govStatusBar { background:#111111; border-top:1px solid #1a1a1a; }"
    "#govStatusText { color:#404040; font-size:9px; background:transparent; }"
    "#govStatusSep  { color:#1a1a1a; font-size:9px; background:transparent; }"
    "#govStatusVal  { color:#808080; font-size:9px; background:transparent; }"
    "#govStatusHigh { color:#d97706; font-size:9px; font-weight:700; background:transparent; }"

    // Scrollbar
    "QScrollBar:vertical { background:#080808; width:5px; }"
    "QScrollBar::handle:vertical { background:#1a1a1a; min-height:20px; }"
    "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }";

// ── Constructor ──────────────────────────────────────────────────────────────

GovDataScreen::GovDataScreen(QWidget* parent) : QWidget(parent) {
    setObjectName("govScreen");
    setStyleSheet(kScreenStyle);
    build_ui();
}

// ── Show / Hide ──────────────────────────────────────────────────────────────

void GovDataScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (!initial_load_done_) {
        initial_load_done_ = true;
        if (provider_list_->count() > 0) {
            provider_list_->setCurrentRow(0);
            activate_provider(0);
        }
    }
    LOG_INFO("GovDataScreen", "shown");
}

void GovDataScreen::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
}

// ── Build UI ─────────────────────────────────────────────────────────────────

void GovDataScreen::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    root->addWidget(build_toolbar());

    // Content: sidebar | panel stack
    auto* content    = new QWidget;
    auto* content_hl = new QHBoxLayout(content);
    content_hl->setContentsMargins(0, 0, 0, 0);
    content_hl->setSpacing(0);

    content_hl->addWidget(build_sidebar());

    panel_stack_ = new QStackedWidget;
    const auto& providers = services::GovDataService::providers();
    for (int i = 0; i < providers.size(); ++i) {
        const auto& prov = providers[i];
        QWidget* panel   = nullptr;

        if (prov.id == "us-treasury") {
            panel = new GovDataTreasuryPanel(panel_stack_);
        } else if (prov.id == "us-congress") {
            panel = new GovDataCongressPanel(panel_stack_);
        } else {
            QString org_label = "Publishers";
            if (prov.id == "openafrica" || prov.id == "universal-ckan")
                org_label = "Organizations";
            else if (prov.id == "hk")
                org_label = "Categories";
            panel = new GovDataProviderPanel(prov.script, prov.color, org_label, panel_stack_);
        }
        panel_stack_->addWidget(panel);
    }

    content_hl->addWidget(panel_stack_, 1);
    root->addWidget(content, 1);
    root->addWidget(build_status_bar());
}

// ── Toolbar ──────────────────────────────────────────────────────────────────

QWidget* GovDataScreen::build_toolbar() {
    header_bar_ = new QWidget;
    header_bar_->setObjectName("govToolbar");
    header_bar_->setFixedHeight(46);

    auto* hl = new QHBoxLayout(header_bar_);
    hl->setContentsMargins(16, 0, 16, 0);
    hl->setSpacing(10);

    // Title block
    auto* col = new QWidget;
    auto* cvl = new QVBoxLayout(col);
    cvl->setContentsMargins(0, 0, 0, 0);
    cvl->setSpacing(0);

    header_title_ = new QLabel("GOVERNMENT DATA EXPLORER");
    header_title_->setObjectName("govToolbarTitle");

    header_subtitle_ = new QLabel("Open government portals · 12 sovereign sources");
    header_subtitle_->setObjectName("govToolbarSub");

    cvl->addWidget(header_title_);
    cvl->addWidget(header_subtitle_);
    hl->addWidget(col);

    hl->addStretch(1);

    // Source count badge
    const auto& providers = services::GovDataService::providers();
    auto* badge = new QLabel(QString::number(providers.size()) + " PORTALS");
    badge->setStyleSheet(
        "color:#d97706; background:rgba(217,119,6,0.1); border:1px solid rgba(217,119,6,0.3);"
        " font-size:9px; font-weight:700; padding:3px 10px; letter-spacing:0.5px;");
    hl->addWidget(badge);

    return header_bar_;
}

// ── Sidebar ──────────────────────────────────────────────────────────────────

QWidget* GovDataScreen::build_sidebar() {
    auto* sidebar = new QWidget;
    sidebar->setObjectName("govSidebar");
    sidebar->setFixedWidth(230);

    auto* vl = new QVBoxLayout(sidebar);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Section header
    auto* hdr = new QWidget;
    hdr->setObjectName("govSidebarHeader");
    hdr->setFixedHeight(30);
    auto* hhl = new QHBoxLayout(hdr);
    hhl->setContentsMargins(14, 0, 10, 0);
    auto* htitle = new QLabel("SOVEREIGN PORTALS");
    htitle->setObjectName("govSidebarTitle");
    const auto& providers = services::GovDataService::providers();
    auto* hcount = new QLabel(QString::number(providers.size()));
    hcount->setStyleSheet(
        "background:rgba(217,119,6,0.15); color:#d97706; font-size:9px;"
        " font-weight:700; padding:1px 6px; background:transparent;");
    hcount->setStyleSheet(
        "color:#d97706; background:rgba(217,119,6,0.12); font-size:9px;"
        " font-weight:700; padding:1px 6px;");
    hhl->addWidget(htitle);
    hhl->addStretch(1);
    hhl->addWidget(hcount);
    vl->addWidget(hdr);

    // Provider list
    provider_list_ = new QListWidget;
    provider_list_->setObjectName("govProviderList");

    for (const auto& prov : providers) {
        auto* item = new QListWidgetItem(provider_list_);
        // Flag + name on first line, country on tooltip
        item->setText(QString("%1  %2").arg(prov.flag, prov.name));
        item->setToolTip(prov.description + "\n" + prov.country);
        item->setData(Qt::UserRole, prov.id);
        item->setData(Qt::UserRole + 1, prov.color);
    }

    connect(provider_list_, &QListWidget::currentRowChanged,
            this, &GovDataScreen::on_provider_selected);
    vl->addWidget(provider_list_, 1);

    return sidebar;
}

// ── Status bar ───────────────────────────────────────────────────────────────

QWidget* GovDataScreen::build_status_bar() {
    auto* bar = new QWidget;
    bar->setObjectName("govStatusBar");
    bar->setFixedHeight(24);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(14, 0, 14, 0);
    hl->setSpacing(6);

    auto sep = [&]() -> QLabel* {
        auto* l = new QLabel("·");
        l->setObjectName("govStatusSep");
        return l;
    };

    auto* lbl = new QLabel("GOVT");
    lbl->setObjectName("govStatusText");
    hl->addWidget(lbl);
    hl->addWidget(sep());

    auto* pl = new QLabel("PORTAL:");
    pl->setObjectName("govStatusText");
    status_portal_ = new QLabel("—");
    status_portal_->setObjectName("govStatusVal");
    hl->addWidget(pl);
    hl->addWidget(status_portal_);
    hl->addWidget(sep());

    auto* cl = new QLabel("COUNTRY:");
    cl->setObjectName("govStatusText");
    status_country_ = new QLabel("—");
    status_country_->setObjectName("govStatusVal");
    hl->addWidget(cl);
    hl->addWidget(status_country_);

    hl->addStretch(1);

    // Ready dot
    auto* dot = new QLabel;
    dot->setFixedSize(6, 6);
    dot->setStyleSheet("background:#16a34a; border-radius:3px;");
    hl->addWidget(dot);
    auto* ready = new QLabel("READY");
    ready->setObjectName("govStatusText");
    ready->setStyleSheet("color:#16a34a; font-size:9px; background:transparent;");
    hl->addWidget(ready);

    return bar;
}

// ── Provider activation ──────────────────────────────────────────────────────

void GovDataScreen::on_provider_selected(int row) {
    if (row >= 0) activate_provider(row);
}

void GovDataScreen::activate_provider(int index) {
    const auto& providers = services::GovDataService::providers();
    if (index < 0 || index >= providers.size()) return;

    active_index_    = index;
    const auto& prov = providers[index];

    // Update toolbar title/subtitle with provider color
    header_subtitle_->setText(QString("%1  %2").arg(prov.flag, prov.full_name));
    header_subtitle_->setStyleSheet(
        QString("color:%1; font-size:10px; background:transparent;").arg(prov.color));
    header_bar_->setStyleSheet(
        QString("#govToolbar { background:#111111; border-bottom:2px solid %1; }").arg(prov.color));

    // Status bar
    if (status_portal_)  status_portal_->setText(prov.name);
    if (status_country_) status_country_->setText(prov.country);

    // Switch panel
    panel_stack_->setCurrentIndex(index);

    // Trigger initial load
    auto* widget = panel_stack_->currentWidget();
    if (auto* ckan = qobject_cast<GovDataProviderPanel*>(widget))
        ckan->load_initial_data();
    else if (auto* treasury = qobject_cast<GovDataTreasuryPanel*>(widget))
        treasury->load_initial_data();
    else if (auto* congress = qobject_cast<GovDataCongressPanel*>(widget))
        congress->load_initial_data();

    LOG_INFO("GovDataScreen", "Activated: " + prov.id);
}

} // namespace fincept::screens
