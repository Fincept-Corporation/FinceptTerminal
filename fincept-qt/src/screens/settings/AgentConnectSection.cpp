// AgentConnectSection.cpp — Settings › AI Agents (MCP). See the header.

#include "screens/settings/AgentConnectSection.h"

#include "mcp/McpProvider.h"
#include "mcp/core/CoreTools.h"
#include "services/coding_agents/CodingAgentService.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QCheckBox>
#include <QClipboard>
#include <QColor>
#include <QComboBox>
#include <QDateTime>
#include <QEvent>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStyle>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>

namespace fincept::screens {

namespace ca = services::coding_agents;
using mcp::McpHttpServer;

namespace agent_connect_detail {

QLabel* make_label(const char* object_name, QWidget* parent, bool wrap = false) {
    auto* l = new QLabel(parent);
    l->setObjectName(QString::fromLatin1(object_name));
    l->setWordWrap(wrap);
    return l;
}

QPushButton* make_button(const char* object_name, QWidget* parent) {
    auto* b = new QPushButton(parent);
    b->setObjectName(QString::fromLatin1(object_name));
    b->setCursor(Qt::PointingHandCursor);
    return b;
}

QFrame* make_card(QWidget* parent) {
    auto* card = new QFrame(parent);
    card->setObjectName(QStringLiteral("acCard"));
    return card;
}

QPlainTextEdit* make_snippet(QWidget* parent) {
    auto* t = new QPlainTextEdit(parent);
    t->setObjectName(QStringLiteral("acSnippet"));
    t->setReadOnly(true);
    t->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    t->setMaximumHeight(120);
    // The token appears here when SHOW is on — never let agents read it off the page.
    t->setProperty("fincept_secret", true);
    return t;
}

QString duration_text(qint64 secs) {
    if (secs < 60)
        return QStringLiteral("%1 s").arg(secs);
    if (secs < 3600)
        return QStringLiteral("%1 min").arg(secs / 60);
    return QStringLiteral("%1 h %2 min").arg(secs / 3600).arg((secs % 3600) / 60);
}

} // namespace agent_connect_detail

using namespace agent_connect_detail;

// ═════════════════════════════════════════════════════════════════════════════
// Activity model
// ═════════════════════════════════════════════════════════════════════════════

McpActivityModel::McpActivityModel(QObject* parent) : QAbstractTableModel(parent) {}

int McpActivityModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

int McpActivityModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : ColCount;
}

QVariant McpActivityModel::headerData(int section, Qt::Orientation orientation, int role) const {
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

QVariant McpActivityModel::data(const QModelIndex& index, int role) const {
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

void McpActivityModel::reset_records(const QList<mcp::ExternalCallRecord>& records) {
    beginResetModel();
    rows_ = records.mid(0, kMaxRows);
    endResetModel();
}

void McpActivityModel::prepend_records(const QList<mcp::ExternalCallRecord>& records) {
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

void McpActivityModel::retranslate_headers() {
    emit headerDataChanged(Qt::Horizontal, 0, ColCount - 1);
}

// ═════════════════════════════════════════════════════════════════════════════
// Section
// ═════════════════════════════════════════════════════════════════════════════

AgentConnectSection::AgentConnectSection(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("agentConnectSection"));

    activity_timer_ = new QTimer(this);
    activity_timer_->setSingleShot(true);
    activity_timer_->setInterval(250);
    connect(activity_timer_, &QTimer::timeout, this, &AgentConnectSection::flush_activity);

    uptime_timer_ = new QTimer(this);
    uptime_timer_->setInterval(5000); // started in showEvent only (P3)
    connect(uptime_timer_, &QTimer::timeout, this, &AgentConnectSection::refresh_status);

    build_ui();
    apply_theme();
    retranslate();

    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this](const ui::ThemeTokens&) { apply_theme(); });
    // Registration results can arrive after the page was hidden — keep this one
    // connection for the widget's lifetime (rare and cheap).
    connect(&ca::CodingAgentService::instance(), &ca::CodingAgentService::client_registration_finished, this,
            &AgentConnectSection::on_registration_finished);
}

void AgentConnectSection::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    auto* scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("acScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    root->addWidget(scroll);

    auto* page = new QWidget(scroll);
    page->setObjectName(QStringLiteral("acPage"));
    auto* v = new QVBoxLayout(page);
    v->setContentsMargins(20, 16, 20, 20);
    v->setSpacing(12);

    title_ = make_label("acTitle", page);
    v->addWidget(title_);
    intro_ = make_label("acHint", page, true);
    v->addWidget(intro_);

    v->addWidget(build_server_card(page));
    v->addWidget(build_token_card(page));
    v->addWidget(build_permissions_card(page));
    v->addWidget(build_connect_card(page));
    v->addWidget(build_tools_card(page));
    v->addWidget(build_activity_card(page), 1);
    scroll->setWidget(page);
}

