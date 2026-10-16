// src/python/PythonSetupManager.cpp
//
// uv-first Python setup: an async pipeline driven from the GUI thread.
// See the header for the stage / lane overview. Every child process is a
// QProcess wired to signals (never waitFor*), its stdout/stderr is streamed
// line by line, and every progress number shown to the user comes from a real
// event (download bytes, uv's own per-package log lines, phase transitions).
#include "python/PythonSetupManager.h"

#include "core/config/AppPaths.h"
#include "core/logging/Logger.h"
#include "ui/notifications/NotificationService.h" // ToastService

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QMutexLocker>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QSysInfo>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QtConcurrent>

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#ifdef Q_OS_WIN
#    include <windows.h>
#else
#    include <signal.h>
#    include <sys/resource.h>
#    include <sys/types.h>
#    include <unistd.h>
#endif

namespace fincept::python {

// ─────────────────────────────────────────────────────────────────────────────
// Internal types. A NAMED namespace on purpose: these are members of
// PythonSetupManager::Run, and GCC's -Wsubobject-linkage fires (→ -Werror) when
// a class with linkage holds anonymous-namespace types inside a unity TU.
// ─────────────────────────────────────────────────────────────────────────────
namespace psm_detail {

/// Tracks one `uv pip compile|install -v` run from its log lines. uv is pinned
/// (0.7.12), so the DEBUG line shapes below are stable; if one ever stops
/// matching, the UI degrades to the phase text instead of a wrong number.
struct PsmUvProgress {
    enum class Phase { Starting, Resolving, Preparing, Installing, Compiling, Finished };
    Phase phase = Phase::Starting;
    int selected = 0;       // "Selecting:" lines — packages pinned so far by the resolver
    int resolved = 0;       // "Resolved N packages"
    QSet<QString> uncached; // packages uv has to download / build
    QSet<QString> prepared; // ... of which finished (wheel / sdist cache lock released)
    QString headline;       // last user-facing uv line

    /// Returns true when something the UI shows changed.
    bool feed(const QString& raw_line);
    /// Download/build fraction of the prepare phase, -1 when unknown.
    int prepare_percent() const {
        if (phase != Phase::Preparing || uncached.isEmpty())
            return -1;
        return static_cast<int>((100LL * prepared.size()) / uncached.size());
    }
};

} // namespace psm_detail

namespace {

using psm_detail::PsmUvProgress;

constexpr const char* kPsmTag = "PythonSetup";

// Hard caps per child process. uv's own UV_HTTP_TIMEOUT (120 s per read)
// already fails a stalled socket; these only catch a genuinely wedged child.
constexpr int kPsmUvVersionTimeoutMs = 30 * 1000;
constexpr int kPsmExtractTimeoutMs = 3 * 60 * 1000;
constexpr int kPsmCurlTimeoutMs = 16 * 60 * 1000;
constexpr int kPsmPythonInstallTimeoutMs = 30 * 60 * 1000;
constexpr int kPsmVenvTimeoutMs = 5 * 60 * 1000;
constexpr int kPsmFreezeTimeoutMs = 2 * 60 * 1000;
constexpr int kPsmCompileTimeoutMs = 30 * 60 * 1000;
constexpr int kPsmInstallTimeoutMs = 3 * 60 * 60 * 1000;
constexpr int kPsmDownloadIdleTimeoutMs = 60 * 1000;
constexpr int kPsmProxyProbeTimeoutMs = 8 * 1000;
constexpr int kPsmProgressCoalesceMs = 150;
constexpr int kPsmTailKeep = 60;
constexpr int kPsmTailShow = 15;

constexpr const char* kPsmReqTrading = "requirements-numpy1.txt";
constexpr const char* kPsmReqAnalytics = "requirements-numpy2.txt";
constexpr const char* kPsmReqCore = "requirements-numpy2-core.txt";
constexpr const char* kPsmVenvTrading = "venv-numpy1";
constexpr const char* kPsmVenvAnalytics = "venv-numpy2";

/// Overall-progress weight per stage: Runtime, Trading, AnalyticsCore, AnalyticsFull.
constexpr std::array<double, kSetupStageCount> kPsmStageWeights{10.0, 25.0, 15.0, 50.0};

constexpr std::size_t psm_idx(SetupStage s) {
    return static_cast<std::size_t>(s);
}

qint64 psm_now_ms() {
    return QDateTime::currentMSecsSinceEpoch();
}

struct PsmUvAsset {
    const char* archive; // release asset file name
    const char* folder;  // top-level folder inside the archive
    const char* sha256;  // official checksum of the 0.7.12 asset
};

PsmUvAsset psm_uv_asset() {
#if defined(Q_OS_WIN)
    if (QSysInfo::currentCpuArchitecture() == QLatin1String("arm64"))
        return {"uv-aarch64-pc-windows-msvc.zip", "uv-aarch64-pc-windows-msvc",
                "fbedfb71356d0e63c86b507cf1434a58406afe6eac77aee9d37b8282d4006e14"};
    return {"uv-x86_64-pc-windows-msvc.zip", "uv-x86_64-pc-windows-msvc",
            "2cf29c8ffaa2549aa0f86927b2510008e8ca3dcd2100277d86faf437382a371b"};
#elif defined(Q_OS_MACOS)
    if (QSysInfo::currentCpuArchitecture() == QLatin1String("arm64"))
        return {"uv-aarch64-apple-darwin.tar.gz", "uv-aarch64-apple-darwin",
                "189108cd026c25d40fb086eaaf320aac52c3f7aab63e185bac51305a1576fc7e"};
    return {"uv-x86_64-apple-darwin.tar.gz", "uv-x86_64-apple-darwin",
            "a338354420dba089218c05d4d585e4bcf174a65fe53260592b2af19ceec85835"};
#else
    const QString arch = QSysInfo::currentCpuArchitecture();
    if (arch == QLatin1String("arm64") || arch == QLatin1String("aarch64"))
        return {"uv-aarch64-unknown-linux-musl.tar.gz", "uv-aarch64-unknown-linux-musl",
                "0589f6d27bbbe2e205aa6fc25e4cd28ab97e984cdba20842de8d3a00714e13e9"};
    return {"uv-x86_64-unknown-linux-musl.tar.gz", "uv-x86_64-unknown-linux-musl",
            "bb493f1e6ae426c06b5a103cb71aa60c678976d2377f0590644538658656c2a7"};
#endif
}

/// Non-empty when the host can never complete setup (musl libc). User-facing.
QString psm_unsupported_platform_reason() {
#if defined(Q_OS_LINUX)
    // uv 0.7.12's download metadata carries gnu / darwin / windows CPython 3.11.9
    // builds only, and PyPI has no musl torch wheels — fail up front instead of
    // after a 13 MB download.
    const QDir loader_dir(QStringLiteral("/lib"));
    const QStringList musl = loader_dir.entryList({QStringLiteral("ld-musl-*.so.1")}, QDir::Files | QDir::System);
    if (!musl.isEmpty()) {
        LOG_ERROR(kPsmTag, "musl libc detected (" + musl.join(", ") + ") — setup cannot proceed");
        return PythonSetupManager::tr("musl libc detected — Fincept Terminal needs a glibc-based Linux. The Python "
                                      "3.11 runtime is published for glibc only, and several analytics libraries "
                                      "(PyTorch among them) ship no musl wheels.");
    }
#endif
    return {};
}

QString psm_sentinel_path(const QString& root) {
    return root + QStringLiteral("/.setup_complete");
}
QString psm_marker_path(const QString& root, const QString& venv) {
    return root + QLatin1Char('/') + venv + QStringLiteral("/.packages_installed");
}
QString psm_core_marker_path(const QString& root) {
    return root + QLatin1Char('/') + QLatin1String(kPsmVenvAnalytics) + QStringLiteral("/.core_installed");
}

QString psm_read_trimmed(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(f.read(64 * 1024)).trimmed();
}

bool psm_write_atomic(const QString& path, const QByteArray& content) {
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    if (f.write(content) != content.size()) {
        f.cancelWriting();
        return false;
    }
    return f.commit();
}

/// key=value pairs of a v2 marker; empty for a legacy (pre-4.5.1) marker.
QHash<QString, QString> psm_parse_marker(const QString& content) {
    QHash<QString, QString> kv;
    const QStringList lines = content.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString& raw : lines) {
        const QString line = raw.trimmed();
        const int eq = static_cast<int>(line.indexOf(QLatin1Char('=')));
        if (eq > 0)
            kv.insert(line.left(eq).trimmed(), line.mid(eq + 1).trimmed());
    }
    if (kv.value(QStringLiteral("fincept-marker")) != QLatin1String("2"))
        kv.clear();
    return kv;
}

bool psm_has_proxy_env(const QProcessEnvironment& env) {
    for (const char* key : {"HTTPS_PROXY", "https_proxy", "HTTP_PROXY", "http_proxy", "ALL_PROXY", "all_proxy"}) {
        if (!env.value(QString::fromLatin1(key)).isEmpty())
            return true;
    }
    return false;
}

/// Explicit HTTPS proxy from the environment, for the Qt download (Qt itself
/// does not read HTTPS_PROXY unless system proxy configuration is enabled).
QNetworkProxy psm_proxy_from_env(const QProcessEnvironment& env) {
    for (const char* key : {"HTTPS_PROXY", "https_proxy", "ALL_PROXY", "all_proxy"}) {
        const QString value = env.value(QString::fromLatin1(key));
        if (value.isEmpty())
            continue;
        const QUrl url = QUrl::fromUserInput(value);
        if (!url.isValid() || url.host().isEmpty())
            continue;
        QNetworkProxy proxy(url.scheme().startsWith(QLatin1String("socks")) ? QNetworkProxy::Socks5Proxy
                                                                             : QNetworkProxy::HttpProxy,
                            url.host(), static_cast<quint16>(url.port(8080)));
        if (!url.userName().isEmpty()) {
            proxy.setUser(url.userName());
            proxy.setPassword(url.password());
        }
        return proxy;
    }
    return QNetworkProxy(QNetworkProxy::NoProxy);
}

QString psm_yn(bool v) {
    return v ? QStringLiteral("Y") : QStringLiteral("N");
}

QString psm_fmt_bytes(qint64 bytes) {
    if (bytes < 0)
        bytes = 0;
    if (bytes >= 1024LL * 1024LL)
        return QString::number(static_cast<double>(bytes) / (1024.0 * 1024.0), 'f', 1) + QStringLiteral(" MB");
    if (bytes >= 1024LL)
        return QString::number(static_cast<double>(bytes) / 1024.0, 'f', 0) + QStringLiteral(" KB");
    return QString::number(bytes) + QStringLiteral(" B");
}

/// Lines that carry information for a human (filters uv -v DEBUG chatter).
bool psm_meaningful_line(const QString& line) {
    const QString t = line.trimmed();
    if (t.isEmpty())
        return false;
    if (t.startsWith(QLatin1String("DEBUG ")) || t.startsWith(QLatin1String("TRACE ")) ||
        t.startsWith(QLatin1String("INFO ")) || t.startsWith(QLatin1String("WARN Skipping file")))
        return false;
    if (t.startsWith(QLatin1String("+ ")) || t.startsWith(QLatin1String("- ")))
        return false; // per-package install listing
    return true;
}

void psm_push_tail(QStringList& tail, const QString& line) {
    if (!psm_meaningful_line(line))
        return;
    tail.append(line.trimmed().left(400));
    while (tail.size() > kPsmTailKeep)
        tail.removeFirst();
}

/// Splits `chunk` (appended to `partial`) into complete lines; with `flush` the
/// trailing partial line is emitted too.
template <typename Fn>
void psm_drain_lines(QByteArray& partial, const QByteArray& chunk, bool flush, Fn&& fn) {
    partial.append(chunk);
    qsizetype start = 0;
    for (;;) {
        const qsizetype nl = partial.indexOf('\n', start);
        if (nl < 0)
            break;
        QByteArray line = partial.mid(start, nl - start);
        if (line.endsWith('\r'))
            line.chop(1);
        fn(line);
        start = nl + 1;
    }
    partial.remove(0, start);
    // A pathological child that never prints a newline must not grow this unbounded.
    if ((flush || partial.size() > (qsizetype{1} << 20)) && !partial.isEmpty()) {
        fn(partial);
        partial.clear();
    }
}

QString psm_root_native() {
    return QDir::toNativeSeparators(AppPaths::root());
}

bool psm_looks_like_tls(const QString& text) {
    const QString t = text.toLower();
    return t.contains(QLatin1String("invalid peer certificate")) || t.contains(QLatin1String("unknownissuer")) ||
           t.contains(QLatin1String("certificate verify failed")) ||
           t.contains(QLatin1String("self-signed certificate")) ||
           t.contains(QLatin1String("self signed certificate")) ||
           t.contains(QLatin1String("unable to get local issuer"));
}

/// One actionable line for the most common failure classes.
QString psm_hint(const QString& text, bool failed_to_start) {
    if (failed_to_start)
        return PythonSetupManager::tr("Hint: the program could not be launched — antivirus software may have "
                                      "quarantined or blocked it. Allow %1 in your antivirus, then retry.")
            .arg(psm_root_native());
    const QString t = text.toLower();
    const auto has = [&t](const char* needle) { return t.contains(QLatin1String(needle)); };
    if (psm_looks_like_tls(text))
        return PythonSetupManager::tr("Hint: a TLS certificate was rejected — a corporate proxy or antivirus is "
                                      "probably intercepting HTTPS. Ask IT to allow pypi.org, files.pythonhosted.org, "
                                      "github.com and download.pytorch.org, or to install the proxy's root "
                                      "certificate in the system store.");
    if (has("dns error") || has("failed to lookup address") || has("no such host is known") ||
        has("name or service not known") || has("nodename nor servname") ||
        has("temporary failure in name resolution") || has("could not resolve host"))
        return PythonSetupManager::tr("Hint: the download servers could not be found (DNS). Check that this "
                                      "computer is online, then retry.");
    if (has("407") || has("proxy authentication") || (has("proxy") && (has("tunnel") || has("connect"))))
        return PythonSetupManager::tr("Hint: a proxy refused the connection. Set HTTPS_PROXY and HTTP_PROXY for "
                                      "Fincept Terminal (with credentials in the URL if required), then retry.");
    if (has("no space left on device") || has("not enough space on the disk") || has("(os error 28)") ||
        has("(os error 112)") || has("disk full"))
        return PythonSetupManager::tr("Hint: the disk is full. Setup needs about 6 GB free on the drive that "
                                      "holds %1.")
            .arg(psm_root_native());
    if (has("(os error 32)") || has("being used by another process"))
        return PythonSetupManager::tr("Hint: a file is in use. Close other Fincept Terminal windows and Python "
                                      "processes, then retry (antivirus scans can also hold files briefly).");
    if (has("(os error 225)") || has("contains a virus"))
        return PythonSetupManager::tr("Hint: antivirus blocked a downloaded package. Allow %1 in your antivirus, "
                                      "then retry.")
            .arg(psm_root_native());
    if (has("(os error 5)") || has("access is denied") || has("permission denied") || has("operation not permitted"))
        return PythonSetupManager::tr("Hint: access was denied — antivirus may be blocking %1, or the folder is "
                                      "read-only. Allow the folder in your antivirus, then retry.")
            .arg(psm_root_native());
    if (has("(os error 206)") || has("filename or extension is too long") || has("file name too long"))
        return PythonSetupManager::tr("Hint: a file path is too long for Windows. Enable long paths "
                                      "(LongPathsEnabled) or use a shorter user folder, then retry.");
    if (has("no solution found when resolving"))
        return PythonSetupManager::tr("Hint: no compatible package versions exist for this platform. Please "
                                      "report this together with the log file.");
    if (has("failed to build") || has("build backend"))
        return PythonSetupManager::tr("Hint: a package had to be compiled from source and the build failed. Retry; "
                                      "if it fails again, please report it together with the log file.");
    if (has("error sending request") || has("connection refused") || has("connection reset") || has("timed out") ||
        has("request failed after") || has("failed to fetch") || has("network is unreachable") ||
        has("failed to download") || has("operation canceled"))
        return PythonSetupManager::tr("Hint: a network error interrupted the download. Retry — everything already "
                                      "downloaded is cached, so a retry resumes where it stopped.");
    return {};
}

/// Package name of a uv cache lock path, e.g.
///   Released lock at `...\wheels-v5\pypi\numpy\numpy-2.4.6-cp311-....lock`
///   Released lock at `.../sdists-v9/index/<digest>/torch/...`
QString psm_lock_package(QStringView body) {
    const qsizetype a = body.indexOf(u'`');
    const qsizetype b = body.lastIndexOf(u'`');
    if (a < 0 || b <= a)
        return {};
    QString path = body.mid(a + 1, b - a - 1).toString();
    path.replace(QLatin1Char('\\'), QLatin1Char('/'));
    const QStringList parts = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (qsizetype i = 0; i + 2 < parts.size(); ++i) {
        const QString& seg = parts[i];
        if (!seg.startsWith(QLatin1String("wheels-v")) && !seg.startsWith(QLatin1String("sdists-v")))
            continue;
        if (parts[i + 1] == QLatin1String("pypi"))
            return PythonSetupManager::canonical_package_name(parts[i + 2]);
        if (parts[i + 1] == QLatin1String("index") && i + 3 < parts.size())
            return PythonSetupManager::canonical_package_name(parts[i + 3]);
        return {};
    }
    return {};
}

int psm_leading_int(const QString& text, int from) {
    int value = 0;
    bool any = false;
    for (qsizetype i = from; i < text.size() && text.at(i).isDigit(); ++i) {
        value = value * 10 + text.at(i).digitValue();
        any = true;
    }
    return any ? value : 0;
}

/// Human form of a uv progress line ("Downloading cpython-… (download) (23.9MiB)").
QString psm_pretty_uv_line(const QString& line) {
    QString t = line.trimmed();
    t.replace(QLatin1String(" (download)"), QString());
    t.replace(QLatin1String("MiB)"), QLatin1String(" MB)"));
    t.replace(QLatin1String("KiB)"), QLatin1String(" KB)"));
    t.replace(QLatin1String("GiB)"), QLatin1String(" GB)"));
    if (t.startsWith(QLatin1String("Waiting to acquire lock")))
        return PythonSetupManager::tr("Waiting for another uv process to release its lock…");
    return t.left(160);
}

// ── Process-tree control ─────────────────────────────────────────────────────
// Windows: each child goes into its own Job Object with KILL_ON_JOB_CLOSE, so
// terminating / closing the job takes uv's own children (build backends,
// bytecode workers) with it — and the OS kills them if this process dies.
// Unix: each child leads its own process group (setpgid in the child), so a
// group SIGKILL does the same.

void* psm_job_adopt(QProcess* proc) {
#ifdef Q_OS_WIN
    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    if (!job)
        return nullptr;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};
    info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation, &info, sizeof(info))) {
        CloseHandle(job);
        return nullptr;
    }
    HANDLE child = OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE, FALSE, static_cast<DWORD>(proc->processId()));
    if (!child) {
        CloseHandle(job);
        return nullptr;
    }
    const BOOL assigned = AssignProcessToJobObject(job, child);
    CloseHandle(child);
    if (!assigned) {
        LOG_DEBUG(kPsmTag, QString("AssignProcessToJobObject failed (%1) — falling back to QProcess::kill")
                               .arg(static_cast<qulonglong>(GetLastError())));
        CloseHandle(job);
        return nullptr;
    }
    return job;
