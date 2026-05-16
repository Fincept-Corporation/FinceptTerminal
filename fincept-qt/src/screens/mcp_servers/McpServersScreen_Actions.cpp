// src/screens/mcp_servers/McpServersScreen_Actions.cpp
// User-action handlers: on_view_changed, install/start/stop/remove server,
// toggle_autostart, refresh, search, view_logs, add_server, tool toggle,
// refresh_installed, refresh_tools, update_status_bar.
// Part of the partial-class split of McpServersScreen.cpp.

#include "screens/mcp_servers/McpServersScreen.h"

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "mcp/McpManager.h"
#include "mcp/McpMarketplace.h"
#include "mcp/McpProvider.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QStackedWidget>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextEdit>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
using namespace fincept::ui;

static const QList<fincept::mcp::MarketplaceEntry>& g_catalog = fincept::mcp::marketplace_catalog();

inline QString kStyle() {
    return QString(

               // Screen / header
               "#mcpScreen  { background: %1; }"
               "#mcpHeader  { background: %2; border-bottom: 2px solid %3; }"
               "#mcpHeaderTitle { color:%4; font-weight:700; background:transparent; }"

               // Tab buttons
               "#mcpViewBtn { background:transparent; color:%5; border:1px solid %8; "
               "  font-weight:700; padding:4px 14px; }"
               "#mcpViewBtn:hover { color:%4; border-color:%9; }"
               "#mcpViewBtn[active=\"true\"] { background:%3; color:%1; border-color:%3; }"

               // Search / refresh
               "#mcpSearchInput { background:%1; color:%4; border:1px solid %8; "
               "  padding:4px 8px; min-width:200px; }"
               "#mcpSearchInput:focus { border-color:%9; }"
               "#mcpRefreshBtn { background:%7; color:%5; border:1px solid %8; "
               "  padding:4px 10px; font-weight:700; }"
               "#mcpRefreshBtn:hover { color:%4; }"

               // ── Marketplace cards ──
               "#mktCard { background:%7; border:1px solid %8; min-height:130px; }"
               "#mktCard:hover { border-color:%9; }"
               "#mktCardName  { color:%4; font-weight:700; background:transparent; }"
               "#mktCardDesc  { color:%5; background:transparent; }"
               "#mktCardCmd   { color:%13; font-family:monospace; background:transparent; }"
               "#mktCardEnv   { color:%3; background:transparent; }"
               "#mktCatBadge  { color:%3; font-weight:700; "
               "  background:rgba(217,119,6,0.12); padding:1px 6px; border:1px solid rgba(217,119,6,0.3); }"
               "#mktAddBtn    { background:%3; color:%1; border:none; padding:5px 16px; "
               "  font-weight:700; }"
               "#mktAddBtn:hover { background:%10; }"
               "#mktInstalledBadge { color:%6; font-weight:700; "
               "  background:rgba(22,163,74,0.12); padding:5px 12px; border:1px solid rgba(22,163,74,0.3); }"

               // Category filter sidebar
               "#catList { background:%2; border:none; outline:none; }"
               "#catList::item { color:%5; font-weight:700; padding:6px 12px; }"
               "#catList::item:hover { color:%4; background:%12; }"
               "#catList::item:selected { color:%3; background:rgba(217,119,6,0.1); "
               "  border-left:2px solid %3; }"

               // ── Installed server cards ──
               "#srvCard { background:%7; border:1px solid %8; margin:4px 0px; }"
               "#srvCard:hover { border-color:%9; }"
               "#srvCardName  { color:%4; font-weight:700; background:transparent; }"
               "#srvCardDesc  { color:%5; background:transparent; }"
               "#srvCardCmd   { color:%13; font-family:monospace; background:transparent; }"
               "#srvCardCat   { color:%5; background:transparent; }"
               "#pillRunning  { color:%6; font-weight:700; "
               "  background:rgba(22,163,74,0.15); padding:2px 10px; "
               "  border:1px solid rgba(22,163,74,0.4); }"
               "#pillStopped  { color:%14; font-weight:700; "
               "  background:rgba(220,38,38,0.10); padding:2px 10px; "
               "  border:1px solid rgba(220,38,38,0.4); }"
               "#pillError    { color:%14; font-weight:700; "
               "  background:rgba(220,38,38,0.10); padding:2px 10px; "
               "  border:1px solid rgba(220,38,38,0.4); }"
               "#pillAutoOn   { color:%6; background:transparent; }"
               "#pillAutoOff  { color:%11; background:transparent; }"

               // Inline card buttons
               "#cardToggleOn  { background:rgba(22,163,74,0.15); color:%6; "
               "  border:1px solid rgba(22,163,74,0.5); "
               "  padding:4px 12px; font-weight:700; }"
               "#cardToggleOn:hover  { background:rgba(22,163,74,0.25); }"
               "#cardToggleOff { background:%7; color:%5; border:1px solid %8; "
               "  padding:4px 12px; font-weight:700; }"
               "#cardToggleOff:hover { color:%4; border-color:%9; }"
               "#cardLogsBtn   { background:transparent; color:%5; border:1px solid %8; "
               "  padding:4px 12px; font-weight:700; }"
               "#cardLogsBtn:hover   { color:%4; }"
               "#cardRemoveBtn { background:transparent; color:%14; border:1px solid %14; "
               "  padding:4px 12px; font-weight:700; }"
               "#cardRemoveBtn:hover { background:rgba(220,38,38,0.12); }"

               // Add server button (full-width sticky)
               "#addSrvBtn { background:%3; color:%1; border:none; "
               "  padding:8px; font-weight:700; }"
               "#addSrvBtn:hover { background:%10; }"

               // Log expander inside card
               "QTextEdit { background:%1; color:%13; border:none; font-family:monospace; }"

               // Tools table
               "QTableWidget { background:%1; color:%4; border:none; gridline-color:%8; }"
               "QTableWidget::item { padding:2px 6px; border-bottom:1px solid %8; }"
               "QHeaderView::section { background:%2; color:%5; border:none; "
               "  border-bottom:1px solid %8; border-right:1px solid %8; "
               "  padding:4px 6px; font-weight:700; }"

               // Status bar
               "#mcpStatusBar { background:%2; border-top:1px solid %8; }"
               "#mcpStatusText      { color:%5;  background:transparent; }"
               "#mcpStatusHighlight { color:%13; background:transparent; }"

               // Scroll
               "QScrollBar:vertical { background:%1; width:6px; }"
               "QScrollBar::handle:vertical { background:%8; min-height:20px; }"
               "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }"
               "QScrollArea { border:none; background:transparent; }")
        .arg(colors::BG_BASE())        // %1
        .arg(colors::BG_RAISED())      // %2
        .arg(colors::AMBER())          // %3
        .arg(colors::TEXT_PRIMARY())   // %4
        .arg(colors::TEXT_SECONDARY()) // %5
        .arg(colors::POSITIVE())       // %6
        .arg(colors::BG_SURFACE())     // %7
        .arg(colors::BORDER_DIM())     // %8
        .arg(colors::BORDER_BRIGHT())  // %9
        .arg(colors::AMBER_DIM())      // %10
        .arg(colors::TEXT_DIM())       // %11
        .arg(colors::BG_HOVER())       // %12
        .arg(colors::CYAN())           // %13
        .arg(colors::NEGATIVE())       // %14
        ;
}

} // namespace