QWidget* AgentConnectSection::build_server_card(QWidget* parent) {
    QFrame* card = make_card(parent);
    auto* v = new QVBoxLayout(card);
    v->setContentsMargins(14, 12, 14, 14);
    v->setSpacing(8);
    server_title_ = make_label("acSection", card);
    v->addWidget(server_title_);

    auto* row = new QHBoxLayout;
    row->setSpacing(6);
    state_lbl_ = make_label("acState", card);
    row->addWidget(state_lbl_);
    url_edit_ = new QLineEdit(card);
    url_edit_->setObjectName(QStringLiteral("acField"));
    url_edit_->setReadOnly(true);
    row->addWidget(url_edit_, 1);
    copy_url_btn_ = make_button("acBtn", card);
    connect(copy_url_btn_, &QPushButton::clicked, this, []() { copy_text(McpHttpServer::instance().url()); });
    row->addWidget(copy_url_btn_);
    start_btn_ = make_button("acPrimaryBtn", card);
    connect(start_btn_, &QPushButton::clicked, this, &AgentConnectSection::on_start);
    row->addWidget(start_btn_);
    stop_btn_ = make_button("acDangerBtn", card);
    connect(stop_btn_, &QPushButton::clicked, this, &AgentConnectSection::on_stop);
    row->addWidget(stop_btn_);
    v->addLayout(row);

    stats_lbl_ = make_label("acHint", card, true);
    v->addWidget(stats_lbl_);
    error_lbl_ = make_label("acError", card, true);
    error_lbl_->hide();
    v->addWidget(error_lbl_);

    auto* opts = new QHBoxLayout;
    opts->setSpacing(10);
    autostart_check_ = new QCheckBox(card);
    autostart_check_->setObjectName(QStringLiteral("acCheck"));
    connect(autostart_check_, &QCheckBox::toggled, this,
            [](bool on) { McpHttpServer::instance().set_enabled_at_startup(on); });
    opts->addWidget(autostart_check_);
    opts->addStretch(1);
    port_lbl_ = make_label("acLabel", card);
    opts->addWidget(port_lbl_);
    port_spin_ = new QSpinBox(card);
    port_spin_->setObjectName(QStringLiteral("acSpin"));
    port_spin_->setRange(1024, 65535);
    port_spin_->setValue(McpHttpServer::kDefaultPort);
    connect(port_spin_, &QSpinBox::editingFinished, this,
            [this]() { McpHttpServer::instance().set_configured_port(static_cast<quint16>(port_spin_->value())); });
    opts->addWidget(port_spin_);
    v->addLayout(opts);
    port_hint_ = make_label("acHint", card, true);
    v->addWidget(port_hint_);
    return card;
}

QWidget* AgentConnectSection::build_token_card(QWidget* parent) {
    QFrame* card = make_card(parent);
    auto* v = new QVBoxLayout(card);
    v->setContentsMargins(14, 12, 14, 14);
    v->setSpacing(8);
    token_title_ = make_label("acSection", card);
    v->addWidget(token_title_);
    auto* row = new QHBoxLayout;
    row->setSpacing(6);
    token_edit_ = new QLineEdit(card);
    token_edit_->setObjectName(QStringLiteral("acField"));
    token_edit_->setReadOnly(true);
    token_edit_->setEchoMode(QLineEdit::Password);
    token_edit_->setProperty("fincept_secret", true);
    row->addWidget(token_edit_, 1);
    show_token_btn_ = make_button("acBtn", card);
    connect(show_token_btn_, &QPushButton::clicked, this, [this]() {
        show_token_ = !show_token_;
        refresh_status();
        refresh_snippet();
        retranslate();
    });
    row->addWidget(show_token_btn_);
    copy_token_btn_ = make_button("acBtn", card);
    connect(copy_token_btn_, &QPushButton::clicked, this, []() { copy_text(McpHttpServer::instance().token()); });
    row->addWidget(copy_token_btn_);
    regen_btn_ = make_button("acDangerBtn", card);
    connect(regen_btn_, &QPushButton::clicked, this, &AgentConnectSection::on_regenerate);
    row->addWidget(regen_btn_);
    v->addLayout(row);
    token_hint_ = make_label("acHint", card, true);
    v->addWidget(token_hint_);
    return card;
}