#else
    Q_UNUSED(proc);
    return nullptr;
#endif
}

void psm_job_close(void* job) {
#ifdef Q_OS_WIN
    if (job)
        CloseHandle(static_cast<HANDLE>(job));
#else
    Q_UNUSED(job);
#endif
}

void psm_kill_tree(QProcess* proc) {
    if (!proc || proc->state() == QProcess::NotRunning)
        return;
#ifdef Q_OS_WIN
    const auto job = static_cast<quintptr>(proc->property("psm_job").toULongLong());
    if (job)
        TerminateJobObject(reinterpret_cast<HANDLE>(job), 1);
#else
    const qint64 pid = proc->processId();
    if (pid > 1) // never 0/1: kill(-0) would signal our own process group
        ::kill(-static_cast<pid_t>(pid), SIGKILL);
#endif
    proc->kill();
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// PsmUvProgress
// ─────────────────────────────────────────────────────────────────────────────

bool psm_detail::PsmUvProgress::feed(const QString& raw_line) {
    const QString line = raw_line.trimmed();
    if (line.isEmpty())
        return false;

    if (line.startsWith(QLatin1String("DEBUG "))) {
        const QStringView body = QStringView(line).mid(6);
        if (body.startsWith(u"Selecting: ")) {
            ++selected;
            if (phase == Phase::Starting)
                phase = Phase::Resolving;
            return true;
        }
        static const QLatin1String kUncached("Identified uncached distribution: ");
        if (body.startsWith(kUncached)) {
            const QString spec = body.mid(kUncached.size()).toString();
            const qsizetype eq = spec.indexOf(QLatin1String("=="));
            uncached.insert(PythonSetupManager::canonical_package_name(eq > 0 ? spec.left(eq) : spec));
            return true;
        }
        if (phase == Phase::Preparing && body.startsWith(u"Released lock at `")) {
            const QString name = psm_lock_package(body);
            if (!name.isEmpty() && uncached.contains(name) && !prepared.contains(name)) {
                prepared.insert(name);
                return true;
            }
        }
        return false;
    }
    if (!psm_meaningful_line(line)) {
        if (line.startsWith(QLatin1String("+ ")) && phase == Phase::Preparing)
            phase = Phase::Installing;
        return false;
    }

    if (line.startsWith(QLatin1String("Resolved "))) {
        resolved = psm_leading_int(line, 9);
        phase = Phase::Preparing;
        headline = line;
        return true;
    }
    if (line.startsWith(QLatin1String("Prepared "))) {
        prepared = uncached;
        phase = Phase::Installing;
        headline = line;
        return true;
    }
    if (line.startsWith(QLatin1String("Uninstalled "))) {
        phase = Phase::Installing;
        headline = line;
        return true;
    }
    if (line.startsWith(QLatin1String("Installed "))) {
        phase = Phase::Compiling;
        headline = line;
        return true;
    }
    if (line.startsWith(QLatin1String("Audited ")) || line.startsWith(QLatin1String("Bytecode compiled "))) {
        phase = Phase::Finished;
        headline = line;
        return true;
    }
    if (line.startsWith(QLatin1String("Using Python ")) || line.startsWith(QLatin1String("Using CPython "))) {
        if (phase == Phase::Starting)
            phase = Phase::Resolving;
        return false;
    }
    headline = psm_pretty_uv_line(line); // Downloading X (12.0 MB), Building X, warnings, errors
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Private nested types
// ─────────────────────────────────────────────────────────────────────────────

struct PythonSetupManager::StepOptions {
    QProcessEnvironment env;
    int timeout_ms = 0;
    bool capture_stdout = false; // keep raw stdout (uv --version, uv pip freeze)
    bool background = false;     // below-normal priority (never on the launch-critical path)
    bool local8bit = false;      // non-uv tools print in the local code page
    QString working_dir;         // child cwd; lets callers pass space-free relative paths to uv
};

struct PythonSetupManager::StepResult {
    enum class Kind { Ok, ExitCode, FailedToStart, TimedOut, Crashed };
    Kind kind = Kind::Ok;
    int exit_code = 0;
    int timeout_ms = 0;
    QString program;
    QString start_error;
    QStringList err_tail;
    QStringList out_tail;
    QByteArray stdout_data;

    bool ok() const { return kind == Kind::Ok; }
    QString tail_text() const { return (err_tail.isEmpty() ? out_tail : err_tail).join(QLatin1Char('\n')); }
};

struct PythonSetupManager::Run {
    struct Lane {
        int index = 0;
        QString venv;
        QString req_file;
        QString core_file; // analytics lane only
        bool active = false;
        bool fresh_venv = false;
        bool locked = false;   // compile produced the lock
        bool fallback = false; // compile failed → install straight from the requirements file
        bool core_done = false;
        bool waiting_gate = false;
        bool succeeded = false;
        bool failed = false;
        bool tls_retried = false;
        QString error;
        PsmUvProgress progress;
    };

    quint64 generation = 0;
    qint64 started_ms = 0;
    bool cancelled = false;
    bool finished = false;
    bool did_work = false;
    bool trading_gate_open = false;
    bool runtime_tls_retried = false;
    bool network_ready = false;
    QString runtime_step_key = QStringLiteral("uv");
    std::vector<std::function<void()>> network_waiters;
    std::array<Lane, 2> lanes{};

    // uv download
    std::unique_ptr<QFile> dl_file;
    std::unique_ptr<QCryptographicHash> dl_hash;
    QString dl_archive;
    bool dl_write_failed = false;
    qint64 dl_bytes = 0;
    qint64 dl_total = 0;
};

// ─────────────────────────────────────────────────────────────────────────────
// Singleton / paths
// ─────────────────────────────────────────────────────────────────────────────

PythonSetupManager& PythonSetupManager::instance() {
    static PythonSetupManager s;
    return s;
}

PythonSetupManager::PythonSetupManager(QObject* parent) : QObject(parent) {
    progress_timer_ = new QTimer(this);
    progress_timer_->setSingleShot(true);
    progress_timer_->setInterval(kPsmProgressCoalesceMs);
    connect(progress_timer_, &QTimer::timeout, this, &PythonSetupManager::flush_progress);

    // QProcess / QNetworkAccessManager children are created on this object's
    // thread, which must be the GUI thread even if a worker touched instance() first.
    if (auto* app = QCoreApplication::instance(); app && thread() != app->thread())
        moveToThread(app->thread());
}

PythonSetupManager::~PythonSetupManager() = default;

QString PythonSetupManager::install_dir() const {
    return fincept::AppPaths::root();
}

QString PythonSetupManager::uv_path() const {
#ifdef _WIN32
    return install_dir() + "/uv/uv.exe";
#else
    return install_dir() + "/uv/uv";
#endif
}

QString PythonSetupManager::python_path(const QString& venv_name) const {
#ifdef _WIN32
    return install_dir() + "/" + venv_name + "/Scripts/python.exe";
#else
    return install_dir() + "/" + venv_name + "/bin/python3";
#endif
}

QString PythonSetupManager::base_python_path() const {
    const QDir dir(install_dir() + "/python");
    QStringList entries = dir.entryList({QStringLiteral("cpython-%1-*").arg(QLatin1String(kPythonVersion))},
                                        QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
#ifdef Q_OS_WIN
    // On Windows ARM64 the x64 build (run under emulation) is requested on
    // purpose — far better wheel coverage — so prefer it if both exist.
    std::stable_partition(entries.begin(), entries.end(),
                          [](const QString& e) { return e.contains(QLatin1String("x86_64")); });
#endif
    for (const QString& entry : std::as_const(entries)) {
#ifdef Q_OS_WIN
        const QString exe = dir.filePath(entry + "/python.exe");
#else
        const QString exe = dir.filePath(entry + "/bin/python3");
#endif
        if (QFileInfo::exists(exe))
            return exe;
    }
    return {};
}

QString PythonSetupManager::python_request() const {
#ifdef Q_OS_WIN
    // An arm64 uv would ask for the windows-aarch64 CPython, and the analytics
    // set has wheels that only exist for win_amd64 (pyqlib among them). The x64
    // interpreter runs under Windows 11's emulation and keeps every wheel available.
    if (QSysInfo::currentCpuArchitecture() == QLatin1String("arm64"))
        return QStringLiteral("cpython-%1-windows-x86_64-none").arg(QLatin1String(kPythonVersion));
#endif
    return QString::fromLatin1(kPythonVersion);
}

QString PythonSetupManager::lock_path(const QString& venv_name) const {
    return install_dir() + "/" + venv_name + "/requirements.lock";
}

bool PythonSetupManager::venv_valid(const QString& venv_name) const {
    if (!QFileInfo::exists(python_path(venv_name)))
        return false;
    QFile cfg(install_dir() + "/" + venv_name + "/pyvenv.cfg");
    if (!cfg.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    QString version;
    QString home;
    while (!cfg.atEnd()) {
        const QString line = QString::fromUtf8(cfg.readLine()).trimmed();
        const qsizetype eq = line.indexOf(QLatin1Char('='));
        if (eq <= 0)
            continue;
        const QString key = line.left(eq).trimmed().toLower();
        const QString value = line.mid(eq + 1).trimmed();
        if (key == QLatin1String("version_info") || key == QLatin1String("version"))
            version = value;
        else if (key == QLatin1String("home"))
            home = value;
    }
    const QString pinned = QString::fromLatin1(kPythonVersion);
    if (!version.isEmpty() && version != pinned && !version.startsWith(pinned + QLatin1Char('.')))
        return false; // built on another interpreter — must be recreated
    if (!home.isEmpty() && !QFileInfo::exists(home))
        return false; // its base interpreter is gone
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Requirements files
// ─────────────────────────────────────────────────────────────────────────────

QString PythonSetupManager::canonical_package_name(const QString& name) {
    static const QRegularExpression kSeparators(QStringLiteral("[-_.]+"));
    QString n = name.trimmed().toLower();
    n.replace(kSeparators, QStringLiteral("-"));
    return n;
}

QString PythonSetupManager::requirement_name(const QString& spec) {
    static const QRegularExpression kName(QStringLiteral(R"(^\s*([A-Za-z0-9](?:[A-Za-z0-9._-]*[A-Za-z0-9])?))"));
    const QRegularExpressionMatch m = kName.match(spec);
    return m.hasMatch() ? m.captured(1) : QString();
}

QStringList PythonSetupManager::read_requirement_lines(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    QStringList specs;
    while (!file.atEnd()) {
        QString line = QString::fromUtf8(file.readLine()).trimmed();
        // Blank lines, comments and pip options (-r, -c, --index-url, …) are not packages.
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')) || line.startsWith(QLatin1Char('-')))
            continue;
        const qsizetype comment = line.indexOf(QLatin1String(" #"));
        if (comment > 0)
            line = line.left(comment).trimmed();
        if (!line.isEmpty())
            specs << line;
    }
    return specs;
}

QString PythonSetupManager::find_requirements_file(const QString& filename) const {
    {
        QMutexLocker lock(&mutex_);
        const auto it = req_path_cache_.constFind(filename);
        if (it != req_path_cache_.constEnd())
            return it.value();
    }

    const QString exe_dir = QCoreApplication::applicationDirPath();
    // Exe-relative layouts first (deterministic for installed builds), then a
    // walk up the tree for dev builds running out of build/<preset>/.
    const QStringList candidates = {
        exe_dir + "/../Resources/" + filename,                     // macOS .app/Contents/Resources (flattened)
        exe_dir + "/resources/" + filename,                        // Windows / AppImage / dev POST_BUILD copy
        exe_dir + "/../resources/" + filename,                     //
        exe_dir + "/../share/fincept-terminal/resources/" + filename, // Linux FHS / Flatpak (<prefix>/bin/..)
        exe_dir + "/../../resources/" + filename,
    };
    QString found;
    for (const QString& c : candidates) {
        if (QFileInfo::exists(c)) {
            found = QDir::cleanPath(c);
            break;
        }
    }
    if (found.isEmpty()) {
        QDir dir(exe_dir);
        while (dir.cdUp()) {
            const QString c = dir.filePath("resources/" + filename);
            if (QFileInfo::exists(c)) {
                found = QDir::cleanPath(c);
                break;
            }
            if (dir.isRoot())
                break;
        }
    }

    if (found.isEmpty()) {
        LOG_ERROR(kPsmTag, QString("Requirements file %1 not found (exe_dir=%2). Tried: %3 and every parent's "
                                   "resources/. The resources/ folder must ship beside the executable.")
                               .arg(filename, exe_dir, candidates.join(", ")));
        return {};
    }
    QMutexLocker lock(&mutex_);
    req_path_cache_.insert(filename, found);
    return found;
}

QString PythonSetupManager::requirements_hash(const QString& filename) const {
    {
        QMutexLocker lock(&mutex_);
        const auto it = req_hash_cache_.constFind(filename);
        if (it != req_hash_cache_.constEnd())
            return it.value();
    }
    const QString path = find_requirements_file(filename);
    if (path.isEmpty())
        return {};
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const QString hash = QString::fromLatin1(QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256).toHex());
    QMutexLocker lock(&mutex_);
    req_hash_cache_.insert(filename, hash);
    return hash;
}

/// Marker v2: proves an install from exactly this requirements content, on this
/// interpreter, with this uv, succeeded. Any of the three changing → re-sync.
QString PythonSetupManager::expected_marker(const QString& requirements_filename) const {
    const QString hash = requirements_hash(requirements_filename);
    if (hash.isEmpty())
        return {};
    return QStringLiteral("fincept-marker=2\nrequirements=%1\npython=%2\nuv=%3")
        .arg(hash, QLatin1String(kPythonVersion), QLatin1String(kUvVersion));
}

// ─────────────────────────────────────────────────────────────────────────────
// uv environment
// ─────────────────────────────────────────────────────────────────────────────

QProcessEnvironment PythonSetupManager::uv_environment() const {
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

    // A user's uv / Python configuration must not redirect the app's own
    // environments (UV_PROJECT_ENVIRONMENT, UV_SYSTEM_PYTHON, UV_PYTHON, a
    // VIRTUAL_ENV left active in the launching shell, …). Network settings are
    // the exception: corporate users legitimately need their mirror / native TLS.
    static const QSet<QString> kKeepUv = {
        QStringLiteral("UV_NATIVE_TLS"),         QStringLiteral("UV_INDEX_URL"),
        QStringLiteral("UV_EXTRA_INDEX_URL"),    QStringLiteral("UV_DEFAULT_INDEX"),
        QStringLiteral("UV_INDEX"),              QStringLiteral("UV_INDEX_STRATEGY"),
        QStringLiteral("UV_KEYRING_PROVIDER"),   QStringLiteral("UV_INSECURE_HOST"),
        QStringLiteral("UV_PYTHON_INSTALL_MIRROR"), QStringLiteral("UV_PYPY_INSTALL_MIRROR"),
    };
    const QStringList keys = env.keys();
    for (const QString& key : keys) {
        const QString upper = key.toUpper();
        if (upper.startsWith(QLatin1String("UV_")) && !kKeepUv.contains(upper))
            env.remove(key);
    }
    for (const char* key : {"VIRTUAL_ENV", "CONDA_PREFIX", "CONDA_DEFAULT_ENV", "PYTHONHOME", "PYTHONPATH",
                            "PYTHONSTARTUP", "PYTHONUSERBASE", "FORCE_COLOR", "CLICOLOR_FORCE"})
        env.remove(QString::fromLatin1(key));

    const QString root = install_dir();
    env.insert("UV_PYTHON_INSTALL_DIR", root + "/python");
    env.insert("UV_CACHE_DIR", root + "/uv-cache"); // shared by setup, Settings → Python Env and uvx
    env.insert("UV_NO_CONFIG", "1");
    env.insert("UV_PYTHON_PREFERENCE", "only-managed"); // a system 3.11.9 is never picked up
    env.insert("UV_CONCURRENT_DOWNLOADS", "32");        // uv's own default is 50; the old clamp to 8 starved it
    env.insert("UV_CONCURRENT_INSTALLS", QString::number(std::max(2, QThread::idealThreadCount())));
    env.insert("UV_HTTP_TIMEOUT", "120");
    env.insert("NO_COLOR", "1");
    // peewee (yfinance → zipline deps) must not try to build its C extension.
    env.insert("PEEWEE_NO_SQLITE_EXTENSIONS", "1");
    env.insert("PEEWEE_NO_C_EXTENSION", "1");
#ifdef Q_OS_LINUX
    // PyPI's Linux torch wheels drag in multi-GB CUDA runtimes; the analytics
    // scripts run on CPU, so resolve the PyTorch family from the CPU index.
    env.insert("UV_TORCH_BACKEND", "cpu");
#endif
    if (native_tls_.load())
        env.insert("UV_NATIVE_TLS", "1");

    QString proxy;
    {
        QMutexLocker lock(&mutex_);
        proxy = system_proxy_;
    }
    if (!proxy.isEmpty() && !psm_has_proxy_env(env)) {
        env.insert("HTTPS_PROXY", proxy);
        env.insert("HTTP_PROXY", proxy);
    }
    return env;
}

// ─────────────────────────────────────────────────────────────────────────────
// Status (cheap, filesystem only)
// ─────────────────────────────────────────────────────────────────────────────

SetupStatus PythonSetupManager::check_status() const {
    SetupStatus st;
    const QString root = install_dir();

    st.uv_installed = QFileInfo::exists(uv_path());
    st.python_installed = !base_python_path().isEmpty();
    st.venv_numpy1_created = venv_valid(QString::fromLatin1(kPsmVenvTrading));
    st.venv_numpy2_created = venv_valid(QString::fromLatin1(kPsmVenvAnalytics));

    const QString marker1 = psm_read_trimmed(psm_marker_path(root, QString::fromLatin1(kPsmVenvTrading)));
    const QString marker2 = psm_read_trimmed(psm_marker_path(root, QString::fromLatin1(kPsmVenvAnalytics)));
    const QString marker_core = psm_read_trimmed(psm_core_marker_path(root));
    const QString expected1 = expected_marker(QString::fromLatin1(kPsmReqTrading));
    const QString expected2 = expected_marker(QString::fromLatin1(kPsmReqAnalytics));

    const auto ready = [](bool created, const QString& marker, const QString& expected) {
        if (!created || marker.isEmpty())
            return false;
        // No requirements file means a broken deployment — nothing could be
        // installed, so don't loop a background sync that can only fail.
        return expected.isEmpty() || marker == expected;
    };
    st.venv_numpy1_ready = ready(st.venv_numpy1_created, marker1, expected1);
    st.venv_numpy2_ready = ready(st.venv_numpy2_created, marker2, expected2);

    // The terminal can launch on venv-numpy2 when it holds (at least) the core
    // packages for the pinned interpreter: a current full marker, any v2 marker
    // for this Python (requirements moved on → background sync), or a legacy
    // pre-4.5.1 marker (SHA-256 / "ok") — an existing install, synced in background.
    const auto same_python = [](const QString& marker) {
        const QHash<QString, QString> kv = psm_parse_marker(marker);
        return !kv.isEmpty() && kv.value(QStringLiteral("python")) == QLatin1String(kPythonVersion);
    };
    const bool legacy_marker = !marker2.isEmpty() && psm_parse_marker(marker2).isEmpty();
    st.core_ready = st.venv_numpy2_created && st.python_installed &&
                    (st.venv_numpy2_ready || same_python(marker2) || same_python(marker_core) || legacy_marker);
    st.needs_setup = !st.core_ready;
    st.needs_package_sync = st.core_ready && !(st.venv_numpy1_ready && st.venv_numpy2_ready);

    LOG_INFO(kPsmTag, QString("check_status: uv=%1 python=%2 venv1=%3/%4 venv2=%5/%6 core=%7 legacy=%8 -> "
                              "needs_setup=%9 needs_sync=%10")
                          .arg(psm_yn(st.uv_installed), psm_yn(st.python_installed), psm_yn(st.venv_numpy1_created),
                               psm_yn(st.venv_numpy1_ready), psm_yn(st.venv_numpy2_created),
                               psm_yn(st.venv_numpy2_ready), psm_yn(st.core_ready), psm_yn(legacy_marker),
                               psm_yn(st.needs_setup))
                          .arg(psm_yn(st.needs_package_sync)));
    return st;
}

SetupSnapshot PythonSetupManager::snapshot() const {
    QMutexLocker lock(&mutex_);
    SetupSnapshot s = snapshot_;
    s.running = running_.load();
    s.core_ready = core_ready_.load();
    s.background = background_mode_.load();
    s.overall_percent = overall_percent_.load();
    return s;
}

void PythonSetupManager::set_background_mode() {
    background_mode_.store(true);
    QMutexLocker lock(&mutex_);
    snapshot_.background = true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Progress plumbing
// ─────────────────────────────────────────────────────────────────────────────

void PythonSetupManager::update_stage(SetupStage s, const std::function<void(SetupStageInfo&)>& fn, bool flush_now) {
    {
        QMutexLocker lock(&mutex_);
        fn(snapshot_.stages[psm_idx(s)]);
    }
    dirty_stages_[psm_idx(s)] = true;
    if (flush_now)
        flush_progress();
    else if (!progress_timer_->isActive())
        progress_timer_->start();
}

void PythonSetupManager::set_stage_fraction(SetupStage s, double fraction) {
    QMutexLocker lock(&mutex_);
    double& slot = stage_fraction_[psm_idx(s)];
    slot = std::max(slot, std::clamp(fraction, 0.0, 1.0)); // milestones only move forward
    double sum = 0.0;
    double weights = 0.0;
    for (std::size_t i = 0; i < stage_fraction_.size(); ++i) {
        sum += kPsmStageWeights[i] * stage_fraction_[i];
        weights += kPsmStageWeights[i];
    }
    const int pct = static_cast<int>(std::floor(sum / weights * 100.0 + 1e-6));
    snapshot_.overall_percent = std::clamp(pct, 0, 100);
    overall_percent_.store(snapshot_.overall_percent);
}

void PythonSetupManager::flush_progress() {
    progress_timer_->stop();
    std::vector<SetupProgress> events;
    {
        QMutexLocker lock(&mutex_);
        for (int i = 0; i < kSetupStageCount; ++i) {
            if (!dirty_stages_[static_cast<std::size_t>(i)])
                continue;
            dirty_stages_[static_cast<std::size_t>(i)] = false;
            const SetupStageInfo& info = snapshot_.stages[static_cast<std::size_t>(i)];
            SetupProgress p;
            p.stage = static_cast<SetupStage>(i);
            p.state = info.state;
            switch (p.stage) {
                case SetupStage::Runtime:
                    p.step = run_ ? run_->runtime_step_key : QStringLiteral("uv");
                    if (run_) {
                        p.bytes_done = run_->dl_bytes;
                        p.bytes_total = run_->dl_total;
                    }
                    break;
                case SetupStage::Trading:
                    p.step = QStringLiteral("packages-numpy1");
                    break;
                case SetupStage::AnalyticsCore:
                    p.step = QStringLiteral("packages-numpy2-core");
                    break;
                case SetupStage::AnalyticsFull:
                    p.step = QStringLiteral("packages-numpy2");
                    break;
            }
            const bool complete = info.state == SetupStageState::Done || info.state == SetupStageState::Skipped;
            p.progress = complete ? 100 : std::max(0, info.percent);
            p.busy = info.state == SetupStageState::Running && info.percent < 0;
            p.detail = info.detail;
            p.is_error = info.state == SetupStageState::Failed;
            p.message = p.is_error ? info.error : info.detail;
            p.overall = snapshot_.overall_percent;
            events.push_back(std::move(p));
        }
    }
    for (const SetupProgress& p : events)
        emit progress_changed(p);
}

// ─────────────────────────────────────────────────────────────────────────────
// Child processes
// ─────────────────────────────────────────────────────────────────────────────

bool PythonSetupManager::alive(quint64 generation) const {
    return run_ && run_->generation == generation && !run_->cancelled && !run_->finished;
}

QProcess* PythonSetupManager::start_step(const QString& program, const QStringList& args, const StepOptions& opts,
                                         LineFn on_line, DoneFn on_done) {
    struct Ctx {
        QByteArray out_partial;
        QByteArray err_partial;
        QByteArray stdout_all;
        QStringList err_tail;
        QStringList out_tail;
        bool done = false;
        bool timed_out = false;
        qint64 started_ms = 0;
    };
    auto ctx = std::make_shared<Ctx>();
    ctx->started_ms = psm_now_ms();
    auto done_fn = std::make_shared<DoneFn>(std::move(on_done));
    auto line_fn = std::make_shared<LineFn>(std::move(on_line));
    const bool capture = opts.capture_stdout;
    const bool local8bit = opts.local8bit;
    const int timeout_ms = opts.timeout_ms;

    auto* proc = new QProcess(this);
    proc->setProcessEnvironment(opts.env);
    proc->setProgram(program);
    proc->setArguments(args);
    if (!opts.working_dir.isEmpty())
        proc->setWorkingDirectory(opts.working_dir);
#ifdef Q_OS_WIN
    const bool low_priority = opts.background;
    proc->setCreateProcessArgumentsModifier([low_priority](QProcess::CreateProcessArguments* cpa) {
        cpa->flags |= CREATE_NO_WINDOW;
        if (low_priority)
            cpa->flags |= BELOW_NORMAL_PRIORITY_CLASS; // inherited by uv's own workers
    });
#else
    const bool low_priority = opts.background;
    proc->setChildProcessModifier([low_priority]() {
        ::setpgid(0, 0); // own process group → cancel() can kill the whole tree
        if (low_priority)
            ::setpriority(PRIO_PROCESS, 0, 10);
    });
#endif

    const auto decode = [local8bit](const QByteArray& bytes) {
        return local8bit ? QString::fromLocal8Bit(bytes) : QString::fromUtf8(bytes);
    };
    const auto handle_out = [ctx, line_fn, capture, decode](const QByteArray& chunk, bool flush) {
        if (capture)
            ctx->stdout_all.append(chunk);
        psm_drain_lines(ctx->out_partial, chunk, flush, [&](const QByteArray& raw) {
            const QString line = decode(raw);
            psm_push_tail(ctx->out_tail, line);
            if (*line_fn)
                (*line_fn)(line, false);
        });
    };
    const auto handle_err = [ctx, line_fn, decode](const QByteArray& chunk, bool flush) {
        psm_drain_lines(ctx->err_partial, chunk, flush, [&](const QByteArray& raw) {
            const QString line = decode(raw);
            psm_push_tail(ctx->err_tail, line);
            if (*line_fn)
                (*line_fn)(line, true);
        });
    };

    connect(proc, &QProcess::readyReadStandardOutput, this,
            [proc, handle_out]() { handle_out(proc->readAllStandardOutput(), false); });
    connect(proc, &QProcess::readyReadStandardError, this,
            [proc, handle_err]() { handle_err(proc->readAllStandardError(), false); });

    connect(proc, &QProcess::started, this, [proc]() {
        if (void* job = psm_job_adopt(proc))
            proc->setProperty("psm_job", QVariant::fromValue(static_cast<qulonglong>(reinterpret_cast<quintptr>(job))));
    });

    const auto finish = [this, proc, ctx, done_fn, program, timeout_ms](StepResult r) {
        r.program = program;
        r.timeout_ms = timeout_ms;
        r.err_tail = ctx->err_tail;
        r.out_tail = ctx->out_tail;
        r.stdout_data = std::move(ctx->stdout_all);
        live_procs_.removeAll(QPointer<QProcess>(proc));
        psm_job_close(reinterpret_cast<void*>(static_cast<quintptr>(proc->property("psm_job").toULongLong())));
        proc->setProperty("psm_job", QVariant());
        proc->deleteLater();
        const qint64 ms = psm_now_ms() - ctx->started_ms;
        if (r.ok())
            LOG_INFO(kPsmTag, QString("%1 finished OK in %2 ms").arg(QFileInfo(program).fileName()).arg(ms));
        else
            LOG_WARN(kPsmTag, QString("%1 failed after %2 ms (kind=%3 exit=%4): %5")
                                  .arg(QFileInfo(program).fileName())
                                  .arg(ms)
                                  .arg(static_cast<int>(r.kind))
                                  .arg(r.exit_code)
                                  .arg(r.tail_text().right(1500)));
        const DoneFn cb = std::move(*done_fn);
        if (cb)
            cb(r);
    };

    connect(proc, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [proc, ctx, handle_out, handle_err, finish](int exit_code, QProcess::ExitStatus status) {
                if (ctx->done)
                    return;
                ctx->done = true;
                handle_out(proc->readAllStandardOutput(), true);
                handle_err(proc->readAllStandardError(), true);
                StepResult r;
                r.exit_code = exit_code;
                if (ctx->timed_out)
                    r.kind = StepResult::Kind::TimedOut;
                else if (status == QProcess::CrashExit)
                    r.kind = StepResult::Kind::Crashed;
                else
                    r.kind = exit_code == 0 ? StepResult::Kind::Ok : StepResult::Kind::ExitCode;
                finish(std::move(r));
            });
    // FailedToStart is the one error after which `finished` never arrives.
    // Crashed/Timedout/Read/Write are followed by `finished` (or are not terminal).
    connect(proc, &QProcess::errorOccurred, this, [proc, ctx, finish](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart || ctx->done)
            return;
        ctx->done = true;
        StepResult r;
        r.kind = StepResult::Kind::FailedToStart;
        r.exit_code = -1;
        r.start_error = proc->errorString();
        finish(std::move(r));
    });

    if (timeout_ms > 0) {
        auto* watchdog = new QTimer(proc);
        watchdog->setSingleShot(true);
        watchdog->setInterval(timeout_ms);
        connect(watchdog, &QTimer::timeout, proc, [proc, ctx, program, timeout_ms]() {
            if (proc->state() == QProcess::NotRunning)
                return;
            LOG_ERROR(kPsmTag, QString("%1 exceeded its %2 min budget — killing it")
                                   .arg(QFileInfo(program).fileName())
                                   .arg(timeout_ms / 60000));
            ctx->timed_out = true;
            psm_kill_tree(proc);
        });
        connect(proc, &QProcess::started, watchdog, qOverload<>(&QTimer::start));
    }

    live_procs_.append(QPointer<QProcess>(proc));
    LOG_INFO(kPsmTag, QString("Running: %1 %2").arg(program, args.join(QLatin1Char(' '))));
    proc->start();
    return proc;
}

QString PythonSetupManager::describe_failure(const QString& what, const StepResult& r) const {
    const QString tool = QFileInfo(r.program).fileName();
    QString head;
    switch (r.kind) {
        case StepResult::Kind::FailedToStart:
            head = tr("%1 — could not start %2 (%3).").arg(what, tool, r.start_error);
            break;
        case StepResult::Kind::TimedOut:
            head = tr("%1 — no result after %2 minutes; the process was stopped.").arg(what).arg(r.timeout_ms / 60000);
            break;
        case StepResult::Kind::Crashed:
            head = tr("%1 — %2 crashed.").arg(what, tool);
            break;
        case StepResult::Kind::ExitCode:
            head = tr("%1 — %2 exited with code %3.").arg(what, tool).arg(r.exit_code);
            break;
        case StepResult::Kind::Ok:
            head = what;
            break;
    }
    QStringList tail = r.err_tail.isEmpty() ? r.out_tail : r.err_tail;
    if (tail.size() > kPsmTailShow)
        tail = tail.mid(tail.size() - kPsmTailShow);
    QString message = head;
    if (!tail.isEmpty())
        message += QLatin1Char('\n') + tail.join(QLatin1Char('\n'));
    const QString hint = psm_hint(head + QLatin1Char('\n') + tail.join(QLatin1Char('\n')),
                                  r.kind == StepResult::Kind::FailedToStart);
    if (!hint.isEmpty())
        message += QLatin1Char('\n') + hint;
    return message;
}

bool PythonSetupManager::retry_with_native_tls(const StepResult& r) {
    if (native_tls_.load() || r.ok() || !psm_looks_like_tls(r.tail_text()))
        return false;
    // uv ships Mozilla's roots (webpki); TLS-inspecting corporate proxies sign
    // with a root that only exists in the OS store. Switch for the rest of the session.
    LOG_WARN(kPsmTag, "TLS certificate rejected — retrying uv with the system certificate store (UV_NATIVE_TLS)");
    native_tls_.store(true);
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Network readiness (system proxy probe)
// ─────────────────────────────────────────────────────────────────────────────

void PythonSetupManager::detect_system_proxy() {
    if (!run_)
        return;
    if (psm_has_proxy_env(QProcessEnvironment::systemEnvironment())) {
        network_ready(); // uv reads HTTPS_PROXY / HTTP_PROXY itself
        return;
    }
    const quint64 gen = run_->generation;
    // systemProxyForQuery can block for seconds (WPAD / PAC evaluation) → worker
    // thread. The pool is drained in ~QCoreApplication, which runs before this
    // singleton's static destructor, so `self` outlives the job.
    PythonSetupManager* self = this;
    (void)QtConcurrent::run([self, gen]() {
        QString found;
        const QList<QNetworkProxy> proxies =
            QNetworkProxyFactory::systemProxyForQuery(QNetworkProxyQuery(QUrl(QStringLiteral("https://pypi.org/simple/"))));
        for (const QNetworkProxy& p : proxies) {
            if (p.type() == QNetworkProxy::HttpProxy && !p.hostName().isEmpty()) {
                found = QStringLiteral("http://%1:%2").arg(p.hostName()).arg(p.port() > 0 ? p.port() : 8080);
                break;
            }
        }
        QMetaObject::invokeMethod(
            self,
            [self, gen, found]() {
                if (!found.isEmpty()) {
                    LOG_INFO(kPsmTag, "System proxy for uv: " + found);
                    QMutexLocker lock(&self->mutex_);
                    self->system_proxy_ = found;
                }
                if (self->run_ && self->run_->generation == gen)
                    self->network_ready();
            },
            Qt::QueuedConnection);
    });
    QTimer::singleShot(kPsmProxyProbeTimeoutMs, this, [this, gen]() {
        if (run_ && run_->generation == gen && !run_->network_ready) {
            LOG_WARN(kPsmTag, "System proxy probe still running after 8 s — continuing without it");
            network_ready();
        }
    });
}

void PythonSetupManager::network_ready() {
    if (!run_ || run_->network_ready)
        return;
    run_->network_ready = true;
    std::vector<std::function<void()>> waiters;
    waiters.swap(run_->network_waiters);
    for (auto& fn : waiters)
        fn();
}

void PythonSetupManager::when_network_ready(std::function<void()> fn) {
    if (!run_)
        return;
    if (run_->network_ready) {
        fn();
        return;
    }
    run_->network_waiters.push_back(std::move(fn));
}

// ─────────────────────────────────────────────────────────────────────────────
// Run lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void PythonSetupManager::run_setup() {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this]() { run_setup(); }, Qt::QueuedConnection);
        return;
    }
    if (running_.load()) {
        LOG_INFO(kPsmTag, "run_setup ignored — a run is already in progress");
        return;
    }

    const SetupStatus st = check_status();

    run_ = std::make_unique<Run>();
    run_->generation = ++generation_;
    run_->started_ms = psm_now_ms();
    auto& trading = run_->lanes[0];
    trading.index = 0;
    trading.venv = QString::fromLatin1(kPsmVenvTrading);
    trading.req_file = QString::fromLatin1(kPsmReqTrading);
    trading.active = !st.venv_numpy1_ready;
    auto& analytics = run_->lanes[1];
    analytics.index = 1;
    analytics.venv = QString::fromLatin1(kPsmVenvAnalytics);
    analytics.req_file = QString::fromLatin1(kPsmReqAnalytics);
    analytics.core_file = QString::fromLatin1(kPsmReqCore);
    analytics.active = !st.venv_numpy2_ready;
    run_->trading_gate_open = !analytics.active;

    core_ready_.store(st.core_ready);
    running_.store(true);
    {
        QMutexLocker lock(&mutex_);
        snapshot_ = SetupSnapshot{};
        snapshot_.running = true;
        snapshot_.started_ms = run_->started_ms;
        snapshot_.background = background_mode_.load();
        stage_fraction_.fill(0.0);
        const auto init = [this](SetupStage s, bool needed, const QString& detail) {
            SetupStageInfo& info = snapshot_.stages[psm_idx(s)];
            info.state = needed ? SetupStageState::Waiting : SetupStageState::Skipped;
            info.percent = needed ? -1 : 100;
            info.detail = needed ? detail : tr("Already installed");
            if (!needed)
                stage_fraction_[psm_idx(s)] = 1.0;
        };
        init(SetupStage::Runtime, true, tr("Checking uv…"));
        init(SetupStage::Trading, trading.active, tr("Waiting for the Python runtime"));
        const bool core_needed = analytics.active && !st.core_ready;
        init(SetupStage::AnalyticsCore, analytics.active, tr("Waiting for the Python runtime"));
        if (analytics.active && !core_needed)
            snapshot_.stages[psm_idx(SetupStage::AnalyticsCore)].detail = tr("Verifying installed core libraries");
        init(SetupStage::AnalyticsFull, analytics.active, tr("Starts after the analytics core"));
    }
    overall_percent_.store(0);
    set_stage_fraction(SetupStage::Runtime, 0.0); // recompute overall from the skipped stages
    dirty_stages_.fill(true);

    LOG_INFO(kPsmTag, QString("=== run_setup #%1: trading lane=%2 analytics lane=%3 (core on disk=%4) ===")
                          .arg(run_->generation)
                          .arg(psm_yn(trading.active), psm_yn(analytics.active), psm_yn(st.core_ready)));
    emit running_changed(true);
    flush_progress();

    if (const QString blocked = psm_unsupported_platform_reason(); !blocked.isEmpty()) {
        runtime_failed(blocked);
        return;
    }
    detect_system_proxy();
    runtime_begin();
}

void PythonSetupManager::cancel(bool wait_for_exit) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, wait_for_exit]() { cancel(wait_for_exit); }, Qt::QueuedConnection);
        return;
    }
    if (run_ && !run_->finished) {
        LOG_WARN(kPsmTag, "Cancelling the Python setup pipeline");
        run_->cancelled = true;
        if (download_reply_)
            download_reply_->abort();
        const QList<QPointer<QProcess>> procs = live_procs_;
        for (const QPointer<QProcess>& p : procs) {
            if (!p)
                continue;
            psm_kill_tree(p.data());
            if (wait_for_exit && p->state() != QProcess::NotRunning)
                p->waitForFinished(2000); // application teardown only (aboutToQuit)
        }
        finish_run(false, tr("Setup was cancelled."));
    }
    if (wait_for_exit && nam_) {
        delete nam_; // before QCoreApplication is gone, not at static destruction
        nam_ = nullptr;
    }
}

