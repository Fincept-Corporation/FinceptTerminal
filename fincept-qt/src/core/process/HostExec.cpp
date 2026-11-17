// HostExec.cpp — host programs from inside the Flatpak sandbox (see HostExec.h).
//
// Verified behaviour this file relies on (flatpak-xdg-utils flatpak-spawn.c,
// flatpak session-helper, flatpak-run / flatpak-context; links in the research
// notes of the change that added it):
//   * flatpak-spawn always forwards fds 0-2; exit code = the child's, or
//     128 + signal; --watch-bus makes the host send killpg(pid, SIGINT) when
//     flatpak-spawn leaves the session bus; without --directory it uses its own
//     (sandbox) cwd, so we always pass --directory=/.
//   * The host child is started with setsid(): it leads its own session and
//     process group (pgid = pid), and its PATH lookup uses the session helper's
//     PATH, not the user's login shell.
//   * HostCommandSignal accepts only pids the SAME bus connection started, so
//     a group started by flatpak-spawn is signalled with `kill` run on the host.
//   * $XDG_RUNTIME_DIR/app/<id> is the host folder of the same name, mounted at
//     /run/flatpak/app/<id> with a symlink from $XDG_RUNTIME_DIR/app.
//   * --share=network keeps the host's network namespace: the host reaches
//     the app's 127.0.0.1 listeners (the MCP server) directly.

#include "core/process/HostExec.h"

#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUuid>

#include <mutex>

#ifdef Q_OS_LINUX
#    include <sys/types.h>
#    include <sys/xattr.h>
#    include <unistd.h>
#endif

namespace fincept::process::host {

namespace host_exec_detail {

constexpr char kHandleName[] = "_fincept_host_exec";
constexpr char kFlatpakBusName[] = "org.freedesktop.Flatpak";
constexpr qint64 kStaleExchangeSecs = 6 * 3600;
constexpr int kProfileBaseTimeoutMs = 5000;
constexpr int kLoginShellTimeoutMs = 10000; // VS Code's default for the same resolution
constexpr int kPlainShellTimeoutMs = 5000;

/// Host side of every wrapped launch. $1 env file (sourced, then deleted),
/// $2 pid file (this shell's pid = the program's pid after exec = its process
/// group: the session helper starts it in a new session), $3 working folder
/// (created like the run folder is natively; empty = the user's HOME), then the
/// program and its arguments. No rc file is read: nothing but the program
/// writes to stdout. The shell's own variables carry a prefix no environment
/// variable of the env file can collide with; they are not exported.
constexpr char kWrapperScript[] = "__fx_e=$1 __fx_p=$2 __fx_d=$3\n"
                                  "shift 3\n"
                                  "if [ -n \"$__fx_d\" ]; then mkdir -p -- \"$__fx_d\" && cd -- \"$__fx_d\" || exit 125; "
                                  "else cd 2>/dev/null; fi\n"
                                  "if [ -n \"$__fx_p\" ]; then echo $$ > \"$__fx_p\"; fi\n"
                                  "if [ -n \"$__fx_e\" ]; then . \"$__fx_e\" || exit 125; rm -f -- \"$__fx_e\"; fi\n"
                                  "exec \"$@\"\n";

/// The session helper's view: HOME, the user's login shell, its own PATH.
constexpr char kProfileScript[] = "s=${SHELL:-}\n"
                                  "if [ -z \"$s\" ] || [ ! -x \"$s\" ]; then\n"
                                  "  s=$(getent passwd \"$(id -u)\" 2>/dev/null | cut -d: -f7)\n"
                                  "fi\n"
                                  "printf '__FX_HOME=%s\\n' \"$HOME\"\n"
                                  "printf '__FX_SHELL=%s\\n' \"$s\"\n"
                                  "printf '__FX_PATH=%s\\n' \"$PATH\"\n";

/// Run by the user's login shell (sh, bash, zsh and fish all take it): PATH
/// between markers, so whatever the rc files print around it is ignored.
constexpr char kPrintPathCommand[] = "printf '\\n__FX_BEGIN__%s__FX_END__\\n' \"$PATH\"";

/// The group of a wrapped launch: $1 pgid, $2 signal name. Leader gone (the
/// local flatpak-spawn has ended): only while no process holds that pid — a
/// group id is not reused while the group has members, so then they are ours.
constexpr char kKillRunningScript[] = "kill -s \"$2\" -- \"-$1\" 2>/dev/null";
constexpr char kKillLeftoversScript[] = "kill -0 \"$1\" 2>/dev/null || kill -s \"$2\" -- \"-$1\" 2>/dev/null";

/// /.flatpak-info as groups of key → value (GKeyFile syntax, lists kept as text).
struct FlatpakInfo {
    bool read = false;
    QHash<QString, QHash<QString, QString>> groups;

