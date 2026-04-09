// src/screens/gov_data/GovDataScreen.cpp
#include "screens/gov_data/GovDataScreen.h"

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "screens/gov_data/GovDataAustraliaPanel.h"
#include "screens/gov_data/GovDataCanadaPanel.h"
#include "screens/gov_data/GovDataProviderPanel.h"
#include "screens/gov_data/GovDataCKANPanel.h"
#include "screens/gov_data/GovDataCongressPanel.h"
#include "screens/gov_data/GovDataFrancePanel.h"
#include "screens/gov_data/GovDataHKPanel.h"
#include "screens/gov_data/GovDataSwissPanel.h"
#include "screens/gov_data/GovDataTreasuryPanel.h"
#include "screens/gov_data/GovDataUKPanel.h"
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

// ── Dynamic stylesheet ──────────────────────────────────────────────────────

static QString build_screen_style() {
    const auto& t = ThemeManager::instance().tokens();
    const auto ac = QColor(t.accent);
    const QString ar = QString("%1,%2,%3").arg(ac.red()).arg(ac.green()).arg(ac.blue());

    const QString bg      = QString::fromLatin1(t.bg_base);
    const QString text    = QString::fromLatin1(t.text_primary);
    const QString raised  = QString::fromLatin1(t.bg_raised);
    const QString bdim    = QString::fromLatin1(t.border_dim);
    const QString surface = QString::fromLatin1(t.bg_surface);
    const QString sec     = QString::fromLatin1(t.text_secondary);
    const QString hover   = QString::fromLatin1(t.bg_hover);
    const QString accent  = QString::fromLatin1(t.accent);
    const QString dim     = QString::fromLatin1(t.text_dim);

    QString s;
    s += QString("#govScreen { background:%1; }").arg(bg);

    // Toolbar
    s += QString("#govToolbar { background:%1; border-bottom:1px solid %2; }").arg(raised, bdim);
    s += QString("#govToolbarTitle { color:%1; font-size:13px; font-weight:700;"
                 "  letter-spacing:1px; background:transparent; }").arg(text);
    s += QString("#govToolbarSub { color:%1; font-size:10px; background:transparent; }").arg(sec);

    // Sidebar
    s += QString("#govSidebar { background:%1; border-right:1px solid %2; }").arg(surface, bdim);
    s += QString("#govSidebarHeader { background:%1; border-bottom:1px solid %2; }").arg(raised, bdim);
    s += QString("#govSidebarTitle { color:%1; font-size:9px; font-weight:700;"
                 "  letter-spacing:1px; background:transparent; }").arg(sec);
    s += "#govProviderList { background:transparent; border:none; outline:none; }";
    s += QString("#govProviderList::item { color:%1; padding:10px 14px;"
                 "  border-bottom:1px solid %2; font-size:11px; }").arg(sec, bdim);
    s += QString("#govProviderList::item:hover { color:%1; background:%2; }").arg(text, hover);
    s += QString("#govProviderList::item:selected { color:%1; background:rgba(%2,0.08);"
                 "  border-left:2px solid %1; padding-left:12px; font-weight:700; }").arg(accent, ar);

    // Status bar
    s += QString("#govStatusBar { background:%1; border-top:1px solid %2; }").arg(raised, bdim);
    s += QString("#govStatusText { color:%1; font-size:9px; background:transparent; }").arg(dim);
    s += QString("#govStatusSep  { color:%1; font-size:9px; background:transparent; }").arg(bdim);
    s += QString("#govStatusVal  { color:%1; font-size:9px; background:transparent; }").arg(sec);
    s += QString("#govStatusHigh { color:%1; font-size:9px; font-weight:700; background:transparent; }").arg(accent);

    // Scrollbar
    s += QString("QScrollBar:vertical { background:%1; width:5px; }").arg(bg);
    s += QString("QScrollBar::handle:vertical { background:%1; min-height:20px; }").arg(bdim);
    s += "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }";
    return s;
}

