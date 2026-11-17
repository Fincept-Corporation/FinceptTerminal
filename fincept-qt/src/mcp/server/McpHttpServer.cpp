// McpHttpServer.cpp — lifecycle, persisted settings, bearer token, activity log
// and connection snippets of the external MCP server. The protocol lives in
// McpServerEngine (on its own thread), the tool side in McpHttpServer_Tools.cpp
// and the prompts/resources in McpHttpServer_Rpc.cpp (see McpHttpServer_p.h).

#include "mcp/server/McpHttpServer.h"

#include "core/config/AppConfig.h"
#include "core/logging/Logger.h"
#include "core/process/HostExec.h"
#include "mcp/server/McpHttpServer_p.h"
#include "mcp/server/McpServerSelfCheck.h"
#include "mcp/server/McpTokenPolicy.h"
#include "mcp/server/ToolApprovalDialog.h"
#include "python/PythonRunner.h"
#include "python/PythonSetupManager.h"
#include "storage/secure/SecureStorage.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMutexLocker>
#include <QPointer>
#include <QRandomGenerator>
#include <QThread>
#include <QThreadPool>
#include <QTimer>

#include <algorithm>
#include <array>

namespace fincept::mcp {

namespace mcp_http_detail {

namespace {

/// Double-quoted CLI argument (paths may contain spaces).
QString cli_quote(const QString& s) {
    QString escaped = s;
    escaped.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    return QLatin1Char('"') + escaped + QLatin1Char('"');
}

/// TOML basic string.
QString toml_string(const QString& s) {
    QString out;
    out.reserve(s.size() + 2);
    out += QLatin1Char('"');
    for (const QChar c : s) {
        switch (c.unicode()) {
            case u'\\':
                out += QStringLiteral("\\\\");
                break;
            case u'"':
                out += QStringLiteral("\\\"");
                break;
            case u'\n':
                out += QStringLiteral("\\n");
                break;
            case u'\r':
                out += QStringLiteral("\\r");
                break;
            case u'\t':
                out += QStringLiteral("\\t");
                break;
            default:
                if (c.unicode() < 0x20 || c.unicode() == 0x7f)
                    out += QStringLiteral("\\u%1").arg(static_cast<int>(c.unicode()), 4, 16, QLatin1Char('0'));
                else
                    out += c;
        }
    }
    out += QLatin1Char('"');
    return out;
}

/// The engine's limits for the terminal (defaults are the tested values).
server::EngineLimits terminal_limits() {
    server::EngineLimits l;
    for (const quint16 p : edition::kPrivateMcpPorts)
        l.forbidden_ports.append(p); // the private build's listeners are never ours to take
    return l;
}

} // namespace

McpRunTokenTable& run_token_table() {
    static McpRunTokenTable table;
    return table;
}

QString generate_token() {
    std::array<quint32, 8> words{};
    QRandomGenerator::system()->fillRange(words.data(), static_cast<qsizetype>(words.size()));
    const QByteArray raw(reinterpret_cast<const char*>(words.data()), static_cast<qsizetype>(sizeof(words)));
    return QString::fromLatin1(raw.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

QThreadPool* BackendShared::ensure_pool() {
    QMutexLocker lock(&pool_mutex);
    if (!pool) {
        pool = new QThreadPool; // owned; see ~McpHttpServer
        pool->setObjectName(QStringLiteral("mcp-http-tools"));
        pool->setMaxThreadCount(kPoolThreads);
        pool->setExpiryTimeout(60 * 1000);
    }
    return pool;
}

} // namespace mcp_http_detail

// ── Impl helpers ─────────────────────────────────────────────────────────────

QString McpHttpServer::Impl::effective_token() const {
    return test_mode ? test_token : load_or_create_token();
}

QString McpHttpServer::Impl::load_or_create_token() const {
    using namespace mcp_http_detail;
    QMutexLocker lock(&token_mutex);
    if (!token.isEmpty())
        return token;
    const QString key = QString::fromLatin1(kTokenStorageKey);
    // SecureStorage::read logs a failure itself (once per key and kind, never the value).
    const mcp_token::TokenDecision d = mcp_token::decide(SecureStorage::instance().read(key));
    switch (d.plan) {
        case mcp_token::TokenPlan::Use:
            token = d.token;
            token_error.clear();
            return token;
        case mcp_token::TokenPlan::Wait:
            // The stored token may exist: minting one now would replace it and break
            // every client configured with it. Stay without a token; start() retries.
            token_error = d.reason;
            return {};
        case mcp_token::TokenPlan::Create:
            break;
    }
    token = generate_token();
    token_error.clear();
    const auto written = SecureStorage::instance().store(key, token);
    if (written.is_err())
        LOG_WARN(kTag, QStringLiteral("Could not persist the MCP server token — clients will need a new one after a "
                                      "restart: %1")
                           .arg(QString::fromStdString(written.error())));
    else
        LOG_AUDIT(kTag, QStringLiteral("Credential added"),
                  {{"credential", kTokenStorageKey}, {"reason", "first use: none stored"}});
    return token;
}

QString McpHttpServer::Impl::effective_url() const {
    quint16 p = bound_port != 0 ? bound_port : (test_mode ? test_port : configured_port);
    if (p == 0)
        p = configured_port;
    return QStringLiteral("http://127.0.0.1:%1/mcp").arg(p);
}

void McpHttpServer::Impl::publish_auth_token(const QString& value) {
    QMutexLocker lock(&shared->auth_mutex);
    shared->auth_token = value;
    // A short, non-reversible id of the credential: binds a session to the
    // token that created it (never logged, never sent).
    shared->auth_fingerprint =
        value.isEmpty() ? QByteArray()
                        : QCryptographicHash::hash(value.toUtf8(), QCryptographicHash::Sha256).toHex().left(16);
}

void McpHttpServer::Impl::record_activity(const ExternalCallRecord& record) {
    {
        QMutexLocker lock(&activity_mutex);
        activity.prepend(record);
        while (activity.size() > mcp_http_detail::kActivityCapacity)
            activity.removeLast();
        ++counters.calls;
        if (record.denied)
            ++counters.denied;
        else if (record.success)
            ++counters.ok;
        else
            ++counters.failed;
    }
    emit q->activity(record);
}

void McpHttpServer::Impl::note_client(const QString& name, const QString& version, const QString& user_agent) {
    Q_UNUSED(version);
    const QString key = name + QLatin1Char('|') + user_agent;
    const QDateTime now = QDateTime::currentDateTime();
    auto known = distinct_clients.find(key);
    if (known != distinct_clients.end()) {
        known->last_seen = now;
        return;
    }
    if (distinct_clients.size() >= mcp_http_detail::kMaxTrackedClients)
        return;
    ExternalClientInfo info;
    info.name = name;
    info.user_agent = user_agent;
    info.first_seen = now;
    info.last_seen = now;
    distinct_clients.insert(key, info);
    emit q->status_changed();
}

void McpHttpServer::Impl::note_client_call(const QString& client) {
    // The activity label may carry " · terminal AI"; the client list is by name.
    const QString name = client.section(QStringLiteral(" · "), 0, 0);
    for (auto it = distinct_clients.begin(); it != distinct_clients.end(); ++it) {
        if (it->name == name) {
            ++it->calls;
            it->last_seen = QDateTime::currentDateTime();
            return;
        }
    }
}

void McpHttpServer::Impl::teardown_engine(const QString& reason) {
    using namespace mcp_http_detail;
    server::McpServerEngine* e = engine;
    QThread* t = thread;
    engine = nullptr;
    thread = nullptr;
    bound_port = 0;
    if (e && t && t->isRunning()) {
        // Answer the calls still running and close every socket ON THE ENGINE'S
        // THREAD. The engine thread never waits for the GUI thread, so this
        // blocking hand-off cannot deadlock; it is bounded by the engine's
        // shutdown flush (≤ 400 ms, normally a few ms). The engine itself is
        // deleted on its own thread as that thread finishes (deleteLater on a
        // thread whose loop is quitting runs in QThread's finish).
        QMetaObject::invokeMethod(
            e,
            [e, reason]() {
                e->shutdown(reason);
                e->deleteLater();
            },
            Qt::BlockingQueuedConnection);
    }
    if (t) {
        t->quit();
        if (t->wait(5000))
            delete t;
        else
            LOG_ERROR(kTag, "MCP server thread did not stop in time — leaking it instead of blocking the exit");
    }
}

// ── Lifecycle ────────────────────────────────────────────────────────────────

McpHttpServer& McpHttpServer::instance() {
    static McpHttpServer s;
    return s;
}

McpHttpServer::McpHttpServer(QObject* parent) : QObject(parent), d_(new Impl(this)) {
    using namespace mcp_http_detail;
    qRegisterMetaType<fincept::mcp::ExternalCallRecord>();
    qRegisterMetaType<fincept::mcp::ExternalClientInfo>();
    auto& cfg = AppConfig::instance();
    d_->enabled_at_startup = cfg.get(QString::fromLatin1(kKeyEnabledAtStartup), false).toBool();
    d_->persisted_allow_ui_control = cfg.get(QString::fromLatin1(kKeyAllowUiControl), true).toBool();
    d_->persisted_allow_scripts = cfg.get(QString::fromLatin1(kKeyAllowScripts), true).toBool();
    d_->persisted_result_budget_kb =
        std::clamp(cfg.get(QString::fromLatin1(kKeyResultBudgetKb), kDefaultResultBudgetKb).toInt(), kMinResultBudgetKb,
                   kMaxResultBudgetKb);
    d_->shared->allow_ui_control.store(d_->persisted_allow_ui_control);
    d_->shared->allow_scripts.store(d_->persisted_allow_scripts);
    d_->shared->result_budget_bytes.store(d_->persisted_result_budget_kb * 1024);
    // Seed auto mode here so pool threads never read QSettings themselves.
    ToolApprovalDialog::load_auto_approve_setting();
    const int port = cfg.get(QString::fromLatin1(kKeyPort), static_cast<int>(kDefaultPort)).toInt();
    d_->configured_port = (port >= kMinUserPort && port <= 65535 && !edition::is_private_mcp_port(static_cast<quint16>(port)))
                              ? static_cast<quint16>(port)
                              : kDefaultPort;

    // The approval UI and every signal receiver live on the GUI thread. If
    // something first touched instance() from a worker, move there.
    QCoreApplication* app = QCoreApplication::instance();
    if (app && thread() != app->thread())
        moveToThread(app->thread());
}

McpHttpServer::~McpHttpServer() {
    d_->shared->accepting.store(false);
    if (d_->engine && d_->thread) {
        // Static destruction without stop() (shutdown_mcp() normally ran at
        // aboutToQuit): the application may be gone, so do not hand work to the
        // engine thread — quit it and let process exit reclaim the rest.
        d_->thread->quit();
        if (!d_->thread->wait(2000))
            d_->thread = nullptr; // still running: leak rather than destroy a live QThread
        d_->engine = nullptr;
    }
    delete d_->thread;
    d_->thread = nullptr;
    {
        QMutexLocker lock(&d_->shared->pool_mutex);
        if (QThreadPool* pool = d_->shared->pool) {
            pool->clear();
            // Pool threads only block on an approval prompt (≤ 50 s); a tool still
            // running at process exit is not worth hanging the exit for — its job
            // holds `shared`, not Impl, so leaking the pool is safe.
            if (pool->waitForDone(2000)) {
                delete pool;
                d_->shared->pool = nullptr;
            }
        }
    }
    delete d_;
}

bool McpHttpServer::start() {
    using namespace mcp_http_detail;
    if (d_->engine)
        return true;

    // The bearer token first: without a readable one the server stays stopped
    // rather than serving under a freshly minted token nobody has configured.
    const QString auth_token = d_->effective_token();
    if (auth_token.isEmpty() && !d_->test_mode) {
        QString why;
        {
            QMutexLocker lock(&d_->token_mutex);
            why = d_->token_error;
        }
        if (why.isEmpty())
            why = QStringLiteral("unknown error");
        d_->start_pending = true;
        const int delay_ms = mcp_token::retry_delay_ms(++d_->token_retry_attempt);
        if (!d_->token_retry) {
            d_->token_retry = new QTimer(this);
            d_->token_retry->setSingleShot(true);
            connect(d_->token_retry, &QTimer::timeout, this, [this]() {
                if (d_->start_pending && !d_->engine)
                    start();
            });
        }
        d_->token_retry->start(delay_ms);
        d_->last_error = QStringLiteral("Could not read secure storage: %1. The server stays stopped so the access "
                                        "token your clients use is not replaced — retrying in %2 s.")
                             .arg(why)
                             .arg(delay_ms / 1000);
        LOG_TRANSITION(kTag, QStringLiteral("token_read"), 1, LogLevel::Warn,
                       logf("MCP server not started: its access token could not be read",
                            {{"reason", why}, {"retry_in_s", delay_ms / 1000}}));
        emit status_changed();
        return false;
    }
    if (d_->start_pending || d_->token_retry_attempt > 0) {
        d_->start_pending = false;
        d_->token_retry_attempt = 0;
        if (d_->token_retry)
            d_->token_retry->stop();
        LOG_TRANSITION(kTag, QStringLiteral("token_read"), 0, LogLevel::Info,
                       QStringLiteral("MCP server access token readable again"));
    }
    d_->publish_auth_token(auth_token);

    // Facts the backend serves from other threads, read here on the GUI thread.
    {
        QMutexLocker lock(&d_->shared->facts_mutex);
        d_->shared->app_version = QCoreApplication::applicationVersion();
        d_->shared->screens_text = build_screens_text();
    }
    if (!d_->backend)
        d_->backend = make_terminal_backend(d_->shared);

    auto* t = new QThread;
    t->setObjectName(QStringLiteral("mcp-http"));
    auto* e = new server::McpServerEngine(d_->backend, terminal_limits());
    e->set_result_budget_bytes(d_->shared->result_budget_bytes.load());
    e->moveToThread(t);
    connect(e, &server::McpServerEngine::call_finished, this, [this](const ExternalCallRecord& record) {
        d_->record_activity(record);
        d_->note_client_call(record.client);
    });
    connect(e, &server::McpServerEngine::client_initialized, this,
            [this](const QString& name, const QString& version, const QString& user_agent, const QString&,
                   const QString&) { d_->note_client(name, version, user_agent); });
    t->start();

    const quint16 first = d_->test_mode ? d_->test_port : d_->configured_port;
    const int range = d_->test_mode ? 0 : kPortFallbackRange;
    bool ok = false;
    QString err;
    QMetaObject::invokeMethod(
        e, [e, first, range, &ok, &err]() { ok = e->listen(first, range, &err); }, Qt::BlockingQueuedConnection);
    d_->engine = e;
    d_->thread = t;
    if (!ok) {
        d_->teardown_engine(QStringLiteral("bind failed"));
        d_->last_error = err;
        LOG_ERROR(kTag, logf("MCP server could not listen", {{"error", err}}));
        emit status_changed();
        return false;
    }
    d_->bound_port = e->port();
    d_->shared->accepting.store(true);
    d_->last_error = (!d_->test_mode && d_->bound_port != first)
                         ? QStringLiteral("Port %1 was in use; listening on %2 instead — clients configured for port "
                                          "%1 need the new URL.")
                               .arg(first)
                               .arg(d_->bound_port)
                         : QString();
    d_->distinct_clients.clear();
    {
        QMutexLocker lock(&d_->activity_mutex);
        d_->counters = ExternalServerStats{};
        d_->counters.started_at = QDateTime::currentDateTime();
    }

    // Build the tool catalogue off the GUI thread now, so the first tools/list is instant.
    std::shared_ptr<BackendShared> state = d_->shared;
    state->ensure_pool()->start([state]() { (void)current_catalogue(*state); });

    LOG_INFO(kTag, logf("MCP server listening",
                        {{"port", d_->bound_port},
                         {"ui_control", allow_ui_control() ? "on" : "look-only"},
                         {"python_scripts", allow_scripts() ? "on" : "off"}}));
    emit status_changed();
    return true;
}

void McpHttpServer::stop() {
    using namespace mcp_http_detail;
    if (d_->start_pending) {
        // The user stopped a server that was waiting for its token: stop retrying.
        d_->start_pending = false;
        d_->token_retry_attempt = 0;
        if (d_->token_retry)
            d_->token_retry->stop();
        if (!d_->engine) {
            d_->last_error.clear();
            emit status_changed();
        }
    }
    if (!d_->engine)
        return;
    d_->shared->accepting.store(false);
    d_->teardown_engine(QStringLiteral("stopped by the user or at exit"));
    {
        QMutexLocker lock(&d_->shared->pool_mutex);
        if (d_->shared->pool)
            d_->shared->pool->clear(); // calls that have not started yet are dropped
    }
    ToolApprovalDialog::deny_all_pending(); // release workers waiting on the user

    LOG_INFO(kTag, QStringLiteral("MCP server stopped"));
    emit status_changed();
}

void McpHttpServer::start_if_enabled() {
    if (!enabled_at_startup())
        return;
    if (!start())
        LOG_WARN(mcp_http_detail::kTag, logf("MCP server not started at launch", {{"error", last_error()}}));
}

bool McpHttpServer::is_running() const {
    return d_->engine != nullptr && d_->bound_port != 0;
}

quint16 McpHttpServer::port() const {
    return d_->engine ? d_->bound_port : 0;
}

QString McpHttpServer::url() const {
    if (!d_->engine)
        return {};
    return QStringLiteral("http://127.0.0.1:%1/mcp").arg(port());
}

QString McpHttpServer::last_error() const {
    return d_->last_error;
}

// ── Token ────────────────────────────────────────────────────────────────────

QString McpHttpServer::token() const {
    return d_->effective_token();
}

void McpHttpServer::regenerate_token() {
    using namespace mcp_http_detail;
    // The user's explicit choice (confirmed in Settings): replacing the stored
    // token is intended here, even when it could not be read.
    const QString fresh = generate_token();
    {
        QMutexLocker lock(&d_->token_mutex);
        d_->token = fresh;
        d_->token_error.clear();
    }
    const auto written = SecureStorage::instance().store(QString::fromLatin1(kTokenStorageKey), fresh);
    if (written.is_err())
        LOG_WARN(kTag, QStringLiteral("Could not persist the regenerated MCP token: %1")
                           .arg(QString::fromStdString(written.error())));
    if (!d_->test_mode)
        d_->publish_auth_token(fresh); // the old token stops working at once
    ToolApprovalDialog::clear_grants(); // revoking the token revokes what its clients were granted
    LOG_AUDIT(kTag, QStringLiteral("Credential rotated"),
              {{"credential", kTokenStorageKey}, {"persisted", written.is_ok()}});
    LOG_INFO(kTag, QStringLiteral("Bearer token regenerated — every configured client must be updated"));
    if (d_->start_pending && !d_->engine)
        start(); // was waiting for a readable token: there is one now
    emit status_changed();
}

QString McpHttpServer::begin_run(const RunTokenPolicy& policy, qint64 ttl_ms) {
    using namespace mcp_http_detail;
    RunTokenPolicy p = policy;
    if (p.run_id.isEmpty()) // a stable, non-secret id for agent attribution (never derived from the token)
        p.run_id = QStringLiteral("run-") +
                   QString::number(QRandomGenerator::system()->generate64(), 16).rightJustified(16, QLatin1Char('0'));
    const QString t = run_token_table().begin(p, ttl_ms, QDateTime::currentMSecsSinceEpoch());
    LOG_DEBUG(kTag, logf("Run token issued", {{"label", policy.label.isEmpty() ? QStringLiteral("in-app run")
                                                                                 : clip_text(policy.label, 60)},
                                              {"harness", policy.harness_id},
                                              {"no_navigation", policy.scope == RunToolScope::NoNavigation},
                                              {"paper_only", policy.paper_only}}));
    return t;
}

void McpHttpServer::end_run(const QString& run_token) {
    auto& table = mcp_http_detail::run_token_table();
    const auto policy = table.check(run_token, QDateTime::currentMSecsSinceEpoch());
    table.end(run_token);
    if (policy && !policy->run_id.isEmpty())
        ToolApprovalDialog::clear_grants_for_run(policy->run_id); // a run's timed grants end with it
}

std::optional<RunTokenPolicy> McpHttpServer::run_token_policy(const QString& supplied) const {
    return mcp_http_detail::run_token_table().check(supplied, QDateTime::currentMSecsSinceEpoch());
}

int McpHttpServer::live_run_tokens() const {
    return mcp_http_detail::run_token_table().live(QDateTime::currentMSecsSinceEpoch());
}

// ── Settings ─────────────────────────────────────────────────────────────────

bool McpHttpServer::enabled_at_startup() const {
    return d_->enabled_at_startup;
}

void McpHttpServer::set_enabled_at_startup(bool on) {
    if (d_->enabled_at_startup == on)
        return;
    d_->enabled_at_startup = on;
    AppConfig::instance().set(QString::fromLatin1(mcp_http_detail::kKeyEnabledAtStartup), on);
    emit status_changed();
}

bool McpHttpServer::allow_ui_control() const {
    return d_->shared->allow_ui_control.load();
}

void McpHttpServer::set_allow_ui_control(bool on) {
    using namespace mcp_http_detail;
    if (d_->test_mode) { // self-test: in memory only
        d_->shared->allow_ui_control.store(on);
        emit status_changed();
        return;
    }
    if (d_->persisted_allow_ui_control == on && d_->shared->allow_ui_control.load() == on)
        return;
    d_->persisted_allow_ui_control = on;
    d_->shared->allow_ui_control.store(on);
    AppConfig::instance().set(QString::fromLatin1(kKeyAllowUiControl), on);
    LOG_INFO(kTag, on ? QStringLiteral("External agents may operate the terminal's UI (risky steps still ask first)")
                      : QStringLiteral("External agents are limited to looking at the terminal"));
    emit status_changed();
}

bool McpHttpServer::allow_scripts() const {
    return d_->shared->allow_scripts.load();
}

void McpHttpServer::set_allow_scripts(bool on) {
    using namespace mcp_http_detail;
    if (d_->test_mode) {
        d_->shared->allow_scripts.store(on);
        emit status_changed();
        return;
    }
    if (d_->persisted_allow_scripts == on && d_->shared->allow_scripts.load() == on)
        return;
    d_->persisted_allow_scripts = on;
    d_->shared->allow_scripts.store(on);
    AppConfig::instance().set(QString::fromLatin1(kKeyAllowScripts), on);
    LOG_INFO(kTag, on ? QStringLiteral("External agents may run the bundled Python scripts")
                      : QStringLiteral("External agents may no longer run Python scripts"));
    emit status_changed();
}

bool McpHttpServer::auto_approve() const {
    return ToolApprovalDialog::auto_approve();
}

void McpHttpServer::set_auto_approve(bool on) {
    if (d_->test_mode) // self-test: never persist
        return;
    if (ToolApprovalDialog::auto_approve() == on)
        return;
    ToolApprovalDialog::set_auto_approve(on);
    emit status_changed();
}

int McpHttpServer::result_budget_kb() const {
    return d_->shared->result_budget_bytes.load() / 1024;
}

void McpHttpServer::set_result_budget_kb(int kb) {
    using namespace mcp_http_detail;
    const int clamped = std::clamp(kb, kMinResultBudgetKb, kMaxResultBudgetKb);
    if (d_->persisted_result_budget_kb == clamped && result_budget_kb() == clamped)
        return;
    d_->persisted_result_budget_kb = clamped;
    d_->shared->result_budget_bytes.store(clamped * 1024);
    if (d_->engine)
        d_->engine->set_result_budget_bytes(clamped * 1024); // atomic: safe across threads
    if (!d_->test_mode)
        AppConfig::instance().set(QString::fromLatin1(kKeyResultBudgetKb), clamped);
    emit status_changed();
}

quint16 McpHttpServer::configured_port() const {
    return d_->configured_port;
}

void McpHttpServer::set_configured_port(quint16 port) {
    using namespace mcp_http_detail;
    if (port < kMinUserPort) {
        LOG_WARN(kTag, logf("Ignoring an MCP server port outside 1024-65535", {{"port", port}}));
        return;
    }
    if (edition::is_private_mcp_port(port)) {
        // The other Fincept edition's MCP port: two servers on one port would
        // answer each other's clients. Kept at the current port; the UI shows why.
        d_->last_error = QStringLiteral("Port %1 is used by the other Fincept Terminal edition; keeping port %2.")
                             .arg(port)
                             .arg(d_->configured_port);
        LOG_WARN(kTag, logf("Refused the other edition's MCP port", {{"port", port}}));
        emit status_changed();
        return;
    }
    if (d_->configured_port == port)
        return;
    d_->configured_port = port;
    AppConfig::instance().set(QString::fromLatin1(kKeyPort), static_cast<int>(port));
    emit status_changed();
}

// ── Activity ─────────────────────────────────────────────────────────────────

QList<ExternalCallRecord> McpHttpServer::recent_activity() const {
    QMutexLocker lock(&d_->activity_mutex);
    return d_->activity;
}

void McpHttpServer::clear_activity() {
    {
        QMutexLocker lock(&d_->activity_mutex);
        d_->activity.clear();
    }
    emit status_changed();
}

int McpHttpServer::connected_client_count() const {
    return static_cast<int>(d_->distinct_clients.size());
}

QList<ExternalClientInfo> McpHttpServer::clients() const {
    QList<ExternalClientInfo> out = d_->distinct_clients.values();
    std::sort(out.begin(), out.end(),
              [](const ExternalClientInfo& a, const ExternalClientInfo& b) { return a.last_seen > b.last_seen; });
    return out;
}

ExternalServerStats McpHttpServer::stats() const {
    QMutexLocker lock(&d_->activity_mutex);
    return d_->counters;
}

// ── Snippets ─────────────────────────────────────────────────────────────────

QString McpHttpServer::claude_code_add_command() const {
    return QStringLiteral("claude mcp add --transport http %1 %2 -H \"Authorization: Bearer %3\" -s user")
        .arg(QString::fromLatin1(edition::kMcpServerName), d_->effective_url(), token());
}

QString McpHttpServer::codex_add_command() const {
    using namespace mcp_http_detail;
    QStringList bridge;
    for (const QString& part : stdio_bridge_argv())
        bridge << cli_quote(part);
    return QStringLiteral("codex mcp add %1 --env %2=%3 -- %4")
        .arg(QString::fromLatin1(edition::kMcpServerName), QString::fromLatin1(kTokenEnvVar), token(),
             bridge.join(QLatin1Char(' ')));
}

QStringList McpHttpServer::stdio_bridge_argv() const {
    QStringList argv{python_for_bridge(), bridge_script_path(), QStringLiteral("--url"), d_->effective_url()};
    if (process::host::in_flatpak()) {
        // The interpreter and the script are inside this app's sandbox, which a
        // client on the user's system cannot execute directly: it starts them in
        // a new instance of the app's sandbox. That instance shares the network
        // namespace (--share=network), so the bridge reaches 127.0.0.1 as before,
        // and `flatpak run` passes the client's environment (the token variable) on.
        const QString python = argv.takeFirst();
        argv.prepend(process::host::app_id());
        argv.prepend(QStringLiteral("--command=") + python);
        argv.prepend(QStringLiteral("run"));
        argv.prepend(QStringLiteral("flatpak"));
    }
    return argv;
}

QString McpHttpServer::codex_config_toml() const {
    using namespace mcp_http_detail;
    // tool_timeout_sec: an approval (≤ 50 s) or a ui_wait (≤ 60 s) can outlast
    // Codex's documented 60 s default.
    return QStringLiteral("[mcp_servers.%1]\nurl = %2\nbearer_token_env_var = %3\ntool_timeout_sec = 300\n")
        .arg(QString::fromLatin1(edition::kMcpServerName), toml_string(d_->effective_url()),
             toml_string(QString::fromLatin1(kTokenEnvVar)));
}

QString McpHttpServer::gemini_add_command() const {
    return QStringLiteral("gemini mcp add --transport http %1 %2 -H \"Authorization: Bearer %3\" -s user")
        .arg(QString::fromLatin1(edition::kMcpServerName), d_->effective_url(), token());
}

QString McpHttpServer::gemini_settings_json() const {
    const QJsonObject server{
        {QStringLiteral("httpUrl"), d_->effective_url()},
        {QStringLiteral("headers"),
         QJsonObject{{QStringLiteral("Authorization"), QStringLiteral("Bearer ") + token()}}},
        {QStringLiteral("timeout"), 300000},
    };
    const QJsonObject root{{QStringLiteral("mcpServers"), QJsonObject{{QString::fromLatin1(edition::kMcpServerName), server}}}};
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

QString McpHttpServer::vscode_json() const {
    const QJsonObject server{
        {QStringLiteral("type"), QStringLiteral("http")},
        {QStringLiteral("url"), d_->effective_url()},
        {QStringLiteral("headers"),
         QJsonObject{{QStringLiteral("Authorization"), QStringLiteral("Bearer ") + token()}}},
    };
    const QJsonObject root{{QStringLiteral("servers"), QJsonObject{{QString::fromLatin1(edition::kMcpServerName), server}}}};
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

QString McpHttpServer::generic_http_json() const {
    const QJsonObject server{
        {QStringLiteral("type"), QStringLiteral("http")},
        {QStringLiteral("url"), d_->effective_url()},
        {QStringLiteral("headers"), QJsonObject{{QStringLiteral("Authorization"), QStringLiteral("Bearer ") + token()}}},
    };
    const QJsonObject root{{QStringLiteral("mcpServers"), QJsonObject{{QString::fromLatin1(edition::kMcpServerName), server}}}};
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

QString McpHttpServer::stdio_bridge_json() const {
    QStringList argv = stdio_bridge_argv(); // Flatpak: flatpak run --command=<python> <app id> <script> --url …
    const QString command = argv.takeFirst();
    const QJsonObject server{
        {QStringLiteral("command"), command},
        {QStringLiteral("args"), QJsonArray::fromStringList(argv)},
        {QStringLiteral("env"), QJsonObject{{QString::fromLatin1(kTokenEnvVar), token()}}},
    };
    const QJsonObject root{{QStringLiteral("mcpServers"), QJsonObject{{QString::fromLatin1(edition::kMcpServerName), server}}}};
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

QString McpHttpServer::python_for_bridge() const {
    auto& setup = python::PythonSetupManager::instance();
    for (const QString& venv : {QStringLiteral("venv-numpy2"), QStringLiteral("venv-numpy1")}) {
        const QString path = setup.python_path(venv);
        if (QFileInfo::exists(path))
            return QDir::toNativeSeparators(path);
    }
    // The managed base interpreter. Scanned here rather than via
    // base_python_path(), which may spawn `uv python find` and block for seconds —
    // this getter runs on the GUI thread.
    const QDir py_dir(setup.install_dir() + QStringLiteral("/python"));
    const QStringList installs = py_dir.entryList({QStringLiteral("cpython-3.*")}, QDir::Dirs, QDir::Name | QDir::Reversed);
    for (const QString& entry : installs) {
#ifdef Q_OS_WIN
        const QString path = py_dir.filePath(entry + QStringLiteral("/python.exe"));
#else
        const QString path = py_dir.filePath(entry + QStringLiteral("/bin/python3"));
#endif
        if (QFileInfo::exists(path))
            return QDir::toNativeSeparators(path);
    }
    const QString runner = python::PythonRunner::instance().python_path();
    if (!runner.isEmpty() && QFileInfo::exists(runner))
        return QDir::toNativeSeparators(runner);
#ifdef Q_OS_WIN
    return QStringLiteral("python");
#else
    return QStringLiteral("python3");
#endif
}

QString McpHttpServer::bridge_script_path() const {
    const QString scripts = python::PythonRunner::instance().scripts_dir();
    return QDir::toNativeSeparators(
        QFileInfo(scripts + QStringLiteral("/fincept_mcp/fincept_mcp_stdio.py")).absoluteFilePath());
}

QJsonObject McpHttpServer::claude_mcp_config_env() const {
    const QJsonObject server{
        {QStringLiteral("type"), QStringLiteral("http")},
        {QStringLiteral("url"), d_->effective_url()},
        {QStringLiteral("headers"),
         QJsonObject{{QStringLiteral("Authorization"),
                      QStringLiteral("Bearer ${%1}").arg(QString::fromLatin1(kTokenEnvVar))}}},
    };
    return QJsonObject{{QStringLiteral("mcpServers"), QJsonObject{{QString::fromLatin1(edition::kMcpServerName), server}}}};
}

QStringList McpHttpServer::codex_config_overrides() const {
    // tool_timeout_sec: Codex's documented 60 s default is shorter than ui_act
    // (approvals, 180 s budget) and ui_wait (≤ 60 s + overhead) — same value as
    // the snippet.
    const QString name = QString::fromLatin1(edition::kMcpServerName);
    return {QStringLiteral("-c"),
            QStringLiteral("mcp_servers.%1.url=\"%2\"").arg(name, d_->effective_url()),
            QStringLiteral("-c"),
            QStringLiteral("mcp_servers.%1.bearer_token_env_var=\"%2\"").arg(name, QString::fromLatin1(kTokenEnvVar)),
            QStringLiteral("-c"),
            QStringLiteral("mcp_servers.%1.tool_timeout_sec=300").arg(name)};
}

// ── Self-test hooks ──────────────────────────────────────────────────────────

void McpHttpServer::set_selftest_overrides(quint16 port, const QString& token, bool allow_ui_control,
                                           bool allow_scripts) {
    d_->test_mode = true;
    d_->test_port = port;
    d_->test_token = token;
    d_->shared->allow_ui_control.store(allow_ui_control);
    d_->shared->allow_scripts.store(allow_scripts);
}

void McpHttpServer::clear_selftest_overrides() {
    d_->test_mode = false;
    d_->test_port = 0;
    d_->test_token.clear();
    d_->shared->allow_ui_control.store(d_->persisted_allow_ui_control);
    d_->shared->allow_scripts.store(d_->persisted_allow_scripts);
    d_->shared->result_budget_bytes.store(d_->persisted_result_budget_kb * 1024);
    if (!d_->engine)
        d_->publish_auth_token(QString()); // the next start() publishes the real token again
}

QString McpHttpServer::advertised_name(const QString& internal_name) {
    return mcp_http_detail::make_advertised_name(internal_name);
}

// ── Connection self-check (McpServerSelfCheck.h) ─────────────────────────────

void run_mcp_self_check_async(QObject* context, std::function<void(const SelfCheckReport&)> done) {
    auto& srv = McpHttpServer::instance();
    SelfCheckOptions o;
    o.url = srv.url();
    o.token = srv.token();
    o.python = srv.python_for_bridge();
    o.bridge_script = srv.bridge_script_path();
    // The guard is only ever read on the GUI thread (QPointer is not
    // thread-safe): the worker posts to the application object, which outlives
    // any settings page, and the delivery checks the page there.
    QPointer<QObject> guard(context);
    QThread* t = QThread::create([o, guard, done]() {
        const SelfCheckReport report = run_mcp_self_check(o);
        if (QCoreApplication* app = QCoreApplication::instance())
            QMetaObject::invokeMethod(
                app,
                [guard, done, report]() {
                    if (guard && done)
                        done(report);
                },
                Qt::QueuedConnection);
    });
    t->setObjectName(QStringLiteral("mcp-self-check"));
    QObject::connect(t, &QThread::finished, t, &QObject::deleteLater);
    t->start();
}

} // namespace fincept::mcp
