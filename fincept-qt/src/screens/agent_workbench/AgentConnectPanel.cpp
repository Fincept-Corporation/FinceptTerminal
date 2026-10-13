// AgentConnectPanel.cpp — CONNECT tab: external MCP server + client setup.

#include "screens/agent_workbench/AgentConnectPanel.h"

#include "screens/agent_workbench/AgentWorkbenchCommon.h"
#include "services/coding_agents/CodingAgentService.h"
#include "ui/theme/Theme.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QClipboard>
#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>

namespace fincept::screens {

namespace ca = services::coding_agents;
using mcp::McpHttpServer;

namespace {

QLabel* conn_label(const char* object_name, QWidget* parent) {
    auto* l = new QLabel(parent);
    l->setObjectName(QString::fromLatin1(object_name));
    return l;
}

QPushButton* conn_button(const char* object_name, QWidget* parent) {
    auto* b = new QPushButton(parent);
    b->setObjectName(QString::fromLatin1(object_name));
    b->setCursor(Qt::PointingHandCursor);
    return b;
}

QPlainTextEdit* snippet_box(QWidget* parent) {
    auto* t = new QPlainTextEdit(parent);
    t->setObjectName(QStringLiteral("awbSnippet"));
    t->setReadOnly(true);
    t->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    t->setMaximumHeight(96);
    return t;
}

} // namespace

// ═════════════════════════════════════════════════════════════════════════════
// Activity model
// ═════════════════════════════════════════════════════════════════════════════

AgentActivityModel::AgentActivityModel(QObject* parent) : QAbstractTableModel(parent) {}

int AgentActivityModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

int AgentActivityModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : ColCount;
}

QVariant AgentActivityModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    switch (section) {
        case ColTime:
            return tr("TIME");
        case ColClient:
            return tr("CLIENT");
        case ColTool:
            return tr("TOOL");
        case ColOutcome:
            return tr("OUTCOME");
        case ColDuration:
            return tr("DURATION");
        default:
            return {};
    }
}

QVariant AgentActivityModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= rows_.size())
        return {};
    const mcp::ExternalCallRecord& r = rows_[index.row()];
    const int col = index.column();
    if (role == Qt::DisplayRole) {
        switch (col) {
            case ColTime:
                return r.at.toLocalTime().toString(QStringLiteral("HH:mm:ss"));
            case ColClient:
                return r.client.isEmpty() ? tr("unknown") : r.client;
            case ColTool:
                return r.tool;
            case ColOutcome: {
                const QString state = r.denied ? tr("DENIED") : (r.success ? tr("OK") : tr("ERROR"));
                return r.summary.isEmpty() ? state : state + QStringLiteral(" · ") + r.summary;
            }
            case ColDuration:
                return tr("%1 ms").arg(r.duration_ms);
            default:
                return {};
        }
    }
    if (role == Qt::ForegroundRole && col == ColOutcome)
        return QColor(r.denied ? ui::colors::WARNING() : (r.success ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()));
    if (role == Qt::ForegroundRole)
        return QColor(ui::colors::TEXT_SECONDARY());
    if (role == Qt::ToolTipRole) {
        if (col == ColTime)
            return r.at.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        if (col == ColOutcome)
            return r.summary;
    }
    if (role == Qt::TextAlignmentRole && col == ColDuration)
        return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
    return {};
}

void AgentActivityModel::reset_records(const QList<mcp::ExternalCallRecord>& records) {
    beginResetModel();
    rows_ = records.mid(0, kMaxRows);
    endResetModel();
}

void AgentActivityModel::prepend_records(const QList<mcp::ExternalCallRecord>& records) {
    if (records.isEmpty())
        return;
    const int n = static_cast<int>(qMin<qsizetype>(records.size(), kMaxRows));
    beginInsertRows({}, 0, n - 1);
    for (int i = n - 1; i >= 0; --i)
        rows_.prepend(records[i]);
    endInsertRows();
    if (rows_.size() > kMaxRows) {
        beginRemoveRows({}, kMaxRows, static_cast<int>(rows_.size()) - 1);
        rows_.erase(rows_.begin() + kMaxRows, rows_.end());
        endRemoveRows();
    }
}