QWidget* AgentConnectSection::build_permissions_card(QWidget* parent) {
    QFrame* card = make_card(parent);
    auto* v = new QVBoxLayout(card);
    v->setContentsMargins(14, 12, 14, 14);
    v->setSpacing(8);
    perms_title_ = make_label("acSection", card);
    v->addWidget(perms_title_);
    look_lbl_ = make_label("acHint", card, true);
    v->addWidget(look_lbl_);
    ui_control_check_ = new QCheckBox(card);
    ui_control_check_->setObjectName(QStringLiteral("acCheck"));
    connect(ui_control_check_, &QCheckBox::toggled, this, &AgentConnectSection::on_ui_control_toggled);
    v->addWidget(ui_control_check_);
    scripts_check_ = new QCheckBox(card);
    scripts_check_->setObjectName(QStringLiteral("acCheck"));
    connect(scripts_check_, &QCheckBox::toggled, this, &AgentConnectSection::on_scripts_toggled);
    v->addWidget(scripts_check_);
    auto_check_ = new QCheckBox(card);
    auto_check_->setObjectName(QStringLiteral("acCheck"));
    connect(auto_check_, &QCheckBox::toggled, this, &AgentConnectSection::on_auto_toggled);
    v->addWidget(auto_check_);
    always_ask_lbl_ = make_label("acNote", card, true);
    v->addWidget(always_ask_lbl_);
    auto* row = new QHBoxLayout;
    budget_lbl_ = make_label("acLabel", card);
    row->addWidget(budget_lbl_);
    budget_combo_ = new QComboBox(card);
    budget_combo_->setObjectName(QStringLiteral("acCombo"));
    for (int kb : {16, 32, 64, 128})
        budget_combo_->addItem(QStringLiteral("%1 KB").arg(kb), kb);
    connect(budget_combo_, &QComboBox::activated, this, [this](int index) {
        McpHttpServer::instance().set_result_budget_kb(budget_combo_->itemData(index).toInt());
    });
    row->addWidget(budget_combo_);
    row->addStretch(1);
    v->addLayout(row);
    return card;
}

QWidget* AgentConnectSection::build_connect_card(QWidget* parent) {
    QFrame* card = make_card(parent);
    auto* v = new QVBoxLayout(card);
    v->setContentsMargins(14, 12, 14, 14);
    v->setSpacing(8);
    connect_title_ = make_label("acSection", card);
    v->addWidget(connect_title_);
    auto* row = new QHBoxLayout;
    client_lbl_ = make_label("acLabel", card);
    row->addWidget(client_lbl_);
    client_combo_ = new QComboBox(card);
    client_combo_->setObjectName(QStringLiteral("acCombo"));
    for (int i = 0; i < ClientCount; ++i)
        client_combo_->addItem(QString());
    connect(client_combo_, &QComboBox::currentIndexChanged, this, [this](int) {
        add_result_->clear();
        refresh_snippet();
        retranslate();
    });
    row->addWidget(client_combo_);
    row->addStretch(1);
    v->addLayout(row);
    client_desc_ = make_label("acHint", card, true);
    v->addWidget(client_desc_);
    snippet_ = make_snippet(card);
    v->addWidget(snippet_);
    auto* actions = new QHBoxLayout;
    actions->setSpacing(6);
    copy_snippet_btn_ = make_button("acBtn", card);
    connect(copy_snippet_btn_, &QPushButton::clicked, this, [this]() {
        auto& srv = McpHttpServer::instance();
        switch (client_combo_->currentIndex()) {
            case ClaudeCode:
                copy_text(srv.claude_code_add_command());
                break;
            case Codex:
                copy_text(srv.codex_add_command());
                break;
            case GeminiCli:
                copy_text(srv.gemini_add_command());
                break;
            case Cursor:
                copy_text(srv.generic_http_json());
                break;
            case VsCode:
                copy_text(srv.vscode_json());
                break;
            default:
                copy_text(srv.stdio_bridge_json());
                break;
        }
    });
    actions->addWidget(copy_snippet_btn_);
    add_btn_ = make_button("acPrimaryBtn", card);
    connect(add_btn_, &QPushButton::clicked, this, &AgentConnectSection::on_register);
    actions->addWidget(add_btn_);
    add_result_ = make_label("acResult", card, true);
    add_result_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    actions->addWidget(add_result_, 1);
    v->addLayout(actions);
    second_lbl_ = make_label("acHint", card, true);
    v->addWidget(second_lbl_);
    second_snippet_ = make_snippet(card);
    v->addWidget(second_snippet_);
    copy_second_btn_ = make_button("acBtn", card);
    connect(copy_second_btn_, &QPushButton::clicked, this, [this]() {
        auto& srv = McpHttpServer::instance();
        copy_text(client_combo_->currentIndex() == Codex ? srv.codex_config_toml() : srv.gemini_settings_json());
    });
    v->addWidget(copy_second_btn_, 0, Qt::AlignLeft);
    return card;
}

