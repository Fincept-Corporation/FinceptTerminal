// HarnessRegistry.cpp — detection, probing, cache and settings for the agent
// harnesses (see HarnessRegistry.h).

#include "services/harness/HarnessRegistry.h"

#include "core/config/AppConfig.h"
#include "core/config/AppPaths.h"
#include "core/logging/Logger.h"
#include "core/privacy/LogRedact.h"
#include "core/process/HostExec.h"
#include "services/harness/HarnessDetect.h"
#include "services/harness/HarnessHostDetect.h"
#include "services/harness/HarnessLaunch.h"
#include "services/harness/IHarnessAdapter.h"
#include "services/harness/adapters/ClaudeAdapter.h"
#include "services/harness/adapters/CodexAdapter.h"
#include "services/harness/adapters/CopilotAdapter.h"
#include "services/harness/adapters/CursorAdapter.h"
#include "services/harness/adapters/GeminiAdapter.h"
#include "services/harness/adapters/GrokAdapter.h"
#include "services/harness/adapters/QwenAdapter.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QPointer>
#include <QProcess>
#include <QSaveFile>
#include <QSet>
#include <QThread>
#include <QThreadPool>
#include <QTimer>

#include <memory>
#include <optional>

namespace fincept::harness {

namespace harness_registry_detail {

constexpr const char* kHarnessRegistryTag = "Harness";
constexpr int kCacheFormat = 1;
constexpr qint64 kMaxCacheBytes = 4 * 1024 * 1024;
constexpr qint64 kMaxProbeOutputBytes = 8 * 1024 * 1024; // `codex debug models` is ~260 KB
constexpr int kCacheSaveDelayMs = 1000;
constexpr qint64 kDefaultCooldownMs = 5LL * 60 * 1000;
constexpr int kShutdownProbeWaitMs = 1000;
constexpr int kShutdownPoolWaitMs = 1500;

enum class ProbeKind { Version, Auth, Models };

struct ProbeJob {
    QString id;
    ProbeKind kind = ProbeKind::Version;
    quint64 generation = 0;
    Installation inst;
    ProbeCommand cmd;
};

/// What one probe process produced. The bytes of a sign-in probe go to the
/// adapter's parse_auth() and nowhere else.
struct ProbeOutcome {
    bool started = true;
    bool timed_out = false;
    bool crashed = false;
    bool overflow = false;
    int exit_code = -1;
    QByteArray out;
    QByteArray err;
    QString error_text; // FailedToStart / launch-spec error
};

/// One harness in registry.json. Never holds credential material.
struct CacheEntry {
    Installation inst; // path, via, node/script, size, mtime, version, version_line
    qint64 checked_at_ms = 0;
    bool has_auth = false;
    AuthStatus auth; // state / method / plan / note only
    bool has_models = false;
    QString models_version;
    qint64 models_checked_at_ms = 0;
    QVector<ModelInfo> models;
};

struct SearchResult {
    QVector<Installation> candidates;
    AuthStatus hint;
    // Flatpak (the search ran on the host, HarnessHostDetect):
    bool on_host = false;
    bool host_blocked = false;       // host commands are not allowed at all
    QString host_error;              // the host could not be searched
    QStringList user_mcp_names;      // the adapter's user_mcp_server_names() over the host's files
};

qint64 registry_now_ms() {
    return QDateTime::currentMSecsSinceEpoch();
}

/// --smoke-test / --selftest-*: detect on demand only, never sign-in or model probes.
bool registry_headless_run() {
    const QStringList args = QCoreApplication::arguments();
    for (const auto& a : args) {
        if (a == QLatin1String("--smoke-test") || a.startsWith(QLatin1String("--selftest")))
            return true;
    }
    return false;
}

QString cfg_key(const QString& id, const char* leaf) {
    return QStringLiteral("harness/%1/%2").arg(id, QLatin1String(leaf));
}

QString default_harness_key() {
    return QStringLiteral("harness/default_id");
}

bool same_path(const QString& a, const QString& b) {
#ifdef Q_OS_WIN
    return a.compare(b, Qt::CaseInsensitive) == 0;
#else
    return a == b;
#endif
}

/// The cached installation is this exact file (same path, launcher, size, mtime).
bool same_file(const Installation& cached, const Installation& now) {
    return now.file_size >= 0 && cached.file_size == now.file_size && cached.file_mtime_ms == now.file_mtime_ms &&
           cached.via == now.via && same_path(cached.path, now.path) &&
           same_path(cached.script_path, now.script_path) && same_path(cached.node_path, now.node_path);
}

/// Worker thread: size + mtime of the file that changes on update.
void stat_installation(Installation& inst) {
    const QFileInfo fi(inst.via == Installation::NodeScript ? inst.script_path : inst.path);
    if (fi.isFile()) {
        inst.file_size = fi.size();
        inst.file_mtime_ms = fi.lastModified().toMSecsSinceEpoch();
    }
}

QString cache_file_path() {
    return AppPaths::cache() + QStringLiteral("/harness/registry.json");
}

// ── registry.json ───────────────────────────────────────────────────────────

QString via_id(Installation::Via via) {
    switch (via) {
        case Installation::CmdShim:
            return QStringLiteral("cmd_shim");
        case Installation::NodeScript:
            return QStringLiteral("node_script");
        case Installation::Wsl:
            return QStringLiteral("wsl");
        case Installation::Native:
            break;
    }
    return QStringLiteral("native");
}

Installation::Via via_from_id(const QString& id) {
    if (id == QLatin1String("cmd_shim"))
        return Installation::CmdShim;
    if (id == QLatin1String("node_script"))
        return Installation::NodeScript;
    if (id == QLatin1String("wsl"))
        return Installation::Wsl;
    return Installation::Native;
}

QString auth_state_id(AuthState s) {
    switch (s) {
        case AuthState::SignedIn:
            return QStringLiteral("signed_in");
        case AuthState::SignedOut:
            return QStringLiteral("signed_out");
        case AuthState::Error:
            return QStringLiteral("error");
        case AuthState::Unknown:
            break;
    }
    return QStringLiteral("unknown");
}

AuthState auth_state_from_id(const QString& id) {
    if (id == QLatin1String("signed_in"))
        return AuthState::SignedIn;
    if (id == QLatin1String("signed_out"))
        return AuthState::SignedOut;
    if (id == QLatin1String("error"))
        return AuthState::Error;
    return AuthState::Unknown;
}

QJsonObject model_to_json(const ModelInfo& m) {
    return QJsonObject{
        {QStringLiteral("id"), m.id},
        {QStringLiteral("label"), m.label},
        {QStringLiteral("description"), m.description},
        {QStringLiteral("is_default"), m.is_default},
        {QStringLiteral("efforts"), QJsonArray::fromStringList(m.efforts)},
        {QStringLiteral("default_effort"), m.default_effort},
        {QStringLiteral("context_window"), QJsonValue(m.context_window)},
    };
}

ModelInfo model_from_json(const QJsonObject& o) {
    ModelInfo m;
    m.id = o.value(QStringLiteral("id")).toString();
    m.label = o.value(QStringLiteral("label")).toString();
    m.description = o.value(QStringLiteral("description")).toString();
    m.is_default = o.value(QStringLiteral("is_default")).toBool();
    for (const QJsonValue& v : o.value(QStringLiteral("efforts")).toArray())
        m.efforts << v.toString();
    m.default_effort = o.value(QStringLiteral("default_effort")).toString();
    m.context_window = o.value(QStringLiteral("context_window")).toInteger();
    return m;
}

QJsonObject cache_entry_to_json(const CacheEntry& c) {
    const Installation& i = c.inst;
    QJsonObject o{
        {QStringLiteral("installation"),
         QJsonObject{
             {QStringLiteral("path"), i.path},
             {QStringLiteral("via"), via_id(i.via)},
             {QStringLiteral("node_path"), i.node_path},
             {QStringLiteral("script_path"), i.script_path},
             {QStringLiteral("size"), QJsonValue(i.file_size)},
             {QStringLiteral("mtime_ms"), QJsonValue(i.file_mtime_ms)},
             {QStringLiteral("version"), i.version},
             {QStringLiteral("version_line"), i.version_line},
         }},
        {QStringLiteral("checked_at_ms"), QJsonValue(c.checked_at_ms)},
    };
    if (c.has_auth) {
        o.insert(QStringLiteral("auth"), QJsonObject{
                                             {QStringLiteral("state"), auth_state_id(c.auth.state)},
                                             {QStringLiteral("method"), c.auth.method},
                                             {QStringLiteral("plan"), c.auth.plan},
                                             {QStringLiteral("note"), c.auth.note},
                                             {QStringLiteral("checked_at_ms"), QJsonValue(c.auth.checked_at_ms)},
                                         });
    }
    if (c.has_models) {
        QJsonArray list;
        for (const ModelInfo& m : c.models)
            list.append(model_to_json(m));
        o.insert(QStringLiteral("models"), QJsonObject{
                                               {QStringLiteral("version"), c.models_version},
                                               {QStringLiteral("checked_at_ms"), QJsonValue(c.models_checked_at_ms)},
                                               {QStringLiteral("list"), list},
                                           });
    }
    return o;
}

CacheEntry cache_entry_from_json(const QJsonObject& o) {
    CacheEntry c;
    const QJsonObject i = o.value(QStringLiteral("installation")).toObject();
    c.inst.path = i.value(QStringLiteral("path")).toString();
    c.inst.via = via_from_id(i.value(QStringLiteral("via")).toString());
    c.inst.node_path = i.value(QStringLiteral("node_path")).toString();
    c.inst.script_path = i.value(QStringLiteral("script_path")).toString();
    c.inst.file_size = i.value(QStringLiteral("size")).toInteger(-1);
    c.inst.file_mtime_ms = i.value(QStringLiteral("mtime_ms")).toInteger();
    c.inst.version = i.value(QStringLiteral("version")).toString();
    c.inst.version_line = i.value(QStringLiteral("version_line")).toString();
    c.checked_at_ms = o.value(QStringLiteral("checked_at_ms")).toInteger();
    const QJsonObject a = o.value(QStringLiteral("auth")).toObject();
    if (!a.isEmpty()) {
        c.has_auth = true;
        c.auth.state = auth_state_from_id(a.value(QStringLiteral("state")).toString());
        c.auth.method = a.value(QStringLiteral("method")).toString();
        c.auth.plan = a.value(QStringLiteral("plan")).toString();
        c.auth.note = a.value(QStringLiteral("note")).toString();
        c.auth.checked_at_ms = a.value(QStringLiteral("checked_at_ms")).toInteger();
    }
    const QJsonObject m = o.value(QStringLiteral("models")).toObject();
    if (!m.isEmpty()) {
        c.models_version = m.value(QStringLiteral("version")).toString();
        c.models_checked_at_ms = m.value(QStringLiteral("checked_at_ms")).toInteger();
        for (const QJsonValue& v : m.value(QStringLiteral("list")).toArray()) {
            ModelInfo info = model_from_json(v.toObject());
            if (!info.id.isEmpty())
                c.models.append(info);
        }
        c.has_models = !c.models.isEmpty();
    }
    return c;
}

/// Worker thread.
QHash<QString, CacheEntry> read_cache_file(const QString& path) {
    QHash<QString, CacheEntry> out;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly) || f.size() > kMaxCacheBytes) {
        if (f.exists())
            LOG_DEBUG(kHarnessRegistryTag, logf("Harness cache could not be read",
                                                {{"path", log_path(path)}, {"bytes", f.size()}, {"error", f.errorString()}}));
        return out;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    const QJsonObject root = doc.object();
    if (root.value(QStringLiteral("format")).toInt() != kCacheFormat)
        return out;
    const QJsonObject harnesses = root.value(QStringLiteral("harnesses")).toObject();
    for (auto it = harnesses.constBegin(); it != harnesses.constEnd(); ++it) {
        CacheEntry c = cache_entry_from_json(it.value().toObject());
        if (!c.inst.path.isEmpty())
            out.insert(it.key(), c);
    }
    return out;
}

QByteArray serialize_cache(const QHash<QString, CacheEntry>& cache) {
    QJsonObject harnesses;
    for (auto it = cache.cbegin(); it != cache.cend(); ++it) {
        if (!it->inst.path.isEmpty())
            harnesses.insert(it.key(), cache_entry_to_json(it.value()));
    }
    return QJsonDocument(QJsonObject{{QStringLiteral("format"), kCacheFormat},
                                     {QStringLiteral("harnesses"), harnesses}})
        .toJson(QJsonDocument::Indented);
}

/// Worker thread (GUI thread only at shutdown).
void write_cache_file(const QString& path, const QByteArray& bytes) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(bytes) != bytes.size() || !f.commit())
        LOG_DEBUG(kHarnessRegistryTag,
                  logf("Harness cache could not be written", {{"path", log_path(path)}, {"error", f.errorString()}}));
}