    QString value(const char* group, const char* key) const {
        return groups.value(QLatin1String(group)).value(QLatin1String(key));
    }
};

FlatpakInfo parse_flatpak_info(const QByteArray& text) {
    FlatpakInfo info;
    info.read = true;
    QString group;
    for (const QByteArray& raw : text.split('\n')) {
        const QString line = QString::fromUtf8(raw).trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']'))) {
            group = line.mid(1, line.size() - 2).trimmed();
            continue;
        }
        const qsizetype eq = line.indexOf(QLatin1Char('='));
        if (eq <= 0 || group.isEmpty())
            continue;
        info.groups[group].insert(line.left(eq).trimmed(), line.mid(eq + 1).trimmed());
    }
    return info;
}

/// Process-wide caches. The permissions of a running sandbox do not change.
struct State {
    QMutex mutex;
    std::optional<bool> override_flatpak;
    std::optional<bool> flatpak;
    std::optional<FlatpakInfo> info;
    std::optional<Access> access;
    std::optional<HostProfile> profile;
};

State& state() {
    static State* const s = new State; // never destroyed: worker threads may still ask during teardown
    return *s;
}

QMutex& profile_resolve_mutex() {
    static QMutex* const m = new QMutex;
    return *m;
}

FlatpakInfo flatpak_info() {
    State& s = state();
    {
        const QMutexLocker lock(&s.mutex);
        if (s.info)
            return *s.info;
    }
    FlatpakInfo info;
    QFile f(QStringLiteral("/.flatpak-info"));
    if (f.open(QIODevice::ReadOnly))
        info = parse_flatpak_info(f.read(256 * 1024));
    const QMutexLocker lock(&s.mutex);
    if (!s.info)
        s.info = info;
    return *s.info;
}

bool under(const QString& path, const QString& root) {
    return !root.isEmpty() && (path == root || path.startsWith(root + QLatin1Char('/')));
}

QString rebase(const QString& path, const QString& from, const QString& to) {
    return to + path.mid(from.size());
}

QString runtime_dir() {
    const QString env = qEnvironmentVariable("XDG_RUNTIME_DIR").trimmed();
    if (!env.isEmpty())
        return QDir::cleanPath(env);
#ifdef Q_OS_LINUX
    return QStringLiteral("/run/user/%1").arg(static_cast<qulonglong>(::getuid()));
#else
    return {};
#endif
}

/// --persist folders (home-relative), e.g. ".local/share".
QStringList persistent_dirs() {
    QStringList out;
    for (QString p : flatpak_info().value("Context", "persistent").split(QLatin1Char(';'), Qt::SkipEmptyParts)) {
        p = p.trimmed();
        if (p.startsWith(QLatin1String("~/")))
            p.remove(0, 2);
        while (p.endsWith(QLatin1Char('/')))
            p.chop(1);
        if (!p.isEmpty() && !p.startsWith(QLatin1Char('/')) && !p.contains(QLatin1String("..")))
            out << p;
    }
    return out;
}

