// src/screens/gov_data/GovDataScreen.cpp
#include "screens/gov_data/GovDataScreen.h"

#include "core/logging/Logger.h"
#include "screens/gov_data/GovDataCongressPanel.h"
#include "screens/gov_data/GovDataProviderPanel.h"
#include "screens/gov_data/GovDataTreasuryPanel.h"
#include "services/gov_data/GovDataService.h"
#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QSplitter>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace fincept::screens {

using namespace fincept::ui;

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────

GovDataScreen::GovDataScreen(QWidget* parent) : QWidget(parent) {
    build_ui();
}

// ─────────────────────────────────────────────────────────────────────────────
// Show / Hide (P2, P3)
// ─────────────────────────────────────────────────────────────────────────────

void GovDataScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    LOG_INFO("GovDataScreen", "Screen shown");

    if (!initial_load_done_) {
        initial_load_done_ = true;
        // Select first provider
        if (provider_list_->count() > 0) {
            provider_list_->setCurrentRow(0);
            activate_provider(0);
        }
    }
}

void GovDataScreen::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    LOG_INFO("GovDataScreen", "Screen hidden");
}

// ─────────────────────────────────────────────────────────────────────────────
// Build UI
// ─────────────────────────────────────────────────────────────────────────────

void GovDataScreen::build_ui() {
    setStyleSheet(
        QString("QWidget { background:%1; color:%2; }").arg(colors::BG_BASE, colors::TEXT_PRIMARY));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Header bar
    root->addWidget(build_header());

    // Content: sidebar + panel stack
    auto* content = new QWidget(this);
    auto* content_hl = new QHBoxLayout(content);
    content_hl->setContentsMargins(0, 0, 0, 0);
    content_hl->setSpacing(0);

    // Sidebar
    content_hl->addWidget(build_sidebar());

    // Panel stack — one panel per provider
    panel_stack_ = new QStackedWidget(content);

    const auto& providers = services::GovDataService::providers();
    for (int i = 0; i < providers.size(); ++i) {
        const auto& prov = providers[i];
        QWidget* panel = nullptr;

        if (prov.id == "us-treasury") {
            panel = new GovDataTreasuryPanel(panel_stack_);
        } else if (prov.id == "us-congress") {
            panel = new GovDataCongressPanel(panel_stack_);
        } else {
            // All other providers use the generic CKAN-style panel
            QString org_label = "Publishers";
            if (prov.id == "openafrica") org_label = "Organizations";
            else if (prov.id == "hk") org_label = "Categories";
            else if (prov.id == "universal-ckan") org_label = "Organizations";

            panel = new GovDataProviderPanel(prov.script, prov.color, org_label, panel_stack_);
        }

        panel_stack_->addWidget(panel);
    }

    content_hl->addWidget(panel_stack_, 1);
    root->addWidget(content, 1);

    // Status bar
    auto* status_bar = new QWidget(this);
    status_bar->setFixedHeight(24);
    status_bar->setStyleSheet(
        QString("background:%1; border-top:1px solid %2;")
            .arg(colors::BG_RAISED, colors::BORDER_DIM));

    auto* status_hl = new QHBoxLayout(status_bar);
    status_hl->setContentsMargins(12, 0, 12, 0);
    status_hl->setSpacing(12);

    auto* portal_label = new QLabel("PORTAL: -", status_bar);
    portal_label->setObjectName("govStatusPortal");
    portal_label->setStyleSheet(
        QString("color:%1; font-family:%2; font-size:9px;")
            .arg(colors::TEXT_SECONDARY, fonts::DATA_FAMILY));
    status_hl->addWidget(portal_label);

    auto* country_label = new QLabel("COUNTRY: -", status_bar);
    country_label->setObjectName("govStatusCountry");
    country_label->setStyleSheet(portal_label->styleSheet());
    status_hl->addWidget(country_label);

    status_hl->addStretch();

    auto* ready_dot = new QLabel(status_bar);
    ready_dot->setFixedSize(6, 6);
    ready_dot->setStyleSheet(
        QString("background:%1; border-radius:3px;").arg(colors::GREEN));
    status_hl->addWidget(ready_dot);

    auto* ready_label = new QLabel("Ready", status_bar);
    ready_label->setStyleSheet(
        QString("color:%1; font-family:%2; font-size:9px;")
            .arg(colors::GREEN, fonts::DATA_FAMILY));
    status_hl->addWidget(ready_label);

    root->addWidget(status_bar);
}