void PythonSetupManager::finish_run(bool success, const QString& error) {
    if (!run_ || run_->finished)
        return;
    run_->finished = true;
    const bool cancelled = run_->cancelled;
    const bool did_work = run_->did_work;

    if (success) {
        if (!psm_write_atomic(psm_sentinel_path(install_dir()), QByteArrayLiteral("ok\n")))
            LOG_WARN(kPsmTag, "Could not write the .setup_complete sentinel");
        set_stage_fraction(SetupStage::Runtime, 1.0);
    }
    {
        QMutexLocker lock(&mutex_);
        snapshot_.finished = true;
        snapshot_.succeeded = success;
        snapshot_.cancelled = cancelled;
        snapshot_.error = error;
        snapshot_.running = false;
    }
    flush_progress();
    running_.store(false);

    if (success) {
        SetupProgress done;
        done.step = QStringLiteral("complete");
        done.progress = 100;
        done.message = tr("Setup complete — all Python environments are ready.");
        done.state = SetupStageState::Done;
        done.overall = overall_percent_.load();
        emit progress_changed(done);
        LOG_INFO(kPsmTag, QString("=== run_setup #%1 succeeded in %2 s ===")
                              .arg(run_->generation)
                              .arg((psm_now_ms() - run_->started_ms) / 1000));
    } else {
        LOG_ERROR(kPsmTag, QString("=== run_setup #%1 %2: %3 ===")
                               .arg(run_->generation)
                               .arg(cancelled ? QStringLiteral("cancelled") : QStringLiteral("failed"))
                               .arg(error));
    }

    emit setup_complete(success, error);
    emit running_changed(false);

    // One notification when the user is no longer looking at the setup screen.
    if (background_mode_.load() && did_work && !cancelled && QCoreApplication::instance() &&
        !QCoreApplication::closingDown()) {
        if (success)
            fincept::ui::ToastService::instance().post(
                fincept::ui::ToastService::Severity::Success,
                tr("Python environment ready — all analytics libraries are installed."), QStringLiteral("python-setup"));
        else
            fincept::ui::ToastService::instance().post(
                fincept::ui::ToastService::Severity::Warning,
                tr("Installing Python libraries failed. Click PYTHON ENV in the status bar to see why and retry."),
                QStringLiteral("python-setup"));
    }
}