void AgentActivityModel::retranslate_headers() {
    emit headerDataChanged(Qt::Horizontal, 0, ColCount - 1);
}

// ═════════════════════════════════════════════════════════════════════════════
// Panel
// ═════════════════════════════════════════════════════════════════════════════

AgentConnectPanel::AgentConnectPanel(QWidget* parent) : QWidget(parent) {
    activity_timer_ = new QTimer(this);
    activity_timer_->setSingleShot(true);
    activity_timer_->setInterval(250);
    connect(activity_timer_, &QTimer::timeout, this, &AgentConnectPanel::flush_activity);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    root->addWidget(scroll);

    auto* page = new QWidget(scroll);
    page->setObjectName(QStringLiteral("awbPage"));
    auto* v = new QVBoxLayout(page);
    v->setContentsMargins(12, 10, 12, 12);
    v->setSpacing(10);

    v->addWidget(build_server_section(page));

    clients_title_ = conn_label("awbSection", page);
    v->addWidget(clients_title_);
    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(10);

    claude_snippet_ = new QLineEdit(page);
    claude_snippet_->setObjectName(QStringLiteral("awbSnippet"));
    claude_snippet_->setReadOnly(true);
    grid->addWidget(build_client_card(page, &claude_title_, &claude_desc_, claude_snippet_, &claude_copy_btn_,
                                      &claude_add_btn_, &claude_result_),
                    0, 0);

    codex_snippet_ = new QLineEdit(page);
    codex_snippet_->setObjectName(QStringLiteral("awbSnippet"));
    codex_snippet_->setReadOnly(true);
    QWidget* codex_card = build_client_card(page, &codex_title_, &codex_desc_, codex_snippet_, &codex_copy_btn_,
                                            &codex_add_btn_, &codex_result_);
    {
        auto* lay = qobject_cast<QVBoxLayout*>(codex_card->layout());
        codex_toml_lbl_ = conn_label("awbHint", codex_card);
        codex_toml_lbl_->setWordWrap(true);
        codex_toml_ = snippet_box(codex_card);
        codex_toml_copy_btn_ = conn_button("awbBtn", codex_card);
        connect(codex_toml_copy_btn_, &QPushButton::clicked, this,
                []() { copy_text(McpHttpServer::instance().codex_config_toml()); });
        if (lay) {
            lay->addWidget(codex_toml_lbl_);
            lay->addWidget(codex_toml_);
            lay->addWidget(codex_toml_copy_btn_, 0, Qt::AlignLeft);
        }
    }
    grid->addWidget(codex_card, 0, 1);

    http_snippet_ = snippet_box(page);
    grid->addWidget(build_client_card(page, &http_title_, &http_desc_, http_snippet_, &http_copy_btn_, nullptr, nullptr),
                    1, 0);
    desktop_snippet_ = snippet_box(page);
    grid->addWidget(build_client_card(page, &desktop_title_, &desktop_desc_, desktop_snippet_, &desktop_copy_btn_,
                                      nullptr, nullptr),
                    1, 1);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    v->addLayout(grid);

    connect(claude_copy_btn_, &QPushButton::clicked, this,
            []() { copy_text(McpHttpServer::instance().claude_code_add_command()); });
    connect(codex_copy_btn_, &QPushButton::clicked, this,
            []() { copy_text(McpHttpServer::instance().codex_add_command()); });
    connect(http_copy_btn_, &QPushButton::clicked, this,
            []() { copy_text(McpHttpServer::instance().generic_http_json()); });
    connect(desktop_copy_btn_, &QPushButton::clicked, this,
            []() { copy_text(McpHttpServer::instance().stdio_bridge_json()); });
    connect(claude_add_btn_, &QPushButton::clicked, this, [this]() { on_register(false); });
    connect(codex_add_btn_, &QPushButton::clicked, this, [this]() { on_register(true); });

    activity_title_ = conn_label("awbSection", page);
    v->addWidget(activity_title_);
    activity_model_ = new AgentActivityModel(this);
    activity_table_ = new QTableView(page);
    activity_table_->setModel(activity_model_);
    activity_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    activity_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    activity_table_->setAlternatingRowColors(true);
    activity_table_->setShowGrid(false);
    activity_table_->setWordWrap(false);
    activity_table_->verticalHeader()->hide();
    activity_table_->verticalHeader()->setDefaultSectionSize(24);
    activity_table_->setMinimumHeight(220);
    auto* hh = activity_table_->horizontalHeader();
    hh->setSectionResizeMode(QHeaderView::Interactive);
    hh->setSectionResizeMode(AgentActivityModel::ColOutcome, QHeaderView::Stretch);
    hh->resizeSection(AgentActivityModel::ColTime, 90);
    hh->resizeSection(AgentActivityModel::ColClient, 160);
    hh->resizeSection(AgentActivityModel::ColTool, 220);
    hh->resizeSection(AgentActivityModel::ColDuration, 90);
    v->addWidget(activity_table_, 1);
    scroll->setWidget(page);

    // Registration results can arrive after the user left the tab — keep this
    // connection for the panel's lifetime (it is rare and cheap).
    connect(&ca::CodingAgentService::instance(), &ca::CodingAgentService::client_registration_finished, this,
            &AgentConnectPanel::on_registration_finished);

    retranslate();
}