QWidget* GovDataScreen::build_header() {
    header_bar_ = new QWidget(this);
    header_bar_->setFixedHeight(40);
    header_bar_->setStyleSheet(
        QString("background:%1; border-bottom:2px solid %2;")
            .arg(colors::BG_RAISED, colors::AMBER));

    auto* hl = new QHBoxLayout(header_bar_);
    hl->setContentsMargins(12, 0, 12, 0);
    hl->setSpacing(10);

    header_title_ = new QLabel("GOVT", header_bar_);
    header_title_->setStyleSheet(
        QString("color:%1; font-family:%2; font-size:14px; font-weight:700;")
            .arg(colors::TEXT_PRIMARY, fonts::DATA_FAMILY));
    hl->addWidget(header_title_);

    auto* sep = new QLabel("|", header_bar_);
    sep->setStyleSheet(
        QString("color:%1; font-size:10px;").arg(colors::TEXT_TERTIARY));
    hl->addWidget(sep);

    header_subtitle_ = new QLabel("Government Data Portals", header_bar_);
    header_subtitle_->setStyleSheet(
        QString("color:%1; font-family:%2; font-size:12px; font-weight:600;")
            .arg(colors::AMBER, fonts::DATA_FAMILY));
    hl->addWidget(header_subtitle_);

    hl->addStretch();

    return header_bar_;
}

QWidget* GovDataScreen::build_sidebar() {
    auto* sidebar = new QWidget(this);
    sidebar->setFixedWidth(220);
    sidebar->setStyleSheet(
        QString("background:%1; border-right:1px solid %2;")
            .arg(colors::BG_SURFACE, colors::BORDER_DIM));

    auto* vl = new QVBoxLayout(sidebar);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Section header
    auto* section_label = new QLabel("SOVEREIGN DATA", sidebar);
    section_label->setStyleSheet(
        QString("padding:10px 12px; font-size:9px; font-weight:700; color:%1;"
                " letter-spacing:1px; border-bottom:1px solid %2;")
            .arg(colors::TEXT_SECONDARY, colors::BORDER_DIM));
    vl->addWidget(section_label);

    // Provider list
    provider_list_ = new QListWidget(sidebar);
    provider_list_->setStyleSheet(
        QString("QListWidget { background:transparent; border:none; }"
                "QListWidget::item { padding:10px 12px; border-bottom:1px solid %1; }"
                "QListWidget::item:selected { background:%2; border-left:3px solid %3; }"
                "QListWidget::item:hover { background:%4; }")
            .arg(colors::BORDER_DIM,
                 QString(colors::AMBER) + "15", colors::AMBER,
                 colors::BG_HOVER));

    const auto& providers = services::GovDataService::providers();
    for (const auto& prov : providers) {
        auto* item = new QListWidgetItem(provider_list_);
        item->setText(QString("%1  %2").arg(prov.flag, prov.name));
        item->setToolTip(prov.description);
        item->setData(Qt::UserRole, prov.id);

        // Set text color
        item->setForeground(QColor(colors::TEXT_PRIMARY));
    }

    connect(provider_list_, &QListWidget::currentRowChanged,
            this, &GovDataScreen::on_provider_selected);

    vl->addWidget(provider_list_, 1);

    return sidebar;
}

// ─────────────────────────────────────────────────────────────────────────────
// Provider selection
// ─────────────────────────────────────────────────────────────────────────────

void GovDataScreen::on_provider_selected(int row) {
    if (row < 0) return;
    activate_provider(row);
}

void GovDataScreen::activate_provider(int index) {
    const auto& providers = services::GovDataService::providers();
    if (index < 0 || index >= providers.size()) return;

    active_index_ = index;
    const auto& prov = providers[index];

    // Update header
    header_subtitle_->setText(QString("%1  %2").arg(prov.flag, prov.full_name));
    header_subtitle_->setStyleSheet(
        QString("color:%1; font-family:%2; font-size:12px; font-weight:600;")
            .arg(prov.color, fonts::DATA_FAMILY));
    header_bar_->setStyleSheet(
        QString("background:%1; border-bottom:2px solid %2;")
            .arg(colors::BG_RAISED, prov.color));

    // Update status bar
    auto* portal_lbl = findChild<QLabel*>("govStatusPortal");
    if (portal_lbl) portal_lbl->setText("PORTAL: " + prov.name);
    auto* country_lbl = findChild<QLabel*>("govStatusCountry");
    if (country_lbl) country_lbl->setText("COUNTRY: " + prov.country);

    // Switch panel
    panel_stack_->setCurrentIndex(index);

    // Trigger initial load on the active panel
    auto* widget = panel_stack_->currentWidget();
    if (auto* ckan = qobject_cast<GovDataProviderPanel*>(widget)) {
        ckan->load_initial_data();
    } else if (auto* treasury = qobject_cast<GovDataTreasuryPanel*>(widget)) {
        treasury->load_initial_data();
    } else if (auto* congress = qobject_cast<GovDataCongressPanel*>(widget)) {
        congress->load_initial_data();
    }

    LOG_INFO("GovDataScreen", QString("Activated provider: %1").arg(prov.id));
}

} // namespace fincept::screens
