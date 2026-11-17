// src/screens/gov_data/GovDataScreen.cpp
#include "screens/gov_data/GovDataScreen.h"

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "screens/common/CommandRows.h"
#include "screens/gov_data/GovDataAustraliaPanel.h"
#include "screens/gov_data/GovDataCongressPanel.h"
#include "screens/gov_data/GovDataFrancePanel.h"
#include "screens/gov_data/GovDataHKPanel.h"
#include "screens/gov_data/GovDataProviderPanel.h"
#include "screens/gov_data/GovDataTreasuryPanel.h"
#include "screens/gov_data/GovDataUKPanel.h"
#include "services/gov_data/GovDataService.h"
#include "ui/theme/Theme.h"

#include <QCoreApplication>
#include <QEvent>
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

    const QString bg = QString::fromLatin1(t.bg_base);
    const QString text = QString::fromLatin1(t.text_primary);
    const QString raised = QString::fromLatin1(t.bg_raised);
    const QString bdim = QString::fromLatin1(t.border_dim);
    const QString surface = QString::fromLatin1(t.bg_surface);
    const QString sec = QString::fromLatin1(t.text_secondary);
    const QString hover = QString::fromLatin1(t.bg_hover);
    const QString accent = QString::fromLatin1(t.accent);
    const QString accent_bg = QString::fromLatin1(t.accent_bg);
    const QString bmed = QString::fromLatin1(t.border_med);

    QString s;
    s += QString("#govScreen { background:%1; }").arg(bg);

    // Toolbar
    s += QString("#govToolbar { background:%1; border-bottom:1px solid %2; }").arg(raised, bdim);
    s += QString("#govToolbarTitle { color:%1; font-size:" + fincept::ui::fonts::px(-2) + "px; font-weight:700;"
                 "  letter-spacing:1px; background:transparent; }")
             .arg(text);
    s += QString("#govToolbarSub { color:%1; font-size:" + fincept::ui::fonts::px(-4) + "px; background:transparent; }").arg(sec);

    // Sidebar
    s += QString("#govSidebar { background:%1; border-right:1px solid %2; }").arg(surface, bdim);
    s += QString("#govSidebarHeader { background:%1; border-bottom:1px solid %2; }").arg(raised, bdim);
    s += QString("#govSidebarTitle { color:%1; font-size:" + fincept::ui::fonts::px(-5) + "px; font-weight:700;"
                 "  letter-spacing:1px; background:transparent; }")
             .arg(sec);
    s += "#govProviderList { background:transparent; border:none; outline:none; }";
    s += QString("#govProviderList::item { color:%1; padding:7px 14px;"
                 "  border-bottom:1px solid %2; font-size:" + fincept::ui::fonts::px(-3) + "px; }")
             .arg(sec, bdim);
    s += QString("#govProviderList::item:hover { color:%1; background:%2; }").arg(text, hover);
    s += QString("#govProviderList::item:selected { color:%1; background:%2;"
                 "  border-left:2px solid %1; padding-left:12px; font-weight:700; }")
             .arg(accent, accent_bg);

    // Status bar
    s += QString("#govStatusBar { background:%1; border-top:1px solid %2; }").arg(raised, bdim);
    // The status bar is a raised surface: labels in secondary text, values in primary.
    s += QString("#govStatusText { color:%1; font-size:" + fincept::ui::fonts::px(-5) + "px; background:transparent; }").arg(sec);
    s += QString("#govStatusSep  { color:%1; font-size:" + fincept::ui::fonts::px(-5) + "px; background:transparent; }").arg(bmed);
    s += QString("#govStatusVal  { color:%1; font-size:" + fincept::ui::fonts::px(-5) + "px; background:transparent; }").arg(text);
    s += QString("#govStatusHigh { color:%1; font-size:" + fincept::ui::fonts::px(-5) + "px; font-weight:700; background:transparent; }").arg(accent);

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
    retranslateUi();
    // Styled from tokens and recorded derived colours (legible / tint): the central re-style pass
    // carries them through a theme or font change, so there is no theme_changed handler. The
    // provider accent is re-applied when another provider is activated.
    refresh_theme();
}

// ── Show / Hide ──────────────────────────────────────────────────────────────

void GovDataScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (!initial_load_done_) {
        initial_load_done_ = true;
        // Honour a provider already chosen by restore_state(); the old code
        // unconditionally forced row 0 and discarded the restored selection.
        const int row = (active_index_ >= 0 && active_index_ < provider_list_->count()) ? active_index_ : 0;
        if (provider_list_->count() > 0) {
            provider_list_->setCurrentRow(row);
            activate_provider(row);
        }
    }
    LOG_DEBUG("GovDataScreen", "shown");
}

void GovDataScreen::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
}

void GovDataScreen::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
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
    // P2 — panels are lazy-constructed on first activation (see make_panel()).
    // Eagerly building all provider panels here created every sub-widget tree,
    // stylesheet and QTimer for portals the user never opens. Stack index 0 is
    // a blank placeholder shown before any provider has been selected.
    panel_stack_->addWidget(new QWidget(panel_stack_));
    panels_.resize(services::GovDataService::providers().size());
    panels_.fill(nullptr);

    content_hl->addWidget(panel_stack_, 1);
    root->addWidget(content, 1);
    root->addWidget(build_status_bar());
}

// ── Lazy panel factory ───────────────────────────────────────────────────────

QWidget* GovDataScreen::make_panel(int index) {
    const auto& providers = services::GovDataService::providers();
    if (index < 0 || index >= providers.size())
        return nullptr;
    const auto& prov = providers[index];
    QWidget* panel = nullptr;
    {
        // CKAN portals: the DATASETS tab opens on the portal's recently updated datasets until a
        // publisher is opened (command names as each script spells them).
        GovProviderOptions ckan_opts;
        ckan_opts.source_label = prov.name;
        if (prov.id == "us-treasury") {
            panel = new GovDataTreasuryPanel(panel_stack_);
        } else if (prov.id == "us-congress") {
            panel = new GovDataCongressPanel(panel_stack_);
        } else if (prov.id == "canada-gov") {
            ckan_opts.recent_command = QStringLiteral("recent-datasets");
            panel = new GovDataProviderPanel(prov.script, prov.color, "Publishers", ckan_opts, panel_stack_);
        } else if (prov.id == "swiss") {
            ckan_opts.watermark_text = "\xF0\x9F\x87\xA8\xF0\x9F\x87\xAD opendata.swiss";
            ckan_opts.recent_command = QStringLiteral("recent-datasets");
            panel = new GovDataProviderPanel(prov.script, prov.color, "Publishers", ckan_opts, panel_stack_);
        } else if (prov.id == "france") {
            panel = new GovDataFrancePanel(panel_stack_);
        } else if (prov.id == "hk") {
            panel = new GovDataHKPanel(panel_stack_);
        } else if (prov.id == "universal-ckan") {
            // universal_ckan_api.py serves several national CKAN portals: the panel asks it for the
            // portal list and sends the selected portal code with every request; it opens on the
            // portal's recently updated datasets (`recent <portal> [rows]`).
            ckan_opts.portals_command = QStringLiteral("portals");
            ckan_opts.recent_command = QStringLiteral("recent");
            ckan_opts.land_on_recent = true;
            panel = new GovDataProviderPanel(prov.script, prov.color, "Publishers", ckan_opts, panel_stack_);
        } else if (prov.id == "uk") {
            // data.gov.uk keeps its dedicated panel (pagination + popular publishers).
            panel = new GovDataUKPanel(panel_stack_);
        } else if (prov.id == "australia") {
            panel = new GovDataAustraliaPanel(panel_stack_);
        } else {
            // openafrica: openafrica_api.py maps publishers / datasets / resources onto its own
            // organizations / org-datasets / dataset-details; `recent` is native.
            ckan_opts.recent_command = QStringLiteral("recent");
            panel = new GovDataProviderPanel(prov.script, prov.color, "Organizations", ckan_opts, panel_stack_);
        }
    }
    if (panel) {
        panel_stack_->addWidget(panel);
        panels_[index] = panel;
        LOG_DEBUG("GovDataScreen", "Created panel for: " + prov.id);
    }
    return panel;
}

// ── Toolbar ──────────────────────────────────────────────────────────────────