namespace fincept::screens {

using namespace fincept::mcp;
using namespace fincept::ui;


void McpServersScreen::on_view_changed(int view) {
    active_view_ = view;
    view_stack_->setCurrentIndex(view);
    ScreenStateManager::instance().notify_changed(this);
    for (int i = 0; i < view_btns_.size(); ++i) {
        view_btns_[i]->setProperty("active", i == view);
        view_btns_[i]->style()->unpolish(view_btns_[i]);
        view_btns_[i]->style()->polish(view_btns_[i]);
    }
    const QStringList names = {"MARKETPLACE", "INSTALLED", "TOOLS"};
    status_view_->setText(names[view]);
    if (view == 0)
        populate_marketplace();
    if (view == 1)
        refresh_installed();
    if (view == 2)
        refresh_tools();
}

void McpServersScreen::on_install_server(int index) {
    if (index < 0 || index >= g_catalog.size())
        return;
    const auto& e = g_catalog[index];

    // Pre-fill the add-server dialog with catalog data
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle("Add  " + e.name);
    dlg->setMinimumWidth(460);
    auto* form = new QFormLayout(dlg);
    form->setContentsMargins(16, 16, 16, 8);
    form->setSpacing(10);

    auto* name_edit = new QLineEdit(e.name);
    form->addRow("Name", name_edit);

    auto* desc_edit = new QLineEdit(e.description);
    form->addRow("Description", desc_edit);

    auto* cmd_edit = new QLineEdit(e.command);
    form->addRow("Command", cmd_edit);

    auto* args_edit = new QLineEdit(e.args.join(' '));
    form->addRow("Arguments", args_edit);

    // Env vars — one field per required key with sample placeholder
    QList<QPair<QString, QLineEdit*>> env_fields;
    if (!e.env_keys.isEmpty()) {
        auto* env_header = new QLabel("Environment Variables");
        env_header->setStyleSheet(QString("color:%1;font-size:9px;font-weight:700;").arg(colors::TEXT_SECONDARY()));
        form->addRow(env_header);
        for (int ki = 0; ki < e.env_keys.size(); ++ki) {
            const QString& key = e.env_keys[ki];
            auto* field = new QLineEdit;
            if (ki < e.env_placeholders.size()) {
                // Pre-fill with sample so user edits in-place rather than typing blind
                field->setText(e.env_placeholders[ki]);
                field->selectAll(); // highlight so first keystroke replaces it
            } else {
                field->setPlaceholderText("Enter " + key);
            }
            form->addRow(key, field);
            env_fields.append({key, field});
        }
    }

    auto* cat_combo = new QComboBox;
    cat_combo->addItems({"utilities", "developer", "database"});
    cat_combo->setCurrentText(e.category);
    form->addRow("Category", cat_combo);

    auto* autostart_check = new QCheckBox("Auto-start on launch");
    form->addRow("", autostart_check);

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(btns, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    form->addRow(btns);

    if (dlg->exec() != QDialog::Accepted) {
        dlg->deleteLater();
        return;
    }

    const QString name = name_edit->text().trimmed();
    if (name.isEmpty()) {
        dlg->deleteLater();
        return;
    }

    McpServerConfig cfg;
    cfg.id = name.toLower().replace(' ', '_');
    cfg.name = name;
    cfg.description = desc_edit->text().trimmed();
    cfg.command = cmd_edit->text().trimmed();
    cfg.args = args_edit->text().trimmed().split(' ', Qt::SkipEmptyParts);
    cfg.category = cat_combo->currentText();
    cfg.enabled = true;
    cfg.auto_start = autostart_check->isChecked();
    for (const auto& [key, field] : env_fields) {
        const QString val = field->text().trimmed();
        if (!val.isEmpty())
            cfg.env[key] = val;
    }

    const auto r = McpManager::instance().save_server(cfg);
    if (r.is_ok()) {
        LOG_INFO("McpServers", "Added from catalog: " + name);
        populate_marketplace();
        on_view_changed(1);
    } else {
        LOG_ERROR("McpServers", "Add failed: " + QString::fromStdString(r.error()));
    }
    dlg->deleteLater();
}

void McpServersScreen::on_start_server() {
    if (selected_server_id_.isEmpty())
        return;
    const QString id = selected_server_id_;
    QPointer<McpServersScreen> self = this;
    (void)QtConcurrent::run([id, self]() {
        const auto result = McpManager::instance().start_server(id);
        QMetaObject::invokeMethod(
            self,
            [self, result]() {
                if (!self)
                    return;
                if (result.is_err())
                    LOG_ERROR("McpServers", "Start failed: " + QString::fromStdString(result.error()));
                self->refresh_installed();
                self->update_status_bar();
            },
            Qt::QueuedConnection);
    });
}

void McpServersScreen::on_stop_server() {
    if (selected_server_id_.isEmpty())
        return;
    const QString id = selected_server_id_;
    QPointer<McpServersScreen> self = this;
    (void)QtConcurrent::run([id, self]() {
        McpManager::instance().stop_server(id);
        QMetaObject::invokeMethod(
            self,
            [self]() {
                if (!self)
                    return;
                self->refresh_installed();
                self->update_status_bar();
            },
            Qt::QueuedConnection);
    });
}

void McpServersScreen::on_remove_server() {
    if (selected_server_id_.isEmpty())
        return;
    const QString id = selected_server_id_;
    QString name;
    for (const auto& s : McpManager::instance().get_servers())
        if (s.id == id) {
            name = s.name;
            break;
        }

    QMessageBox mb(QMessageBox::Question, "Remove Server", "Remove \"" + name + "\"?\nThis cannot be undone.",
                   QMessageBox::Yes | QMessageBox::Cancel, this);
    if (mb.exec() != QMessageBox::Yes)
        return;

    McpManager::instance().stop_server(id);
    McpManager::instance().remove_server(id);
    selected_server_id_.clear();
    refresh_installed();
    update_status_bar();
}

void McpServersScreen::on_toggle_autostart() {
    if (selected_server_id_.isEmpty())
        return;
    auto servers = McpManager::instance().get_servers();
    for (auto& s : servers) {
        if (s.id != selected_server_id_)
            continue;
        s.auto_start = !s.auto_start;
        McpManager::instance().save_server(s);
        refresh_installed();
        break;
    }
}

void McpServersScreen::on_server_selected(QListWidgetItem*) {} // unused — cards are self-contained

void McpServersScreen::on_refresh() {
    if (active_view_ == 0)
        populate_marketplace();
    else if (active_view_ == 1)
        refresh_installed();
    else
        refresh_tools();
    update_status_bar();
}

void McpServersScreen::on_search_changed(const QString& text) {
    if (active_view_ == 2) {
        for (int i = 0; i < tools_table_->rowCount(); ++i) {
            auto* n = tools_table_->item(i, 1);
            auto* s = tools_table_->item(i, 2);
            auto* d = tools_table_->item(i, 4);
            const bool match = text.isEmpty() || (n && n->text().contains(text, Qt::CaseInsensitive)) ||
                               (s && s->text().contains(text, Qt::CaseInsensitive)) ||
                               (d && d->text().contains(text, Qt::CaseInsensitive));
            tools_table_->setRowHidden(i, !match);
        }
        return;
    }
    // Marketplace / installed: re-populate with filter
    if (active_view_ == 0)
        populate_marketplace();
    if (active_view_ == 1)
        refresh_installed();
}

void McpServersScreen::on_view_logs() {
    // Handled inline per card — this slot kept for compat
}

void McpServersScreen::on_add_server() {
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle("Add Custom MCP Server");
    dlg->setMinimumWidth(460);
    auto* form = new QFormLayout(dlg);
    form->setContentsMargins(16, 16, 16, 8);
    form->setSpacing(10);

    auto* name_edit = new QLineEdit;
    name_edit->setPlaceholderText("e.g. My Custom Server");
    form->addRow("Name", name_edit);

    auto* desc_edit = new QLineEdit;
    desc_edit->setPlaceholderText("Short description");
    form->addRow("Description", desc_edit);

    auto* cmd_edit = new QLineEdit("uvx");
    form->addRow("Command", cmd_edit);

    auto* args_edit = new QLineEdit;
    args_edit->setPlaceholderText("e.g. my-mcp-package --flag value");
    form->addRow("Arguments", args_edit);

    auto* env_edit = new QLineEdit;
    env_edit->setPlaceholderText("KEY=value KEY2=value2");
    form->addRow("Env Vars", env_edit);

    auto* cat_combo = new QComboBox;
    cat_combo->addItems({"utilities", "developer", "database"});
    form->addRow("Category", cat_combo);

    auto* autostart_check = new QCheckBox("Auto-start on launch");
    form->addRow("", autostart_check);

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(btns, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    form->addRow(btns);

    if (dlg->exec() != QDialog::Accepted) {
        dlg->deleteLater();
        return;
    }

    const QString name = name_edit->text().trimmed();
    if (name.isEmpty()) {
        dlg->deleteLater();
        return;
    }

    McpServerConfig cfg;
    cfg.id = name.toLower().replace(' ', '_');
    cfg.name = name;
    cfg.description = desc_edit->text().trimmed();
    cfg.command = cmd_edit->text().trimmed();
    cfg.args = args_edit->text().trimmed().split(' ', Qt::SkipEmptyParts);
    cfg.category = cat_combo->currentText();
    cfg.enabled = true;
    cfg.auto_start = autostart_check->isChecked();

    for (const auto& pair : env_edit->text().trimmed().split(' ', Qt::SkipEmptyParts)) {
        const int eq = pair.indexOf('=');
        if (eq > 0)
            cfg.env[pair.left(eq)] = pair.mid(eq + 1);
    }

    const auto r = McpManager::instance().save_server(cfg);
    if (r.is_ok()) {
        LOG_INFO("McpServers", "Added custom server: " + name);
        refresh_installed();
        update_status_bar();
    } else {
        LOG_ERROR("McpServers", "Failed: " + QString::fromStdString(r.error()));
    }
    dlg->deleteLater();
}

void McpServersScreen::on_tool_enabled_changed(int row, int col) {
    if (col != 0)
        return;
    auto* ci = tools_table_->item(row, 0);
    auto* ni = tools_table_->item(row, 1);
    auto* si = tools_table_->item(row, 2);
    if (!ci || !ni || !si)
        return;
    if (si->data(Qt::UserRole).toString() != QString(INTERNAL_SERVER_ID))
        return;
    const bool enabled = (ci->checkState() == Qt::Checked);
    McpProvider::instance().set_tool_enabled(ni->text(), enabled);
    LOG_INFO("McpServers", QString("Tool '%1' %2").arg(ni->text(), enabled ? "enabled" : "disabled"));
}

// ── Data helpers ──────────────────────────────────────────────────────────────

void McpServersScreen::refresh_installed() {
    const QString search = search_input_ ? search_input_->text().trimmed() : "";

    // Remove old cards
    while (inst_cards_layout_->count() > 1) {
        auto* item = inst_cards_layout_->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }

    const auto servers = McpManager::instance().get_servers();
    int shown = 0;

    for (const auto& s : servers) {
        if (!search.isEmpty() && !s.name.contains(search, Qt::CaseInsensitive) &&
            !s.description.contains(search, Qt::CaseInsensitive) && !s.category.contains(search, Qt::CaseInsensitive))
            continue;

        auto* card = build_server_card(s);
        inst_cards_layout_->insertWidget(inst_cards_layout_->count() - 1, card);
        ++shown;
    }

    if (shown == 0) {
        auto* empty = new QLabel(
            servers.empty()
                ? "No servers installed yet.\nUse MARKETPLACE to add one, or click ADD CUSTOM MCP SERVER below."
                : "No servers match the search.");
        empty->setObjectName("srvCardDesc");
        empty->setAlignment(Qt::AlignCenter);
        empty->setWordWrap(true);
        inst_cards_layout_->insertWidget(0, empty);
    }
}

void McpServersScreen::refresh_tools() {
    disconnect(tools_table_, &QTableWidget::cellChanged, this, &McpServersScreen::on_tool_enabled_changed);

    tools_table_->setSortingEnabled(false);
    tools_table_->setRowCount(0);

    const auto internal_tools = McpProvider::instance().list_all_tools();
    const auto external_tools = McpManager::instance().get_all_external_tools();
    const int total = static_cast<int>(internal_tools.size() + external_tools.size());
    tools_table_->setRowCount(total);

    int row = 0;
    for (const auto& t : internal_tools) {
        const bool en = McpProvider::instance().is_tool_enabled(t.name);
        auto* chk = new QTableWidgetItem;
        chk->setCheckState(en ? Qt::Checked : Qt::Unchecked);
        chk->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        tools_table_->setItem(row, 0, chk);
        tools_table_->setItem(row, 1, new QTableWidgetItem(t.name));
        auto* si = new QTableWidgetItem("Fincept Terminal");
        si->setData(Qt::UserRole, QString(INTERNAL_SERVER_ID));
        si->setForeground(QColor(colors::AMBER()));
        tools_table_->setItem(row, 2, si);
        tools_table_->setItem(row, 3, new QTableWidgetItem("internal"));
        tools_table_->setItem(row, 4, new QTableWidgetItem(t.description));
        tools_table_->setRowHeight(row, 26);
        ++row;
    }
    for (const auto& t : external_tools) {
        auto* chk = new QTableWidgetItem;
        chk->setCheckState(Qt::Checked);
        chk->setFlags(Qt::ItemIsEnabled);
        tools_table_->setItem(row, 0, chk);
        tools_table_->setItem(row, 1, new QTableWidgetItem(t.name));
        auto* si = new QTableWidgetItem(t.server_name);
        si->setData(Qt::UserRole, t.server_id);
        si->setForeground(QColor(colors::CYAN()));
        tools_table_->setItem(row, 2, si);
        tools_table_->setItem(row, 3, new QTableWidgetItem("external"));
        tools_table_->setItem(row, 4, new QTableWidgetItem(t.description));
        tools_table_->setRowHeight(row, 26);
        ++row;
    }

    tools_table_->resizeColumnToContents(1);
    tools_table_->resizeColumnToContents(2);
    tools_table_->resizeColumnToContents(3);
    tools_table_->setSortingEnabled(true);
    tools_count_->setText(QString("%1 tools  (%2 internal · %3 external)")
                              .arg(total)
                              .arg(internal_tools.size())
                              .arg(external_tools.size()));

    connect(tools_table_, &QTableWidget::cellChanged, this, &McpServersScreen::on_tool_enabled_changed);
}

void McpServersScreen::update_status_bar() {
    const auto servers = McpManager::instance().get_servers();
    int running = 0;
    for (const auto& s : servers)
        if (s.status == ServerStatus::Running)
            ++running;
    status_count_->setText(QString::number(servers.size()) + " servers");
    status_running_->setText(QString::number(running) + " running");
}

// ── IStatefulScreen ───────────────────────────────────────────────────────────

} // namespace fincept::screens