QWidget* AgentConnectPanel::build_server_section(QWidget* parent) {
    auto* card = new QFrame(parent);
    card->setObjectName(QStringLiteral("awbCard"));
    auto* v = new QVBoxLayout(card);
    v->setContentsMargins(12, 10, 12, 12);
    v->setSpacing(8);

    server_title_ = conn_label("awbSection", card);
    v->addWidget(server_title_);

    auto* status_row = new QHBoxLayout;
    status_row->setSpacing(6);
    state_lbl_ = conn_label("awbServerState", card);
    status_row->addWidget(state_lbl_);
    url_edit_ = new QLineEdit(card);
    url_edit_->setObjectName(QStringLiteral("awbSnippet"));
    url_edit_->setReadOnly(true);
    status_row->addWidget(url_edit_, 1);
    copy_url_btn_ = conn_button("awbBtn", card);
    connect(copy_url_btn_, &QPushButton::clicked, this, []() { copy_text(McpHttpServer::instance().url()); });
    status_row->addWidget(copy_url_btn_);
    start_btn_ = conn_button("awbPrimaryBtn", card);
    connect(start_btn_, &QPushButton::clicked, this, &AgentConnectPanel::on_start);
    status_row->addWidget(start_btn_);
    stop_btn_ = conn_button("awbDangerBtn", card);
    connect(stop_btn_, &QPushButton::clicked, this, &AgentConnectPanel::on_stop);
    status_row->addWidget(stop_btn_);
    v->addLayout(status_row);

    auto* info_row = new QHBoxLayout;
    clients_lbl_ = conn_label("awbHint", card);
    info_row->addWidget(clients_lbl_);
    error_lbl_ = conn_label("awbErrorBanner", card);
    error_lbl_->setWordWrap(true);
    error_lbl_->hide();
    info_row->addWidget(error_lbl_, 1);
    v->addLayout(info_row);

    autostart_check_ = new QCheckBox(card);
    connect(autostart_check_, &QCheckBox::toggled, this,
            [](bool on) { McpHttpServer::instance().set_enabled_at_startup(on); });
    v->addWidget(autostart_check_);

    auto* toolset_row = new QHBoxLayout;
    toolset_row->setSpacing(12);
    toolset_lbl_ = conn_label("awbLabel", card);
    toolset_row->addWidget(toolset_lbl_);
    essentials_radio_ = new QRadioButton(card);
    full_radio_ = new QRadioButton(card);
    auto* toolset_group = new QButtonGroup(this);
    toolset_group->addButton(essentials_radio_);
    toolset_group->addButton(full_radio_);
    connect(essentials_radio_, &QRadioButton::toggled, this, [](bool on) {
        McpHttpServer::instance().set_toolset(on ? mcp::ExternalToolset::Essentials : mcp::ExternalToolset::Full);
    });
    toolset_row->addWidget(essentials_radio_);
    toolset_row->addWidget(full_radio_);
    toolset_row->addStretch(1);
    v->addLayout(toolset_row);

    actions_check_ = new QCheckBox(card);
    actions_check_->setObjectName(QStringLiteral("awbDangerToggle"));
    connect(actions_check_, &QCheckBox::toggled, this, &AgentConnectPanel::on_actions_toggled);
    v->addWidget(actions_check_);
    actions_hint_ = conn_label("awbHint", card);
    actions_hint_->setWordWrap(true);
    v->addWidget(actions_hint_);

    auto* port_row = new QHBoxLayout;
    port_row->setSpacing(8);
    port_lbl_ = conn_label("awbLabel", card);
    port_row->addWidget(port_lbl_);
    port_spin_ = new QSpinBox(card);
    port_spin_->setRange(1024, 65535);
    port_spin_->setValue(McpHttpServer::kDefaultPort);
    connect(port_spin_, &QSpinBox::editingFinished, this, [this]() {
        McpHttpServer::instance().set_configured_port(static_cast<quint16>(port_spin_->value()));
    });
    port_row->addWidget(port_spin_);
    port_hint_ = conn_label("awbHint", card);
    port_row->addWidget(port_hint_, 1);
    v->addLayout(port_row);

    auto* token_row = new QHBoxLayout;
    token_row->setSpacing(6);
    token_lbl_ = conn_label("awbLabel", card);
    token_row->addWidget(token_lbl_);
    token_edit_ = new QLineEdit(card);
    token_edit_->setObjectName(QStringLiteral("awbSnippet"));
    token_edit_->setReadOnly(true);
    token_edit_->setEchoMode(QLineEdit::Password);
    token_row->addWidget(token_edit_, 1);
    show_token_btn_ = conn_button("awbBtn", card);
    connect(show_token_btn_, &QPushButton::clicked, this, [this]() {
        show_token_ = !show_token_;
        refresh_status();
        refresh_snippets();
        retranslate();
    });
    token_row->addWidget(show_token_btn_);
    copy_token_btn_ = conn_button("awbBtn", card);
    connect(copy_token_btn_, &QPushButton::clicked, this, []() { copy_text(McpHttpServer::instance().token()); });
    token_row->addWidget(copy_token_btn_);
    regen_btn_ = conn_button("awbDangerBtn", card);
    connect(regen_btn_, &QPushButton::clicked, this, &AgentConnectPanel::on_regenerate);
    token_row->addWidget(regen_btn_);
    v->addLayout(token_row);
    return card;
}

