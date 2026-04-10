// src/screens/gov_data/GovDataScreen.cpp
#include "screens/gov_data/GovDataScreen.h"

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "screens/gov_data/GovDataAustraliaPanel.h"
#include "screens/gov_data/GovDataCongressPanel.h"
#include "screens/gov_data/GovDataFrancePanel.h"
#include "screens/gov_data/GovDataHKPanel.h"
#include "screens/gov_data/GovDataProviderPanel.h"
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

    const QString bg = QString::fromLatin1(t.bg_base);
    const QString text = QString::fromLatin1(t.text_primary);
    const QString raised = QString::fromLatin1(t.bg_raised);
    const QString bdim = QString::fromLatin1(t.border_dim);
    const QString surface = QString::fromLatin1(t.bg_surface);
    const QString sec = QString::fromLatin1(t.text_secondary);
    const QString hover = QString::fromLatin1(t.bg_hover);
    const QString accent = QString::fromLatin1(t.accent);
    const QString dim = QString::fromLatin1(t.text_dim);

    QString s;
    s += QString("#govScreen { background:%1; }").arg(bg);

    // Toolbar
    s += QString("#govToolbar { background:%1; border-bottom:1px solid %2; }").arg(raised, bdim);
    s += QString("#govToolbarTitle { color:%1; font-size:12px; font-weight:700;"
                 "  letter-spacing:1px; background:transparent; }")
             .arg(text);
    s += QString("#govToolbarSub { color:%1; font-size:10px; background:transparent; }").arg(sec);

    // Sidebar
    s += QString("#govSidebar { background:%1; border-right:1px solid %2; }").arg(surface, bdim);
    s += QString("#govSidebarHeader { background:%1; border-bottom:1px solid %2; }").arg(raised, bdim);
    s += QString("#govSidebarTitle { color:%1; font-size:9px; font-weight:700;"
                 "  letter-spacing:1px; background:transparent; }")
             .arg(sec);
    s += "#govProviderList { background:transparent; border:none; outline:none; }";
    s += QString("#govProviderList::item { color:%1; padding:7px 14px;"
                 "  border-bottom:1px solid %2; font-size:11px; }")
             .arg(sec, bdim);
    s += QString("#govProviderList::item:hover { color:%1; background:%2; }").arg(text, hover);
    s += QString("#govProviderList::item:selected { color:%1; background:rgba(%2,0.08);"
                 "  border-left:2px solid %1; padding-left:12px; font-weight:700; }")
             .arg(accent, ar);

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
    build_ui();

    connect(&ThemeManager::instance(), &ThemeManager::theme_changed, this, &GovDataScreen::refresh_theme);
    refresh_theme();
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
    auto* content = new QWidget(this);
    auto* content_hl = new QHBoxLayout(content);
    content_hl->setContentsMargins(0, 0, 0, 0);
    content_hl->setSpacing(0);

    content_hl->addWidget(build_sidebar());

    panel_stack_ = new QStackedWidget;
    const auto& providers = services::GovDataService::providers();
    for (int i = 0; i < providers.size(); ++i) {
        const auto& prov = providers[i];
        QWidget* panel = nullptr;

        if (prov.id == "us-treasury") {
            panel = new GovDataTreasuryPanel(panel_stack_);
        } else if (prov.id == "us-congress") {
            panel = new GovDataCongressPanel(panel_stack_);
        } else if (prov.id == "canada-gov") {
            panel = new GovDataProviderPanel(prov.script, prov.color, "Publishers", {}, panel_stack_);
        } else if (prov.id == "swiss") {
            GovProviderOptions swiss_opts;
            swiss_opts.watermark_text = "\xF0\x9F\x87\xA8\xF0\x9F\x87\xAD opendata.swiss";
            panel = new GovDataProviderPanel(prov.script, prov.color, "Publishers", swiss_opts, panel_stack_);
        } else if (prov.id == "france") {
            panel = new GovDataFrancePanel(panel_stack_);
        } else if (prov.id == "hk") {
            panel = new GovDataHKPanel(panel_stack_);
        } else if (prov.id == "universal-ckan") {
            GovProviderOptions ckan_opts;
            ckan_opts.portal_combo_items = {"data.gov.uk", "open.canada.ca", "data.gov.au",
                                            "data.gov.hk", "opendata.swiss", "data.gouv.fr",
                                            "data.gov", "openafrica.net"};
            ckan_opts.portal_combo_tooltip =
                "This panel uses datagovuk_api.py which queries data.gov.uk.\n"
                "The selector shows all CKAN portals covered by the universal provider.";
            panel = new GovDataProviderPanel(prov.script, prov.color, "Publishers", ckan_opts, panel_stack_);
        } else if (prov.id == "australia") {
            panel = new GovDataAustraliaPanel(panel_stack_);
        } else {
            // openafrica, spain — still use generic until dedicated scripts exist
            panel = new GovDataProviderPanel(prov.script, prov.color, "Organizations", {}, panel_stack_);
        }
        panel_stack_->addWidget(panel);
    }

    content_hl->addWidget(panel_stack_, 1);
    root->addWidget(content, 1);
    root->addWidget(build_status_bar());
}

// ── Toolbar ──────────────────────────────────────────────────────────────────