void PythonSetupManager::maybe_finish_run() {
    if (!run_ || run_->finished || run_->cancelled)
        return;
    QStringList errors;
    for (const auto& lane : run_->lanes) {
        if (!lane.active)
            continue;
        if (!lane.succeeded && !lane.failed)
            return; // still running
        if (lane.failed)
            errors << lane.error;
    }
    finish_run(errors.isEmpty(), errors.join(QStringLiteral("\n\n")));
}

// ─────────────────────────────────────────────────────────────────────────────
// Runtime stage: uv + CPython
// ─────────────────────────────────────────────────────────────────────────────

void PythonSetupManager::runtime_begin() {
    update_stage(SetupStage::Runtime, [](SetupStageInfo& s) {
        s.state = SetupStageState::Running;
        s.started_ms = psm_now_ms();
        s.percent = -1;
        s.detail = tr("Checking uv…");
    });
    if (QFileInfo::exists(uv_path()))
        runtime_check_uv_version(false);
    else
        runtime_download_uv();
}

void PythonSetupManager::runtime_failed(const QString& error) {
    if (!run_)
        return;
    update_stage(
        SetupStage::Runtime,
        [&error](SetupStageInfo& s) {
            s.state = SetupStageState::Failed;
            s.error = error;
            s.finished_ms = psm_now_ms();
        },
        true);
    for (auto& lane : run_->lanes) {
        if (!lane.active)
            continue;
        lane.failed = true;
        lane.error = error;
        const SetupStage stage = lane.index == 0 ? SetupStage::Trading : SetupStage::AnalyticsCore;
        update_stage(stage, [](SetupStageInfo& s) { s.detail = tr("Not started — the Python runtime is missing"); });
    }
    finish_run(false, tr("Runtime: %1").arg(error));
}

