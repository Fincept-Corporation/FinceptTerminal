// McpServersSection.cpp — Settings → MCP Servers.
//
// MCP servers are managed on the MCP Servers screen (screens/mcp_servers):
// marketplace, custom servers, start/stop, logs, auto-start and the per-tool
// on/off table. This section used to carry a second copy of that UI (server
// list, add/remove, start/stop, tool table) driving the same McpManager, so it
// now only explains where those controls live and links there.

#include "screens/settings/McpServersSection.h"

#include "core/events/EventBus.h"
#include "screens/settings/SettingsStyles.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QVariantMap>

namespace fincept::screens {

namespace {

/// Wrapped plain-text paragraph, styled by the section sheet (#mcpSectionBody).
QLabel* mcp_section_body_label() {
    auto* lbl = new QLabel;
    lbl->setObjectName(QStringLiteral("mcpSectionBody"));
    lbl->setTextFormat(Qt::PlainText);
    lbl->setWordWrap(true);
    return lbl;
}

} // namespace

McpServersSection::McpServersSection(QWidget* parent) : QWidget(parent) {
    build_ui();
    retranslateUi();
    refresh_theme();
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this](const ui::ThemeTokens&) { refresh_theme(); });
}

void McpServersSection::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 24, 24, 24);
    root->setSpacing(0);

    title_lbl_ = new QLabel;
    title_lbl_->setObjectName(QStringLiteral("mcpSectionTitle"));
    root->addWidget(title_lbl_);
    root->addSpacing(16);

    auto* sep = new QFrame;
    sep->setObjectName(QStringLiteral("mcpSectionSep"));
    sep->setFixedHeight(1);
    root->addWidget(sep);
    root->addSpacing(16);

    about_lbl_ = mcp_section_body_label();
    root->addWidget(about_lbl_);
    root->addSpacing(10);

    manage_lbl_ = mcp_section_body_label();
    root->addWidget(manage_lbl_);
    root->addSpacing(10);

    tools_lbl_ = mcp_section_body_label();
    root->addWidget(tools_lbl_);
    root->addSpacing(20);

    open_btn_ = new QPushButton;
    open_btn_->setCursor(Qt::PointingHandCursor);
    open_btn_->setFixedHeight(32);
    // Same route the Data Sources section and the info screens use:
    // WindowFrame's nav.switch_screen subscriber opens the panel registered as
    // "mcp_servers" (constructing it on first use) in this window.
    connect(open_btn_, &QPushButton::clicked, this, []() {
        EventBus::instance().publish(QStringLiteral("nav.switch_screen"),
                                     QVariantMap{{QStringLiteral("screen_id"), QStringLiteral("mcp_servers")}});
    });
    auto* btn_row = new QHBoxLayout;
    btn_row->setContentsMargins(0, 0, 0, 0);
    btn_row->addWidget(open_btn_);
    btn_row->addStretch();
    root->addLayout(btn_row);

    root->addStretch();
}

void McpServersSection::refresh_theme() {
    using namespace settings_styles;
    // One sheet for the whole section, keyed by objectName (P7). The open
    // button is the section's only QPushButton, so btn_primary_ss()'s type
    // selector (amber fill, dark label) reaches exactly that button.
    setStyleSheet(QString("QLabel#mcpSectionTitle{%1}"
                          "QLabel#mcpSectionBody{%2}"
                          "QFrame#mcpSectionSep{background:%3;border:none;}")
                      .arg(section_title_ss(), label_ss(), QString(ui::colors::BORDER_DIM())) +
                  btn_primary_ss());
}

void McpServersSection::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void McpServersSection::retranslateUi() {
    if (title_lbl_)
        title_lbl_->setText(tr("MCP SERVERS"));
    if (about_lbl_)
        about_lbl_->setText(tr("MCP (Model Context Protocol) servers are programs that give the AI assistant extra "
                               "tools, such as database access, file search or third-party APIs. Fincept Terminal "
                               "also ships its own built-in (internal) tools."));
    if (manage_lbl_)
        manage_lbl_->setText(tr("Servers are managed on the MCP Servers screen: add one from the marketplace or as a "
                                "custom server, start or stop it, view its logs and choose whether it starts "
                                "automatically."));
    if (tools_lbl_)
        tools_lbl_->setText(tr("Individual internal tools can be switched on or off in that screen's TOOLS view. "
                               "Your choices are kept across restarts."));
    if (open_btn_) {
        open_btn_->setText(tr("OPEN MCP SERVERS"));
        open_btn_->setAccessibleName(tr("Open the MCP Servers screen"));
    }
}

} // namespace fincept::screens