/// `name` may be talked to under /.flatpak-info: the whole session bus, or a
/// [Session Bus Policy] entry (exact or "prefix.*") with talk / own.
bool bus_name_allowed(const FlatpakInfo& info, const QString& name) {
    const QStringList sockets = info.value("Context", "sockets").split(QLatin1Char(';'), Qt::SkipEmptyParts);
    for (const QString& s : sockets) {
        if (s.trimmed() == QLatin1String("session-bus"))
            return true;
    }
    const QHash<QString, QString> policy = info.groups.value(QStringLiteral("Session Bus Policy"));
    for (auto it = policy.cbegin(); it != policy.cend(); ++it) {
        const QString level = it.value().trimmed();
        if (level != QLatin1String("talk") && level != QLatin1String("own"))
            continue;
        const QString key = it.key().trimmed();
        if (key == name)
            return true;
        if (key.endsWith(QLatin1String(".*"))) {
            const QString prefix = key.left(key.size() - 2);
            if (name == prefix || name.startsWith(prefix + QLatin1Char('.')))
                return true;
        }
    }
    return false;
}

QString spawn_program() {
    static const QString found = []() {
        QString p = QStandardPaths::findExecutable(QStringLiteral("flatpak-spawn"));
        if (p.isEmpty() && QFileInfo(QStringLiteral("/usr/bin/flatpak-spawn")).isExecutable())
            p = QStringLiteral("/usr/bin/flatpak-spawn");
        return p;
    }();
    return found;
}

/// Private exchange folder for env / pid files, visible to the host at the
/// same path; 0700; files older than kStaleExchangeSecs (left by a crash) are
/// removed on first use. Empty outside Flatpak or when it cannot be created.
QString exchange_dir() {
    if (!in_flatpak())
        return {};
    const QString rt = runtime_dir();
    const QString id = app_id();
    if (rt.isEmpty() || id.isEmpty())
        return {};
    const QString dir = rt + QStringLiteral("/app/") + id + QStringLiteral("/fincept-host");
    if (!QDir().mkpath(dir))
        return {};
    QFile::setPermissions(dir, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    static std::once_flag swept;
    std::call_once(swept, [dir]() {
        const QDateTime cutoff = QDateTime::currentDateTimeUtc().addSecs(-kStaleExchangeSecs);
        const QFileInfoList old = QDir(dir).entryInfoList({QStringLiteral("*.env"), QStringLiteral("*.pid")}, QDir::Files);
        for (const QFileInfo& fi : old) {
            if (fi.lastModified().toUTC() < cutoff)
                QFile::remove(fi.absoluteFilePath());
        }
    });
    return dir;
}

/// Created 0600 (never readable by anyone else, not even for a moment).
bool write_private_file(const QString& path, const QByteArray& bytes) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::NewOnly, QFileDevice::ReadOwner | QFileDevice::WriteOwner))
        return false;
    const bool ok = f.write(bytes) == bytes.size();
    f.close();
    if (!ok)
        QFile::remove(path);
    return ok;
}

bool valid_env_name(const QString& name) {
    static const QRegularExpression rx(QStringLiteral("^[A-Za-z_][A-Za-z0-9_]*$"));
    return rx.match(name).hasMatch() && !name.startsWith(QLatin1String("__fx_")); // the wrapper's own
}

QString sh_quote(const QString& value) {
    QString v = value;
    v.replace(QLatin1Char('\''), QStringLiteral("'\\''"));
    return QLatin1Char('\'') + v + QLatin1Char('\'');
}

/// Proxy settings of the session reach a host CLI when set (never unset there).
const QStringList& proxy_names() {
    static const QStringList names{QStringLiteral("HTTP_PROXY"),  QStringLiteral("HTTPS_PROXY"),
                                   QStringLiteral("NO_PROXY"),    QStringLiteral("ALL_PROXY"),
                                   QStringLiteral("http_proxy"),  QStringLiteral("https_proxy"),
                                   QStringLiteral("no_proxy"),    QStringLiteral("all_proxy")};
    return names;
}