QString version_probe_error(const ProbeOutcome& o, int timeout_ms) {
    if (!o.started)
        return o.error_text.isEmpty() ? QStringLiteral("The CLI could not be started.") : o.error_text;
    if (o.timed_out)
        return QStringLiteral("`--version` did not answer within %1 s.").arg(timeout_ms / 1000);
    if (o.overflow)
        return QStringLiteral("`--version` printed an unexpected amount of output.");
    const QString err = QString::fromUtf8(o.err).trimmed();
    if (!err.isEmpty())
        return err.left(300);
    return QStringLiteral("`--version` exited with code %1").arg(o.exit_code);
}

} // namespace harness_registry_detail

using namespace harness_registry_detail;

// ── Private state ────────────────────────────────────────────────────────────

struct HarnessRegistry::Impl {
    struct Entry {
        std::shared_ptr<const IHarnessAdapter> adapter;
        HarnessStatus st;
        quint64 generation = 0; // bumps with every detection pass
        bool searching = false;
        bool probing_version = false;
        bool rerun = false; // a refresh arrived while detecting
        QVector<Installation> candidates;
        int next = 0;
        std::optional<Installation> fallback; // first candidate whose version could not be read
        QString fallback_error;
        QStringList rejected; // binaries that identified as another program
        int auth_inflight = 0;
        int models_inflight = 0;
        bool force_auth = false;
        bool force_models = false;
        QString models_version; // version the CLI model list in st.models belongs to
        AuthStatus hint;        // env-var names + file existence, from the last search
    };