QWidget* AgentConnectSection::build_tools_card(QWidget* parent) {
    QFrame* card = make_card(parent);
    auto* v = new QVBoxLayout(card);
    v->setContentsMargins(14, 12, 14, 14);
    v->setSpacing(6);
    tools_title_ = make_label("acSection", card);
    v->addWidget(tools_title_);
    tools_list_ = make_label("acMono", card, true);
    tools_list_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    v->addWidget(tools_list_);
    return card;
}

QWidget* AgentConnectSection::build_activity_card(QWidget* parent) {
    QFrame* card = make_card(parent);
    auto* v = new QVBoxLayout(card);
    v->setContentsMargins(14, 12, 14, 14);
    v->setSpacing(8);
    auto* head = new QHBoxLayout;
    activity_title_ = make_label("acSection", card);
    head->addWidget(activity_title_);
    head->addStretch(1);
    clear_btn_ = make_button("acBtn", card);
    connect(clear_btn_, &QPushButton::clicked, this, [this]() {
        McpHttpServer::instance().clear_activity();
        pending_activity_.clear();
        activity_model_->reset_records({});
    });
    head->addWidget(clear_btn_);
    v->addLayout(head);

    activity_model_ = new McpActivityModel(this);
    activity_table_ = new QTableView(card);
    activity_table_->setObjectName(QStringLiteral("acTable"));
    activity_table_->setModel(activity_model_);
    activity_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    activity_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    activity_table_->setAlternatingRowColors(true);
    activity_table_->setShowGrid(false);
    activity_table_->setWordWrap(false);
    activity_table_->verticalHeader()->hide();
    activity_table_->verticalHeader()->setDefaultSectionSize(24);
    activity_table_->setMinimumHeight(240);
    auto* hh = activity_table_->horizontalHeader();
    hh->setSectionResizeMode(QHeaderView::Interactive);
    hh->setSectionResizeMode(McpActivityModel::ColOutcome, QHeaderView::Stretch);
    hh->resizeSection(McpActivityModel::ColTime, 80);
    hh->resizeSection(McpActivityModel::ColClient, 150);
    hh->resizeSection(McpActivityModel::ColTool, 130);
    hh->resizeSection(McpActivityModel::ColDuration, 90);
    v->addWidget(activity_table_, 1);

    clients_title_ = make_label("acLabel", card);
    v->addWidget(clients_title_);
    clients_list_ = make_label("acHint", card, true);
    clients_list_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    v->addWidget(clients_list_);
    return card;
}

void AgentConnectSection::apply_theme() {
    using namespace ui::colors;
    // One stylesheet for the whole page, by object name (P7) — re-applied only
    // when the theme changes.
    setStyleSheet(
        QStringLiteral(
            "#agentConnectSection, #acScroll, #acPage { background:%1; }"
            "#acCard { background:%2; border:1px solid %3; }"
            "#acTitle { color:%4; font-weight:700; letter-spacing:0.5px; font-size:15px; }"
            "#acSection { color:%4; font-weight:700; letter-spacing:0.5px; }"
            "#acLabel { color:%5; font-weight:600; }"
            "#acHint { color:%6; }"
            "#acNote { color:%5; border-left:2px solid %4; padding-left:8px; }"
            "#acMono { color:%5; font-family:'Consolas','Menlo',monospace; }"
            "#acError { color:%7; background:%8; padding:6px; }"
            "#acState[state=\"running\"] { color:%9; font-weight:700; }"
            "#acState[state=\"stopped\"] { color:%6; font-weight:700; }"
            "#acState[state=\"error\"] { color:%7; font-weight:700; }"
            "#acResult[state=\"ok\"] { color:%9; }"
            "#acResult[state=\"error\"] { color:%7; }"
            "#acResult { color:%6; }"
            "QLineEdit#acField, QPlainTextEdit#acSnippet { background:%10; color:%11; border:1px solid %3; "
            "padding:5px; font-family:'Consolas','Menlo',monospace; }"
            "QSpinBox#acSpin, QComboBox#acCombo { background:%10; color:%11; border:1px solid %3; padding:4px 6px; "
            "min-width:90px; }"
            "QComboBox#acCombo QAbstractItemView { background:%10; color:%11; selection-background-color:%12; }"
            "QCheckBox#acCheck { color:%11; }"
            "QPushButton#acBtn { background:%10; color:%11; border:1px solid %13; padding:5px 12px; }"
            "QPushButton#acBtn:hover { background:%12; }"
            "QPushButton#acPrimaryBtn { background:%4; color:%1; border:none; padding:5px 14px; font-weight:700; }"
            "QPushButton#acPrimaryBtn:disabled { background:%12; color:%6; }"
            "QPushButton#acDangerBtn { background:%10; color:%7; border:1px solid %3; padding:5px 12px; }"
            "QPushButton#acDangerBtn:disabled, QPushButton#acBtn:disabled { color:%6; }"
            "QTableView#acTable { background:%10; alternate-background-color:%2; color:%5; border:1px solid %3; "
            "gridline-color:%3; }"
            "QTableView#acTable QHeaderView::section { background:%2; color:%6; border:none; "
            "border-bottom:1px solid %3; padding:4px; font-weight:700; }")
            .arg(BG_BASE(), BG_SURFACE(), BORDER_DIM(), AMBER(), TEXT_SECONDARY(), TEXT_DIM(), NEGATIVE(),
                 NEGATIVE_BG(), POSITIVE())
            .arg(BG_RAISED(), TEXT_PRIMARY(), BG_HOVER(), BORDER_BRIGHT()));
}