QWidget* GovDataScreen::build_toolbar() {
    header_bar_ = new QWidget(this);
    header_bar_->setObjectName("govToolbar");
    header_bar_->setMinimumHeight(38);

    auto* hl = new QHBoxLayout(header_bar_);
    hl->setContentsMargins(14, 0, 14, 0);
    hl->setSpacing(8);

    header_title_ = new QLabel;
    header_title_->setObjectName("govToolbarTitle");
    header_title_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    hl->addWidget(header_title_);

    // Separator dot
    auto* sep = new QLabel("·");
    sep->setObjectName("govToolbarSub");
    sep->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    hl->addWidget(sep);

    header_subtitle_ = new QLabel;
    header_subtitle_->setObjectName("govToolbarSub");
    header_subtitle_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    hl->addWidget(header_subtitle_);

    hl->addStretch(1);

    // Source count badge — compact pill, stored as member for refresh_theme()
    provider_badge_ = new QLabel;
    provider_badge_->setMinimumHeight(20);
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
    hdr->setMinimumHeight(26);
    auto* hhl = new QHBoxLayout(hdr);
    hhl->setContentsMargins(12, 0, 8, 0);
    hhl->setSpacing(6);
    sidebar_title_ = new QLabel;
    sidebar_title_->setObjectName("govSidebarTitle");
    const auto& providers = services::GovDataService::providers();
    sidebar_count_ = new QLabel(QString::number(providers.size()));
    sidebar_count_->setMinimumHeight(16);
    sidebar_count_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    hhl->addWidget(sidebar_title_);
    hhl->addStretch(1);
    hhl->addWidget(sidebar_count_);
    vl->addWidget(hdr);

    // Provider list
    provider_list_ = new QListWidget;
    provider_list_->setObjectName("govProviderList");
    provider_list_->setAccessibleName(tr("Sovereign open-data portals"));

    for (const auto& prov : providers) {
        auto* item = new QListWidgetItem(provider_list_);
        // Flag + name on first line, country on tooltip
        item->setText(QString("%1  %2").arg(prov.flag, prov.name));
        item->setToolTip(prov.description + "\n" + prov.country);
        // Screen readers can't announce the flag emoji usefully — give the row
        // a plain-text accessible label.
        item->setData(Qt::AccessibleTextRole, QString("%1 — %2").arg(prov.name, prov.country));
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
    bar->setMinimumHeight(24);

    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(14, 0, 14, 0);
    hl->setSpacing(6);

    auto sep = [&]() -> QLabel* {
        auto* l = new QLabel("·");
        l->setObjectName("govStatusSep");
        return l;
    };

    status_govt_lbl_ = new QLabel;
    status_govt_lbl_->setObjectName("govStatusText");
    hl->addWidget(status_govt_lbl_);
    hl->addWidget(sep());

    status_portal_lbl_ = new QLabel;
    status_portal_lbl_->setObjectName("govStatusText");
    status_portal_ = new QLabel("—");
    status_portal_->setObjectName("govStatusVal");
    hl->addWidget(status_portal_lbl_);
    hl->addWidget(status_portal_);
    hl->addWidget(sep());

    status_country_lbl_ = new QLabel;
    status_country_lbl_->setObjectName("govStatusText");
    status_country_ = new QLabel("—");
    status_country_->setObjectName("govStatusVal");
    hl->addWidget(status_country_lbl_);
    hl->addWidget(status_country_);

    hl->addStretch(1);

    // Ready dot
    auto* dot = new QLabel;
    dot->setFixedSize(6, 6);
    dot->setStyleSheet(QString("background:%1; border-radius:3px;").arg(colors::SUCCESS()));
    hl->addWidget(dot);
    status_ready_lbl_ = new QLabel;
    status_ready_lbl_->setObjectName("govStatusText");
    status_ready_lbl_->setStyleSheet(
        QString("color:%1; font-size:" + fincept::ui::fonts::px(-5) + "px; background:transparent;").arg(colors::SUCCESS()));
    hl->addWidget(status_ready_lbl_);

    return bar;
}

// ── Re-translation ───────────────────────────────────────────────────────────

void GovDataScreen::retranslateUi() {
    const int n_providers = services::GovDataService::providers().size();

    if (header_title_)
        header_title_->setText(tr("GOVERNMENT DATA EXPLORER"));

    // Subtitle: when a provider is active, activate_provider() overwrites this
    // with "{flag} {full_name}" — which is data, not translatable. Only set
    // the default landing-page text when no provider has been selected yet.
    if (header_subtitle_ && active_index_ < 0)
        header_subtitle_->setText(tr("Open government portals · %1 sovereign sources").arg(n_providers));

    if (provider_badge_)
        provider_badge_->setText(tr("%1 PORTALS").arg(n_providers));

    if (sidebar_title_)
        sidebar_title_->setText(tr("SOVEREIGN PORTALS"));

    if (status_govt_lbl_)
        status_govt_lbl_->setText(tr("GOVT"));
    if (status_portal_lbl_)
        status_portal_lbl_->setText(tr("PORTAL:"));
    if (status_country_lbl_)
        status_country_lbl_->setText(tr("COUNTRY:"));
    if (status_ready_lbl_)
        status_ready_lbl_->setText(tr("READY"));
}

// ── Theme refresh ────────────────────────────────────────────────────────────

void GovDataScreen::refresh_theme() {
    // Runs on every provider switch. Re-setting a stylesheet re-computes the
    // style of every widget under it even when the text is unchanged — on the
    // screen root that was most of the cost of each click — so only changes
    // are applied.
    const auto set_sheet = [](QWidget* w, const QString& sheet) {
        if (w && w->styleSheet() != sheet)
            w->setStyleSheet(sheet);
    };
    set_sheet(this, build_screen_style());

    const auto& t = ThemeManager::instance().tokens();
    // The active provider's identity colour (picked against the dark theme), made legible on the
    // active one; the theme accent until a provider is chosen.
    const QColor accent = active_provider_color_.isEmpty() ? QColor(t.accent)
                                                           : colors::legible_color(QColor(active_provider_color_));

    // Provider count badge (top-right pill)
    set_sheet(provider_badge_, QString("color:%1; background:%2; border:1px solid %3; font-weight:700;"
                                       " padding:2px 8px; letter-spacing:0.5px; font-size:%4px;")
                                   .arg(accent.name(), colors::tint(accent, 0.1), colors::tint(accent, 0.3))
                                   .arg(fincept::ui::fonts::px(-5)));

    // Sidebar count pill
    set_sheet(sidebar_count_, QString("color:%1; background:%2; font-weight:700; padding:1px 5px; font-size:%3px;")
                                  .arg(t.accent, t.accent_bg)
                                  .arg(fincept::ui::fonts::px(-5)));

    // Provider-color accent on toolbar border + subtitle
    if (!active_provider_color_.isEmpty() && header_bar_ && header_subtitle_) {
        set_sheet(header_bar_, QString("#govToolbar { background:%1; border-bottom:2px solid %2; }")
                                   .arg(t.bg_raised, accent.name()));
        set_sheet(header_subtitle_, QString("color:%1; background:transparent; font-size:%2px;")
                                        .arg(accent.name())
                                        .arg(fincept::ui::fonts::px(-4)));
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

    // Keep the sidebar highlight in step — restore_state()/programmatic
    // activation used to switch the panel without moving the list selection.
    if (provider_list_ && provider_list_->currentRow() != index) {
        QSignalBlocker block(provider_list_);
        provider_list_->setCurrentRow(index);
    }

    // Switch panel (lazy-constructing it on first use)
    QWidget* panel = (index < panels_.size()) ? panels_[index] : nullptr;
    const bool first_open = (panel == nullptr);
    if (!panel)
        panel = make_panel(index);
    if (panel)
        panel_stack_->setCurrentWidget(panel);

    // Re-apply theme so provider-color accents on toolbar/badge reflect new selection
    refresh_theme();

    // Trigger the initial load only the first time a panel is opened — all
    // panel types expose load_initial_data() as a public slot. Re-invoking it
    // on every revisit threw away whatever the user had drilled into.
    if (panel && first_open)
        QMetaObject::invokeMethod(panel, "load_initial_data");

    LOG_DEBUG("GovDataScreen", "Activated: " + prov.id);
}

// ── IStatefulScreen ───────────────────────────────────────────────────────────

QVariantMap GovDataScreen::save_state() const {
    const auto& providers = services::GovDataService::providers();
    const QString id = (active_index_ >= 0 && active_index_ < providers.size()) ? providers[active_index_].id : QString();
    // The id survives reordering of the provider list; the index is kept for older builds.
    return {{"provider_id", id}, {"provider_index", active_index_}};
}

void GovDataScreen::restore_state(const QVariantMap& state) {
    int idx = -1;
    const QString id = state.value("provider_id").toString();
    if (!id.isEmpty()) {
        const auto& providers = services::GovDataService::providers();
        for (int i = 0; i < providers.size(); ++i) {
            if (providers[i].id == id) {
                idx = i;
                break;
            }
        }
    } else {
        idx = state.value("provider_index", -1).toInt();
    }
    if (idx >= 0)
        activate_provider(idx);
}

bool GovDataScreen::open_section(const QString& section, const QString& focus) {
    Q_UNUSED(focus); // the provider panels take no focus from a link
    QString key = section.trimmed();
    if (key.isEmpty())
        return true;
    QString rest;
    if (command_rows::split_kind(key, "provider", &rest))
        key = rest;
    const auto& providers = services::GovDataService::providers();
    int index = -1;
    for (int i = 0; i < providers.size() && index < 0; ++i)
        if (providers[i].id == key)
            index = i;
    for (int i = 0; i < providers.size() && index < 0; ++i)
        if (providers[i].id.compare(key, Qt::CaseInsensitive) == 0)
            index = i;
    if (index < 0 || !provider_list_ || index >= provider_list_->count())
        return false;
    // The sidebar click path: currentRowChanged → on_provider_selected (activate + state).
    if (provider_list_->currentRow() != index)
        provider_list_->setCurrentRow(index);
    else if (active_index_ != index)
        on_provider_selected(index);
    return true;
}

// ── Search box rows (core/command/CommandRegistry.h; screens/common/CommandRows.h) ──
namespace {

QList<command::CommandItem> gov_command_rows() {
    namespace rows = command_rows;
    const QString screen = QStringLiteral("gov_data");
    const QString where = GovDataScreen::tr("Government Data");
    auto row = [&](const char* id, const QString& title, const QStringList& keywords) {
        return rows::section(screen, QString::fromLatin1(id), title, keywords, where);
    };
    return {
        row("us-treasury", GovDataScreen::tr("US Treasury"),
            {QStringLiteral("treasury"), QStringLiteral("bonds"), QStringLiteral("yields"), QStringLiteral("auctions"),
             QStringLiteral("t-bills"), QStringLiteral("treasury securities"), QStringLiteral("united states")}),
        row("us-congress", GovDataScreen::tr("US Congress"),
            {QStringLiteral("congress"), QStringLiteral("bills"), QStringLiteral("legislation"),
             QStringLiteral("resolutions"), QStringLiteral("senate"), QStringLiteral("house")}),
        row("canada-gov", GovDataScreen::tr("Canada open data"),
            {QStringLiteral("canada"), QStringLiteral("canadian government"), QStringLiteral("open data"),
             QStringLiteral("datasets")}),
        row("swiss", GovDataScreen::tr("Swiss open data"),
            {QStringLiteral("switzerland"), QStringLiteral("opendata.swiss"), QStringLiteral("open data"),
             QStringLiteral("datasets")}),
        row("france", GovDataScreen::tr("France open data"),
            {QStringLiteral("france"), QStringLiteral("data.gouv.fr"), QStringLiteral("french government"),
             QStringLiteral("company registry"), QStringLiteral("open data")}),
        row("hk", GovDataScreen::tr("Hong Kong open data"),
            {QStringLiteral("hong kong"), QStringLiteral("data.gov.hk"), QStringLiteral("open data"),
             QStringLiteral("datasets")}),
        row("openafrica", GovDataScreen::tr("openAFRICA"),
            {QStringLiteral("africa"), QStringLiteral("african open data"), QStringLiteral("open data"),
             QStringLiteral("datasets")}),
        row("universal-ckan", GovDataScreen::tr("CKAN portals"),
            {QStringLiteral("ckan"), QStringLiteral("ireland"), QStringLiteral("italy"), QStringLiteral("netherlands"),
             QStringLiteral("latin america"), QStringLiteral("open data portals")}),
        row("australia", GovDataScreen::tr("Australia open data"),
            {QStringLiteral("australia"), QStringLiteral("data.gov.au"), QStringLiteral("open data"),
             QStringLiteral("datasets")}),
        row("uk", GovDataScreen::tr("UK open data"),
            {QStringLiteral("united kingdom"), QStringLiteral("britain"), QStringLiteral("data.gov.uk"),
             QStringLiteral("open data"), QStringLiteral("publishers")}),
    };
}

void gov_install_commands() {
    command_rows::install(QStringLiteral("gov_data"), &gov_command_rows);
}

} // namespace

Q_COREAPP_STARTUP_FUNCTION(gov_install_commands)

} // namespace fincept::screens