QWidget* AgentConnectPanel::build_client_card(QWidget* parent, QLabel** title, QLabel** desc, QWidget* snippet,
                                              QPushButton** copy, QPushButton** action, QLabel** result) {
    auto* card = new QFrame(parent);
    card->setObjectName(QStringLiteral("awbCard"));
    auto* v = new QVBoxLayout(card);
    v->setContentsMargins(12, 10, 12, 12);
    v->setSpacing(6);
    *title = conn_label("awbCardTitle", card);
    v->addWidget(*title);
    *desc = conn_label("awbHint", card);
    (*desc)->setWordWrap(true);
    v->addWidget(*desc);
    snippet->setParent(card);
    v->addWidget(snippet);
    auto* row = new QHBoxLayout;
    row->setSpacing(6);
    *copy = conn_button("awbBtn", card);
    row->addWidget(*copy);
    if (action) {
        *action = conn_button("awbPrimaryBtn", card);
        row->addWidget(*action);
    }
    if (result) {
        *result = conn_label("awbResult", card);
        (*result)->setWordWrap(true);
        (*result)->setTextInteractionFlags(Qt::TextSelectableByMouse);
        row->addWidget(*result, 1);
    } else {
        row->addStretch(1);
    }
    v->addLayout(row);
    v->addStretch(1);
    return card;
}