    explicit Impl(HarnessRegistry* owner) : q(owner) {}

    HarnessRegistry* q;
    QStringList order;
    QHash<QString, Entry> entries;
    QHash<QString, CacheEntry> cache;
    QList<ProbeJob> queue;
    QSet<QProcess*> live;
    int running = 0;
    QThreadPool* pool = nullptr; // no parent; see shutdown()
    QTimer* save_timer = nullptr;
    // Flatpak: MCP entry names of the user's own harness config, read on the
    // host (detection, refresh_host_mcp_names) — the sandbox cannot read it.
    QHash<QString, QStringList> host_mcp_names;
    QSet<QString> host_mcp_refreshing;
    bool initialized = false; // settings migrated, cache load started
    bool started = false;     // initialize_deferred() ran
    bool migrated = false;
    bool pump_scheduled = false;
    bool shutting_down = false;

    void add(std::shared_ptr<const IHarnessAdapter> adapter) {
        const QString id = adapter->descriptor().id;
        Entry e;
        e.adapter = std::move(adapter);
        e.st.id = id;
        order << id;
        entries.insert(id, std::move(e));
    }

    QString display_name(const QString& id) const {
        const auto it = entries.constFind(id);
        return it == entries.cend() ? id : it->adapter->descriptor().display_name;
    }

    static void sync_flags(Entry& e) {
        e.st.detecting = e.searching || e.probing_version;
        e.st.probing_auth = e.auth_inflight > 0;
        e.st.probing_models = e.models_inflight > 0;
    }

    void init() {
        if (initialized)
            return;
        initialized = true;
        migrate_settings();
        load_cache();
    }

    void migrate_settings();
    void load_cache();
    void merge_cache(const QHash<QString, CacheEntry>& loaded);
    void start_detection(const QString& id);
    void on_search_done(const QString& id, quint64 generation, const SearchResult& result);
    void try_next_candidate(const QString& id);
    void on_version_result(const ProbeJob& job, const ProbeOutcome& o);
    void finish_detection(const QString& id, const std::optional<Installation>& inst, const QString& error);
    void after_detection(const QString& id);
    void enqueue(ProbeJob job);
    void schedule_pump();
    void pump();
    void start_probe(const ProbeJob& job);
    void release_probe(QProcess* proc);
    void on_probe_done(const ProbeJob& job, const ProbeOutcome& o);
    void on_auth_result(const ProbeJob& job, const ProbeOutcome& o);
    void on_models_result(const ProbeJob& job, const ProbeOutcome& o);
    void refresh_host_mcp_names(const QString& id);
    void schedule_save();
    void save_now(bool synchronous);
    /// `save_cache`: false from the destructor (static destruction — the
    /// profile paths may already be gone; an unsaved cache is merely re-probed).
    void shutdown(bool save_cache);
};

void HarnessRegistry::Impl::migrate_settings() {
    if (migrated)
        return;
    migrated = true;
    auto& cfg = AppConfig::instance();
    static const struct {
        const char* id;
        const char* legacy_key;
    } kLegacy[] = {{"claude", "coding_agents/claude_path"}, {"codex", "coding_agents/codex_path"}};
    for (const auto& l : kLegacy) {
        const QString legacy = QString::fromLatin1(l.legacy_key);
        const QVariant old = cfg.get(legacy);
        if (!old.isValid())
            continue;
        const QString value = old.toString().trimmed();
        const QString key = cfg_key(QString::fromLatin1(l.id), "path_override");
        if (!value.isEmpty() && cfg.get(key).toString().trimmed().isEmpty())
            cfg.set(key, value);
        cfg.remove(legacy);
        LOG_INFO(kHarnessRegistryTag, QStringLiteral("Moved setting %1 to %2").arg(legacy, key));
    }
}

void HarnessRegistry::Impl::load_cache() {
    if (!pool)
        return;
    const QString path = cache_file_path();
    QPointer<HarnessRegistry> self(q);
    // Same one-thread pool as the searches, submitted first: the cache is in
    // place before any search result reaches the GUI thread.
    pool->start([self, path]() {
        const QHash<QString, CacheEntry> loaded = read_cache_file(path);
        QCoreApplication* app = QCoreApplication::instance();
        if (!app)
            return;
        QMetaObject::invokeMethod(
            app,
            [self, loaded]() {
                if (self)
                    self->d_->merge_cache(loaded);
            },
            Qt::QueuedConnection);
    });
}

void HarnessRegistry::Impl::merge_cache(const QHash<QString, CacheEntry>& loaded) {
    for (auto it = loaded.cbegin(); it != loaded.cend(); ++it) {
        if (!cache.contains(it.key()))
            cache.insert(it.key(), it.value());
    }
}

// ── Detection ────────────────────────────────────────────────────────────────

void HarnessRegistry::Impl::start_detection(const QString& id) {
    auto it = entries.find(id);
    if (it == entries.end() || shutting_down || !pool)
        return;
    Entry& e = *it;
    ++e.generation;
    e.searching = true;
    e.probing_version = false;
    e.rerun = false;
    e.candidates.clear();
    e.next = 0;
    e.fallback.reset();
    e.fallback_error.clear();
    e.rejected.clear();
    sync_flags(e);

    const DetectContext ctx = make_system_detect_context(q->path_override(id));
    const std::shared_ptr<const IHarnessAdapter> adapter = e.adapter;
    const quint64 generation = e.generation;
    // Flatpak: the CLIs are programs of the user's system, outside the sandbox —
    // the same search rules, answered by the host (HarnessHostDetect.h).
    const bool on_host = process::host::in_flatpak();
    QPointer<HarnessRegistry> self(q);
    pool->start([self, id, generation, adapter, ctx, on_host]() {
        SearchResult r;
        if (on_host) {
            HostSearchResult h = search_on_host(*adapter, ctx.override_path);
            r.on_host = true;
            r.candidates = h.candidates; // size + mtime from the host
            r.hint = h.hint;
            r.host_blocked = h.blocked;
            r.host_error = h.error;
            r.user_mcp_names = h.user_mcp_server_names;
        } else {
            r.candidates = find_candidates(*adapter, ctx);
            for (Installation& c : r.candidates)
                stat_installation(c);
            r.hint = adapter->auth_hint(ctx); // names + existence only
        }
        QCoreApplication* app = QCoreApplication::instance();
        if (!app)
            return;
        QMetaObject::invokeMethod(
            app,
            [self, id, generation, r]() {
                if (self)
                    self->d_->on_search_done(id, generation, r);
            },
            Qt::QueuedConnection);
    });
    emit q->status_changed(id);
}