/// The host-side difference between the session helper's environment and the
/// one the caller prepared: what the app changed or removed relative to its own
/// environment, the `mirror` names exactly as prepared, and the proxies. Never
/// the rest of the sandbox's environment.
QByteArray env_script(const QProcessEnvironment& base, const QProcessEnvironment& wanted, const QStringList& mirror) {
    QStringList unset;
    QStringList exports;
    const auto add_unset = [&unset](const QString& n) {
        if (valid_env_name(n) && !unset.contains(n))
            unset << n;
    };
    for (const QString& n : base.keys()) {
        if (!wanted.contains(n))
            add_unset(n);
    }
    for (const QString& n : mirror) {
        if (!wanted.contains(n))
            add_unset(n);
    }
    QStringList lines;
    if (!unset.isEmpty())
        lines << QStringLiteral("unset ") + unset.join(QLatin1Char(' '));
    for (const QString& n : wanted.keys()) {
        if (!valid_env_name(n))
            continue;
        const QString v = wanted.value(n);
        const bool changed = !base.contains(n) || base.value(n) != v;
        if (changed || mirror.contains(n) || proxy_names().contains(n))
            lines << QStringLiteral("export %1=%2").arg(n, sh_quote(v));
    }
    lines << QString(); // trailing newline
    return lines.join(QLatin1Char('\n')).toUtf8();
}

/// The files of one wrapped launch, owned by its QProcess.
class HostExecHandle final : public QObject {
  public:
    explicit HostExecHandle(QProcess* owner) : QObject(owner) { setObjectName(QLatin1String(kHandleName)); }
    ~HostExecHandle() override { remove_files(); }

    void remove_files() {
        if (!env_file.isEmpty())
            QFile::remove(env_file); // normally gone already: the wrapper deletes it before exec
        if (!pid_file.isEmpty())
            QFile::remove(pid_file);
    }

    QString env_file;
    QString pid_file;
};

HostExecHandle* handle_of(const QProcess* proc) {
    if (!proc)
        return nullptr;
    for (QObject* c : proc->children()) {
        if (c->objectName() == QLatin1String(kHandleName)) {
            if (auto* h = dynamic_cast<HostExecHandle*>(c))
                return h;
        }
    }
    return nullptr;
}

QString between_markers(const QByteArray& out) {
    const QString text = QString::fromUtf8(out);
    const qsizetype b = text.lastIndexOf(QLatin1String("__FX_BEGIN__"));
    if (b < 0)
        return {};
    const qsizetype from = b + 12;
    const qsizetype e = text.indexOf(QLatin1String("__FX_END__"), from);
    if (e < 0)
        return {};
    const QString value = text.mid(from, e - from).trimmed();
    if (value.isEmpty() || value.contains(QLatin1Char('\n')) || !value.contains(QLatin1Char('/')))
        return {};
    return value;
}

QString describe_failure(const HostRun& r) {
    if (!r.started)
        return r.error.isEmpty() ? QStringLiteral("the host command could not be started") : r.error;
    if (r.timed_out)
        return QStringLiteral("your system did not answer in time");
    const QString err = QString::fromUtf8(r.err).trimmed();
    return err.isEmpty() ? QStringLiteral("exit code %1").arg(r.exit_code) : err.left(300);
}