void AgentConnectSection::retranslate() {
    title_->setText(tr("AI AGENTS (MCP)"));
    intro_->setText(tr("Let Claude Code, Codex, Gemini CLI, Cursor, VS Code or Claude Desktop work in this terminal. "
                       "They operate it the way you do — open screens, read tables, charts and news, work the "
                       "controls — and run the bundled Python analytics, through ten tools. Orders, money moves, "
                       "spends and deletes ask you first unless you turn on auto mode."));
    server_title_->setText(tr("SERVER"));
    copy_url_btn_->setText(tr("COPY URL"));
    start_btn_->setText(tr("START"));
    stop_btn_->setText(tr("STOP"));
    autostart_check_->setText(tr("Start with the terminal"));
    port_lbl_->setText(tr("PORT"));

    // Names for controls with no label of their own — read by screen readers and
    // by agents operating the terminal (who would otherwise see "ac field").
    url_edit_->setAccessibleName(tr("Server address"));
    token_edit_->setAccessibleName(tr("Access token"));
    snippet_->setAccessibleName(tr("Connect command"));
    second_snippet_->setAccessibleName(tr("Client config"));
    activity_table_->setAccessibleName(tr("Activity"));

    token_title_->setText(tr("ACCESS TOKEN"));
    show_token_btn_->setText(show_token_ ? tr("HIDE") : tr("SHOW"));
    copy_token_btn_->setText(tr("COPY"));
    regen_btn_->setText(tr("REGENERATE"));
    regen_btn_->setToolTip(tr("Mint a new token — every connected client must be updated"));
    token_hint_->setText(tr("Clients send it as “Authorization: Bearer <token>”. It never leaves this computer: the "
                            "server only listens on 127.0.0.1."));

    perms_title_->setText(tr("PERMISSIONS"));
    look_lbl_->setText(tr("Always allowed: looking — what is open, a screen's contents, tables, charts, screenshots."));
    ui_control_check_->setText(tr("Operate the terminal — open screens, click, type, arrange panels"));
    scripts_check_->setText(tr("Run the bundled Python analytics scripts"));
    auto_check_->setText(tr("Auto mode — skip every approval (fully agentic)"));
    auto_check_->setToolTip(tr("Agents place orders, move money, spend credits, delete data and run sensitive "
                               "scripts without asking you. Applies to AI Chat and agent runs too."));
    update_auto_note();
    budget_lbl_->setText(tr("LARGEST RESULT SENT INLINE"));
    budget_combo_->setToolTip(tr("Bigger results are stored and the agent pages them with ui_read"));

    connect_title_->setText(tr("CONNECT YOUR AGENT"));
    client_lbl_->setText(tr("CLIENT"));
    client_combo_->setItemText(ClaudeCode, tr("Claude Code"));
    client_combo_->setItemText(Codex, tr("OpenAI Codex CLI"));
    client_combo_->setItemText(GeminiCli, tr("Gemini CLI"));
    client_combo_->setItemText(Cursor, tr("Cursor / Windsurf / other HTTP clients"));
    client_combo_->setItemText(VsCode, tr("VS Code (Copilot)"));
    client_combo_->setItemText(ClaudeDesktop, tr("Claude Desktop / stdio clients"));
    copy_snippet_btn_->setText(tr("COPY"));
    add_btn_->setText(client_combo_->currentIndex() == Codex ? tr("ADD TO CODEX") : tr("ADD TO CLAUDE CODE"));
    switch (client_combo_->currentIndex()) {
        case ClaudeCode:
            client_desc_->setText(tr("Run this once in a terminal (or press ADD). Claude Code then sees the tools as "
                                     "mcp__fincept_terminal__* in every project; /mcp shows the connection and the "
                                     "prompts appear as slash commands."));
            break;
        case Codex:
            client_desc_->setText(
                tr("Run this once (or press ADD) — it registers the terminal through the bundled stdio "
                   "bridge. Codex can also connect over HTTP directly with the config.toml entry "
                   "below; set FINCEPT_MCP_TOKEN in your environment for it."));
            second_lbl_->setText(tr("~/.codex/config.toml"));
            copy_second_btn_->setText(tr("COPY TOML"));
            break;
        case GeminiCli:
            client_desc_->setText(tr("Run this once, or add the settings.json entry below to ~/.gemini/settings.json "
                                     "(user) or .gemini/settings.json (project)."));
            second_lbl_->setText(tr("~/.gemini/settings.json"));
            copy_second_btn_->setText(tr("COPY JSON"));
            break;
        case Cursor:
            client_desc_->setText(
                tr("Paste into ~/.cursor/mcp.json (Windsurf: ~/.codeium/windsurf/mcp_config.json — use "
                   "\"serverUrl\" for \"url\") or your client's MCP settings. Streamable HTTP."));
            break;
        case VsCode:
            client_desc_->setText(
                tr("Paste into .vscode/mcp.json in a workspace (or your user mcp.json), then start the "
                   "server from the MCP view and use Agent mode."));
            break;
        default:
            client_desc_->setText(
                tr("Paste into claude_desktop_config.json (Settings › Developer › Edit Config) or any "
                   "stdio-only client's config, then restart it. It runs the bundled bridge "
                   "script with the terminal's Python — standard library only."));
            break;
    }

    tools_title_->setText(tr("WHAT AGENTS GET — %1 TOOLS").arg(mcp::core::core_tool_names().size()));
    {
        QStringList lines;
        for (const QString& name : mcp::core::core_tool_names()) {
            const auto info = mcp::McpProvider::instance().find_tool(name);
            QString desc = info ? info->description : QString();
            const int stop = desc.indexOf(QLatin1Char('.'));
            if (stop > 0)
                desc = desc.left(stop);
            lines << QStringLiteral("%1 — %2").arg(name.leftJustified(14, QLatin1Char(' ')), desc);
        }
        tools_list_->setText(lines.join(QLatin1Char('\n')));
    }

    activity_title_->setText(tr("ACTIVITY — LAST 200 CALLS"));
    clear_btn_->setText(tr("CLEAR"));
    clients_title_->setText(tr("CLIENTS SINCE START"));
    activity_model_->retranslate_headers();
    refresh_status();
}