void HarnessRegistry::Impl::on_search_done(const QString& id, quint64 generation, const SearchResult& result) {
    auto it = entries.find(id);
    if (it == entries.end() || it->generation != generation || !it->searching || shutting_down)
        return;
    Entry& e = *it;
    e.searching = false;
    e.hint = result.hint;
    e.candidates = result.candidates;
    e.next = 0;
    e.st.host_blocked = result.host_blocked;
    if (result.on_host && result.host_error.isEmpty())
        host_mcp_names.insert(id, result.user_mcp_names);
    if (e.candidates.isEmpty()) {
        const QString configured = q->path_override(id);
        QString why;
        if (!result.host_error.isEmpty())
            why = result.host_error; // blocked sandbox, or the host did not answer: never "not installed"
        else if (!configured.isEmpty())
            why = QStringLiteral("The configured path %1 does not exist%2.")
                      .arg(QDir::toNativeSeparators(configured),
                           result.on_host ? QStringLiteral(" on your system") : QString());
        else if (result.on_host)
            why = QStringLiteral("%1 was not found on your system (searched outside the Flatpak sandbox, with your "
                                 "login shell's PATH). Install it in a terminal on your computer, then press RE-DETECT.")
                      .arg(display_name(id));
        finish_detection(id, std::nullopt, why);
        return;
    }
    e.probing_version = true;
    sync_flags(e);
    try_next_candidate(id);
}

void HarnessRegistry::Impl::try_next_candidate(const QString& id) {
    auto it = entries.find(id);
    if (it == entries.end())
        return;
    Entry& e = *it;
    if (e.next < e.candidates.size()) {
        Installation cand = e.candidates.at(e.next++);
        // Same file as last time, already identified: no probe needed.
        const auto c = cache.constFind(id);
        if (c != cache.cend() && same_file(c->inst, cand) && !c->inst.version.isEmpty() &&
            !e.adapter->parse_version(c->inst.version_line).isEmpty()) {
            cand.version = c->inst.version;
            cand.version_line = c->inst.version_line;
            finish_detection(id, cand, {});
            return;
        }
        ProbeJob job;
        job.id = id;
        job.kind = ProbeKind::Version;
        job.generation = e.generation;
        job.inst = cand;
        job.cmd = e.adapter->version_command();
        enqueue(std::move(job));
        return;
    }
    // Every candidate tried.
    if (e.fallback) {
        const Installation fallback = *e.fallback;
        finish_detection(id, fallback, e.fallback_error);
        return;
    }
    finish_detection(id, std::nullopt,
                     e.rejected.isEmpty() ? QString()
                                          : QStringLiteral("Found, but not %1: %2")
                                                .arg(display_name(id), e.rejected.join(QStringLiteral("; "))));
}

void HarnessRegistry::Impl::on_version_result(const ProbeJob& job, const ProbeOutcome& o) {
    auto it = entries.find(job.id);
    if (it == entries.end() || it->generation != job.generation || !it->probing_version)
        return;
    Entry& e = *it;
    const QString out = QString::fromUtf8(o.out);
    const QString text = out.trimmed().isEmpty() ? QString::fromUtf8(o.err) : out;
    const bool ran = o.started && !o.timed_out && !o.crashed && !o.overflow;
    Installation cand = job.inst;
    const VersionVerdict verdict = check_version(*e.adapter, cand, o.exit_code, ran, text);
    if (verdict == VersionVerdict::Accepted) {
        finish_detection(job.id, cand, {});
        return;
    }
    if (verdict == VersionVerdict::Rejected) {
        const QString why = QStringLiteral("%1 reports “%2”").arg(QDir::toNativeSeparators(cand.path), cand.version_line);
        if (cand.path_overridden) { // the user's explicit choice is kept, with a warning
            cand.version.clear();
            finish_detection(job.id, cand,
                             QStringLiteral("This does not look like %1: %2.").arg(display_name(job.id), why));
            return;
        }
        e.rejected << why;
    } else {
        const QString err = version_probe_error(o, job.cmd.timeout_ms);
        if (cand.path_overridden) {
            finish_detection(job.id, cand, err);
            return;
        }
        if (!e.fallback) {
            e.fallback = cand;
            e.fallback_error = err;
        }
    }
    try_next_candidate(job.id);
}

void HarnessRegistry::Impl::finish_detection(const QString& id, const std::optional<Installation>& inst,
                                             const QString& error) {
    auto it = entries.find(id);
    if (it == entries.end())
        return;
    Entry& e = *it;
    const std::optional<Installation> before = e.st.installation;
    e.searching = false;
    e.probing_version = false;
    e.candidates.clear();
    e.next = 0;
    e.fallback.reset();
    e.st.installation = inst;
    e.st.detection_complete = true;
    e.st.detect_error = error;
    sync_flags(e);
    const QString name = display_name(id);
    if (!inst) {
        e.st.auth = e.hint;
        e.st.models.clear();
        e.st.models_from_cli = false;
        e.st.models_checked_at_ms = 0;
        e.st.models_error.clear();
        e.models_version.clear();
        if (before)
            LOG_INFO(kHarnessRegistryTag, QStringLiteral("%1 is no longer found").arg(name));
    } else {
        CacheEntry& c = cache[id];
        if (!same_file(c.inst, *inst) || c.inst.version != inst->version)
            c = CacheEntry{}; // another binary: its sign-in state and models do not carry over
        c.inst = *inst;
        c.checked_at_ms = registry_now_ms();
        schedule_save();
        if (!before || !same_path(before->path, inst->path) || before->version != inst->version) {
            LOG_INFO(kHarnessRegistryTag,
                     QStringLiteral("Agent CLI found harness=%1 version=%2 via=%3 path=%4")
                         .arg(id, inst->version.isEmpty() ? QStringLiteral("unknown") : inst->version,
                              inst->via == Installation::NodeScript ? QStringLiteral("npm_node")
                              : inst->via == Installation::CmdShim  ? QStringLiteral("npm_cmd")
                                                                    : QStringLiteral("native"),
                              log_path(inst->path))); // home -> ~, install dir -> <app> (LogRedact.h)
        }
    }
    emit q->status_changed(id);

    it = entries.find(id); // a slot may have re-entered
    if (it == entries.end() || shutting_down)
        return;
    if (it->rerun) {
        start_detection(id);
        return;
    }
    if (inst)
        after_detection(id);
}