HostProfile resolve_profile(int timeout_ms) {
    HostProfile p;
    QElapsedTimer clock;
    clock.start();
    const HostRun base = run_blocking({QStringLiteral("/bin/sh"), QStringLiteral("-c"), QString::fromLatin1(kProfileScript)},
                                      qMin(timeout_ms, kProfileBaseTimeoutMs), 64 * 1024);
    if (!base.started || base.timed_out || base.exit_code != 0) {
        p.error = describe_failure(base);
        return p;
    }
    QString shell;
    QString session_path;
    for (const QString& line : QString::fromUtf8(base.out).split(QLatin1Char('\n'))) {
        if (line.startsWith(QLatin1String("__FX_HOME=")))
            p.home = line.mid(10).trimmed();
        else if (line.startsWith(QLatin1String("__FX_SHELL=")))
            shell = line.mid(11).trimmed();
        else if (line.startsWith(QLatin1String("__FX_PATH=")))
            session_path = line.mid(10).trimmed();
    }
    p.path = session_path;
    p.source = QStringLiteral("session");
    if (shell.startsWith(QLatin1Char('/'))) {
        // Interactive login shell first (nvm, volta, … are often set up only
        // for interactive shells), then a plain login shell.
        const struct {
            const char* flags[3];
            int budget_ms;
            const char* source;
        } attempts[] = {{{"-l", "-i", "-c"}, kLoginShellTimeoutMs, "login-shell"},
                        {{"-l", "-c", nullptr}, kPlainShellTimeoutMs, "login-shell (non-interactive)"}};
        for (const auto& a : attempts) {
            const int left = timeout_ms - static_cast<int>(clock.elapsed());
            const int budget = qMin(left, a.budget_ms);
            if (budget < 500)
                break;
            QStringList argv{shell};
            for (const char* f : a.flags) {
                if (f)
                    argv << QString::fromLatin1(f);
            }
            argv << QString::fromLatin1(kPrintPathCommand);
            const HostRun r = run_blocking(argv, budget, 1024 * 1024);
            const QString found = r.started ? between_markers(r.out) : QString();
            if (!found.isEmpty()) {
                p.path = found;
                p.source = QString::fromLatin1(a.source);
                break;
            }
        }
    }
    if (p.path.isEmpty())
        p.path = fallback_host_path();
    if (p.home.isEmpty())
        p.home = QDir::homePath();
    p.ok = true;
    return p;
}

} // namespace host_exec_detail

using namespace host_exec_detail;

// ── Where this process runs ─────────────────────────────────────────────────

bool in_flatpak() {
    State& s = state();
    const QMutexLocker lock(&s.mutex);
    if (s.override_flatpak)
        return *s.override_flatpak;
    if (!s.flatpak) {
#ifdef Q_OS_LINUX
        // /.flatpak-info is what every Flatpak sandbox has (as InstallContext
        // checks). FLATPAK_ID alone is not enough: it leaks into the environment
        // of programs started from a sandboxed IDE's host terminal.
        s.flatpak = QFileInfo::exists(QStringLiteral("/.flatpak-info"));
#else
        s.flatpak = false;
#endif
    }
    return *s.flatpak;
}

QString app_id() {
    if (!in_flatpak())
        return {};
    const QString from_info = flatpak_info().value("Application", "name");
    if (!from_info.isEmpty())
        return from_info;
    return qEnvironmentVariable("FLATPAK_ID").trimmed();
}

Access access() {
    State& s = state();
    {
        const QMutexLocker lock(&s.mutex);
        if (s.access)
            return *s.access;
    }
    Access a = Access::Direct;
    if (in_flatpak()) {
        const FlatpakInfo info = flatpak_info();
        if (info.read && !bus_name_allowed(info, QString::fromLatin1(kFlatpakBusName)))
            a = Access::PermissionMissing;
        else if (spawn_program().isEmpty())
            a = Access::ToolMissing;
        else
            a = Access::Available;
    }
    const QMutexLocker lock(&s.mutex);
    if (!s.access)
        s.access = a;
    return *s.access;
}

bool blocked() {
    const Access a = access();
    return a == Access::PermissionMissing || a == Access::ToolMissing;
}