void AgentConnectSection::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslate();
    QWidget::changeEvent(event);
}

void AgentConnectSection::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    subscribe();
    uptime_timer_->start();
    refresh_status();
    refresh_snippet();
    refresh_clients();
    activity_model_->reset_records(McpHttpServer::instance().recent_activity());
}

void AgentConnectSection::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    unsubscribe();
    uptime_timer_->stop();
    activity_timer_->stop();
    pending_activity_.clear();
}

void AgentConnectSection::subscribe() {
    if (!conns_.isEmpty())
        return;
    auto& srv = McpHttpServer::instance();
    conns_ << connect(&srv, &McpHttpServer::status_changed, this, [this]() {
        refresh_status();
        refresh_snippet();
        refresh_clients();
    });
    conns_ << connect(&srv, &McpHttpServer::activity, this, [this](const mcp::ExternalCallRecord& record) {
        pending_activity_.prepend(record);
        if (!activity_timer_->isActive())
            activity_timer_->start();
    });
}

void AgentConnectSection::unsubscribe() {
    for (const auto& c : std::as_const(conns_))
        disconnect(c);
    conns_.clear();
}

void AgentConnectSection::flush_activity() {
    activity_model_->prepend_records(pending_activity_);
    pending_activity_.clear();
    refresh_status(); // counters
    refresh_clients();
}

QString AgentConnectSection::masked(const QString& text) const {
    if (show_token_)
        return text;
    const QString token = McpHttpServer::instance().token();
    if (token.isEmpty())
        return text;
    QString out = text;
    out.replace(token, QStringLiteral("••••••••"));
    return out;
}

void AgentConnectSection::copy_text(const QString& text) {
    if (!text.isEmpty())
        QGuiApplication::clipboard()->setText(text);
}