void HarnessRegistry::Impl::after_detection(const QString& id) {
    auto it = entries.find(id);
    if (it == entries.end() || !it->st.installation || shutting_down)
        return;
    Entry& e = *it;
    const Installation inst = *e.st.installation;
    const qint64 now = registry_now_ms();
    const CacheEntry c = cache.value(id);
    const bool cache_matches = same_file(c.inst, inst) && c.inst.version == inst.version;
    const bool probes_allowed = !registry_headless_run();

    // ── Sign-in state (TTL kAuthTtlMs) ──
    const std::optional<ProbeCommand> auth_cmd = e.adapter->auth_command();
    if (auth_cmd && probes_allowed) {
        const qint64 age = now - c.auth.checked_at_ms;
        const bool fresh = cache_matches && c.has_auth && age >= 0 && age < kAuthTtlMs;
        if (fresh && !e.force_auth) {
            e.st.auth = c.auth;
            e.st.auth.api_key_env_present = e.hint.api_key_env_present;
        } else if (e.auth_inflight == 0) {
            enqueue(ProbeJob{id, ProbeKind::Auth, e.generation, inst, *auth_cmd});
        }
    } else {
        e.st.auth = e.hint;
        e.st.auth.checked_at_ms = now;
    }
    e.force_auth = false;

    // ── Models (TTL kModelsTtlMs, or until the version changes) ──
    const std::optional<ProbeCommand> models_cmd = e.adapter->models_command();
    if (models_cmd && probes_allowed) {
        const qint64 age = now - c.models_checked_at_ms;
        const bool fresh = cache_matches && c.has_models && c.models_version == inst.version && age >= 0 &&
                           age < kModelsTtlMs;
        if (fresh && !e.force_models) {
            e.st.models = c.models;
            e.st.models_from_cli = true;
            e.st.models_checked_at_ms = c.models_checked_at_ms;
            e.st.models_error.clear();
            e.models_version = inst.version;
        } else {
            if (!e.st.models_from_cli || e.models_version != inst.version) {
                e.st.models = e.adapter->builtin_models(inst.version);
                e.st.models_from_cli = false;
            }
            if (e.models_inflight == 0)
                enqueue(ProbeJob{id, ProbeKind::Models, e.generation, inst, *models_cmd});
        }
    } else {
        e.st.models = e.adapter->builtin_models(inst.version);
        e.st.models_from_cli = false;
        e.st.models_checked_at_ms = now;
        e.models_version = inst.version;
    }
    e.force_models = false;
    sync_flags(e);
    emit q->status_changed(id);
}

// ── Probes (QProcess on the GUI event loop, ≤ kMaxConcurrentProbes) ─────────

void HarnessRegistry::Impl::enqueue(ProbeJob job) {
    auto it = entries.find(job.id);
    if (it == entries.end() || shutting_down)
        return;
    if (job.kind == ProbeKind::Auth)
        ++it->auth_inflight;
    else if (job.kind == ProbeKind::Models)
        ++it->models_inflight;
    sync_flags(*it);
    if (job.kind == ProbeKind::Version) {
        // Identification first: it is what runs wait for.
        qsizetype pos = 0;
        while (pos < queue.size() && queue.at(pos).kind == ProbeKind::Version)
            ++pos;
        queue.insert(pos, std::move(job));
    } else {
        queue.append(std::move(job));
    }
    schedule_pump();
}

void HarnessRegistry::Impl::schedule_pump() {
    if (pump_scheduled || shutting_down)
        return;
    pump_scheduled = true;
    QTimer::singleShot(0, q, [this]() {
        pump_scheduled = false;
        pump();
    });
}

void HarnessRegistry::Impl::pump() {
    while (!shutting_down && running < kMaxConcurrentProbes && !queue.isEmpty())
        start_probe(queue.takeFirst());
}

void HarnessRegistry::Impl::start_probe(const ProbeJob& job) {
    const LaunchSpec spec = make_launch_spec(job.inst, job.cmd.args);
    if (!spec.error.isEmpty()) {
        ProbeOutcome o;
        o.started = false;
        o.error_text = spec.error;
        on_probe_done(job, o);
        return;
    }
    auto* proc = new QProcess(q);
    proc->setProcessEnvironment(q->child_environment(job.id, job.inst));
    proc->setProcessChannelMode(QProcess::SeparateChannels);
    proc->setStandardInputFile(QProcess::nullDevice()); // a probe never waits on stdin
    const auto ent = entries.constFind(job.id);
    apply_launch_spec(proc, spec, /*own_tree=*/false,
                      ent != entries.cend() ? host_env_names(ent->adapter->descriptor()) : QStringList{});

    auto outcome = std::make_shared<ProbeOutcome>();
    auto done = std::make_shared<bool>(false);
    auto* timer = new QTimer(proc);
    timer->setSingleShot(true);
    QObject::connect(timer, &QTimer::timeout, proc, [proc, outcome]() {
        outcome->timed_out = true;
        proc->kill();
    });
    auto drain = [proc, outcome]() {
        const QByteArray out = proc->readAllStandardOutput();
        const QByteArray err = proc->readAllStandardError();
        if (outcome->overflow)
            return;
        outcome->out += out;
        outcome->err += err;
        if (outcome->out.size() + outcome->err.size() > kMaxProbeOutputBytes) {
            outcome->overflow = true;
            outcome->out.clear();
            outcome->err.clear();
            proc->kill();
        }
    };
    QObject::connect(proc, &QProcess::readyReadStandardOutput, q, drain);
    QObject::connect(proc, &QProcess::readyReadStandardError, q, drain);
    QObject::connect(proc, &QProcess::finished, q,
                     [this, proc, outcome, done, drain, job](int code, QProcess::ExitStatus status) {
                         if (*done)
                             return;
                         *done = true;
                         drain();
                         outcome->exit_code = code;
                         outcome->crashed = status == QProcess::CrashExit;
                         release_probe(proc);
                         on_probe_done(job, *outcome);
                     });
    QObject::connect(proc, &QProcess::errorOccurred, q, [this, proc, outcome, done, job](QProcess::ProcessError e) {
        if (e != QProcess::FailedToStart || *done)
            return;
        *done = true;
        outcome->started = false;
        outcome->error_text = proc->errorString();
        release_probe(proc);
        on_probe_done(job, *outcome);
    });
    live.insert(proc);
    ++running;
    timer->start(job.cmd.timeout_ms);
    proc->start();
}

void HarnessRegistry::Impl::release_probe(QProcess* proc) {
    if (live.remove(proc))
        --running;
    QObject::disconnect(proc, nullptr, q, nullptr);
    proc->deleteLater();
    schedule_pump();
}

void HarnessRegistry::Impl::on_probe_done(const ProbeJob& job, const ProbeOutcome& o) {
    if (shutting_down)
        return;
    switch (job.kind) {
        case ProbeKind::Version:
            on_version_result(job, o);
            break;
        case ProbeKind::Auth:
            on_auth_result(job, o);
            break;
        case ProbeKind::Models:
            on_models_result(job, o);
            break;
    }
}