QString reenable_command() {
    const QString id = app_id();
    return QStringLiteral("flatpak override --user --talk-name=%1 %2")
        .arg(QString::fromLatin1(kFlatpakBusName), id.isEmpty() ? QStringLiteral("<app id>") : id);
}

QString blocked_reason() {
    switch (access()) {
        case Access::PermissionMissing:
            return QStringLiteral("Coding agents run on your system, outside the Flatpak sandbox, and the permission for "
                                  "that (talking to %1) has been removed from this app — for example in Flatseal. "
                                  "Allow it again with: %2 — then restart Fincept Terminal.")
                .arg(QString::fromLatin1(kFlatpakBusName), reenable_command());
        case Access::ToolMissing:
            return QStringLiteral("Coding agents run on your system, outside the Flatpak sandbox, but this Flatpak runtime "
                                  "has no flatpak-spawn to start them. Update it (flatpak update) and restart Fincept "
                                  "Terminal.");
        case Access::Direct:
        case Access::Available:
            break;
    }
    return {};
}

void set_flatpak_override_for_testing(std::optional<bool> on) {
    State& s = state();
    const QMutexLocker lock(&s.mutex);
    s.override_flatpak = on;
    s.flatpak.reset();
    s.access.reset();
    s.profile.reset();
}

// ── Paths ───────────────────────────────────────────────────────────────────

std::optional<QString> host_path(const QString& path) {
    if (!in_flatpak())
        return path;
    if (path.trimmed().isEmpty())
        return std::nullopt;
    const QString p = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    const QString id = app_id();
    const QString rt = runtime_dir();
    const QString home = QDir::cleanPath(QDir::homePath());

    // The per-app runtime folder: canonical sandbox spelling back to the shared one.
    if (!id.isEmpty()) {
        const QString app_rt = rt.isEmpty() ? QString() : rt + QStringLiteral("/app/") + id;
        const QString sandbox_rt = QStringLiteral("/run/flatpak/app/") + id;
        if (under(p, sandbox_rt))
            return app_rt.isEmpty() ? std::nullopt : std::optional<QString>(rebase(p, sandbox_rt, app_rt));
        if (under(p, app_rt))
            return p;
    }
    // Document portal (a file dialog's answer): the host sees the same FUSE tree.
    if (!rt.isEmpty()) {
        if (under(p, QStringLiteral("/run/flatpak/doc")))
            return rebase(p, QStringLiteral("/run/flatpak/doc"), rt + QStringLiteral("/doc"));
        if (under(p, rt + QStringLiteral("/doc")))
            return p;
    }
    // The app's own ~/.var/app/<id> and the folders persisted into it.
    if (!id.isEmpty() && !home.isEmpty()) {
        const QString var_app = home + QStringLiteral("/.var/app/") + id;
        if (under(p, var_app))
            return p;
        for (const QString& rel : persistent_dirs()) {
            const QString inside = home + QLatin1Char('/') + rel;
            if (under(p, inside))
                return rebase(p, inside, var_app + QLatin1Char('/') + rel);
        }
    }
    // Trees that exist only inside the sandbox (the runtime, the app, its /tmp …).
    static const char* const kPrivate[] = {"/app", "/usr",  "/etc", "/tmp", "/var/tmp", "/var/run", "/run",
                                           "/proc", "/sys", "/dev", "/bin", "/sbin",    "/lib",     "/lib64",
                                           "/.flatpak-info"};
    for (const char* root : kPrivate) {
        if (under(p, QString::fromLatin1(root)))
            return std::nullopt;
    }
    // Anything else is a path of the user's own system (~/projects/x, /mnt/data/x).
    return p;
}

QString shared_path(const QString& path) {
    const std::optional<QString> h = host_path(path);
    return h ? *h : path;
}