void AgentConnectSection::refresh_status() {
    auto& srv = McpHttpServer::instance();
    const bool running = srv.is_running();
    const QString err = srv.last_error();
    const QString state =
        running ? QStringLiteral("running") : (err.isEmpty() ? QStringLiteral("stopped") : QStringLiteral("error"));
    state_lbl_->setText(running ? tr("● RUNNING") : (err.isEmpty() ? tr("● STOPPED") : tr("● ERROR")));
    if (state_lbl_->property("state").toString() != state) {
        state_lbl_->setProperty("state", state);
        state_lbl_->style()->unpolish(state_lbl_);
        state_lbl_->style()->polish(state_lbl_);
    }
    url_edit_->setText(running ? srv.url() : tr("not listening — press START"));
    copy_url_btn_->setEnabled(running);
    start_btn_->setEnabled(!running);
    stop_btn_->setEnabled(running);
    if (running) {
        const mcp::ExternalServerStats st = srv.stats();
        const qint64 up = st.started_at.isValid() ? st.started_at.secsTo(QDateTime::currentDateTime()) : 0;
        stats_lbl_->setText(tr("%1 client(s) · %2 calls (%3 ok, %4 failed, %5 denied) · up %6")
                                .arg(srv.connected_client_count())
                                .arg(st.calls)
                                .arg(st.ok)
                                .arg(st.failed)
                                .arg(st.denied)
                                .arg(duration_text(up)));
    } else {
        stats_lbl_->setText(tr("Start the server, then connect a client below."));
    }
    error_lbl_->setVisible(!err.isEmpty());
    error_lbl_->setText(err);
    {
        const QSignalBlocker b(autostart_check_);
        autostart_check_->setChecked(srv.enabled_at_startup());
    }
    {
        const QSignalBlocker b(ui_control_check_);
        ui_control_check_->setChecked(srv.allow_ui_control());
    }
    {
        const QSignalBlocker b(scripts_check_);
        scripts_check_->setChecked(srv.allow_scripts());
    }
    {
        const QSignalBlocker b(auto_check_);
        auto_check_->setChecked(srv.auto_approve());
    }
    update_auto_note();
    {
        const QSignalBlocker b(budget_combo_);
        const int idx = budget_combo_->findData(srv.result_budget_kb());
        if (idx >= 0)
            budget_combo_->setCurrentIndex(idx);
    }
    if (!port_spin_->hasFocus()) {
        const QSignalBlocker b(port_spin_);
        port_spin_->setValue(srv.configured_port());
    }
    if (running && srv.port() != srv.configured_port())
        port_hint_->setText(tr("Listening on %1 because %2 was busy — clients need the URL above. Changes apply on the "
                               "next start.")
                                .arg(srv.port())
                                .arg(srv.configured_port()));
    else
        port_hint_->setText(tr("Port changes apply the next time the server starts."));
    token_edit_->setEchoMode(show_token_ ? QLineEdit::Normal : QLineEdit::Password);
    token_edit_->setText(srv.token());
}

void AgentConnectSection::refresh_snippet() {
    auto& srv = McpHttpServer::instance();
    const int client = client_combo_->currentIndex();
    QString text;
    QString second;
    switch (client) {
        case ClaudeCode:
            text = srv.claude_code_add_command();
            break;
        case Codex:
            text = srv.codex_add_command();
            second = srv.codex_config_toml();
            break;
        case GeminiCli:
            text = srv.gemini_add_command();
            second = srv.gemini_settings_json();
            break;
        case Cursor:
            text = srv.generic_http_json();
            break;
        case VsCode:
            text = srv.vscode_json();
            break;
        default:
            text = srv.stdio_bridge_json();
            break;
    }
    snippet_->setPlainText(masked(text));
    second_lbl_->setVisible(!second.isEmpty());
    second_snippet_->setVisible(!second.isEmpty());
    copy_second_btn_->setVisible(!second.isEmpty());
    second_snippet_->setPlainText(masked(second));

    const bool one_click = client == ClaudeCode || client == Codex;
    add_btn_->setVisible(one_click);
    if (one_click) {
        auto& svc = ca::CodingAgentService::instance();
        const bool installed = svc.cli_info(client == Codex ? ca::AgentKind::Codex : ca::AgentKind::Claude).installed();
        add_btn_->setEnabled(installed && !awaiting_registration_);
        add_btn_->setToolTip(installed ? QString()
                                       : (client == Codex ? tr("Codex CLI was not found on this computer")
                                                          : tr("Claude Code was not found on this computer")));
    }
}

void AgentConnectSection::refresh_clients() {
    const QList<mcp::ExternalClientInfo> clients = McpHttpServer::instance().clients();
    if (clients.isEmpty()) {
        clients_list_->setText(tr("No client has connected since the server started."));
        return;
    }
    QStringList lines;
    for (const auto& c : clients) {
        lines << tr("%1 — %2 call(s), last seen %3%4")
                     .arg(c.name)
                     .arg(c.calls)
                     .arg(c.last_seen.toLocalTime().toString(QStringLiteral("HH:mm:ss")))
                     .arg(c.user_agent.isEmpty() ? QString() : QStringLiteral(" · ") + c.user_agent.left(60));
        if (lines.size() >= 12)
            break;
    }
    clients_list_->setText(lines.join(QLatin1Char('\n')));
}