void HarnessRegistry::Impl::on_auth_result(const ProbeJob& job, const ProbeOutcome& o) {
    auto it = entries.find(job.id);
    if (it == entries.end())
        return;
    Entry& e = *it;
    e.auth_inflight = qMax(0, e.auth_inflight - 1);
    AuthStatus a;
    if (!o.started || o.timed_out || o.overflow) {
        a.state = AuthState::Error;
        a.note = !o.started  ? QStringLiteral("The sign-in check could not be started.")
                 : o.timed_out ? QStringLiteral("The sign-in check did not answer in time.")
                               : QStringLiteral("The sign-in check printed an unexpected amount of output.");
    } else {
        // The adapter reads its named fields; the bytes are not kept or logged.
        a = e.adapter->parse_auth(o.crashed ? -1 : o.exit_code, o.out, o.err);
    }
    a.api_key_env_present = e.hint.api_key_env_present;
    a.checked_at_ms = registry_now_ms();
    if (e.st.installation && same_path(e.st.installation->path, job.inst.path)) {
        e.st.auth = a;
        auto c = cache.find(job.id);
        if (c != cache.end() && same_file(c->inst, job.inst)) {
            // Only definite answers are cached; an error is retried next time.
            c->has_auth = a.state == AuthState::SignedIn || a.state == AuthState::SignedOut;
            c->auth = a;
            schedule_save();
        }
    }
    sync_flags(e);
    emit q->status_changed(job.id);
}

void HarnessRegistry::Impl::on_models_result(const ProbeJob& job, const ProbeOutcome& o) {
    auto it = entries.find(job.id);
    if (it == entries.end())
        return;
    Entry& e = *it;
    e.models_inflight = qMax(0, e.models_inflight - 1);
    const bool ran = o.started && !o.timed_out && !o.crashed && !o.overflow;
    const QVector<ModelInfo> list = ran ? e.adapter->parse_models(o.exit_code, o.out) : QVector<ModelInfo>{};
    const bool current = e.st.installation && same_path(e.st.installation->path, job.inst.path) &&
                         e.st.installation->version == job.inst.version;
    if (current) {
        const qint64 now = registry_now_ms();
        if (!list.isEmpty()) {
            e.st.models = list;
            e.st.models_from_cli = true;
            e.st.models_checked_at_ms = now;
            e.st.models_error.clear();
            e.models_version = job.inst.version;
            auto c = cache.find(job.id);
            if (c != cache.end() && same_file(c->inst, job.inst)) {
                c->has_models = true;
                c->models = list;
                c->models_version = job.inst.version;
                c->models_checked_at_ms = now;
                schedule_save();
            }
        } else {
            if (!e.st.models_from_cli || e.models_version != job.inst.version) {
                e.st.models = e.adapter->builtin_models(job.inst.version);
                e.st.models_from_cli = false;
            }
            e.st.models_error = QStringLiteral("The model list could not be read from the CLI; showing the built-in list.");
        }
    }
    sync_flags(e);
    emit q->status_changed(job.id);
}

// ── Flatpak: the user's MCP entry names, read on the host ───────────────────

void HarnessRegistry::Impl::refresh_host_mcp_names(const QString& id) {
    const auto it = entries.constFind(id);
    if (it == entries.cend() || !pool || shutting_down || host_mcp_refreshing.contains(id))
        return;
    host_mcp_refreshing.insert(id);
    const std::shared_ptr<const IHarnessAdapter> adapter = it->adapter;
    QPointer<HarnessRegistry> self(q);
    pool->start([self, id, adapter]() {
        const std::optional<QStringList> names = host_user_mcp_server_names(*adapter);
        QCoreApplication* app = QCoreApplication::instance();
        if (!app)
            return;
        QMetaObject::invokeMethod(
            app,
            [self, id, names]() {
                if (!self)
                    return;
                self->d_->host_mcp_refreshing.remove(id);
                if (names) // a failed read keeps what was known
                    self->d_->host_mcp_names.insert(id, *names);
            },
            Qt::QueuedConnection);
    });
}

// ── Cache file ───────────────────────────────────────────────────────────────

void HarnessRegistry::Impl::schedule_save() {
    if (save_timer && !shutting_down)
        save_timer->start();
}

void HarnessRegistry::Impl::save_now(bool synchronous) {
    if (save_timer)
        save_timer->stop();
    const QByteArray bytes = serialize_cache(cache);
    const QString path = cache_file_path();
    if (synchronous || !pool) {
        write_cache_file(path, bytes);
        return;
    }
    pool->start([path, bytes]() { write_cache_file(path, bytes); });
}

void HarnessRegistry::Impl::shutdown(bool save_cache) {
    if (shutting_down)
        return;
    const bool save_pending = save_cache && save_timer && save_timer->isActive();
    shutting_down = true;
    queue.clear();
    const QList<QProcess*> procs = live.values();
    live.clear();
    running = 0;
    for (QProcess* p : procs) {
        QObject::disconnect(p, nullptr, q, nullptr);
        if (p->state() != QProcess::NotRunning) {
            p->kill();
            p->waitForFinished(kShutdownProbeWaitMs); // teardown only
        }
        delete p;
    }
    if (save_pending)
        save_now(/*synchronous=*/true); // teardown only: a few KB
    if (pool) {
        pool->clear();
        // A search stuck on an unreachable network path is abandoned (the pool
        // is leaked), never waited for without bound.
        if (pool->waitForDone(kShutdownPoolWaitMs))
            delete pool;
        pool = nullptr;
    }
}

// ── Public API ───────────────────────────────────────────────────────────────

HarnessRegistry& HarnessRegistry::instance() {
    static HarnessRegistry s;
    return s;
}

HarnessRegistry::HarnessRegistry() : QObject(nullptr), d_(std::make_unique<Impl>(this)) {
    // Registration order = the order every list shows (installed ones first).
    // Verified against this machine's CLIs and recorded runs:
    d_->add(std::make_shared<ClaudeAdapter>());
    d_->add(std::make_shared<CodexAdapter>());
    // BETA (descriptor().beta): Grok from its --help, the rest from vendor docs.
    d_->add(std::make_shared<GrokAdapter>());
    d_->add(std::make_shared<CursorAdapter>());
    d_->add(std::make_shared<CopilotAdapter>());
    d_->add(std::make_shared<GeminiAdapter>());
    d_->add(std::make_shared<QwenAdapter>());

    d_->pool = new QThreadPool;
    d_->pool->setMaxThreadCount(1); // FIFO: the cache load lands before any search result

    d_->save_timer = new QTimer(this);
    d_->save_timer->setSingleShot(true);
    d_->save_timer->setInterval(kCacheSaveDelayMs);
    connect(d_->save_timer, &QTimer::timeout, this, [this]() { d_->save_now(false); });

    if (auto* app = QCoreApplication::instance()) {
        connect(app, &QCoreApplication::aboutToQuit, this, [this]() { d_->shutdown(/*save_cache=*/true); });
        // QProcess and timers need GUI-thread affinity.
        if (QThread::currentThread() != app->thread())
            moveToThread(app->thread());
    }
}

