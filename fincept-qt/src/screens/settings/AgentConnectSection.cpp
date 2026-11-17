// AgentConnectSection.cpp — Settings › AI Agents. See the header.

#include "screens/settings/AgentConnectSection.h"

#include "core/process/HostExec.h"
#include "mcp/McpProvider.h"
#include "mcp/core/CoreTools.h"
#include "mcp/server/McpClientProfile.h"
#include "mcp/server/McpServerSelfCheck.h"
#include "screens/agent_config/HarnessListWidget.h"
#include "services/coding_agents/CodingAgentService.h"
#include "services/harness/HarnessRegistry.h"
#include "services/harness/IHarnessAdapter.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QCheckBox>
#include <QClipboard>
#include <QColor>
#include <QComboBox>
#include <QDateTime>
#include <QEvent>
#include <QFontMetrics>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QStyle>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <optional>

namespace fincept::screens {

namespace ca = services::coding_agents;
using mcp::McpHttpServer;

namespace agent_connect_detail {

// Name the terminal registers itself under in a harness's own config: this
// edition's own ("fincept_community"), never one the private build uses.
constexpr const char* kAcServerName = harness::kMcpServerName;
constexpr int kAcToolTimeoutSec = 300;

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

/// One argument as a user would type it in a shell (display / copy only — the
/// one-click ADD passes the argument list directly, never through a shell).
QString ac_shell_quote(const QString& arg) {
    static const QString special = QStringLiteral(" \t\"'$&|<>;()*?`\\");
    bool plain = !arg.isEmpty();
    for (const QChar c : arg) {
        if (special.contains(c)) {
            plain = false;
            break;
        }
    }
    if (plain)
        return arg;
    if (arg.contains(QLatin1Char('$')) && !arg.contains(QLatin1Char('\'')))
        return QLatin1Char('\'') + arg + QLatin1Char('\''); // no variable expansion
    QString escaped = arg;
    escaped.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    return QLatin1Char('"') + escaped + QLatin1Char('"');
}

std::optional<harness::McpRegistration> ac_registration(const QString& harness_id) {
    const harness::IHarnessAdapter* adapter = harness::HarnessRegistry::instance().adapter(harness_id);
    if (!adapter)
        return std::nullopt;
    harness::McpEndpoint ep;
    ep.name = QString::fromLatin1(kAcServerName);
    ep.url = McpHttpServer::instance().url();
    if (ep.url.isEmpty()) // stopped: show the address it will listen on
        ep.url = QStringLiteral("http://127.0.0.1:%1/mcp").arg(McpHttpServer::instance().configured_port());
    ep.token_env_var = QString::fromLatin1(McpHttpServer::kTokenEnvVar);
    ep.tool_timeout_s = kAcToolTimeoutSec;
    return adapter->mcp_registration(ep);
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
    // State, not price direction: SUCCESS / DANGER / WARNING.
    if (role == Qt::ForegroundRole && col == ColOutcome)
        return QColor(r.denied ? ui::colors::WARNING() : (r.success ? ui::colors::SUCCESS() : ui::colors::DANGER()));
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

AgentConnectSection::AgentConnectSection(QWidget* parent, Sections sections) : QWidget(parent), sections_(sections) {
    setObjectName(QStringLiteral("agentConnectSection"));
    // Once, before any child exists; the theme system rewrites it on an
    // appearance change (re-setting it in a theme_changed handler re-styles the
    // whole page widget by widget).
    setStyleSheet(style_sheet());

    activity_timer_ = new QTimer(this);
    activity_timer_->setSingleShot(true);
    activity_timer_->setInterval(250);
    connect(activity_timer_, &QTimer::timeout, this, &AgentConnectSection::flush_activity);

    uptime_timer_ = new QTimer(this);
    uptime_timer_->setInterval(5000); // started in showEvent only (P3)
    connect(uptime_timer_, &QTimer::timeout, this, &AgentConnectSection::refresh_status);

    build_ui();
    fit_rows();
    retranslate();

    // Row heights and column widths follow the font, once a change has settled.
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, &AgentConnectSection::fit_rows);
    // Registration results can arrive after the page was hidden — keep this one
    // connection for the widget's lifetime (rare and cheap).
    connect(&ca::CodingAgentService::instance(), &ca::CodingAgentService::client_registration_finished, this,
            &AgentConnectSection::on_registration_finished);
    // A harness found or lost changes whether ADD can run.
    connect(&harness::HarnessRegistry::instance(), &harness::HarnessRegistry::status_changed, this,
            [this](const QString& id) {
                if (id == current_harness() && isVisible())
                    refresh_snippet();
            });
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
    if (sections_ == Sections::All) {
        // AGENT HARNESSES — the same widget as the workbench's HARNESSES tab.
        harness_title_ = make_label("acTitle", page);
        v->addWidget(harness_title_);
        harness_list_ = new HarnessListWidget(page);
        v->addWidget(harness_list_);
        mcp_title_ = make_label("acTitle", page);
        v->addWidget(mcp_title_);
    }
    intro_ = make_label("acHint", page, true);
    v->addWidget(intro_);

    v->addWidget(build_server_card(page));
    v->addWidget(build_token_card(page));
    v->addWidget(build_permissions_card(page));
    v->addWidget(build_connect_card(page));
    v->addWidget(build_check_card(page));
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
    start_btn_->setProperty("fincept_agent_risk", "account"); // agents: opens the terminal to AI agents
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
    autostart_check_->setProperty("fincept_agent_risk", "account"); // agents: opens the terminal to AI agents at every start
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
    show_token_btn_->setProperty("fincept_agent_risk", "account"); // agents: reveals the agents' access token
    connect(show_token_btn_, &QPushButton::clicked, this, [this]() {
        show_token_ = !show_token_;
        refresh_status();
        refresh_snippet();
        retranslate();
    });
    row->addWidget(show_token_btn_);
    copy_token_btn_ = make_button("acBtn", card);
    copy_token_btn_->setProperty("fincept_agent_risk", "account"); // agents: copies the agents' access token
    connect(copy_token_btn_, &QPushButton::clicked, this, []() { copy_text(McpHttpServer::instance().token()); });
    row->addWidget(copy_token_btn_);
    regen_btn_ = make_button("acDangerBtn", card);
    regen_btn_->setProperty("fincept_agent_risk", "account"); // agents: mints a new agents' access token
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
    auto_check_->setProperty("fincept_agent_risk", "account"); // agents: turns off every approval for agents
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
    client_combo_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    client_combo_->setMinimumContentsLength(18);
    rebuild_client_list();
    connect(client_combo_, &QComboBox::currentIndexChanged, this, [this](int) {
        add_result_->clear();
        refresh_snippet();
        retranslate();
    });
    row->addWidget(client_combo_, 1);
    row->addStretch(1);
    v->addLayout(row);
    client_desc_ = make_label("acHint", card, true);
    v->addWidget(client_desc_);
    snippet_ = make_snippet(card);
    v->addWidget(snippet_);
    auto* actions = new QHBoxLayout;
    actions->setSpacing(6);
    copy_snippet_btn_ = make_button("acBtn", card);
    connect(copy_snippet_btn_, &QPushButton::clicked, this, [this]() { copy_text(client_command(true)); });
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
    connect(copy_second_btn_, &QPushButton::clicked, this, [this]() { copy_text(client_config(true)); });
    v->addWidget(copy_second_btn_, 0, Qt::AlignLeft);
    return card;
}

QWidget* AgentConnectSection::build_check_card(QWidget* parent) {
    QFrame* card = make_card(parent);
    auto* v = new QVBoxLayout(card);
    v->setContentsMargins(14, 12, 14, 14);
    v->setSpacing(8);
    auto* head = new QHBoxLayout;
    check_title_ = make_label("acSection", card);
    head->addWidget(check_title_);
    head->addStretch(1);
    check_btn_ = make_button("acBtn", card);
    connect(check_btn_, &QPushButton::clicked, this, &AgentConnectSection::on_check_connections);
    head->addWidget(check_btn_);
    v->addLayout(head);
    check_hint_ = make_label("acHint", card, true);
    v->addWidget(check_hint_);

    check_model_ = new QStandardItemModel(0, 3, this);
    check_table_ = new QTableView(card);
    check_table_->setObjectName(QStringLiteral("acTable"));
    check_table_->setModel(check_model_);
    check_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    check_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    check_table_->setAlternatingRowColors(true);
    check_table_->setShowGrid(false);
    check_table_->setWordWrap(false);
    check_table_->verticalHeader()->hide();
    check_table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    check_table_->horizontalHeader()->setStretchLastSection(true);
    check_table_->hide(); // shown with the first report
    v->addWidget(check_table_);
    return card;
}

void AgentConnectSection::on_check_connections() {
    if (check_running_)
        return;
    if (!McpHttpServer::instance().is_running()) {
        check_hint_->setText(tr("Start the server first — the check talks to it the way each client does."));
        return;
    }
    check_running_ = true;
    check_btn_->setEnabled(false);
    check_hint_->setText(tr("Checking… each client's handshake, tool list and one read-only call (ui_overview). No AI "
                            "model is called."));
    QPointer<AgentConnectSection> self(this);
    mcp::run_mcp_self_check_async(this, [self](const mcp::SelfCheckReport& report) {
        if (!self)
            return;
        // One row per client profile (and the server / bridge rows), in the
        // order the check ran them: the first failing step and its reason, or
        // what passed.
        struct Row {
            QString id;
            bool ok = true;
            QString failure{};
            QString list_detail{};
            int steps = 0;
            qint64 ms = 0;
        };
        QList<Row> rows;
        for (const mcp::SelfCheckStep& s : report.steps) {
            auto it = std::find_if(rows.begin(), rows.end(), [&s](const Row& r) { return r.id == s.profile; });
            if (it == rows.end()) {
                rows.append(Row{s.profile});
                it = rows.end() - 1;
            }
            ++it->steps;
            it->ms += s.ms;
            if (!s.ok && it->ok) {
                it->ok = false;
                it->failure = s.step + QStringLiteral(": ") + s.detail;
            }
            if (s.ok && (s.step == QLatin1String("tools/list") || s.step == QLatin1String("edition") ||
                         s.profile == QLatin1String("stdio-bridge")))
                it->list_detail = s.detail;
        }
        QStringList clients, results, details;
        for (const Row& r : rows) {
            QString name = r.id;
            if (r.id == QLatin1String("server"))
                name = AgentConnectSection::tr("Server (which edition answers)");
            else if (r.id == QLatin1String("protocol"))
                name = AgentConnectSection::tr("Newer MCP clients (2026-07-28 fallback)");
            else if (r.id == QLatin1String("stdio-bridge"))
                name = AgentConnectSection::tr("stdio bridge (Claude Desktop)");
            else
                for (const mcp::profiles::ClientKind k : mcp::profiles::all_kinds()) {
                    if (r.id == QLatin1String(mcp::profiles::profile(k).id))
                        name = QString::fromUtf8(mcp::profiles::profile(k).display);
                }
            clients << name;
            results << (r.ok ? AgentConnectSection::tr("PASS") : AgentConnectSection::tr("FAIL"));
            details << (r.ok ? AgentConnectSection::tr("%1 step(s) in %2 ms%3")
                                   .arg(r.steps)
                                   .arg(r.ms)
                                   .arg(r.list_detail.isEmpty() ? QString() : QStringLiteral(" · ") + r.list_detail)
                             : r.failure);
        }
        self->show_check_report(clients, results, details);
    });
}

void AgentConnectSection::show_check_report(const QStringList& clients, const QStringList& results,
                                            const QStringList& details) {
    check_running_ = false;
    check_done_ = true;
    check_btn_->setEnabled(true);
    check_model_->removeRows(0, check_model_->rowCount());
    int failed = 0;
    for (qsizetype i = 0; i < clients.size(); ++i) {
        QList<QStandardItem*> row{new QStandardItem(clients.at(i)), new QStandardItem(results.value(i)),
                                  new QStandardItem(details.value(i))};
        row.at(2)->setToolTip(details.value(i));
        check_model_->appendRow(row);
        failed += results.value(i) == tr("PASS") ? 0 : 1;
    }
    check_hint_->setText(clients.isEmpty() ? tr("The check could not run.")
                         : failed == 0     ? tr("Every client connected: handshake, tool list and a read-only call "
                                                "passed. No AI model was called.")
                                           : tr("%1 of %2 failed — the reason is in the row.").arg(failed).arg(clients.size()));
    check_table_->setVisible(!clients.isEmpty());
    retranslate_check_headers();
    fit_rows();
}

void AgentConnectSection::retranslate_check_headers() {
    check_model_->setHorizontalHeaderLabels({tr("CLIENT"), tr("RESULT"), tr("DETAILS")});
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
    activity_table_->setMinimumHeight(240);
    auto* hh = activity_table_->horizontalHeader();
    hh->setSectionResizeMode(QHeaderView::Interactive);
    hh->setSectionResizeMode(McpActivityModel::ColOutcome, QHeaderView::Stretch);
    v->addWidget(activity_table_, 1);

    clients_title_ = make_label("acLabel", card);
    v->addWidget(clients_title_);
    clients_list_ = make_label("acHint", card, true);
    clients_list_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    v->addWidget(clients_list_);
    return card;
}

QString AgentConnectSection::style_sheet() const {
    using namespace ui::colors;
    // One stylesheet for the whole page, by object name (P7).
    //
    // Text widgets are transparent so they show the card behind them. Settings
    // gives its whole tree a selector-less `background:BG_BASE` (every other
    // settings section clears it per widget via settings_styles::label_ss /
    // check_ss); without this rule each label and checkbox here painted a
    // black band across its card. This sheet is nearer than the screen's, so it
    // wins; the low specificity (type only) lets the #ac… rules below still set
    // their own backgrounds (#acError). State colours are SUCCESS / DANGER, not
    // the price-direction POSITIVE / NEGATIVE.
    return QString("QLabel, QCheckBox, QRadioButton { background:transparent; }"
                   "#agentConnectSection, #acScroll, #acPage { background:%1; }"
                   "#acCard { background:%2; border:1px solid %3; }"
                   "#acTitle { color:%4; font-weight:700; letter-spacing:0.5px; font-size:" +
                   fincept::ui::fonts::px(1) +
                   "px; }"
                   "#acSection { color:%4; font-weight:700; letter-spacing:0.5px; }"
                   "#acLabel { color:%5; font-weight:600; }"
                   "#acHint { color:%6; }"
                   "#acNote { color:%5; border-left:2px solid %4; padding-left:8px; }"
                   "#acMono { color:%5; }"
                   "#acError { color:%7; background:%8; padding:6px; }"
                   "#acState[state=\"running\"] { color:%9; font-weight:700; }"
                   "#acState[state=\"stopped\"] { color:%6; font-weight:700; }"
                   "#acState[state=\"error\"] { color:%7; font-weight:700; }"
                   "#acResult[state=\"ok\"] { color:%9; }"
                   "#acResult[state=\"error\"] { color:%7; }"
                   "#acResult { color:%6; }"
                   "QLineEdit#acField, QPlainTextEdit#acSnippet { background:%10; color:%11; border:1px solid %3; "
                   "padding:5px; }"
                   "QSpinBox#acSpin, QComboBox#acCombo { background:%10; color:%11; border:1px solid %3; padding:4px 6px; "
                   "min-width:90px; }"
                   "QComboBox#acCombo QAbstractItemView { background:%10; color:%11; selection-background-color:%12; }"
                   "QCheckBox#acCheck { color:%11; }"
                   "QPushButton#acBtn { background:%10; color:%11; border:1px solid %13; padding:5px 12px; }"
                   "QPushButton#acBtn:hover { background:%12; }"
                   "QPushButton#acPrimaryBtn { background:%4; color:%14; border:none; padding:5px 14px; font-weight:700; }"
                   "QPushButton#acPrimaryBtn:disabled { background:%10; color:%15; }"
                   "QPushButton#acDangerBtn { background:%10; color:%7; border:1px solid %3; padding:5px 12px; }"
                   "QPushButton#acDangerBtn:disabled, QPushButton#acBtn:disabled { color:%15; }"
                   "QTableView#acTable { background:%10; alternate-background-color:%2; color:%5; border:1px solid %3; "
                   "gridline-color:%3; }"
                   "QTableView#acTable QHeaderView::section { background:%2; color:%6; border:none; "
                   "border-bottom:1px solid %3; padding:4px; font-weight:700; }")
        .arg(BG_BASE(), BG_SURFACE(), BORDER_DIM(), AMBER(), TEXT_SECONDARY(), TEXT_DIM(), DANGER(), tint(DANGER()),
             SUCCESS())
        .arg(BG_RAISED(), TEXT_PRIMARY(), BG_HOVER(), BORDER_BRIGHT(), TEXT_ON_ACCENT(), TEXT_TERTIARY());
}

void AgentConnectSection::fit_rows() {
    // Row height and starting column widths follow the user's font size.
    activity_table_->verticalHeader()->setDefaultSectionSize(qMax(24, QFontMetrics(ui::fonts::font()).height() + 8));
    const auto col_w = [](int w) { return qMax(w, w * ui::fonts::font_px(0) / ui::ThemeManager::kDefaultFontPx); };
    auto* hh = activity_table_->horizontalHeader();
    hh->resizeSection(McpActivityModel::ColTime, col_w(80));
    hh->resizeSection(McpActivityModel::ColClient, col_w(150));
    hh->resizeSection(McpActivityModel::ColTool, col_w(130));
    hh->resizeSection(McpActivityModel::ColDuration, col_w(90));
    if (check_table_) {
        const int row_h = qMax(24, QFontMetrics(ui::fonts::font()).height() + 8);
        check_table_->verticalHeader()->setDefaultSectionSize(row_h);
        auto* ch = check_table_->horizontalHeader();
        ch->resizeSection(0, col_w(230));
        ch->resizeSection(1, col_w(70));
        // Every row visible without an inner scroll (about a dozen rows).
        check_table_->setMinimumHeight(row_h * (qMax(3, check_model_->rowCount()) + 1) + 4);
    }
}

void AgentConnectSection::rebuild_client_list() {
    // Registry-driven: every harness that documents a way to add an MCP server,
    // then the clients that are not harnesses.
    auto& registry = harness::HarnessRegistry::instance();
    const QString keep = current_client();
    const QSignalBlocker b(client_combo_);
    client_combo_->clear();
    client_keys_.clear();
    for (const QString& id : registry.ids()) {
        const std::optional<harness::McpRegistration> reg = ac_registration(id);
        if (!reg || (reg->manual_args.isEmpty() && reg->config_snippet.isEmpty() && !reg->add))
            continue;
        const harness::IHarnessAdapter* a = registry.adapter(id);
        QString text = registry.display_name(id);
        if (a && a->descriptor().beta)
            text += QStringLiteral(" · ") + tr("BETA");
        client_combo_->addItem(text);
        client_keys_ << QStringLiteral("harness:") + id;
    }
    client_combo_->addItem(QString());
    client_keys_ << QStringLiteral("vscode");
    client_combo_->addItem(QString());
    client_keys_ << QStringLiteral("stdio");
    client_combo_->addItem(QString());
    client_keys_ << QStringLiteral("http");
    const int idx = client_keys_.indexOf(keep);
    client_combo_->setCurrentIndex(idx >= 0 ? static_cast<int>(idx) : 0);
}

QString AgentConnectSection::current_client() const {
    if (!client_combo_)
        return {};
    const int i = client_combo_->currentIndex();
    return (i >= 0 && i < client_keys_.size()) ? client_keys_.at(i) : QString();
}

QString AgentConnectSection::current_harness() const {
    const QString key = current_client();
    return key.startsWith(QLatin1String("harness:")) ? key.mid(8) : QString();
}

QString AgentConnectSection::client_command(bool reveal) const {
    auto& srv = McpHttpServer::instance();
    const QString key = current_client();
    QString text;
    if (key == QLatin1String("vscode"))
        text = srv.vscode_json();
    else if (key == QLatin1String("stdio"))
        text = srv.stdio_bridge_json();
    else if (key == QLatin1String("http"))
        text = srv.generic_http_json();
    else if (const QString id = current_harness(); !id.isEmpty()) {
        const std::optional<harness::McpRegistration> reg = ac_registration(id);
        const harness::IHarnessAdapter* a = harness::HarnessRegistry::instance().adapter(id);
        if (reg && a && !reg->manual_args.isEmpty()) {
            QStringList parts{a->descriptor().executable_names.value(0, id)};
            for (const QString& arg : reg->manual_args)
                parts << ac_shell_quote(arg);
            text = parts.join(QLatin1Char(' '));
        } else if (reg) {
            text = reg->config_snippet; // only a config entry is documented
        }
        text.replace(QLatin1String(harness::kMcpTokenPlaceholder), srv.token());
    }
    return reveal ? text : masked(text);
}

QString AgentConnectSection::client_config(bool reveal) const {
    const QString id = current_harness();
    if (id.isEmpty())
        return {};
    const std::optional<harness::McpRegistration> reg = ac_registration(id);
    if (!reg || reg->manual_args.isEmpty() || reg->config_snippet.isEmpty())
        return {}; // without a command, the config entry is the main snippet already
    QString text = reg->config_snippet;
    text.replace(QLatin1String(harness::kMcpTokenPlaceholder), McpHttpServer::instance().token());
    return reveal ? text : masked(text);
}

void AgentConnectSection::retranslate() {
    title_->setText(sections_ == Sections::All ? tr("AI AGENTS") : tr("TERMINAL MCP SERVER"));
    if (harness_title_) {
        harness_title_->setText(tr("AGENT HARNESSES"));
        mcp_title_->setText(tr("TERMINAL MCP SERVER"));
    }
    {
        QString intro = tr("Let your own agents — the harnesses above, VS Code, Claude Desktop or any MCP client — work in "
                           "this terminal. They operate it the way you do — open screens, read tables, charts and news, "
                           "work the controls — and run the bundled Python analytics, through ten tools. Orders, money "
                           "moves, spends and deletes ask you first unless you turn on auto mode.");
        if (process::host::in_flatpak())
            intro += QLatin1Char(' ') + tr("Coding agents run on your system, outside the Flatpak sandbox.");
        intro_->setText(intro);
    }
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
    for (int i = 0; i < client_keys_.size(); ++i) {
        const QString& key = client_keys_.at(i);
        if (key == QLatin1String("vscode"))
            client_combo_->setItemText(i, tr("VS Code (Copilot)"));
        else if (key == QLatin1String("stdio"))
            client_combo_->setItemText(i, tr("Claude Desktop / stdio clients"));
        else if (key == QLatin1String("http"))
            client_combo_->setItemText(i, tr("Windsurf / other HTTP clients"));
    }
    copy_snippet_btn_->setText(tr("COPY"));
    copy_second_btn_->setText(tr("COPY ENTRY"));

    check_title_->setText(tr("CONNECTION CHECK"));
    check_btn_->setText(tr("CHECK CONNECTIONS"));
    check_btn_->setToolTip(tr("Connects to the running server the way each supported client does — handshake, tool "
                              "list, one read-only call — and reports pass or fail per client. Calls no AI model."));
    check_table_->setAccessibleName(tr("Connection check results"));
    if (!check_running_ && !check_done_)
        check_hint_->setText(tr("Checks that Claude Code, Codex, Gemini CLI, Qwen Code, Cursor, Copilot, Grok, VS Code and "
                                "stdio clients can connect, before you set one up. It first confirms that this edition of "
                                "the terminal is the one answering, so the access token is never sent elsewhere."));
    retranslate_check_headers();

    const QString key = current_client();
    const QString hid = current_harness();
    if (!hid.isEmpty()) {
        auto& registry = harness::HarnessRegistry::instance();
        const std::optional<harness::McpRegistration> reg = ac_registration(hid);
        const QString name = registry.display_name(hid);
        add_btn_->setText(tr("ADD TO %1").arg(name.toUpper()).replace(QLatin1Char('&'), QStringLiteral("&&")));
        QStringList desc;
        if (reg && reg->add)
            desc << tr("Run this once in a terminal, or press ADD — ADD keeps the token off the command line.");
        else if (reg && !reg->manual_args.isEmpty())
            desc << tr("Run this once in a terminal. %1 only takes the token on its command line, so the terminal "
                       "does not run it for you; SHOW reveals the token.")
                        .arg(name);
        else
            desc << tr("Add this entry to %1's MCP configuration.").arg(name);
        if (reg && !reg->note.isEmpty())
            desc << reg->note;
        if (reg && reg->token_from_env)
            desc << tr("Set %1 to the access token above in your user environment (Windows: Settings › "
                       "System › About › Advanced system settings › Environment Variables; macOS / Linux: export it in "
                       "your shell profile). Runs started from the workbench get it automatically.")
                        .arg(QString::fromLatin1(harness::kMcpTokenEnvVar));
        client_desc_->setText(desc.join(QLatin1Char(' ')));
        second_lbl_->setText(reg && !reg->config_path_hint.isEmpty() ? tr("Or add this to %1:").arg(reg->config_path_hint)
                                                                     : QString());
    } else if (key == QLatin1String("vscode")) {
        client_desc_->setText(tr("Paste into .vscode/mcp.json in a workspace (or your user mcp.json), then start the "
                                 "server from the MCP view and use Agent mode."));
    } else if (key == QLatin1String("stdio")) {
        client_desc_->setText(
            process::host::in_flatpak()
                ? tr("Paste into claude_desktop_config.json (Settings › Developer › Edit Config) or any stdio-only "
                     "client's config, then restart it. The bridge script and the terminal's Python are inside this "
                     "app's Flatpak sandbox, so the entry starts them with “flatpak run” — standard library only.")
                : tr("Paste into claude_desktop_config.json (Settings › Developer › Edit Config) or any "
                     "stdio-only client's config, then restart it. It runs the bundled bridge script with the "
                     "terminal's Python — standard library only."));
    } else {
        client_desc_->setText(tr("Paste into your client's MCP settings (Windsurf: ~/.codeium/windsurf/mcp_config.json — "
                                 "use \"serverUrl\" for \"url\"). Streamable HTTP."));
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
    const QString token = srv.token();
    token_edit_->setText(token);
    // Empty only when secure storage could not be read (the error line says why);
    // no token is minted then, so the clients' saved one keeps working.
    token_edit_->setPlaceholderText(token.isEmpty() ? tr("not available — secure storage could not be read")
                                                    : QString());
    copy_token_btn_->setEnabled(!token.isEmpty());
}

void AgentConnectSection::refresh_snippet() {
    snippet_->setPlainText(client_command(false));
    const QString second = client_config(false);
    second_lbl_->setVisible(!second.isEmpty());
    second_snippet_->setVisible(!second.isEmpty());
    copy_second_btn_->setVisible(!second.isEmpty());
    second_snippet_->setPlainText(second);

    const QString hid = current_harness();
    const std::optional<harness::McpRegistration> reg = hid.isEmpty() ? std::nullopt : ac_registration(hid);
    const bool one_click = reg && reg->add;
    add_btn_->setVisible(one_click);
    if (one_click) {
        auto& registry = harness::HarnessRegistry::instance();
        const harness::HarnessStatus st = registry.status(hid);
        const bool installed = st.installed();
        add_btn_->setEnabled(installed && awaiting_harness_.isEmpty());
        add_btn_->setToolTip(installed         ? QString()
                             : st.host_blocked ? st.detect_error
                                               : tr("%1 was not found on this computer").arg(registry.display_name(hid)));
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
        tr("Mint a new access token? Every connected client stops working until it is updated with the new token "
           "(clients that read %1 need the variable updated too).")
            .arg(QString::fromLatin1(harness::kMcpTokenEnvVar)),
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
    const QString hid = current_harness();
    if (hid.isEmpty())
        return;
    awaiting_harness_ = hid;
    add_btn_->setEnabled(false);
    add_result_->setText(tr("Working…"));
    add_result_->setProperty("state", QString());
    add_result_->style()->unpolish(add_result_);
    add_result_->style()->polish(add_result_);
    ca::CodingAgentService::instance().register_with_client(hid, false);
}

void AgentConnectSection::on_registration_finished(const QString& harness_id, bool ok, bool already_exists,
                                                   const QString& message) {
    if (awaiting_harness_.isEmpty() || awaiting_harness_ != harness_id)
        return; // another page asked
    const QString product = harness::HarnessRegistry::instance().display_name(harness_id);
    if (already_exists) {
        const auto answer =
            QMessageBox::question(this, tr("Replace existing server?"),
                                  tr("%1 already has an MCP server named “%2”:\n\n%3\n\nReplace it with "
                                     "this terminal's server?")
                                      .arg(product, QString::fromLatin1(kAcServerName), message.left(1200)),
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer == QMessageBox::Yes) {
            add_result_->setText(tr("Replacing…"));
            ca::CodingAgentService::instance().register_with_client(harness_id, true);
            return;
        }
        awaiting_harness_.clear();
        add_result_->setText(tr("Kept the existing entry."));
        refresh_snippet();
        return;
    }
    awaiting_harness_.clear();
    add_result_->setText(ok ? tr("Added. Restart %1 sessions to pick it up.").arg(product) : message.left(400));
    add_result_->setToolTip(message);
    add_result_->setProperty("state", ok ? QStringLiteral("ok") : QStringLiteral("error"));
    add_result_->style()->unpolish(add_result_);
    add_result_->style()->polish(add_result_);
    refresh_snippet();
}

} // namespace fincept::screens