QString document_host_path(const QString& path) {
    if (path.isEmpty() || !in_flatpak())
        return path;
    const QString p = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    const QString rt = runtime_dir();
    const bool from_portal =
        under(p, QStringLiteral("/run/flatpak/doc")) || (!rt.isEmpty() && under(p, rt + QStringLiteral("/doc")));
    if (!from_portal)
        return path;
#ifdef Q_OS_LINUX
    // The document portal's own record of the host path (xdg-desktop-portal
    // "Documents & FUSE": user.document-portal.host-path, read-only).
    const QByteArray native = QFile::encodeName(p);
    char buf[4096];
    const ssize_t n = ::getxattr(native.constData(), "user.document-portal.host-path", buf, sizeof(buf) - 1);
    if (n > 0) {
        QByteArray raw(buf, static_cast<qsizetype>(n));
        while (raw.endsWith('\0'))
            raw.chop(1);
        const QString host = QFile::decodeName(raw);
        if (host.startsWith(QLatin1Char('/')))
            return QDir::cleanPath(host);
    }
#endif
    return path;
}

// ── The host user's environment ─────────────────────────────────────────────

HostProfile host_profile_blocking(int timeout_ms) {
    if (const std::optional<HostProfile> cached = host_profile_cached())
        return *cached;
    if (!in_flatpak()) {
        HostProfile p;
        p.ok = true;
        p.home = QDir::homePath();
        p.path = qEnvironmentVariable("PATH");
        p.source = QStringLiteral("session");
        return p;
    }
    // One resolution at a time; the others wait for (and reuse) its answer.
    const QMutexLocker resolving(&profile_resolve_mutex());
    if (const std::optional<HostProfile> cached = host_profile_cached())
        return *cached;
    HostProfile p = resolve_profile(timeout_ms);
    if (p.ok) {
        State& s = state();
        const QMutexLocker lock(&s.mutex);
        s.profile = p; // a failure is not cached: the next search asks again
    }
    return p;
}

std::optional<HostProfile> host_profile_cached() {
    State& s = state();
    const QMutexLocker lock(&s.mutex);
    return s.profile;
}

QString fallback_host_path() {
    return QStringLiteral("/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin");
}

// ── One command on the host ─────────────────────────────────────────────────

HostRun run_blocking(const QStringList& argv, int timeout_ms, qint64 max_output_bytes) {
    HostRun r;
    if (argv.isEmpty()) {
        r.error = QStringLiteral("nothing to run");
        return r;
    }
    QProcess proc;
    if (in_flatpak()) {
        const QString spawn = spawn_program();
        if (spawn.isEmpty()) {
            r.error = QStringLiteral("flatpak-spawn is missing from the Flatpak runtime");
            return r;
        }
        QStringList args{QStringLiteral("--host"), QStringLiteral("--watch-bus"), QStringLiteral("--directory=/")};
        args << argv;
        proc.setProgram(spawn);
        proc.setArguments(args);
    } else {
        proc.setProgram(argv.constFirst());
        proc.setArguments(argv.mid(1));
    }
    proc.setStandardInputFile(QProcess::nullDevice());
    proc.setProcessChannelMode(QProcess::SeparateChannels);
    proc.start();
    if (!proc.waitForStarted(qMax(1000, timeout_ms))) {
        r.error = proc.errorString();
        return r;
    }
    r.started = true;
    if (!proc.waitForFinished(qMax(1, timeout_ms)) && proc.state() != QProcess::NotRunning) {
        r.timed_out = true;
        proc.kill(); // flatpak-spawn leaves the bus: --watch-bus interrupts the host side
        proc.waitForFinished(1000);
    }
    r.out = proc.readAllStandardOutput();
    r.err = proc.readAllStandardError();
    if (r.out.size() + r.err.size() > max_output_bytes) {
        r.out.clear();
        r.err.clear();
        r.error = QStringLiteral("unexpectedly large output");
        r.exit_code = -1;
        return r;
    }
    r.exit_code = proc.exitStatus() == QProcess::NormalExit ? proc.exitCode() : -1;
    return r;
}