void AgentConnectPanel::retranslate() {
    server_title_->setText(tr("FINCEPT MCP SERVER — LET YOUR OWN AGENTS USE THIS TERMINAL"));
    copy_url_btn_->setText(tr("COPY URL"));
    start_btn_->setText(tr("START"));
    stop_btn_->setText(tr("STOP"));
    autostart_check_->setText(tr("Start with the terminal"));
    toolset_lbl_->setText(tr("TOOLSET"));
    essentials_radio_->setText(tr("Essentials (recommended)"));
    essentials_radio_->setToolTip(tr("Discovery + job tools and a curated set of research tools. Everything else stays "
                                     "reachable through tool_list and tool_invoke."));
    full_radio_->setText(tr("Full catalogue (~900 tools)"));
    full_radio_->setToolTip(tr("Advertise every tool. Only for clients that search large tool lists."));
    actions_check_->setText(tr("Allow external agents to run actions (orders, files, settings)"));
    actions_hint_->setText(tr("Off by default. When on, connected agents may call tools that change state. Every "
                              "live-trading action still needs your approval in the terminal."));
    port_lbl_->setText(tr("PORT"));
    token_lbl_->setText(tr("ACCESS TOKEN"));
    show_token_btn_->setText(show_token_ ? tr("HIDE") : tr("SHOW"));
    copy_token_btn_->setText(tr("COPY"));
    regen_btn_->setText(tr("REGENERATE"));
    regen_btn_->setToolTip(tr("Mint a new token — every connected client must be updated"));

    clients_title_->setText(tr("CONNECT YOUR AGENTS"));
    claude_title_->setText(tr("CLAUDE CODE"));
    claude_desc_->setText(tr("Adds the terminal as an HTTP MCP server for your user (all projects). Then ask Claude "
                             "Code for quotes, filings, portfolios — tools appear as mcp__fincept_terminal__*."));
    claude_copy_btn_->setText(tr("COPY COMMAND"));
    claude_add_btn_->setText(tr("ADD TO CLAUDE CODE"));
    codex_title_->setText(tr("OPENAI CODEX CLI"));
    codex_desc_->setText(tr("Registers the terminal through the bundled stdio bridge (Python, standard library only)."));
    codex_copy_btn_->setText(tr("COPY COMMAND"));
    codex_add_btn_->setText(tr("ADD TO CODEX"));
    codex_toml_lbl_->setText(tr("Or add this to ~/.codex/config.toml and set FINCEPT_MCP_TOKEN in your environment:"));
    codex_toml_copy_btn_->setText(tr("COPY TOML"));
    http_title_->setText(tr("CURSOR / OTHER HTTP CLIENTS"));
    http_desc_->setText(tr("Paste into ~/.cursor/mcp.json or your client's MCP settings (Streamable HTTP)."));
    http_copy_btn_->setText(tr("COPY JSON"));
    desktop_title_->setText(tr("CLAUDE DESKTOP"));
    desktop_desc_->setText(tr("Paste into claude_desktop_config.json (Settings › Developer › Edit Config), then "
                              "restart Claude Desktop."));
    desktop_copy_btn_->setText(tr("COPY JSON"));

    activity_title_->setText(tr("EXTERNAL ACTIVITY — LAST 200 CALLS"));
    activity_model_->retranslate_headers();
    refresh_status();
}

void AgentConnectPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslate();
    QWidget::changeEvent(event);
}

void AgentConnectPanel::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    subscribe();
    refresh_status();
    refresh_snippets();
    activity_model_->reset_records(McpHttpServer::instance().recent_activity());
}

void AgentConnectPanel::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    unsubscribe();
    activity_timer_->stop();
    pending_activity_.clear();
}

void AgentConnectPanel::subscribe() {
    if (!conns_.isEmpty())
        return;
    auto& srv = McpHttpServer::instance();
    conns_ << connect(&srv, &McpHttpServer::status_changed, this, [this]() {
        refresh_status();
        refresh_snippets();
    });
    conns_ << connect(&srv, &McpHttpServer::activity, this, [this](const mcp::ExternalCallRecord& record) {
        pending_activity_.prepend(record);
        if (!activity_timer_->isActive())
            activity_timer_->start();
    });
}

