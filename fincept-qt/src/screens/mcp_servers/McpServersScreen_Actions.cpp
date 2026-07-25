// src/screens/mcp_servers/McpServersScreen_Actions.cpp
// User-action handlers: on_view_changed, install/start/stop/remove server,
// toggle_autostart, refresh, search, view_logs, add_server, tool toggle,
// refresh_installed, refresh_tools, update_status_bar.
// Part of the partial-class split of McpServersScreen.cpp.

#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "mcp/McpManager.h"
#include "mcp/McpMarketplace.h"
#include "mcp/McpProvider.h"
#include "screens/mcp_servers/McpServersScreen.h"
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

// NOTE: the screen stylesheet (kStyle) lives in McpServersScreen.cpp and is
// applied once to the whole screen. This translation unit used to carry a
// byte-identical [[maybe_unused]] copy (~120 lines) that nothing referenced.

} // namespace

namespace fincept::screens {

using namespace fincept::mcp;
using namespace fincept::ui;

namespace {

/// Split a command-line argument string the way a shell would: whitespace
/// separates, single/double quotes group. `args_edit->text().split(' ')` broke
/// every path containing a space (e.g. --dir "C:\Program Files\x").
QStringList split_args(const QString& text) {
    QStringList out;
    QString current;
    QChar quote;
    bool in_token = false;
    for (const QChar c : text) {
        if (!quote.isNull()) {
            if (c == quote)
                quote = QChar();
            else
                current += c;
            continue;
        }
        if (c == QLatin1Char('"') || c == QLatin1Char('\'')) {
            quote = c;
            in_token = true;
            continue;
        }
        if (c.isSpace()) {
            if (in_token) {
                out << current;
                current.clear();
                in_token = false;
            }
            continue;
        }
        current += c;
        in_token = true;
    }
    if (in_token)
        out << current;
    return out;
}

/// Environment variable names that should never be echoed on screen.
bool env_key_is_secret(const QString& key) {
    const QString k = key.toUpper();
    return k.contains(QLatin1String("KEY")) || k.contains(QLatin1String("TOKEN")) ||
           k.contains(QLatin1String("SECRET")) || k.contains(QLatin1String("PASSWORD")) ||
           k.contains(QLatin1String("CREDENTIAL")) || k.contains(QLatin1String("AUTH")) ||
           k.contains(QLatin1String("DSN")) || k.contains(QLatin1String("CONNECTION_STRING"));
}

} // namespace

void McpServersScreen::on_view_changed(int view) {
    active_view_ = view;
    view_stack_->setCurrentIndex(view);
    ScreenStateManager::instance().notify_changed(this);
    for (int i = 0; i < view_btns_.size(); ++i) {
        view_btns_[i]->setProperty("active", i == view);
        view_btns_[i]->style()->unpolish(view_btns_[i]);
        view_btns_[i]->style()->polish(view_btns_[i]);
    }
    const QStringList names = {tr("MARKETPLACE"), tr("INSTALLED"), tr("TOOLS")};
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
    dlg->setWindowTitle(tr("Add  %1").arg(e.name));
    dlg->setMinimumWidth(460);
    auto* form = new QFormLayout(dlg);
    form->setContentsMargins(16, 16, 16, 8);
    form->setSpacing(10);

    auto* name_edit = new QLineEdit(e.name);
    form->addRow(tr("Name"), name_edit);

    auto* desc_edit = new QLineEdit(e.description);
    form->addRow(tr("Description"), desc_edit);

    auto* cmd_edit = new QLineEdit(e.command);
    form->addRow(tr("Command"), cmd_edit);

    auto* args_edit = new QLineEdit(e.args.join(' '));
    form->addRow(tr("Arguments"), args_edit);

    // Env vars — one field per required key with sample placeholder
    QList<QPair<QString, QLineEdit*>> env_fields;
    if (!e.env_keys.isEmpty()) {
        auto* env_header = new QLabel(tr("Environment Variables"));
        env_header->setStyleSheet(QString("color:%1;font-size:9px;font-weight:700;").arg(colors::TEXT_SECONDARY()));
        form->addRow(env_header);
        for (int ki = 0; ki < e.env_keys.size(); ++ki) {
            const QString& key = e.env_keys[ki];
            auto* field = new QLineEdit;
            const bool secret = env_key_is_secret(key);
            if (secret) {
                // These are almost always API keys; don't render them in the
                // clear over the user's shoulder. Toggle reveals on demand.
                field->setEchoMode(QLineEdit::Password);
                field->setPlaceholderText(tr("Enter %1").arg(key));
            } else if (ki < e.env_placeholders.size()) {
                // Pre-fill with sample so user edits in-place rather than typing blind
                field->setText(e.env_placeholders[ki]);
                field->selectAll(); // highlight so first keystroke replaces it
            } else {
                field->setPlaceholderText(tr("Enter %1").arg(key));
            }
            field->setAccessibleName(key);
            // env-var key (e.g. "OPENAI_API_KEY") is the API contract — pass through verbatim.
            if (secret) {
                auto* row = new QWidget(dlg);
                auto* rl = new QHBoxLayout(row);
                rl->setContentsMargins(0, 0, 0, 0);
                rl->setSpacing(6);
                rl->addWidget(field, 1);
                auto* reveal = new QPushButton(tr("SHOW"), row);
                reveal->setCheckable(true);
                reveal->setCursor(Qt::PointingHandCursor);
                reveal->setAccessibleName(tr("Reveal %1").arg(key));
                QObject::connect(reveal, &QPushButton::toggled, field, [field, reveal](bool on) {
                    field->setEchoMode(on ? QLineEdit::Normal : QLineEdit::Password);
                    reveal->setText(on ? QObject::tr("HIDE") : QObject::tr("SHOW"));
                });
                rl->addWidget(reveal);
                form->addRow(key, row);
            } else {
                form->addRow(key, field);
            }
            env_fields.append({key, field});
        }
        auto* env_note = new QLabel(tr("Values are stored locally and handed to the server process as environment "
                                       "variables."));
        env_note->setWordWrap(true);
        env_note->setStyleSheet(QString("color:%1;font-size:9px;").arg(colors::TEXT_DIM()));
        form->addRow(env_note);
    }

    // Combo items stay as API keys (saved to McpServerConfig.category).
    auto* cat_combo = new QComboBox;
    cat_combo->addItems({"utilities", "developer", "database"});
    cat_combo->setCurrentText(e.category);
    form->addRow(tr("Category"), cat_combo);

    auto* autostart_check = new QCheckBox(tr("Auto-start on launch"));
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
    cfg.args = split_args(args_edit->text().trimmed());
    cfg.category = cat_combo->currentText();
    cfg.enabled = true;
    cfg.auto_start = autostart_check->isChecked();
    for (const auto& [key, field] : env_fields) {
        const QString val = field->text().trimmed();
        if (!val.isEmpty())
            cfg.env[key] = val;
    }

    // save_server() upserts on id, and the id is derived from the name — a
    // second server called the same thing silently replaced the first one.
    for (const auto& existing : McpManager::instance().get_servers()) {
        if (existing.id != cfg.id)
            continue;
        if (QMessageBox::question(
                this, tr("Replace Server"),
                tr("A server named \"%1\" already exists and will be overwritten.\n\nContinue?").arg(existing.name),
                QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes) {
            dlg->deleteLater();
            return;
        }
        break;
    }

    const auto r = McpManager::instance().save_server(cfg);
    if (r.is_ok()) {
        LOG_INFO("McpServers", "Added from catalog: " + name);
        populate_marketplace();
        on_view_changed(1);
    } else {
        const QString err = QString::fromStdString(r.error());
        LOG_ERROR("McpServers", "Add failed: " + err);
        QMessageBox::warning(this, tr("Add Server"), tr("Could not save \"%1\":\n\n%2").arg(name, err));
    }
    dlg->deleteLater();
}

// NOTE: start / stop / remove / autostart are driven from the per-server card
// (see build_server_card in McpServersScreen_Layout.cpp), which owns its own
// server id. The screen-level variants of those slots were never connected and
// keyed off a `selected_server_id_` that nothing ever assigned, so they have
// been removed rather than left as decoys.

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

void McpServersScreen::on_add_server() {
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle(tr("Add Custom MCP Server"));
    dlg->setMinimumWidth(460);
    auto* form = new QFormLayout(dlg);
    form->setContentsMargins(16, 16, 16, 8);
    form->setSpacing(10);

    auto* name_edit = new QLineEdit;
    name_edit->setPlaceholderText(tr("e.g. My Custom Server"));
    form->addRow(tr("Name"), name_edit);

    auto* desc_edit = new QLineEdit;
    desc_edit->setPlaceholderText(tr("Short description"));
    form->addRow(tr("Description"), desc_edit);

    // "uvx" is the binary name on PATH — not user-facing copy.
    auto* cmd_edit = new QLineEdit("uvx");
    form->addRow(tr("Command"), cmd_edit);

    auto* args_edit = new QLineEdit;
    args_edit->setPlaceholderText(tr("e.g. my-mcp-package --flag value"));
    form->addRow(tr("Arguments"), args_edit);

    auto* env_edit = new QLineEdit;
    // "KEY=value" is shell-syntax sample — keep ASCII so user knows the literal format.
    env_edit->setPlaceholderText(tr("KEY=value KEY2=value2"));
    env_edit->setEchoMode(QLineEdit::Password); // this field routinely holds API keys
    env_edit->setAccessibleName(tr("Environment variables"));
    form->addRow(tr("Env Vars"), env_edit);

    auto* env_reveal = new QCheckBox(tr("Show env values"));
    connect(env_reveal, &QCheckBox::toggled, env_edit,
            [env_edit](bool on) { env_edit->setEchoMode(on ? QLineEdit::Normal : QLineEdit::Password); });
    form->addRow("", env_reveal);

    auto* cat_combo = new QComboBox;
    cat_combo->addItems({"utilities", "developer", "database"});
    form->addRow(tr("Category"), cat_combo);

    auto* autostart_check = new QCheckBox(tr("Auto-start on launch"));
    autostart_check->setToolTip(tr("Launches this server at terminal startup without asking for confirmation."));
    form->addRow("", autostart_check);

    auto* warn = new QLabel(tr("This runs a program on your machine and exposes its tools to the assistant. "
                               "Only add servers you trust."));
    warn->setWordWrap(true);
    warn->setStyleSheet(QString("color:%1;font-size:9px;").arg(colors::WARNING()));
    form->addRow(warn);

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
    cfg.args = split_args(args_edit->text().trimmed());
    cfg.category = cat_combo->currentText();
    cfg.enabled = true;
    cfg.auto_start = autostart_check->isChecked();

    if (cfg.command.isEmpty()) {
        QMessageBox::warning(this, tr("Add Server"), tr("A command is required — this is the program to run."));
        dlg->deleteLater();
        return;
    }

    // Quote-aware so a value containing spaces survives (KEY="a b c").
    for (const auto& pair : split_args(env_edit->text().trimmed())) {
        const int eq = pair.indexOf('=');
        if (eq > 0)
            cfg.env[pair.left(eq)] = pair.mid(eq + 1);
    }

    for (const auto& existing : McpManager::instance().get_servers()) {
        if (existing.id != cfg.id)
            continue;
        if (QMessageBox::question(
                this, tr("Replace Server"),
                tr("A server named \"%1\" already exists and will be overwritten.\n\nContinue?").arg(existing.name),
                QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes) {
            dlg->deleteLater();
            return;
        }
        break;
    }

    const auto r = McpManager::instance().save_server(cfg);
    if (r.is_ok()) {
        LOG_INFO("McpServers", "Added custom server: " + name);
        refresh_installed();
        update_status_bar();
    } else {
        const QString err = QString::fromStdString(r.error());
        LOG_ERROR("McpServers", "Failed: " + err);
        QMessageBox::warning(this, tr("Add Server"), tr("Could not save \"%1\":\n\n%2").arg(name, err));
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
                ? tr("No servers installed yet.\nUse MARKETPLACE to add one, or click ADD CUSTOM MCP SERVER below.")
                : tr("No servers match the search."));
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
        // "Fincept Terminal" is the product name — kept in English for brand consistency.
        auto* si = new QTableWidgetItem(QStringLiteral("Fincept Terminal"));
        si->setData(Qt::UserRole, QString(INTERNAL_SERVER_ID));
        si->setForeground(QColor(colors::AMBER()));
        tools_table_->setItem(row, 2, si);
        tools_table_->setItem(row, 3, new QTableWidgetItem(tr("internal")));
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
        tools_table_->setItem(row, 3, new QTableWidgetItem(tr("external")));
        tools_table_->setItem(row, 4, new QTableWidgetItem(t.description));
        tools_table_->setRowHeight(row, 26);
        ++row;
    }

    tools_table_->resizeColumnToContents(1);
    tools_table_->resizeColumnToContents(2);
    tools_table_->resizeColumnToContents(3);
    tools_table_->setSortingEnabled(true);
    tools_count_->setText(
        tr("%1 tools  (%2 internal · %3 external)").arg(total).arg(internal_tools.size()).arg(external_tools.size()));

    connect(tools_table_, &QTableWidget::cellChanged, this, &McpServersScreen::on_tool_enabled_changed);
}

void McpServersScreen::update_status_bar() {
    const auto servers = McpManager::instance().get_servers();
    int running = 0;
    for (const auto& s : servers)
        if (s.status == ServerStatus::Running)
            ++running;
    status_count_->setText(tr("%1 servers").arg(servers.size()));
    status_running_->setText(tr("%1 running").arg(running));
}

// ── IStatefulScreen ───────────────────────────────────────────────────────────

} // namespace fincept::screens