HarnessRegistry::~HarnessRegistry() {
    if (d_)
        d_->shutdown(/*save_cache=*/false); // normally done at aboutToQuit already
}

void HarnessRegistry::initialize_deferred() {
    if (d_->started || d_->shutting_down)
        return;
    d_->started = true;
    d_->init();
    if (registry_headless_run()) {
        LOG_DEBUG(kHarnessRegistryTag, "Headless run — harness detection on demand only");
        return;
    }
    ensure_detected();
}

QStringList HarnessRegistry::ids() const {
    return d_->order;
}

const IHarnessAdapter* HarnessRegistry::adapter(const QString& id) const {
    const auto it = d_->entries.constFind(id);
    return it == d_->entries.cend() ? nullptr : it->adapter.get();
}

bool HarnessRegistry::is_known(const QString& id) const {
    return d_->entries.contains(id);
}

QString HarnessRegistry::display_name(const QString& id) const {
    const auto it = d_->entries.constFind(id);
    if (it != d_->entries.cend())
        return it->adapter->descriptor().display_name;
    return tr("unknown harness (%1)").arg(id.isEmpty() ? QStringLiteral("?") : id.left(40));
}

QStringList HarnessRegistry::installed_ids() const {
    QStringList out;
    for (const QString& id : d_->order) {
        const auto it = d_->entries.constFind(id);
        if (it != d_->entries.cend() && it->st.installation)
            out << id;
    }
    return out;
}

QString HarnessRegistry::preferred_harness() const {
    const QString def = default_harness();
    const QStringList installed = installed_ids();
    if (!def.isEmpty() && installed.contains(def))
        return def;
    if (!installed.isEmpty())
        return installed.constFirst();
    if (!def.isEmpty())
        return def;
    return d_->order.isEmpty() ? QString() : d_->order.constFirst();
}

HarnessStatus HarnessRegistry::status(const QString& id) const {
    const auto it = d_->entries.constFind(id);
    if (it == d_->entries.cend()) {
        HarnessStatus unknown;
        unknown.id = id;
        unknown.detection_complete = true;
        return unknown;
    }
    return it->st;
}

bool HarnessRegistry::detection_complete(const QString& id) const {
    const auto it = d_->entries.constFind(id);
    return it == d_->entries.cend() || it->st.detection_complete;
}

void HarnessRegistry::ensure_detected(const QString& id) {
    if (d_->shutting_down)
        return;
    d_->init();
    const QStringList targets = id.isEmpty() ? d_->order : QStringList{id};
    for (const QString& h : targets) {
        const auto it = d_->entries.constFind(h);
        if (it != d_->entries.cend() && !it->st.detection_complete && !it->searching && !it->probing_version)
            d_->start_detection(h);
    }
}

void HarnessRegistry::refresh(const QString& id, RefreshScope scope) {
    if (d_->shutting_down)
        return;
    d_->init();
    const QStringList targets = id.isEmpty() ? d_->order : QStringList{id};
    for (const QString& h : targets) {
        auto it = d_->entries.find(h);
        if (it == d_->entries.end())
            continue;
        if (scope_has(scope, RefreshScope::Auth))
            it->force_auth = true;
        if (scope_has(scope, RefreshScope::Models))
            it->force_models = true;
        if (scope_has(scope, RefreshScope::Paths) || scope_has(scope, RefreshScope::Version)) {
            if (it->searching || it->probing_version)
                it->rerun = true; // sign-in / models follow the detection
            else
                d_->start_detection(h);
            continue;
        }
        if (it->st.installation && !it->searching && !it->probing_version)
            d_->after_detection(h);
    }
}

void HarnessRegistry::revalidate() {
    if (d_->shutting_down)
        return;
    d_->init();
    if (registry_headless_run())
        return; // detection on demand only; never sign-in or model probes
    ensure_detected();
    const bool on_host = process::host::in_flatpak();
    for (const QString& h : std::as_const(d_->order)) {
        const auto it = d_->entries.constFind(h);
        if (it != d_->entries.cend() && it->st.installation && !it->searching && !it->probing_version) {
            d_->after_detection(h); // TTL-respecting: re-probes only what is stale
            if (on_host)
                d_->refresh_host_mcp_names(h); // so the next run disables what the user's config has now
        }
    }
}

QStringList HarnessRegistry::user_mcp_server_names(const QString& id) {
    const IHarnessAdapter* a = adapter(id);
    if (!a)
        return {};
    if (!process::host::in_flatpak())
        return a->user_mcp_server_names(make_system_detect_context(QString())); // a small local file
    // Flatpak: the config is on the host. The names read there by the last
    // detection / refresh (revalidate() runs whenever a harness screen is shown),
    // and a fresh read for the next run — never a wait on the GUI thread.
    d_->refresh_host_mcp_names(id);
    return d_->host_mcp_names.value(id);
}

QString HarnessRegistry::path_override(const QString& id) const {
    d_->migrate_settings();
    return AppConfig::instance().get(cfg_key(id, "path_override"), QString()).toString().trimmed();
}

void HarnessRegistry::set_path_override(const QString& id, const QString& path) {
    d_->migrate_settings();
    const QString key = cfg_key(id, "path_override");
    const QString value = path.trimmed();
    if (value.isEmpty())
        AppConfig::instance().remove(key);
    else
        AppConfig::instance().set(key, value);
    refresh(id, RefreshScope::Detect);
}

QString HarnessRegistry::default_model(const QString& id) const {
    return AppConfig::instance().get(cfg_key(id, "default_model"), QString()).toString().trimmed();
}

void HarnessRegistry::set_default_model(const QString& id, const QString& model) {
    const QString value = model.trimmed();
    if (value.isEmpty())
        AppConfig::instance().remove(cfg_key(id, "default_model"));
    else
        AppConfig::instance().set(cfg_key(id, "default_model"), value);
    emit defaults_changed();
}

QString HarnessRegistry::default_effort(const QString& id) const {
    return AppConfig::instance().get(cfg_key(id, "default_effort"), QString()).toString().trimmed();
}

void HarnessRegistry::set_default_effort(const QString& id, const QString& effort) {
    const QString value = effort.trimmed();
    if (value.isEmpty())
        AppConfig::instance().remove(cfg_key(id, "default_effort"));
    else
        AppConfig::instance().set(cfg_key(id, "default_effort"), value);
    emit defaults_changed();
}

bool HarnessRegistry::enabled_for_terminal(const QString& id) const {
    // Explicit opt-in, and only with the user's consent (a value stored before
    // the consent dialog existed does not count).
    return AppConfig::instance().get(cfg_key(id, "enabled_for_terminal"), false).toBool() && terminal_consent(id);
}

