// McpHttpServer.cpp — lifecycle, persisted settings, bearer token, activity log
// and connection snippets of the external MCP server. The transport lives in
// McpHttpServer_Http.cpp, JSON-RPC in McpHttpServer_Rpc.cpp and the tool side in
// McpHttpServer_Tools.cpp (see McpHttpServer_p.h).

#include "mcp/server/McpHttpServer.h"

#include "core/config/AppConfig.h"
#include "core/logging/Logger.h"
#include "mcp/server/McpHttpServer_p.h"
#include "mcp/server/ToolApprovalDialog.h"
#include "python/PythonRunner.h"
#include "python/PythonSetupManager.h"
#include "storage/secure/SecureStorage.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonDocument>
#include <QMutexLocker>
#include <QRandomGenerator>
#include <QTcpServer>
#include <QTcpSocket>
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

} // namespace

QString generate_token() {
    std::array<quint32, 8> words{};
    QRandomGenerator::system()->fillRange(words.data(), static_cast<qsizetype>(words.size()));
    const QByteArray raw(reinterpret_cast<const char*>(words.data()), static_cast<qsizetype>(sizeof(words)));
    return QString::fromLatin1(raw.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
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
    const auto stored = SecureStorage::instance().retrieve(key);
    if (stored.is_ok() && !stored.value().trimmed().isEmpty()) {
        token = stored.value().trimmed();
        return token;
    }
    token = generate_token();
    const auto written = SecureStorage::instance().store(key, token);
    if (written.is_err())
        LOG_WARN(kTag, QStringLiteral("Could not persist the MCP server token — clients will need a new one after a "
                                      "restart: %1")
                           .arg(QString::fromStdString(written.error())));
    else
        LOG_INFO(kTag, QStringLiteral("Generated the MCP server bearer token"));
    return token;
}

QString McpHttpServer::Impl::effective_url() const {
    quint16 p = server ? static_cast<quint16>(server->serverPort()) : (test_mode ? test_port : configured_port);
    if (p == 0)
        p = configured_port;
    return QStringLiteral("http://127.0.0.1:%1/mcp").arg(p);
}

void McpHttpServer::Impl::ensure_pool() {
    if (pool)
        return;
    pool = new QThreadPool; // owned; see ~McpHttpServer
    pool->setObjectName(QStringLiteral("mcp-http-tools"));
    pool->setMaxThreadCount(mcp_http_detail::kPoolThreads);
    pool->setExpiryTimeout(60 * 1000);
}

void McpHttpServer::Impl::record_activity(const ExternalCallRecord& record) {
    {
        QMutexLocker lock(&activity_mutex);
        activity.prepend(record);
        while (activity.size() > mcp_http_detail::kActivityCapacity)
            activity.removeLast();
    }
    emit q->activity(record);
}

// ── Lifecycle ────────────────────────────────────────────────────────────────

McpHttpServer& McpHttpServer::instance() {
    static McpHttpServer s;
    return s;
}

McpHttpServer::McpHttpServer(QObject* parent) : QObject(parent), d_(new Impl(this)) {
    using namespace mcp_http_detail;
    qRegisterMetaType<fincept::mcp::ExternalCallRecord>();
    auto& cfg = AppConfig::instance();
    d_->enabled_at_startup = cfg.get(QString::fromLatin1(kKeyEnabledAtStartup), false).toBool();
    d_->persisted_allow_actions = cfg.get(QString::fromLatin1(kKeyAllowActions), false).toBool();
    d_->shared->allow_actions.store(d_->persisted_allow_actions);
    d_->toolset = cfg.get(QString::fromLatin1(kKeyToolset), QStringLiteral("essentials")).toString() ==
                          QLatin1String("full")
                      ? ExternalToolset::Full
                      : ExternalToolset::Essentials;
    const int port = cfg.get(QString::fromLatin1(kKeyPort), static_cast<int>(kDefaultPort)).toInt();
    d_->configured_port = (port >= kMinUserPort && port <= 65535) ? static_cast<quint16>(port) : kDefaultPort;

    d_->sweeper = new QTimer(this);
    d_->sweeper->setInterval(kSweepIntervalMs);
    connect(d_->sweeper, &QTimer::timeout, this, [this]() { d_->sweep(); });

    // The listener, every socket and the approval UI live on the GUI thread. If
    // something first touched instance() from a worker, move there (children too).
    QCoreApplication* app = QCoreApplication::instance();
    if (app && thread() != app->thread())
        moveToThread(app->thread());
}

McpHttpServer::~McpHttpServer() {
    d_->shared->alive.store(false);
    d_->shared->accepting.store(false);
    if (d_->server) {
        QObject::disconnect(d_->server, nullptr, this, nullptr);
        d_->server->close();
    }
    for (auto it = d_->conns.cbegin(); it != d_->conns.cend(); ++it)
        QObject::disconnect(it.key(), nullptr, this, nullptr);
    if (d_->pool) {
        d_->pool->clear();
        // A tool still running at process exit is not worth hanging the exit for:
        // its job holds `shared` (not Impl), so leaking the pool is safe.
        if (d_->pool->waitForDone(2000))
            delete d_->pool;
    }
    delete d_;
}

bool McpHttpServer::start() {
    using namespace mcp_http_detail;
    if (d_->server)
        return true;

    auto* srv = new QTcpServer(this);
    quint16 bound = 0;
    QString err;
    const quint16 first = d_->test_mode ? d_->test_port : d_->configured_port;
    const int last = d_->test_mode ? first : std::min(65535, static_cast<int>(first) + kPortFallbackRange);
    for (int p = first; p <= last; ++p) {
        if (srv->listen(QHostAddress::LocalHost, static_cast<quint16>(p))) {
            bound = static_cast<quint16>(srv->serverPort());
            break;
        }
        err = srv->errorString();
    }
    if (bound == 0) {
        d_->last_error = first == last ? QStringLiteral("Could not listen on 127.0.0.1:%1 — %2").arg(first).arg(err)
                                       : QStringLiteral("Could not listen on 127.0.0.1 ports %1-%2 — %3")
                                             .arg(first)
                                             .arg(last)
                                             .arg(err);
        LOG_ERROR(kTag, d_->last_error);
        delete srv;
        emit status_changed();
        return false;
    }

    d_->server = srv;
    connect(srv, &QTcpServer::newConnection, this, [this]() { d_->on_new_connection(); });
    connect(srv, &QTcpServer::acceptError, this, [this](QAbstractSocket::SocketError) {
        if (d_->server)
            LOG_WARN(kTag, QStringLiteral("Accept failed: %1").arg(d_->server->errorString()));
    });
    ++d_->epoch;
    d_->last_error = (!d_->test_mode && bound != first)
                         ? QStringLiteral("Port %1 was in use; listening on %2 instead — clients configured for port "
                                          "%1 need the new URL.")
                               .arg(first)
                               .arg(bound)
                         : QString();
    d_->distinct_clients.clear();
    d_->client_by_agent.clear();
    d_->shared->accepting.store(true);
    d_->sweeper->start();

    // Build the tool catalogue off the GUI thread now, so the first tools/list is instant.
    d_->ensure_pool();
    std::shared_ptr<WorkerShared> state = d_->shared;
    d_->pool->start([state]() { (void)current_catalogue(*state); });

    LOG_INFO(kTag, QStringLiteral("Listening on %1 (toolset: %2, actions: %3)")
                       .arg(url(), d_->toolset == ExternalToolset::Full ? QStringLiteral("full")
                                                                        : QStringLiteral("essentials"),
                            allow_actions() ? QStringLiteral("allowed") : QStringLiteral("read-only")));
    emit status_changed();
    return true;
}

void McpHttpServer::stop() {
    using namespace mcp_http_detail;
    if (!d_->server)
        return;
    d_->shared->accepting.store(false);
    d_->sweeper->stop();

    QTcpServer* srv = d_->server;
    d_->server = nullptr;
    QObject::disconnect(srv, nullptr, this, nullptr);
    srv->close();
    const QList<QTcpSocket*> sockets = d_->conns.keys();
    d_->conns.clear();
    for (QTcpSocket* s : sockets) {
        QObject::disconnect(s, nullptr, this, nullptr);
        s->abort();
    }
    srv->deleteLater(); // the accepted sockets are its children and go with it
    d_->pending.clear();
    if (d_->pool)
        d_->pool->clear(); // calls that have not started yet are dropped
    ToolApprovalDialog::deny_all_pending(); // release workers waiting on the user

    LOG_INFO(kTag, QStringLiteral("Stopped"));
    emit status_changed();
}

void McpHttpServer::start_if_enabled() {
    if (!enabled_at_startup())
        return;
    if (!start())
        LOG_WARN(mcp_http_detail::kTag, QStringLiteral("Not started at launch: %1").arg(last_error()));
}

bool McpHttpServer::is_running() const {
    return d_->server != nullptr && d_->server->isListening();
}

quint16 McpHttpServer::port() const {
    return d_->server ? static_cast<quint16>(d_->server->serverPort()) : 0;
}

QString McpHttpServer::url() const {
    if (!d_->server)
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
    const QString fresh = generate_token();
    {
        QMutexLocker lock(&d_->token_mutex);
        d_->token = fresh;
    }
    const auto written = SecureStorage::instance().store(QString::fromLatin1(kTokenStorageKey), fresh);
    if (written.is_err())
        LOG_WARN(kTag, QStringLiteral("Could not persist the regenerated MCP token: %1")
                           .arg(QString::fromStdString(written.error())));
    ToolApprovalDialog::clear_grants(); // revoking the token revokes what its clients were granted
    LOG_INFO(kTag, QStringLiteral("Bearer token regenerated — every configured client must be updated"));
    emit status_changed();
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

bool McpHttpServer::allow_actions() const {
    return d_->shared->allow_actions.load();
}

void McpHttpServer::set_allow_actions(bool on) {
    using namespace mcp_http_detail;
    if (d_->test_mode) { // self-test: in memory only
        d_->shared->allow_actions.store(on);
        emit status_changed();
        return;
    }
    if (d_->persisted_allow_actions == on && d_->shared->allow_actions.load() == on)
        return;
    d_->persisted_allow_actions = on;
    d_->shared->allow_actions.store(on);
    AppConfig::instance().set(QString::fromLatin1(kKeyAllowActions), on);
    LOG_INFO(kTag, on ? QStringLiteral("External agents may now run state-changing tools (live trading still needs "
                                       "per-call approval)")
                      : QStringLiteral("External agents are limited to read-only tools"));
    emit status_changed();
}

ExternalToolset McpHttpServer::toolset() const {
    return d_->toolset;
}

void McpHttpServer::set_toolset(ExternalToolset t) {
    if (d_->toolset == t)
        return;
    d_->toolset = t;
    AppConfig::instance().set(QString::fromLatin1(mcp_http_detail::kKeyToolset),
                              t == ExternalToolset::Full ? QStringLiteral("full") : QStringLiteral("essentials"));
    emit status_changed();
}

quint16 McpHttpServer::configured_port() const {
    return d_->configured_port;
}

void McpHttpServer::set_configured_port(quint16 port) {
    using namespace mcp_http_detail;
    if (port < kMinUserPort) {
        LOG_WARN(kTag, QStringLiteral("Ignoring MCP server port %1 — use 1024-65535").arg(port));
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

int McpHttpServer::connected_client_count() const {
    return static_cast<int>(d_->distinct_clients.size());
}

// ── Snippets ─────────────────────────────────────────────────────────────────

QString McpHttpServer::claude_code_add_command() const {
    return QStringLiteral("claude mcp add --transport http fincept_terminal %1 -H \"Authorization: Bearer %2\" -s user")
        .arg(d_->effective_url(), token());
}

QString McpHttpServer::codex_add_command() const {
    using namespace mcp_http_detail;
    return QStringLiteral("codex mcp add fincept_terminal --env %1=%2 -- %3 %4 --url %5")
        .arg(QString::fromLatin1(kTokenEnvVar), token(), cli_quote(python_for_bridge()),
             cli_quote(bridge_script_path()), d_->effective_url());
}

QString McpHttpServer::codex_config_toml() const {
    using namespace mcp_http_detail;
    return QStringLiteral("[mcp_servers.fincept_terminal]\nurl = %1\nbearer_token_env_var = %2\n")
        .arg(toml_string(d_->effective_url()), toml_string(QString::fromLatin1(kTokenEnvVar)));
}

QString McpHttpServer::generic_http_json() const {
    const QJsonObject server{
        {QStringLiteral("type"), QStringLiteral("http")},
        {QStringLiteral("url"), d_->effective_url()},
        {QStringLiteral("headers"), QJsonObject{{QStringLiteral("Authorization"), QStringLiteral("Bearer ") + token()}}},
    };
    const QJsonObject root{{QStringLiteral("mcpServers"), QJsonObject{{QStringLiteral("fincept_terminal"), server}}}};
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

QString McpHttpServer::stdio_bridge_json() const {
    const QJsonObject server{
        {QStringLiteral("command"), python_for_bridge()},
        {QStringLiteral("args"), QJsonArray{bridge_script_path(), QStringLiteral("--url"), d_->effective_url()}},
        {QStringLiteral("env"), QJsonObject{{QString::fromLatin1(kTokenEnvVar), token()}}},
    };
    const QJsonObject root{{QStringLiteral("mcpServers"), QJsonObject{{QStringLiteral("fincept_terminal"), server}}}};
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
        QFileInfo(scripts + QStringLiteral("/mcp/fincept_mcp_stdio.py")).absoluteFilePath());
}

QJsonObject McpHttpServer::claude_mcp_config_env() const {
    const QJsonObject server{
        {QStringLiteral("type"), QStringLiteral("http")},
        {QStringLiteral("url"), d_->effective_url()},
        {QStringLiteral("headers"),
         QJsonObject{{QStringLiteral("Authorization"),
                      QStringLiteral("Bearer ${%1}").arg(QString::fromLatin1(kTokenEnvVar))}}},
    };
    return QJsonObject{{QStringLiteral("mcpServers"), QJsonObject{{QStringLiteral("fincept"), server}}}};
}

QStringList McpHttpServer::codex_config_overrides() const {
    return {QStringLiteral("-c"), QStringLiteral("mcp_servers.fincept.url=\"%1\"").arg(d_->effective_url()),
            QStringLiteral("-c"),
            QStringLiteral("mcp_servers.fincept.bearer_token_env_var=\"%1\"").arg(QString::fromLatin1(kTokenEnvVar))};
}

// ── Self-test hooks ──────────────────────────────────────────────────────────

void McpHttpServer::set_selftest_overrides(quint16 port, const QString& token, bool allow_actions) {
    d_->test_mode = true;
    d_->test_port = port;
    d_->test_token = token;
    d_->shared->allow_actions.store(allow_actions);
}

void McpHttpServer::clear_selftest_overrides() {
    d_->test_mode = false;
    d_->test_port = 0;
    d_->test_token.clear();
    d_->shared->allow_actions.store(d_->persisted_allow_actions);
}

QString McpHttpServer::advertised_name(const QString& internal_name) {
    return mcp_http_detail::make_advertised_name(internal_name);
}

} // namespace fincept::mcp