// ── The launch boundary ─────────────────────────────────────────────────────

bool wrap(QProcess* proc, const QStringList& mirror_env) {
    if (!proc || !in_flatpak())
        return false;
    const QString program = proc->program();
    const QStringList args = proc->arguments();
    const QProcessEnvironment base = QProcessEnvironment::systemEnvironment();
    QProcessEnvironment wanted = proc->processEnvironment();
    if (wanted.isEmpty())
        wanted = base; // never set: the child would inherit ours
    const QString cwd = proc->workingDirectory();
    QString host_cwd;
    if (!cwd.isEmpty()) {
        const std::optional<QString> mapped = host_path(cwd);
        host_cwd = mapped ? *mapped : QString(); // sandbox-private: the host side starts in HOME
    }

    HostExecHandle* h = handle_of(proc);
    if (h)
        h->remove_files(); // configured again: the earlier files are not used
    else
        h = new HostExecHandle(proc);
    h->env_file.clear();
    h->pid_file.clear();
    const QString dir = exchange_dir();
    if (!dir.isEmpty()) {
        const QString stem = dir + QLatin1Char('/') + QUuid::createUuid().toString(QUuid::WithoutBraces);
        if (write_private_file(stem + QStringLiteral(".env"), env_script(base, wanted, mirror_env))) {
            h->env_file = stem + QStringLiteral(".env");
            h->pid_file = stem + QStringLiteral(".pid");
        }
    }

    QStringList a{QStringLiteral("--host"), QStringLiteral("--watch-bus"), QStringLiteral("--directory=/"),
                  QStringLiteral("/bin/sh"), QStringLiteral("-c")};
    if (h->env_file.isEmpty()) {
        // No private place for the environment: refuse, rather than put the
        // run's token on a command line.
        a << QStringLiteral("echo 'Fincept Terminal could not prepare the environment for this program in its "
                            "runtime folder (XDG_RUNTIME_DIR/app).' >&2; exit 125");
    } else {
        a << QString::fromLatin1(kWrapperScript) << QStringLiteral("fincept-host") << h->env_file << h->pid_file
          << host_cwd << program << args;
    }
    const QString spawn = spawn_program();
    proc->setProgram(spawn.isEmpty() ? QStringLiteral("flatpak-spawn") : spawn);
    proc->setArguments(a);
    proc->setWorkingDirectory(QString()); // the host side changes into host_cwd itself
    proc->setProcessEnvironment(base);    // flatpak-spawn needs nothing of the CLI's environment
    return true;
}

bool is_wrapped(const QProcess* proc) {
    return handle_of(proc) != nullptr;
}

bool signal_host_group(const QProcess* proc, bool force) {
    const HostExecHandle* h = handle_of(proc);
    if (!h || h->pid_file.isEmpty())
        return false;
    QFile f(h->pid_file);
    if (!f.open(QIODevice::ReadOnly))
        return false; // the wrapper has not run (yet): flatpak-spawn's own end reaches the host side
    bool ok = false;
    const qint64 pgid = QString::fromLatin1(f.read(32)).trimmed().toLongLong(&ok);
    if (!ok || pgid <= 1)
        return false;
    const QString spawn = spawn_program();
    if (spawn.isEmpty())
        return false;
    const bool leader_running = proc->state() != QProcess::NotRunning;
    // Fire and forget, without --watch-bus: it must finish even when the app is quitting.
    return QProcess::startDetached(spawn, {QStringLiteral("--host"), QStringLiteral("--directory=/"),
                                           QStringLiteral("/bin/sh"), QStringLiteral("-c"),
                                           QString::fromLatin1(leader_running ? kKillRunningScript : kKillLeftoversScript),
                                           QStringLiteral("fincept-kill"), QString::number(pgid),
                                           force ? QStringLiteral("KILL") : QStringLiteral("TERM")});
}

} // namespace fincept::process::host