QWidget* GovDataScreen::build_toolbar() {
    header_bar_ = new QWidget(this);
    header_bar_->setObjectName("govToolbar");
    header_bar_->setFixedHeight(38);

    auto* hl = new QHBoxLayout(header_bar_);
    hl->setContentsMargins(14, 0, 14, 0);
    hl->setSpacing(8);

    header_title_ = new QLabel("GOVERNMENT DATA EXPLORER");
    header_title_->setObjectName("govToolbarTitle");
    header_title_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    hl->addWidget(header_title_);

    // Separator dot
    auto* sep = new QLabel("·");
    sep->setObjectName("govToolbarSub");
    sep->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    hl->addWidget(sep);

    header_subtitle_ = new QLabel(
        QString("Open government portals · %1 sovereign sources").arg(services::GovDataService::providers().size()));
    header_subtitle_->setObjectName("govToolbarSub");
    header_subtitle_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    hl->addWidget(header_subtitle_);

    hl->addStretch(1);

    // Source count badge — compact pill, stored as member for refresh_theme()
    const auto& providers = services::GovDataService::providers();
    provider_badge_ = new QLabel(QString::number(providers.size()) + " PORTALS");
    provider_badge_->setFixedHeight(20);
    provider_badge_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    hl->addWidget(provider_badge_);

    return header_bar_;
}

// ── Sidebar ──────────────────────────────────────────────────────────────────

QWidget* GovDataScreen::build_sidebar() {
    auto* sidebar = new QWidget(this);
    sidebar->setObjectName("govSidebar");
    sidebar->setFixedWidth(230);

    auto* vl = new QVBoxLayout(sidebar);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Section header
    auto* hdr = new QWidget(this);
    hdr->setObjectName("govSidebarHeader");
    hdr->setFixedHeight(26);
    auto* hhl = new QHBoxLayout(hdr);
    hhl->setContentsMargins(12, 0, 8, 0);
    hhl->setSpacing(6);
    auto* htitle = new QLabel("SOVEREIGN PORTALS");
    htitle->setObjectName("govSidebarTitle");
    const auto& providers = services::GovDataService::providers();
    sidebar_count_ = new QLabel(QString::number(providers.size()));
    sidebar_count_->setFixedHeight(16);
    sidebar_count_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    hhl->addWidget(htitle);
    hhl->addStretch(1);
    hhl->addWidget(sidebar_count_);
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

    connect(provider_list_, &QListWidget::currentRowChanged, this, &GovDataScreen::on_provider_selected);
    vl->addWidget(provider_list_, 1);

    return sidebar;
}

// ── Status bar ───────────────────────────────────────────────────────────────

QWidget* GovDataScreen::build_status_bar() {
    auto* bar = new QWidget(this);
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

// ── Theme refresh ────────────────────────────────────────────────────────────

void GovDataScreen::refresh_theme() {
    setStyleSheet(build_screen_style());

    const auto& t = ThemeManager::instance().tokens();
    const QString accent_color = active_provider_color_.isEmpty()
                                     ? QString::fromLatin1(t.accent)
                                     : active_provider_color_;
    const auto ac = QColor(accent_color);
    const QString ar = QString("%1,%2,%3").arg(ac.red()).arg(ac.green()).arg(ac.blue());

    // Re-style provider count badge (top-right pill)
    if (provider_badge_) {
        provider_badge_->setStyleSheet(
            QString("color:%1; background:rgba(%2,0.1); border:1px solid rgba(%2,0.3);"
                    " font-size:9px; font-weight:700; padding:2px 8px; letter-spacing:0.5px;")
                .arg(accent_color, ar));
    }

    // Re-style sidebar count pill
    if (sidebar_count_) {
        const auto& tk = ThemeManager::instance().tokens();
        const auto ac2 = QColor(tk.accent);
        const QString ar2 = QString("%1,%2,%3").arg(ac2.red()).arg(ac2.green()).arg(ac2.blue());
        sidebar_count_->setStyleSheet(
            QString("color:%1; background:rgba(%2,0.12); font-size:9px;"
                    " font-weight:700; padding:1px 5px;")
                .arg(tk.accent, ar2));
    }

    // Re-apply provider-color accent on toolbar border + subtitle
    if (!active_provider_color_.isEmpty() && header_bar_ && header_subtitle_) {
        header_bar_->setStyleSheet(
            QString("#govToolbar { background:%1; border-bottom:2px solid %2; }")
                .arg(t.bg_raised, active_provider_color_));
        header_subtitle_->setStyleSheet(
            QString("color:%1; font-size:10px; background:transparent;").arg(active_provider_color_));
    }
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
    if (index < 0 || index >= providers.size())
        return;

    active_index_ = index;
    const auto& prov = providers[index];

    // Store provider accent color so refresh_theme() can apply it
    active_provider_color_ = prov.color;

    // Update toolbar subtitle text
    header_subtitle_->setText(QString("%1  %2").arg(prov.flag, prov.full_name));

    // Status bar
    if (status_portal_)
        status_portal_->setText(prov.name);
    if (status_country_)
        status_country_->setText(prov.country);

    // Switch panel
    panel_stack_->setCurrentIndex(index);

    // Re-apply theme so provider-color accents on toolbar/badge reflect new selection
    refresh_theme();

    // Trigger initial load — all panel types expose load_initial_data() as a public slot
    QMetaObject::invokeMethod(panel_stack_->currentWidget(), "load_initial_data");

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