void PythonSetupManager::runtime_check_uv_version(bool after_install) {
    const quint64 gen = run_->generation;
    StepOptions opts;
    opts.env = uv_environment();
    opts.timeout_ms = kPsmUvVersionTimeoutMs;
    opts.capture_stdout = true;
    start_step(uv_path(), {QStringLiteral("--version")}, opts, {}, [this, gen, after_install](const StepResult& r) {
        if (!alive(gen))
            return;
        const QString out = QString::fromUtf8(r.stdout_data).trimmed(); // "uv 0.7.12 (dc3fd4647 2025-06-06)"
        const QString want = QStringLiteral("uv %1").arg(QLatin1String(kUvVersion));
        const bool right_version = out == want || out.startsWith(want + QLatin1Char(' '));
        if (r.ok() && right_version) {
            set_stage_fraction(SetupStage::Runtime, 0.3);
            update_stage(SetupStage::Runtime, [](SetupStageInfo& s) {
                s.percent = -1;
                s.detail = tr("uv %1 ready").arg(QLatin1String(kUvVersion));
            });
            if (base_python_path().isEmpty())
                when_network_ready([this, gen]() {
                    if (alive(gen))
                        runtime_install_python();
                });
            else
                runtime_done();
            return;
        }
        if (after_install) {
            runtime_failed(r.ok() ? tr("The downloaded uv reports \"%1\" instead of uv %2.").arg(out, QLatin1String(kUvVersion))
                                  : describe_failure(tr("Running the downloaded uv"), r));
            return;
        }
        // A truncated binary, a lost +x bit or another uv version: replace it.
        LOG_WARN(kPsmTag, QString("Existing uv unusable or not %1 (%2) — re-downloading")
                              .arg(QLatin1String(kUvVersion), out.left(80)));
        runtime_download_uv();
    });
}