void AgentConnectPanel::unsubscribe() {
    for (const auto& c : std::as_const(conns_))
        disconnect(c);
    conns_.clear();
}

void AgentConnectPanel::flush_activity() {
    activity_model_->prepend_records(pending_activity_);
    pending_activity_.clear();
}

QString AgentConnectPanel::masked(const QString& text) const {
    if (show_token_)
        return text;
    const QString token = McpHttpServer::instance().token();
    if (token.isEmpty())
        return text;
    QString out = text;
    out.replace(token, QStringLiteral("••••••••"));
    return out;
}

void AgentConnectPanel::copy_text(const QString& text) {
    if (!text.isEmpty())
        QGuiApplication::clipboard()->setText(text);
}

void AgentConnectPanel::refresh_status() {
    auto& srv = McpHttpServer::instance();
    const bool running = srv.is_running();
    const QString err = srv.last_error();
    const QString state = running ? QStringLiteral("running") : (err.isEmpty() ? QStringLiteral("stopped")
                                                                               : QStringLiteral("error"));
    state_lbl_->setText(running ? tr("● RUNNING") : (err.isEmpty() ? tr("● STOPPED") : tr("● ERROR")));
    if (state_lbl_->property("state").toString() != state) {
        state_lbl_->setProperty("state", state);
        awb_repolish(state_lbl_);
    }
    url_edit_->setText(running ? srv.url() : tr("not listening"));
    copy_url_btn_->setEnabled(running);
    start_btn_->setEnabled(!running);
    stop_btn_->setEnabled(running);
    clients_lbl_->setText(tr("%1 client(s) connected since start").arg(srv.connected_client_count()));
    error_lbl_->setVisible(!err.isEmpty());
    error_lbl_->setText(err);
    {
        const QSignalBlocker b(autostart_check_);
        autostart_check_->setChecked(srv.enabled_at_startup());
    }
    {
        const QSignalBlocker b1(essentials_radio_);
        const QSignalBlocker b2(full_radio_);
        const bool essentials = srv.toolset() == mcp::ExternalToolset::Essentials;
        essentials_radio_->setChecked(essentials);
        full_radio_->setChecked(!essentials);
    }
    {
        const QSignalBlocker b(actions_check_);
        actions_check_->setChecked(srv.allow_actions());
    }
    if (!port_spin_->hasFocus()) {
        const QSignalBlocker b(port_spin_);
        port_spin_->setValue(srv.configured_port());
    }
    if (running && srv.port() != srv.configured_port())
        port_hint_->setText(tr("Listening on %1 (the configured port was busy). Changes apply on the next start.")
                                .arg(srv.port()));
    else
        port_hint_->setText(tr("Changes apply the next time the server starts."));
    token_edit_->setEchoMode(show_token_ ? QLineEdit::Normal : QLineEdit::Password);
    token_edit_->setText(srv.token());
}

void AgentConnectPanel::refresh_snippets() {
    auto& srv = McpHttpServer::instance();
    claude_snippet_->setText(masked(srv.claude_code_add_command()));
    codex_snippet_->setText(masked(srv.codex_add_command()));
    codex_toml_->setPlainText(masked(srv.codex_config_toml()));
    http_snippet_->setPlainText(masked(srv.generic_http_json()));
    desktop_snippet_->setPlainText(masked(srv.stdio_bridge_json()));
    auto& svc = ca::CodingAgentService::instance();
    const bool claude_ok = svc.cli_info(ca::AgentKind::Claude).installed();
    const bool codex_ok = svc.cli_info(ca::AgentKind::Codex).installed();
    claude_add_btn_->setToolTip(claude_ok ? QString() : tr("Claude Code CLI not found on this computer"));
    codex_add_btn_->setToolTip(codex_ok ? QString() : tr("Codex CLI not found on this computer"));
}