void HarnessRegistry::set_enabled_for_terminal(const QString& id, bool on) {
    if (on && !terminal_consent(id)) {
        // Not without the user's consent: whoever asked gets the dialog first.
        // defaults_changed lets a checkbox that already flipped show "off" again.
        emit defaults_changed();
        emit terminal_consent_requested(id);
        return;
    }
    AppConfig::instance().set(cfg_key(id, "enabled_for_terminal"), on);
    emit defaults_changed();
}

bool HarnessRegistry::terminal_consent(const QString& id) const {
    const auto it = d_->entries.constFind(id);
    if (it == d_->entries.cend())
        return false;
    // The consent names the vendor it was given for; a different vendor needs a new one.
    const QString vendor = AppConfig::instance().get(cfg_key(id, "terminal_consent_vendor"), QString()).toString();
    return !vendor.isEmpty() && vendor == it->adapter->descriptor().vendor;
}

void HarnessRegistry::grant_terminal_consent(const QString& id, bool enable) {
    const auto it = d_->entries.constFind(id);
    if (it == d_->entries.cend())
        return;
    auto& cfg = AppConfig::instance();
    cfg.set(cfg_key(id, "terminal_consent_vendor"), it->adapter->descriptor().vendor);
    cfg.set(cfg_key(id, "terminal_consent_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    if (enable)
        cfg.set(cfg_key(id, "enabled_for_terminal"), true);
    LOG_AUDIT(kHarnessRegistryTag, "Terminal AI consent given",
              {{"harness", id}, {"vendor", it->adapter->descriptor().vendor}, {"enabled", enable}});
    emit defaults_changed();
}

void HarnessRegistry::revoke_terminal_consent(const QString& id) {
    auto& cfg = AppConfig::instance();
    cfg.remove(cfg_key(id, "terminal_consent_vendor"));
    cfg.remove(cfg_key(id, "terminal_consent_at"));
    cfg.set(cfg_key(id, "enabled_for_terminal"), false);
    LOG_AUDIT(kHarnessRegistryTag, "Terminal AI consent withdrawn", {{"harness", id}});
    emit defaults_changed();
}

QString HarnessRegistry::env_policy(const QString& id) const {
    const QString v = AppConfig::instance().get(cfg_key(id, "env_policy"), QString()).toString();
    return v == QLatin1String(kEnvPolicySubscriptionOnly) ? v : QString::fromLatin1(kEnvPolicyInherit);
}

void HarnessRegistry::set_env_policy(const QString& id, const QString& policy) {
    if (policy == QLatin1String(kEnvPolicySubscriptionOnly))
        AppConfig::instance().set(cfg_key(id, "env_policy"), policy);
    else
        AppConfig::instance().remove(cfg_key(id, "env_policy"));
    emit defaults_changed();
}

QString HarnessRegistry::default_harness() const {
    const QString id = AppConfig::instance().get(default_harness_key(), QString()).toString().trimmed();
    return d_->entries.contains(id) ? id : QString();
}

void HarnessRegistry::set_default_harness(const QString& id) {
    if (id.isEmpty() || !d_->entries.contains(id))
        AppConfig::instance().remove(default_harness_key());
    else
        AppConfig::instance().set(default_harness_key(), id);
    emit defaults_changed();
}

bool HarnessRegistry::allow_full_access() const {
    return AppConfig::instance().get(QStringLiteral("harness/allow_full_access"), false).toBool();
}

void HarnessRegistry::set_allow_full_access(bool on) {
    if (on != allow_full_access())
        LOG_AUDIT(kHarnessRegistryTag, on ? "Full-access agent runs allowed" : "Full-access agent runs disallowed",
                  {{"allowed", on}});
    if (on)
        AppConfig::instance().set(QStringLiteral("harness/allow_full_access"), true);
    else
        AppConfig::instance().remove(QStringLiteral("harness/allow_full_access"));
    emit defaults_changed();
}

bool HarnessRegistry::api_key_reaches_child(const QString& id) const {
    const auto it = d_->entries.constFind(id);
    if (it == d_->entries.cend())
        return false;
    const bool present = it->st.auth.api_key_env_present || it->hint.api_key_env_present;
    if (!present)
        return false;
    // "Subscription only" strips the keys — except for a harness the terminal
    // supports with API keys only (the policy does not apply there).
    return it->adapter->descriptor().api_key_only || env_policy(id) != QLatin1String(kEnvPolicySubscriptionOnly);
}

QStringList HarnessRegistry::efforts(const QString& id, const QString& model) const {
    const auto it = d_->entries.constFind(id);
    if (it == d_->entries.cend())
        return {};
    for (const ModelInfo& m : it->st.models) {
        if (m.id == model && !m.efforts.isEmpty())
            return m.efforts;
    }
    const QString version = it->st.installation ? it->st.installation->version : QString();
    return it->adapter->efforts(model, version);
}

void HarnessRegistry::report_failure(const QString& id, FailureKind kind, qint64 retry_at_ms) {
    auto it = d_->entries.find(id);
    if (it == d_->entries.end())
        return;
    it->st.last_failure = kind;
    const qint64 now = registry_now_ms();
    if (kind == FailureKind::RateLimit)
        it->st.cooldown_until_ms = retry_at_ms > now ? retry_at_ms : now + kDefaultCooldownMs;
    if (kind == FailureKind::Auth)
        refresh(id, RefreshScope::Auth);
    else if (kind == FailureKind::NotInstalled)
        refresh(id, RefreshScope::Detect);
    emit status_changed(id);
}

void HarnessRegistry::report_success(const QString& id) {
    auto it = d_->entries.find(id);
    if (it == d_->entries.end())
        return;
    const bool changed = it->st.last_failure != FailureKind::None || it->st.cooldown_until_ms != 0;
    it->st.last_failure = FailureKind::None;
    it->st.cooldown_until_ms = 0;
    if (it->st.auth.state == AuthState::SignedOut || it->st.auth.state == AuthState::Error)
        refresh(id, RefreshScope::Auth); // the run just proved otherwise
    if (changed)
        emit status_changed(id);
}

QProcessEnvironment HarnessRegistry::child_environment(const QString& id, const Installation& inst,
                                                       const LaunchPlan* plan, const QString& mcp_token) const {
    ChildEnvOptions opt;
    if (plan) {
        opt.set = plan->env_set;
        opt.unset = plan->env_unset;
    }
    if (env_policy(id) == QLatin1String(kEnvPolicySubscriptionOnly)) {
        // An API-key-only harness keeps its keys: without one it cannot run at all.
        if (const IHarnessAdapter* a = adapter(id); a && !a->descriptor().api_key_only)
            opt.strip = a->descriptor().api_key_env_vars;
    }
    opt.mcp_token = mcp_token;
    return ::fincept::harness::child_environment(inst, opt);
}

} // namespace fincept::harness