// ── Constructor ──────────────────────────────────────────────────────────────

GovDataScreen::GovDataScreen(QWidget* parent) : QWidget(parent) {
    setObjectName("govScreen");
    setStyleSheet(build_screen_style());
    build_ui();

    connect(&ThemeManager::instance(), &ThemeManager::theme_changed, this, [this]() {
        setStyleSheet(build_screen_style());
    });
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
        } else if (prov.id == "canada-gov") {
            panel = new GovDataCanadaPanel(panel_stack_);
        } else if (prov.id == "swiss") {
            panel = new GovDataSwissPanel(panel_stack_);
        } else if (prov.id == "france") {
            panel = new GovDataFrancePanel(panel_stack_);
        } else if (prov.id == "hk") {
            panel = new GovDataHKPanel(panel_stack_);
        } else if (prov.id == "universal-ckan") {
            panel = new GovDataCKANPanel(panel_stack_);
        } else if (prov.id == "australia") {
            panel = new GovDataAustraliaPanel(panel_stack_);
        } else {
            // openafrica, spain — still use generic until dedicated scripts exist
            panel = new GovDataProviderPanel(prov.script, prov.color, "Organizations", panel_stack_);
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

    header_subtitle_ = new QLabel(
        QString("Open government portals · %1 sovereign sources")
            .arg(services::GovDataService::providers().size()));
    header_subtitle_->setObjectName("govToolbarSub");

    cvl->addWidget(header_title_);
    cvl->addWidget(header_subtitle_);
    hl->addWidget(col);

    hl->addStretch(1);

    // Source count badge
    const auto& providers = services::GovDataService::providers();
    auto* badge = new QLabel(QString::number(providers.size()) + " PORTALS");
    const auto& t = ThemeManager::instance().tokens();
    auto bc = QColor(t.accent);
    badge->setStyleSheet(
        QString("color:%1; background:rgba(%2,%3,%4,0.1); border:1px solid rgba(%2,%3,%4,0.3);"
                " font-size:9px; font-weight:700; padding:3px 10px; letter-spacing:0.5px;")
        .arg(t.accent).arg(bc.red()).arg(bc.green()).arg(bc.blue()));
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
    {
        const auto& tk = ThemeManager::instance().tokens();
        auto ac2 = QColor(tk.accent);
        hcount->setStyleSheet(
            QString("color:%1; background:rgba(%2,%3,%4,0.12); font-size:9px;"
                    " font-weight:700; padding:1px 6px;")
            .arg(tk.accent).arg(ac2.red()).arg(ac2.green()).arg(ac2.blue()));
    }
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
    dot->setStyleSheet(QString("background:%1; border-radius:3px;").arg(colors::POSITIVE()));
    hl->addWidget(dot);
    auto* ready = new QLabel("READY");
    ready->setObjectName("govStatusText");
    ready->setStyleSheet(QString("color:%1; font-size:9px; background:transparent;").arg(colors::POSITIVE()));
    hl->addWidget(ready);

    return bar;
}

// ── Provider activation ──────────────────────────────────────────────────────

void GovDataScreen::on_provider_selected(int row) {
    if (row >= 0) {
        activate_provider(row);
        ScreenStateManager::instance().notify_changed(this);
    }
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
    const auto& tk = ThemeManager::instance().tokens();
    header_bar_->setStyleSheet(
        QString("#govToolbar { background:%1; border-bottom:2px solid %2; }").arg(tk.bg_raised, prov.color));

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

// ── IStatefulScreen ───────────────────────────────────────────────────────────

QVariantMap GovDataScreen::save_state() const {
    return {{"provider_index", active_index_}};
}

void GovDataScreen::restore_state(const QVariantMap& state) {
    const int idx = state.value("provider_index", -1).toInt();
    if (idx >= 0)
        activate_provider(idx);
}

} // namespace fincept::screens