void PythonSetupManager::runtime_download_uv() {
    const quint64 gen = run_->generation;
    run_->did_work = true;
    run_->runtime_step_key = QStringLiteral("uv");
    const PsmUvAsset asset = psm_uv_asset();
    const QString dir = install_dir() + "/uv";
    if (!QDir().mkpath(dir)) {
        runtime_failed(tr("Cannot create %1 — check disk space and folder permissions.").arg(QDir::toNativeSeparators(dir)));
        return;
    }
    run_->dl_archive = dir + "/" + QLatin1String(asset.archive);
    const QString part = run_->dl_archive + ".part";
    QFile::remove(run_->dl_archive);
    QFile::remove(part);
    run_->dl_file = std::make_unique<QFile>(part);
    if (!run_->dl_file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        runtime_failed(tr("Cannot write %1: %2").arg(QDir::toNativeSeparators(part), run_->dl_file->errorString()));
        return;
    }
    run_->dl_hash = std::make_unique<QCryptographicHash>(QCryptographicHash::Sha256);
    run_->dl_write_failed = false;
    run_->dl_bytes = 0;
    run_->dl_total = 0;

    if (!nam_)
        nam_ = new QNetworkAccessManager(this);
    nam_->setProxy(psm_proxy_from_env(QProcessEnvironment::systemEnvironment()));

    const QUrl url(QStringLiteral("https://github.com/astral-sh/uv/releases/download/%1/%2")
                       .arg(QLatin1String(kUvVersion), QLatin1String(asset.archive)));
    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setTransferTimeout(kPsmDownloadIdleTimeoutMs); // aborts only when no byte arrives for 60 s
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("FinceptTerminal/%1").arg(QCoreApplication::applicationVersion()));

    LOG_INFO(kPsmTag, "Downloading uv from " + url.toString());
    update_stage(SetupStage::Runtime, [](SetupStageInfo& s) {
        s.percent = 0;
        s.detail = tr("Downloading uv %1…").arg(QLatin1String(kUvVersion));
    });

    QNetworkReply* reply = nam_->get(req);
    download_reply_ = reply;
    connect(reply, &QNetworkReply::readyRead, this, [this, reply, gen]() {
        if (!alive(gen) || !run_->dl_file)
            return;
        const QByteArray chunk = reply->readAll();
        run_->dl_hash->addData(chunk);
        if (run_->dl_file->write(chunk) != chunk.size() && !run_->dl_write_failed) {
            run_->dl_write_failed = true;
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::downloadProgress, this, [this, gen](qint64 got, qint64 total) {
        if (!alive(gen))
            return;
        run_->dl_bytes = got;
        run_->dl_total = total;
        const int pct = total > 0 ? static_cast<int>((100 * got) / total) : -1;
        if (total > 0)
            set_stage_fraction(SetupStage::Runtime, 0.3 * static_cast<double>(got) / static_cast<double>(total));
        update_stage(SetupStage::Runtime, [got, total, pct](SetupStageInfo& s) {
            s.percent = pct;
            s.detail = total > 0 ? tr("Downloading uv %1 — %2 of %3")
                                       .arg(QLatin1String(kUvVersion), psm_fmt_bytes(got), psm_fmt_bytes(total))
                                 : tr("Downloading uv %1 — %2").arg(QLatin1String(kUvVersion), psm_fmt_bytes(got));
        });
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, gen, part]() {
        reply->deleteLater();
        if (download_reply_ == reply)
            download_reply_ = nullptr;
        if (!alive(gen)) {
            if (run_ && run_->dl_file && run_->generation == gen)
                run_->dl_file->close();
            QFile::remove(part);
            return;
        }
        const QByteArray rest = reply->readAll();
        if (!rest.isEmpty() && run_->dl_file) {
            run_->dl_hash->addData(rest);
            if (run_->dl_file->write(rest) != rest.size())
                run_->dl_write_failed = true;
        }
        const QString write_error = run_->dl_file ? run_->dl_file->errorString() : QString();
        if (run_->dl_file)
            run_->dl_file->close();
        const int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (run_->dl_write_failed) {
            QFile::remove(part);
            runtime_failed(tr("Writing the uv download failed (%1). Check free disk space.").arg(write_error));
            return;
        }
        if (reply->error() != QNetworkReply::NoError || (http != 0 && http != 200)) {
            QFile::remove(part);
            QString why = reply->errorString();
            if (http != 0 && http != 200)
                why += QStringLiteral(" (HTTP %1)").arg(http);
            runtime_download_uv_with_curl(why);
            return;
        }
        const QString got = QString::fromLatin1(run_->dl_hash->result().toHex());
        const PsmUvAsset a = psm_uv_asset();
        if (got.compare(QLatin1String(a.sha256), Qt::CaseInsensitive) != 0) {
            const qint64 size = QFileInfo(part).size();
            QFile::remove(part);
            runtime_failed(tr("The uv download failed its SHA-256 integrity check (%1 bytes received). This usually "
                              "means a captive portal or proxy returned a web page instead of the file — sign in to "
                              "the network, then retry.")
                               .arg(size));
            return;
        }
        QFile::remove(run_->dl_archive);
        if (!QFile::rename(part, run_->dl_archive)) {
            runtime_failed(tr("Cannot move the uv download into place (%1).").arg(QDir::toNativeSeparators(run_->dl_archive)));
            return;
        }
        runtime_extract_uv(run_->dl_archive, false);
    });
}

void PythonSetupManager::runtime_download_uv_with_curl(const QString& why) {
    LOG_WARN(kPsmTag, "Qt download of uv failed (" + why + ") — retrying with curl");
    const quint64 gen = run_->generation;
    run_->dl_bytes = 0;
    run_->dl_total = 0;
    update_stage(SetupStage::Runtime, [](SetupStageInfo& s) {
        s.percent = -1;
        s.detail = tr("Direct download failed — retrying with curl…");
    });
    const PsmUvAsset asset = psm_uv_asset();
    const QString url = QStringLiteral("https://github.com/astral-sh/uv/releases/download/%1/%2")
                            .arg(QLatin1String(kUvVersion), QLatin1String(asset.archive));
    const QString part = run_->dl_archive + ".part";
#ifdef Q_OS_WIN
    const QString curl = QStringLiteral("curl.exe"); // ships with Windows 10 1803+
#else
    const QString curl = QStringLiteral("curl");
#endif
    StepOptions opts;
    opts.env = uv_environment(); // carries HTTPS_PROXY when a system proxy was detected
    opts.timeout_ms = kPsmCurlTimeoutMs;
    opts.local8bit = true;
    const QStringList args = {QStringLiteral("-L"),         QStringLiteral("--fail"),
                              QStringLiteral("-sS"),        QStringLiteral("--retry"),
                              QStringLiteral("3"),          QStringLiteral("--retry-delay"),
                              QStringLiteral("2"),          QStringLiteral("--connect-timeout"),
                              QStringLiteral("30"),         QStringLiteral("--max-time"),
                              QStringLiteral("900"),        QStringLiteral("-o"),
                              part,                         url};
    start_step(curl, args, opts, {}, [this, gen, why, part](const StepResult& r) {
        if (!alive(gen))
            return;
        if (!r.ok()) {
            QFile::remove(part);
            runtime_failed(tr("Could not download uv.\nDirect download: %1\n%2")
                               .arg(why, describe_failure(tr("curl fallback"), r)));
            return;
        }
        runtime_verify_archive(part);
    });
}

void PythonSetupManager::runtime_verify_archive(const QString& part_path) {
    const quint64 gen = run_->generation;
    update_stage(SetupStage::Runtime, [](SetupStageInfo& s) { s.detail = tr("Verifying download (SHA-256)…"); });
    PythonSetupManager* self = this; // outlives the global pool (see detect_system_proxy)
    (void)QtConcurrent::run([self, gen, part_path]() {
        QString digest;
        QFile f(part_path);
        if (f.open(QIODevice::ReadOnly)) {
            QCryptographicHash h(QCryptographicHash::Sha256);
            if (h.addData(&f))
                digest = QString::fromLatin1(h.result().toHex());
        }
        const qint64 size = QFileInfo(part_path).size();
        QMetaObject::invokeMethod(
            self,
            [self, gen, part_path, digest, size]() {
                if (!self->alive(gen))
                    return;
                const PsmUvAsset a = psm_uv_asset();
                if (digest.compare(QLatin1String(a.sha256), Qt::CaseInsensitive) != 0) {
                    QFile::remove(part_path);
                    self->runtime_failed(tr("The uv download failed its SHA-256 integrity check (%1 bytes received). "
                                            "This usually means a captive portal or proxy returned a web page "
                                            "instead of the file — sign in to the network, then retry.")
                                             .arg(size));
                    return;
                }
                QFile::remove(self->run_->dl_archive);
                if (!QFile::rename(part_path, self->run_->dl_archive)) {
                    self->runtime_failed(tr("Cannot move the uv download into place (%1).")
                                             .arg(QDir::toNativeSeparators(self->run_->dl_archive)));
                    return;
                }
                self->runtime_extract_uv(self->run_->dl_archive, false);
            },
            Qt::QueuedConnection);
    });
}

void PythonSetupManager::runtime_extract_uv(const QString& archive_path, bool use_powershell) {
    const quint64 gen = run_->generation;
    const QString dir = install_dir() + "/uv";
    update_stage(SetupStage::Runtime, [](SetupStageInfo& s) {
        s.percent = -1;
        s.detail = tr("Extracting uv…");
    });

    // Remove the old binaries first so a broken one can never survive the update.
#ifdef Q_OS_WIN
    const QStringList binaries = {QStringLiteral("uv.exe"), QStringLiteral("uvx.exe"), QStringLiteral("uvw.exe")};
#else
    const QStringList binaries = {QStringLiteral("uv"), QStringLiteral("uvx")};
#endif
    for (const QString& b : binaries)
        QFile::remove(dir + "/" + b);
    if (QFileInfo::exists(uv_path())) {
        runtime_failed(tr("The old uv binary at %1 is locked — another Fincept Terminal or uv process is still "
                          "running, or antivirus is holding it. Close it (or restart the computer), then retry.")
                           .arg(QDir::toNativeSeparators(uv_path())));
        return;
    }

    StepOptions opts;
    opts.env = uv_environment();
    opts.timeout_ms = kPsmExtractTimeoutMs;
    opts.local8bit = true;
    QString program;
    QStringList args;
#ifdef Q_OS_WIN
    if (use_powershell) {
        // Single-quoted PowerShell literals: a quote inside the path is doubled.
        QString a = QDir::toNativeSeparators(archive_path);
        QString d = QDir::toNativeSeparators(dir);
        a.replace(QLatin1Char('\''), QStringLiteral("''"));
        d.replace(QLatin1Char('\''), QStringLiteral("''"));
        program = QStringLiteral("powershell");
        args = {QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive"), QStringLiteral("-Command"),
                QStringLiteral("Expand-Archive -LiteralPath '%1' -DestinationPath '%2' -Force").arg(a, d)};
    } else {
        program = QStringLiteral("tar.exe"); // bsdtar, ships with Windows 10 1803+ and reads zip
        args = {QStringLiteral("-xf"), QDir::toNativeSeparators(archive_path), QStringLiteral("-C"),
                QDir::toNativeSeparators(dir)};
    }
#else
    Q_UNUSED(use_powershell);
    program = QStringLiteral("tar");
    args = {QStringLiteral("-xzf"), archive_path, QStringLiteral("-C"), dir, QStringLiteral("--strip-components=1")};
#endif
    start_step(program, args, opts, {}, [this, gen, archive_path, use_powershell, dir](const StepResult& r) {
        if (!alive(gen))
            return;
        if (!r.ok()) {
#ifdef Q_OS_WIN
            if (!use_powershell) {
                LOG_WARN(kPsmTag, "tar.exe could not extract uv — falling back to PowerShell Expand-Archive");
                runtime_extract_uv(archive_path, true);
                return;
            }
#else
            Q_UNUSED(use_powershell);
#endif
            runtime_failed(describe_failure(tr("Extracting uv"), r));
            return;
        }
        // Some archives wrap the binaries in a uv-<target>/ folder: lift them out
        // and remove the leftover folder.
        const QString nested = dir + "/" + QLatin1String(psm_uv_asset().folder);
        if (QFileInfo(nested).isDir()) {
            const QStringList inner = QDir(nested).entryList(QDir::Files);
            for (const QString& f : inner) {
                QFile::remove(dir + "/" + f);
                QFile::rename(nested + "/" + f, dir + "/" + f);
            }
            QDir(nested).removeRecursively();
        }
        QFile::remove(archive_path);
#ifndef Q_OS_WIN
        for (const QString& b : {QStringLiteral("uv"), QStringLiteral("uvx")}) {
            const QString p = dir + "/" + b;
            if (QFileInfo::exists(p))
                QFile::setPermissions(p, QFile::permissions(p) | QFileDevice::ExeOwner | QFileDevice::ExeGroup |
                                             QFileDevice::ExeOther);
        }
#endif
        if (!QFileInfo::exists(uv_path())) {
            runtime_failed(tr("uv was not found after extracting the archive (%1).").arg(QDir::toNativeSeparators(uv_path())));
            return;
        }
        LOG_INFO(kPsmTag, "uv installed at " + uv_path());
        runtime_check_uv_version(true);
    });
}

void PythonSetupManager::runtime_install_python() {
    const quint64 gen = run_->generation;
    run_->did_work = true;
    run_->runtime_step_key = QStringLiteral("python");
    update_stage(SetupStage::Runtime, [](SetupStageInfo& s) {
        s.percent = -1;
        s.detail = tr("Installing Python %1…").arg(QLatin1String(kPythonVersion));
    });

    QStringList args = {QStringLiteral("python"), QStringLiteral("install"), python_request()};
    // A cpython dir without its interpreter is debris from an interrupted run;
    // uv would otherwise consider it installed.
    const QStringList dirs = QDir(install_dir() + "/python")
                                 .entryList({QStringLiteral("cpython-%1-*").arg(QLatin1String(kPythonVersion))},
                                            QDir::Dirs | QDir::NoDotAndDotDot);
    if (!dirs.isEmpty())
        args << QStringLiteral("--reinstall");

    StepOptions opts;
    opts.env = uv_environment();
    opts.timeout_ms = kPsmPythonInstallTimeoutMs;
    start_step(
        uv_path(), args, opts,
        [this, gen](const QString& line, bool is_stderr) {
            if (!is_stderr || !alive(gen) || !psm_meaningful_line(line))
                return;
            const QString text = psm_pretty_uv_line(line);
            update_stage(SetupStage::Runtime, [text](SetupStageInfo& s) { s.detail = text; });
        },
        [this, gen](const StepResult& r) {
            if (!alive(gen))
                return;
            if (!r.ok()) {
                if (!run_->runtime_tls_retried && retry_with_native_tls(r)) {
                    run_->runtime_tls_retried = true;
                    runtime_install_python();
                    return;
                }
                runtime_failed(describe_failure(tr("Installing Python %1").arg(QLatin1String(kPythonVersion)), r));
                return;
            }
            if (base_python_path().isEmpty()) {
                runtime_failed(tr("Python %1 was installed but its interpreter is missing under %2.")
                                   .arg(QLatin1String(kPythonVersion), QDir::toNativeSeparators(install_dir() + "/python")));
                return;
            }
            LOG_INFO(kPsmTag, "Python installed at " + base_python_path());
            runtime_done();
        });
}

void PythonSetupManager::runtime_done() {
    const quint64 gen = run_->generation;
    set_stage_fraction(SetupStage::Runtime, 1.0);
    update_stage(
        SetupStage::Runtime,
        [](SetupStageInfo& s) {
            s.state = SetupStageState::Done;
            s.percent = 100;
            s.finished_ms = psm_now_ms();
            s.detail = tr("uv %1 · Python %2").arg(QLatin1String(kUvVersion), QLatin1String(kPythonVersion));
        },
        true);
    when_network_ready([this, gen]() {
        if (!alive(gen))
            return;
        // Analytics first: its core install is what lets the user in.
        if (run_->lanes[1].active)
            lane_start(1);
        if (run_ && run_->generation == gen && run_->lanes[0].active)
            lane_start(0);
        maybe_finish_run();
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// Package lanes
// ─────────────────────────────────────────────────────────────────────────────

void PythonSetupManager::lane_start(int lane) {
    Run::Lane& L = run_->lanes[static_cast<std::size_t>(lane)];
    const SetupStage stage = lane == 0 ? SetupStage::Trading : SetupStage::AnalyticsCore;
    update_stage(stage, [](SetupStageInfo& s) {
        s.state = SetupStageState::Running;
        s.started_ms = psm_now_ms();
        s.percent = -1;
    });
    if (find_requirements_file(L.req_file).isEmpty()) {
        lane_failed(lane, tr("Setup file missing: %1 — reinstall Fincept Terminal.").arg(L.req_file));
        return;
    }
    if (venv_valid(L.venv)) {
        L.fresh_venv = false;
        lane_seed_preferences(lane);
    } else {
        lane_create_venv(lane);
    }
}

void PythonSetupManager::lane_create_venv(int lane) {
    Run::Lane& L = run_->lanes[static_cast<std::size_t>(lane)];
    const quint64 gen = run_->generation;
    run_->did_work = true;
    const SetupStage stage = lane == 0 ? SetupStage::Trading : SetupStage::AnalyticsCore;
    update_stage(stage, [](SetupStageInfo& s) { s.detail = tr("Creating virtual environment…"); });

    const QString path = install_dir() + "/" + L.venv;
    // Markers live inside the venv; drop them so a recreated venv can never
    // inherit "installed" from the one it replaces.
    QFile::remove(psm_marker_path(install_dir(), L.venv));
    if (lane == 1)
        QFile::remove(psm_core_marker_path(install_dir()));

    const auto create = [this, gen, lane, path]() {
        if (!alive(gen))
            return;
        StepOptions opts;
        opts.env = uv_environment();
        opts.timeout_ms = kPsmVenvTimeoutMs;
        // The exact managed interpreter — never a version request that could match another one.
        const QStringList args = {QStringLiteral("venv"), path, QStringLiteral("--python"), base_python_path()};
        start_step(uv_path(), args, opts, {}, [this, gen, lane](const StepResult& r) {
            if (!alive(gen))
                return;
            Run::Lane& lane_ref = run_->lanes[static_cast<std::size_t>(lane)];
            if (!r.ok()) {
                lane_failed(lane, describe_failure(tr("Creating %1").arg(lane_ref.venv), r));
                return;
            }
            lane_ref.fresh_venv = true;
            set_stage_fraction(lane == 0 ? SetupStage::Trading : SetupStage::AnalyticsCore, lane == 0 ? 0.05 : 0.1);
            lane_seed_preferences(lane);
        });
    };

    // A directory without pyvenv.cfg is debris from an interrupted run (uv will
    // not overwrite a non-venv directory). Removing it can touch thousands of
    // files, so it runs on a worker thread.
    if (QFileInfo::exists(path) && !QFileInfo::exists(path + "/pyvenv.cfg")) {
        LOG_WARN(kPsmTag, "Removing a stale non-venv directory: " + path);
        PythonSetupManager* self = this; // outlives the global pool (see detect_system_proxy)
        (void)QtConcurrent::run([self, path, create]() {
            QDir(path).removeRecursively();
            QMetaObject::invokeMethod(self, create, Qt::QueuedConnection);
        });
        return;
    }
    create();
}

void PythonSetupManager::lane_seed_preferences(int lane) {
    Run::Lane& L = run_->lanes[static_cast<std::size_t>(lane)];
    const quint64 gen = run_->generation;
    const QString lock = lock_path(L.venv);
    if (L.fresh_venv) {
        QFile::remove(lock); // empty venv: resolve fresh
        lane_compile(lane);
        return;
    }
    // Existing venv: seed the lock with what is installed. `uv pip compile`
    // prefers the pins of an existing output file, so a sync only changes what
    // the requirements force — no surprise upgrades of a working install, and
    // nothing replaced underneath a running terminal.
    const SetupStage stage = lane == 0 ? SetupStage::Trading : SetupStage::AnalyticsCore;
    update_stage(stage, [](SetupStageInfo& s) { s.detail = tr("Reading installed packages…"); });
    StepOptions opts;
    opts.env = uv_environment();
    opts.timeout_ms = kPsmFreezeTimeoutMs;
    opts.capture_stdout = true;
    const QStringList args = {QStringLiteral("pip"), QStringLiteral("freeze"), QStringLiteral("--python"),
                              python_path(L.venv)};
    start_step(uv_path(), args, opts, {}, [this, gen, lane, lock](const StepResult& r) {
        if (!alive(gen))
            return;
        // Only plain `name==version` pins: anything else (editable / URL installs)
        // would make compile reject the whole output file.
        static const QRegularExpression kPin(QStringLiteral(R"(^[A-Za-z0-9][A-Za-z0-9._-]*==[^\s;#]+$)"));
        QStringList pins;
        if (r.ok()) {
            const QStringList lines = QString::fromUtf8(r.stdout_data).split(QLatin1Char('\n'));
            for (const QString& raw : lines) {
                const QString line = raw.trimmed();
                if (kPin.match(line).hasMatch())
                    pins << line;
            }
        } else {
            LOG_WARN(kPsmTag, "uv pip freeze failed — resolving without preferences");
        }
        if (pins.isEmpty())
            QFile::remove(lock);
        else if (!psm_write_atomic(lock, (pins.join(QLatin1Char('\n')) + QLatin1Char('\n')).toUtf8()))
            QFile::remove(lock);
        lane_compile(lane);
    });
}

void PythonSetupManager::lane_compile(int lane) {
    Run::Lane& L = run_->lanes[static_cast<std::size_t>(lane)];
    const quint64 gen = run_->generation;
    const SetupStage stage = lane == 0 ? SetupStage::Trading : SetupStage::AnalyticsCore;
    L.progress = PsmUvProgress{};
    update_stage(stage, [](SetupStageInfo& s) {
        s.percent = -1;
        s.detail = tr("Resolving dependencies…");
    });
    const QString lock = lock_path(L.venv);
    StepOptions opts;
    opts.env = uv_environment();
    opts.timeout_ms = kPsmCompileTimeoutMs;
    const QStringList args = {QStringLiteral("pip"),
                              QStringLiteral("compile"),
                              find_requirements_file(L.req_file),
                              QStringLiteral("--python"),
                              python_path(L.venv),
                              QStringLiteral("-o"),
                              lock,
                              QStringLiteral("--no-header"),
                              QStringLiteral("--no-annotate"),
                              QStringLiteral("-v")};
    start_step(
        uv_path(), args, opts,
        [this, gen, lane, stage](const QString& line, bool is_stderr) {
            if (!is_stderr || !alive(gen))
                return;
            Run::Lane& lr = run_->lanes[static_cast<std::size_t>(lane)];
            if (!lr.progress.feed(line))
                return;
            const int selected = lr.progress.selected;
            const int resolved = lr.progress.resolved;
            update_stage(stage, [selected, resolved](SetupStageInfo& s) {
                s.detail = resolved > 0 ? tr("Resolved %1 packages").arg(resolved)
                                        : tr("Resolving dependencies — %1 packages").arg(selected);
            });
        },
        [this, gen, lane, stage, lock](const StepResult& r) {
            if (!alive(gen))
                return;
            Run::Lane& lr = run_->lanes[static_cast<std::size_t>(lane)];
            if (r.ok() && QFileInfo(lock).size() > 0) {
                lr.locked = true;
                set_stage_fraction(stage, lane == 0 ? 0.15 : 0.4);
                const QString core_path = lane == 1 ? find_requirements_file(lr.core_file) : QString();
                if (lane == 1 && !core_path.isEmpty()) {
                    lane_install_core(lane);
                } else {
                    if (lane == 1)
                        LOG_WARN(kPsmTag, "requirements-numpy2-core.txt not bundled — installing the full set first");
                    lane_install_full(lane);
                }
                return;
            }
            if (!lr.tls_retried && retry_with_native_tls(r)) {
                lr.tls_retried = true;
                lane_compile(lane);
                return;
            }
            // Resolution failed (or produced nothing): install straight from the
            // requirements file. uv's installer resolves too, so this both retries
            // a transient failure and surfaces the real error if it is not transient.
            LOG_WARN(kPsmTag, QString("[%1] uv pip compile failed — falling back to a direct install").arg(lr.venv));
            lr.fallback = true;
            lr.locked = false;
            QFile::remove(lock);
            if (lane == 1)
                open_trading_gate(); // no early core without a lock — don't hold the trading lane back
            lane_install_full(lane);
        });
}

void PythonSetupManager::lane_install_core(int lane) {
    Run::Lane& L = run_->lanes[static_cast<std::size_t>(lane)];
    const quint64 gen = run_->generation;
    run_->did_work = true;
    L.progress = PsmUvProgress{};
    update_stage(SetupStage::AnalyticsCore, [](SetupStageInfo& s) {
        s.percent = -1;
        s.detail = tr("Installing core libraries…");
    });
    StepOptions opts;
    opts.env = uv_environment();
    opts.timeout_ms = kPsmInstallTimeoutMs;
    // uv 0.7.x mis-parses a constraints/requirements path containing a space
    // ("Application Support" on macOS): run from the venv dir, pass the lock relatively.
    opts.working_dir = QFileInfo(lock_path(L.venv)).absolutePath();
    // No --compile-bytecode here: this is the launch-critical path. The full pass
    // compiles the whole site-packages (core included) in the background.
    const QStringList args = {QStringLiteral("pip"),
                              QStringLiteral("install"),
                              QStringLiteral("--python"),
                              python_path(L.venv),
                              QStringLiteral("-r"),
                              find_requirements_file(L.core_file),
                              QStringLiteral("-c"),
                              QFileInfo(lock_path(L.venv)).fileName(),
                              QStringLiteral("-v")};
    start_step(
        uv_path(), args, opts,
        [this, gen, lane](const QString& line, bool is_stderr) {
            if (!is_stderr || !alive(gen))
                return;
            Run::Lane& lr = run_->lanes[static_cast<std::size_t>(lane)];
            if (!lr.progress.feed(line))
                return;
            const PsmUvProgress& p = lr.progress;
            const int pct = p.prepare_percent();
            if (pct >= 0)
                set_stage_fraction(SetupStage::AnalyticsCore, 0.4 + 0.55 * pct / 100.0);
            const QString text = p.phase == PsmUvProgress::Phase::Preparing && !p.uncached.isEmpty()
                                     ? tr("Downloading %1 / %2 packages").arg(p.prepared.size()).arg(p.uncached.size())
                                 : p.phase == PsmUvProgress::Phase::Installing || p.phase == PsmUvProgress::Phase::Compiling
                                     ? tr("Installing %1 packages…").arg(p.resolved)
                                     : p.headline;
            const QString headline = p.headline;
            update_stage(SetupStage::AnalyticsCore, [pct, text, headline](SetupStageInfo& s) {
                s.percent = pct;
                s.detail = (pct >= 0 && (headline.startsWith(QLatin1String("Downloading ")) ||
                                         headline.startsWith(QLatin1String("Building "))))
                               ? text + QStringLiteral(" · ") + headline
                               : text;
            });
        },
        [this, gen, lane](const StepResult& r) {
            if (!alive(gen))
                return;
            Run::Lane& lr = run_->lanes[static_cast<std::size_t>(lane)];
            if (!r.ok()) {
                if (!lr.tls_retried && retry_with_native_tls(r)) {
                    lr.tls_retried = true;
                    lane_install_core(lane);
                    return;
                }
                lane_failed(lane, describe_failure(tr("Installing the analytics core"), r));
                return;
            }
            const QString marker = expected_marker(lr.core_file);
            if (!marker.isEmpty() && !psm_write_atomic(psm_core_marker_path(install_dir()), (marker + "\n").toUtf8()))
                LOG_WARN(kPsmTag, "Could not write the core marker");
            lr.core_done = true;
            set_stage_fraction(SetupStage::AnalyticsCore, 1.0);
            const int resolved = lr.progress.resolved;
            update_stage(
                SetupStage::AnalyticsCore,
                [resolved](SetupStageInfo& s) {
                    s.state = SetupStageState::Done;
                    s.percent = 100;
                    s.finished_ms = psm_now_ms();
                    s.detail = tr("%1 core packages ready").arg(resolved);
                },
                true);
            core_ready_.store(true);
            {
                QMutexLocker lock(&mutex_);
                snapshot_.core_ready = true;
            }
            LOG_INFO(kPsmTag, "Analytics core ready — the terminal can launch");
            emit core_ready();
            open_trading_gate();
            if (alive(gen))
                lane_install_full(lane);
        });
}

void PythonSetupManager::lane_install_full(int lane) {
    Run::Lane& L = run_->lanes[static_cast<std::size_t>(lane)];
    const quint64 gen = run_->generation;
    if (lane == 0 && !run_->trading_gate_open) {
        // Priority, not a barrier: the trading lane starts downloading as soon as
        // the analytics core (the launch-critical ~40 packages) is in, or as soon
        // as the analytics lane fails — never later.
        L.waiting_gate = true;
        const int resolved = L.progress.resolved;
        update_stage(SetupStage::Trading, [resolved](SetupStageInfo& s) {
            s.detail = resolved > 0 ? tr("Resolved %1 packages — queued until the analytics core is installed").arg(resolved)
                                    : tr("Queued until the analytics core is installed");
        });
        return;
    }
    L.waiting_gate = false;
    run_->did_work = true;
    const SetupStage stage = lane == 0 ? SetupStage::Trading : SetupStage::AnalyticsFull;
    if (lane == 1) {
        update_stage(SetupStage::AnalyticsFull, [](SetupStageInfo& s) {
            s.state = SetupStageState::Running;
            if (s.started_ms == 0)
                s.started_ms = psm_now_ms();
            s.percent = -1;
        });
        if (L.fallback)
            update_stage(SetupStage::AnalyticsCore,
                         [](SetupStageInfo& s) { s.detail = tr("Installing together with the full analytics set…"); });
    }
    L.progress = PsmUvProgress{};
    update_stage(stage, [](SetupStageInfo& s) {
        s.percent = -1;
        s.detail = tr("Preparing installation…");
    });

    StepOptions opts;
    opts.env = uv_environment();
    opts.timeout_ms = kPsmInstallTimeoutMs;
    opts.background = true; // keep the terminal responsive while this runs
    // --compile-bytecode: PythonRunner sets PYTHONDONTWRITEBYTECODE=1, so .pyc
    // files are never written at runtime — without precompiling, every script
    // run would recompile pandas & co. from source (~0.6 s per process).
    // See lane_install_core: run from the venv dir so the lock is passed without spaces.
    if (!L.fallback)
        opts.working_dir = QFileInfo(lock_path(L.venv)).absolutePath();
    const QString source = L.fallback ? find_requirements_file(L.req_file) : QFileInfo(lock_path(L.venv)).fileName();
    const QStringList args = {QStringLiteral("pip"),
                              QStringLiteral("install"),
                              QStringLiteral("--python"),
                              python_path(L.venv),
                              QStringLiteral("-r"),
                              source,
                              QStringLiteral("--compile-bytecode"),
                              QStringLiteral("-v")};
    const double base = lane == 0 ? 0.15 : 0.0;
    const double span = lane == 0 ? 0.8 : 0.95;
    start_step(
        uv_path(), args, opts,
        [this, gen, lane, stage, base, span](const QString& line, bool is_stderr) {
            if (!is_stderr || !alive(gen))
                return;
            Run::Lane& lr = run_->lanes[static_cast<std::size_t>(lane)];
            if (!lr.progress.feed(line))
                return;
            const PsmUvProgress& p = lr.progress;
            const int pct = p.prepare_percent();
            if (pct >= 0)
                set_stage_fraction(stage, base + span * pct / 100.0);
            QString text;
            switch (p.phase) {
                case PsmUvProgress::Phase::Starting:
                case PsmUvProgress::Phase::Resolving:
                    text = tr("Resolving — %1 packages").arg(p.selected);
                    break;
                case PsmUvProgress::Phase::Preparing:
                    text = p.uncached.isEmpty()
                               ? tr("Checking %1 packages…").arg(p.resolved)
                               : tr("Downloading %1 / %2 packages").arg(p.prepared.size()).arg(p.uncached.size());
                    break;
                case PsmUvProgress::Phase::Installing:
                    text = tr("Installing %1 packages…").arg(p.uncached.isEmpty() ? p.resolved : static_cast<int>(p.uncached.size()));
                    break;
                case PsmUvProgress::Phase::Compiling:
                    text = tr("Compiling bytecode…");
                    break;
                case PsmUvProgress::Phase::Finished:
                    text = p.headline;
                    break;
            }
            const QString headline = p.headline;
            const bool show_headline = p.phase == PsmUvProgress::Phase::Preparing &&
                                       (headline.startsWith(QLatin1String("Downloading ")) ||
                                        headline.startsWith(QLatin1String("Building ")) ||
                                        headline.startsWith(QLatin1String("Waiting ")));
            update_stage(stage, [pct, text, headline, show_headline](SetupStageInfo& s) {
                s.percent = pct;
                s.detail = show_headline ? text + QStringLiteral(" · ") + headline : text;
            });
        },
        [this, gen, lane](const StepResult& r) {
            if (!alive(gen))
                return;
            Run::Lane& lr = run_->lanes[static_cast<std::size_t>(lane)];
            if (!r.ok()) {
                if (!lr.tls_retried && retry_with_native_tls(r)) {
                    lr.tls_retried = true;
                    lane_install_full(lane);
                    return;
                }
                lane_failed(lane, describe_failure(lane == 0 ? tr("Installing the trading libraries")
                                                             : tr("Installing the analytics libraries"),
                                                   r));
                return;
            }
            const QString marker = expected_marker(lr.req_file);
            if (!marker.isEmpty() &&
                !psm_write_atomic(psm_marker_path(install_dir(), lr.venv), (marker + "\n").toUtf8()))
                LOG_WARN(kPsmTag, "Could not write the package marker for " + lr.venv);
            if (lane == 1 && !lr.core_done) {
                // Fallback / no core file: everything (core included) is in now.
                const QString core_marker = expected_marker(lr.core_file);
                if (!core_marker.isEmpty())
                    psm_write_atomic(psm_core_marker_path(install_dir()), (core_marker + "\n").toUtf8());
                lr.core_done = true;
                set_stage_fraction(SetupStage::AnalyticsCore, 1.0);
                update_stage(SetupStage::AnalyticsCore, [](SetupStageInfo& s) {
                    s.state = SetupStageState::Done;
                    s.percent = 100;
                    s.finished_ms = psm_now_ms();
                    s.detail = tr("Core libraries ready");
                });
                core_ready_.store(true);
                {
                    QMutexLocker lock(&mutex_);
                    snapshot_.core_ready = true;
                }
                emit core_ready();
                if (!alive(gen))
                    return;
            }
            lane_succeeded(lane);
        });
}

void PythonSetupManager::lane_succeeded(int lane) {
    Run::Lane& L = run_->lanes[static_cast<std::size_t>(lane)];
    L.succeeded = true;
    const SetupStage stage = lane == 0 ? SetupStage::Trading : SetupStage::AnalyticsFull;
    set_stage_fraction(stage, 1.0);
    const int resolved = L.progress.resolved;
    update_stage(
        stage,
        [resolved](SetupStageInfo& s) {
            s.state = SetupStageState::Done;
            s.percent = 100;
            s.finished_ms = psm_now_ms();
            s.detail = resolved > 0 ? tr("%1 packages ready").arg(resolved) : tr("Up to date");
        },
        true);
    LOG_INFO(kPsmTag, QString("[%1] lane complete").arg(L.venv));
    maybe_finish_run();
}

void PythonSetupManager::lane_failed(int lane, const QString& error) {
    Run::Lane& L = run_->lanes[static_cast<std::size_t>(lane)];
    if (L.failed || L.succeeded)
        return;
    L.failed = true;
    L.waiting_gate = false;
    const QString label = lane == 0 ? tr("Trading libraries") : (L.core_done ? tr("Analytics libraries") : tr("Analytics core"));
    L.error = label + QStringLiteral(": ") + error;
    LOG_ERROR(kPsmTag, QString("[%1] %2").arg(L.venv, error));

    const SetupStage failed_stage =
        lane == 0 ? SetupStage::Trading : (L.core_done ? SetupStage::AnalyticsFull : SetupStage::AnalyticsCore);
    update_stage(
        failed_stage,
        [&error](SetupStageInfo& s) {
            s.state = SetupStageState::Failed;
            s.error = error;
            s.detail = error.section(QLatin1Char('\n'), 0, 0);
            s.finished_ms = psm_now_ms();
        },
        true);
    if (lane == 1 && !L.core_done) {
        const bool combined = L.fallback; // the full row was running the combined install
        update_stage(SetupStage::AnalyticsFull, [&error, combined](SetupStageInfo& s) {
            if (combined) {
                s.state = SetupStageState::Failed;
                s.error = error;
                s.detail = error.section(QLatin1Char('\n'), 0, 0);
                s.finished_ms = psm_now_ms();
            } else {
                s.state = SetupStageState::Waiting;
                s.detail = tr("Not started — the analytics core did not install");
            }
        });
    }
    if (lane == 1)
        open_trading_gate(); // a failed analytics lane must never hold the trading lane back
    maybe_finish_run();
}

void PythonSetupManager::open_trading_gate() {
    if (!run_ || run_->trading_gate_open)
        return;
    run_->trading_gate_open = true;
    Run::Lane& trading = run_->lanes[0];
    if (trading.waiting_gate && !trading.failed && !run_->cancelled && !run_->finished)
        lane_install_full(0);
}

} // namespace fincept::python