void AgentConnectPanel::on_start() {
    auto& srv = McpHttpServer::instance();
    if (!srv.start())
        QMessageBox::warning(this, tr("MCP server"),
                             tr("The server could not start: %1").arg(srv.last_error().isEmpty() ? tr("unknown error")
                                                                                                 : srv.last_error()));
    refresh_status();
    refresh_snippets();
}

void AgentConnectPanel::on_stop() {
    if (ca::CodingAgentService::instance().has_active_tool_runs()) {
        const auto answer = QMessageBox::question(
            this, tr("Stop MCP server"),
            tr("Agent runs in the workbench are using the terminal tools. Stopping the server cuts them off. Stop "
               "anyway?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
    }
    McpHttpServer::instance().stop();
    refresh_status();
}

void AgentConnectPanel::on_regenerate() {
    const auto answer = QMessageBox::question(
        this, tr("Regenerate access token"),
        tr("Mint a new access token? Every connected client (Claude Code, Codex, Cursor, Claude Desktop) stops working "
           "until it is updated with the new token."),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;
    McpHttpServer::instance().regenerate_token();
    refresh_status();
    refresh_snippets();
}

void AgentConnectPanel::on_actions_toggled(bool on) {
    if (on) {
        const auto answer = QMessageBox::warning(
            this, tr("Allow external actions?"),
            tr("Agents connected to this terminal will be able to call tools that change state — place or cancel "
               "orders, write files, change settings.\n\nLive-trading actions still ask for your approval in the "
               "terminal each time.\n\nOnly enable this if you trust every client that holds the access token."),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
        if (answer != QMessageBox::Yes) {
            const QSignalBlocker b(actions_check_);
            actions_check_->setChecked(false);
            return;
        }
    }
    McpHttpServer::instance().set_allow_actions(on);
}

void AgentConnectPanel::on_register(bool codex) {
    QPushButton* btn = codex ? codex_add_btn_ : claude_add_btn_;
    QLabel* result = codex ? codex_result_ : claude_result_;
    (codex ? awaiting_codex_ : awaiting_claude_) = true;
    btn->setEnabled(false);
    result->setText(tr("Working…"));
    result->setProperty("state", QString());
    awb_repolish(result);
    ca::CodingAgentService::instance().register_with_client(codex ? ca::AgentKind::Codex : ca::AgentKind::Claude,
                                                            false);
}

void AgentConnectPanel::on_registration_finished(const QString& agent, bool ok, bool already_exists,
                                                 const QString& message) {
    const bool codex = agent == ca::agent_id(ca::AgentKind::Codex);
    bool& awaiting = codex ? awaiting_codex_ : awaiting_claude_;
    if (!awaiting)
        return; // another window's workbench asked
    QPushButton* btn = codex ? codex_add_btn_ : claude_add_btn_;
    QLabel* result = codex ? codex_result_ : claude_result_;
    const QString product = codex ? tr("Codex") : tr("Claude Code");
    if (already_exists) {
        const auto answer = QMessageBox::question(
            this, tr("Replace existing server?"),
            tr("%1 already has an MCP server named “fincept_terminal”:\n\n%2\n\nReplace it with this terminal's server?")
                .arg(product, message.left(1200)),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer == QMessageBox::Yes) {
            result->setText(tr("Replacing…")); // still awaiting the second result
            ca::CodingAgentService::instance().register_with_client(codex ? ca::AgentKind::Codex
                                                                          : ca::AgentKind::Claude,
                                                                    true);
            return;
        }
        awaiting = false;
        btn->setEnabled(true);
        result->setText(tr("Kept the existing entry."));
        result->setProperty("state", QString());
        awb_repolish(result);
        return;
    }
    awaiting = false;
    btn->setEnabled(true);
    result->setText(ok ? tr("Added. Restart %1 sessions to pick it up.").arg(product) : message);
    result->setToolTip(message);
    result->setProperty("state", ok ? QStringLiteral("ok") : QStringLiteral("error"));
    awb_repolish(result);
}

} // namespace fincept::screens