void AgentConnectSection::on_start() {
    auto& srv = McpHttpServer::instance();
    if (!srv.start())
        QMessageBox::warning(this, tr("MCP server"),
                             tr("The server could not start: %1")
                                 .arg(srv.last_error().isEmpty() ? tr("unknown error") : srv.last_error()));
    refresh_status();
    refresh_snippet();
}

void AgentConnectSection::on_stop() {
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

void AgentConnectSection::on_regenerate() {
    const auto answer = QMessageBox::question(
        this, tr("Regenerate access token"),
        tr("Mint a new access token? Every connected client (Claude Code, Codex, Gemini CLI, Cursor, VS Code, Claude "
           "Desktop) stops working until it is updated with the new token."),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;
    McpHttpServer::instance().regenerate_token();
    refresh_status();
    refresh_snippet();
}

void AgentConnectSection::on_ui_control_toggled(bool on) {
    McpHttpServer::instance().set_allow_ui_control(on);
}

void AgentConnectSection::on_scripts_toggled(bool on) {
    McpHttpServer::instance().set_allow_scripts(on);
}

void AgentConnectSection::on_auto_toggled(bool on) {
    if (on) {
        const auto answer = QMessageBox::warning(
            this, tr("Turn on auto mode?"),
            tr("With auto mode on, agents never ask you first. They can place, change and cancel LIVE orders, move "
               "money, spend credits, delete data, sign out and run sensitive scripts on their own.\n\nThis applies "
               "to external agents, AI Chat and agent runs. Paper workflow runs stay paper, and password / API-key "
               "fields are still never read or typed.\n\nTurn auto mode on?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            const QSignalBlocker b(auto_check_);
            auto_check_->setChecked(false);
            return;
        }
    }
    McpHttpServer::instance().set_auto_approve(on);
    update_auto_note();
}

void AgentConnectSection::update_auto_note() {
    if (!always_ask_lbl_)
        return;
    if (McpHttpServer::instance().auto_approve()) {
        always_ask_lbl_->setText(tr("AUTO MODE IS ON: nothing asks you first — orders, money moves, spends, deletes, "
                                    "sign-out and sensitive scripts run on the agent's say-so. Every such action is "
                                    "logged. Paper workflow runs stay paper; password and API-key fields are never "
                                    "read or typed by an agent."));
    } else {
        always_ask_lbl_->setText(
            tr("Whatever is allowed here, these always ask you in the terminal first: placing, "
               "changing or cancelling orders, moving money, spending credits, deleting data, "
               "signing out, and sensitive scripts (exchange, agent runners). Password and API-key "
               "fields are never read or typed by an agent."));
    }
}

void AgentConnectSection::on_register() {
    const bool codex = client_combo_->currentIndex() == Codex;
    awaiting_registration_ = true;
    add_btn_->setEnabled(false);
    add_result_->setText(tr("Working…"));
    add_result_->setProperty("state", QString());
    add_result_->style()->unpolish(add_result_);
    add_result_->style()->polish(add_result_);
    ca::CodingAgentService::instance().register_with_client(codex ? ca::AgentKind::Codex : ca::AgentKind::Claude,
                                                            false);
}

void AgentConnectSection::on_registration_finished(const QString& agent, bool ok, bool already_exists,
                                                   const QString& message) {
    if (!awaiting_registration_)
        return; // another page asked
    const bool codex = agent == ca::agent_id(ca::AgentKind::Codex);
    const QString product = codex ? tr("Codex") : tr("Claude Code");
    if (already_exists) {
        const auto answer =
            QMessageBox::question(this, tr("Replace existing server?"),
                                  tr("%1 already has an MCP server named “fincept_terminal”:\n\n%2\n\nReplace it with "
                                     "this terminal's server?")
                                      .arg(product, message.left(1200)),
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer == QMessageBox::Yes) {
            add_result_->setText(tr("Replacing…"));
            ca::CodingAgentService::instance().register_with_client(
                codex ? ca::AgentKind::Codex : ca::AgentKind::Claude, true);
            return;
        }
        awaiting_registration_ = false;
        add_result_->setText(tr("Kept the existing entry."));
        refresh_snippet();
        return;
    }
    awaiting_registration_ = false;
    add_result_->setText(ok ? tr("Added. Restart %1 sessions to pick it up.").arg(product) : message);
    add_result_->setToolTip(message);
    add_result_->setProperty("state", ok ? QStringLiteral("ok") : QStringLiteral("error"));
    add_result_->style()->unpolish(add_result_);
    add_result_->style()->polish(add_result_);
    refresh_snippet();
}

} // namespace fincept::screens
