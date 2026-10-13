// ConnectorCatalog.cpp — loads resources/data_connectors.json (see ConnectorCatalog.h).

#include "services/connectors/ConnectorCatalog.h"

#include "core/logging/Logger.h"
#include "storage/secure/SecureStorage.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMutexLocker>
#include <QSet>

namespace fincept::services::connectors {

namespace {

constexpr const char* kConnCatalogTag = "ConnectorCatalog";
constexpr int kConnCatalogSchema = 1;

/// Where the catalogue may live, in priority order. Mirrors how resources/component_catalog.json
/// is found (exe_dir/resources first), plus the packaged layouts: the macOS bundle flattens
/// MacOS/resources/* into Contents/Resources/, the Linux FHS install puts resources under
/// share/fincept-terminal/, and a dev run from the source tree finds it relative to the CWD or
/// two levels above build/<preset>/.
QStringList conn_cat_candidate_paths() {
    const QString exe = QCoreApplication::instance() ? QCoreApplication::applicationDirPath() : QString();
    QStringList out;
    if (!exe.isEmpty()) {
        out << exe + "/resources/data_connectors.json" << exe + "/data_connectors.json"
            << exe + "/../Resources/data_connectors.json" << exe + "/../Resources/resources/data_connectors.json"
            << exe + "/../share/fincept-terminal/resources/data_connectors.json"
            << exe + "/../../resources/data_connectors.json";
    }
    out << QStringLiteral("resources/data_connectors.json");
    return out;
}

QStringList conn_cat_string_list(const QJsonValue& v) {
    QStringList out;
    for (const auto& e : v.toArray()) {
        const QString s = e.toString();
        if (!s.isEmpty())
            out << s;
    }
    return out;
}

} // namespace

// ── ConnectorInfo ───────────────────────────────────────────────────────────

bool ConnectorInfo::has_command(const QString& command) const {
    for (const auto& c : commands)
        if (c.name == command)
            return true;
    return false;
}

QString ConnectorInfo::args_hint(const QString& command) const {
    for (const auto& c : commands)
        if (c.name == command)
            return c.args_hint;
    return {};
}

QStringList ConnectorInfo::command_names() const {
    QStringList out;
    out.reserve(commands.size());
    for (const auto& c : commands)
        out << c.name;
    return out;
}

// ── ConnectorCatalog ────────────────────────────────────────────────────────

ConnectorCatalog& ConnectorCatalog::instance() {
    static ConnectorCatalog s;
    return s;
}

bool ConnectorCatalog::ensure_loaded() {
    QMutexLocker lock(&mutex_);
    if (attempted_)
        return loaded_;
    return load_locked();
}

bool ConnectorCatalog::reload() {
    QMutexLocker lock(&mutex_);
    // connectors() hands out a reference to connectors_; a reload swaps it under the lock, so
    // only call this when no consumer is iterating (tests, explicit developer action).
    attempted_ = false;
    loaded_ = false;
    connectors_.clear();
    by_name_.clear();
    by_script_.clear();
    categories_.clear();
    return load_locked();
}

bool ConnectorCatalog::load_locked() {
    attempted_ = true;
    QStringList tried;
    for (const QString& path : conn_cat_candidate_paths()) {
        QFile f(path);
        if (!f.exists())
            continue;
        tried << QDir::cleanPath(path);
        if (!f.open(QIODevice::ReadOnly)) {
            load_error_ = QStringLiteral("Cannot open %1: %2").arg(path, f.errorString());
            continue;
        }
        if (parse_locked(f.readAll(), QFileInfo(path).absoluteFilePath()))
            return true;
    }
    if (tried.isEmpty())
        load_error_ = QStringLiteral("data_connectors.json not found next to the executable (resources/)");
    LOG_ERROR(kConnCatalogTag, "Connector catalogue unavailable: " + load_error_);
    return false;
}

bool ConnectorCatalog::parse_locked(const QByteArray& json, const QString& source) {
    QJsonParseError pe{};
    const QJsonDocument doc = QJsonDocument::fromJson(json, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        load_error_ = QStringLiteral("%1 is not valid JSON: %2").arg(source, pe.errorString());
        return false;
    }
    const QJsonObject root = doc.object();
    const int schema = root.value("schema").toInt();
    if (schema != kConnCatalogSchema) {
        load_error_ = QStringLiteral("%1 has schema %2, expected %3").arg(source).arg(schema).arg(kConnCatalogSchema);
        return false;
    }

    QVector<ConnectorInfo> list;
    const QJsonArray arr = root.value("connectors").toArray();
    list.reserve(arr.size());
    for (const auto& v : arr) {
        const QJsonObject o = v.toObject();
        ConnectorInfo c;
        c.name = o.value("name").toString();
        c.script = o.value("script").toString();
        if (c.name.isEmpty() || c.script.isEmpty() || c.script.contains(QLatin1String("..")))
            continue;
        c.title = o.value("title").toString(c.name);
        c.description = o.value("desc").toString();
        c.category = o.value("category").toString(QStringLiteral("Other"));
        c.region = o.value("region").toString(QStringLiteral("Global"));
        for (const auto& cv : o.value("commands").toArray()) {
            const QJsonObject co = cv.toObject();
            ConnectorCommand cmd;
            cmd.name = co.value("name").toString();
            cmd.args_hint = co.value("args_hint").toString();
            if (!cmd.name.isEmpty())
                c.commands.append(cmd);
        }
        if (c.commands.isEmpty())
            continue;
        c.default_command = o.value("default_command").toString();
        if (!c.has_command(c.default_command))
            c.default_command = c.commands.first().name;
        c.default_args = conn_cat_string_list(o.value("default_args"));
        c.env_keys = conn_cat_string_list(o.value("env_keys"));
        c.key_required = o.value("key_required").toBool(false) && !c.env_keys.isEmpty();
        c.key_note = o.value("key_note").toString();
        c.docs_url = o.value("docs_url").toString();
        c.ttl_sec = o.value("ttl_sec").toInt(0);
        c.mcp = o.value("mcp").toBool(true);
        c.broken = o.value("broken").toString();
        list.append(std::move(c));
    }
    if (list.isEmpty()) {
        load_error_ = QStringLiteral("%1 contains no connectors").arg(source);
        return false;
    }

    connectors_ = std::move(list);
    by_name_.clear();
    by_script_.clear();
    for (int i = 0; i < connectors_.size(); ++i) {
        by_name_.insert(connectors_[i].name, i);
        by_script_.insert(connectors_[i].script, i);
    }
    categories_ = conn_cat_string_list(root.value("categories"));
    QSet<QString> seen(categories_.begin(), categories_.end());
    for (const auto& c : connectors_) {
        if (!seen.contains(c.category)) {
            seen.insert(c.category);
            categories_ << c.category;
        }
    }
    source_path_ = source;
    load_error_.clear();
    loaded_ = true;
    LOG_INFO(kConnCatalogTag, QString("Loaded %1 data connectors in %2 categories from %3")
                                  .arg(connectors_.size())
                                  .arg(categories_.size())
                                  .arg(source));
    return true;
}

QString ConnectorCatalog::load_error() const {
    QMutexLocker lock(&mutex_);
    return load_error_;
}

QString ConnectorCatalog::source_path() const {
    QMutexLocker lock(&mutex_);
    return source_path_;
}

const QVector<ConnectorInfo>& ConnectorCatalog::connectors() const {
    // Immutable after the (mutex-guarded) load; callers ensure_loaded() first.
    return connectors_;
}

const ConnectorInfo* ConnectorCatalog::find(const QString& name) const {
    QMutexLocker lock(&mutex_);
    const auto it = by_name_.constFind(name);
    return it == by_name_.constEnd() ? nullptr : &connectors_[it.value()];
}

const ConnectorInfo* ConnectorCatalog::find_by_script(const QString& script) const {
    QMutexLocker lock(&mutex_);
    const auto it = by_script_.constFind(script);
    return it == by_script_.constEnd() ? nullptr : &connectors_[it.value()];
}

QStringList ConnectorCatalog::categories() const {
    QMutexLocker lock(&mutex_);
    return categories_;
}

int ConnectorCatalog::count() const {
    QMutexLocker lock(&mutex_);
    return connectors_.size();
}

// ── Key presence ────────────────────────────────────────────────────────────

bool ConnectorCatalog::key_in_environment(const QString& env_key) {
    // A key exported in the shell reaches Python too: PythonRunner keeps managed keys it
    // finds in the inherited environment (it only strips unmanaged credential-shaped vars).
    return !qEnvironmentVariable(env_key.toUtf8().constData()).trimmed().isEmpty();
}

bool ConnectorCatalog::key_in_secure_storage(const QString& env_key) {
    const auto r = SecureStorage::instance().retrieve(env_key);
    return r.is_ok() && !r.value().trimmed().isEmpty(); // the value goes out of scope here
}

std::optional<bool> ConnectorCatalog::cached_key_present(const QString& env_key) const {
    QMutexLocker lock(&key_mutex_);
    const auto it = key_presence_.constFind(env_key);
    if (it == key_presence_.constEnd())
        return std::nullopt;
    return it.value();
}

bool ConnectorCatalog::key_present(const QString& env_key) {
    if (const auto cached = cached_key_present(env_key))
        return *cached;
    const bool present = key_in_secure_storage(env_key) || key_in_environment(env_key);
    set_key_present(env_key, present);
    return present;
}

void ConnectorCatalog::refresh_key_presence() {
    const QStringList keys = all_env_keys();
    QHash<QString, bool> fresh;
    fresh.reserve(keys.size());
    for (const QString& k : keys)
        fresh.insert(k, key_in_secure_storage(k) || key_in_environment(k));
    QMutexLocker lock(&key_mutex_);
    key_presence_ = std::move(fresh);
}

void ConnectorCatalog::set_key_present(const QString& env_key, bool present) {
    QMutexLocker lock(&key_mutex_);
    key_presence_.insert(env_key, present);
}

KeyState ConnectorCatalog::key_state(const ConnectorInfo& connector) const {
    if (connector.env_keys.isEmpty())
        return KeyState::NotNeeded;
    int present = 0;
    for (const QString& k : connector.env_keys) {
        const auto p = cached_key_present(k);
        if (!p.has_value())
            return KeyState::Unknown;
        if (*p)
            ++present;
    }
    if (present == connector.env_keys.size())
        return KeyState::Ready;
    return present == 0 ? KeyState::Missing : KeyState::Partial;
}

QStringList ConnectorCatalog::all_env_keys() const {
    QMutexLocker lock(&mutex_);
    QStringList out;
    QSet<QString> seen;
    for (const auto& c : connectors_) {
        for (const auto& k : c.env_keys) {
            if (!seen.contains(k)) {
                seen.insert(k);
                out << k;
            }
        }
    }
    return out;
}

QString key_requirement_text(const ConnectorInfo& connector) {
    if (connector.env_keys.isEmpty())
        return QStringLiteral("No API key needed.");
    const QString keys = connector.env_keys.join(QStringLiteral(", "));
    QString text = connector.key_required ? QStringLiteral("Requires %1.").arg(keys)
                                          : QStringLiteral("Optional key: %1 (works without it).").arg(keys);
    if (!connector.key_note.isEmpty())
        text += QLatin1Char(' ') + connector.key_note;
    return text;
}

} // namespace fincept::services::connectors
