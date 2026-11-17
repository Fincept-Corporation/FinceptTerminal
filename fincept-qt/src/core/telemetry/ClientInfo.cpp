#include "core/telemetry/ClientInfo.h"

#include "core/config/AppSettings.h"
#include "services/updater/InstallContext.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QLocale>
#include <QNetworkRequest>
#include <QScreen>
#include <QSettings>
#include <QSysInfo>
#include <QThread>
#include <QThreadPool>
#include <QTimeZone>
#include <QUuid>

#if defined(Q_OS_WIN)
#    include <windows.h>
#elif defined(Q_OS_MACOS)
#    include <sys/sysctl.h>
#    include <sys/types.h>
#elif defined(Q_OS_UNIX)
#    include <unistd.h>
#endif

// Helpers are prefixed ci_ (unity builds batch this file with others).

namespace fincept::telemetry {

namespace {

/// Facts that do not change while the process runs.
struct ClientFacts {
    QString app_version;
    QString platform;
    QString install_kind;
    QString os_name;
    QString os_version;
    QString kernel;
    QString arch;
    QString qt_version;
    QString locale;
    QString timezone;
    int cpu_cores = 0;
    int memory_gb = 0; ///< 0 = not known (the field is then left out)
    QByteArray user_agent;
};

QString ci_app_version() {
    const QString v = QCoreApplication::applicationVersion();
#ifdef FINCEPT_VERSION_STRING
    return v.isEmpty() ? QStringLiteral(FINCEPT_VERSION_STRING) : v;
#else
    return v.isEmpty() ? QStringLiteral("0.0.0") : v;
#endif
}

QString ci_install_kind(services::updater::InstallKind kind) {
    using services::updater::InstallKind;
    switch (kind) {
        case InstallKind::WindowsIfw:
            return QStringLiteral("ifw");
        case InstallKind::MacAppBundle:
            return QStringLiteral("dmg");
        case InstallKind::LinuxAppImage:
            return QStringLiteral("appimage");
        case InstallKind::LinuxDeb:
            return QStringLiteral("deb");
        case InstallKind::LinuxRpm:
            return QStringLiteral("rpm");
        case InstallKind::LinuxFlatpak:
            return QStringLiteral("flatpak");
        case InstallKind::Unknown:
            break;
    }
    return QStringLiteral("source"); // dev build, unpacked tree
}

/// Printable ASCII without the User-Agent's own delimiters ";()".
QString ci_ua_token(const QString& s) {
    QString out;
    out.reserve(s.size());
    for (const QChar c : s) {
        const char16_t u = c.unicode();
        if (u < 0x20 || u > 0x7e || u == u';' || u == u'(' || u == u')')
            continue;
        out.append(c);
    }
    return out.simplified();
}

/// Installed physical memory rounded to whole GiB; 0 when it cannot be read.
int ci_memory_gb() {
    quint64 bytes = 0;
#if defined(Q_OS_WIN)
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status))
        bytes = status.ullTotalPhys;
#elif defined(Q_OS_MACOS)
    quint64 mem = 0;
    size_t len = sizeof(mem);
    if (sysctlbyname("hw.memsize", &mem, &len, nullptr, 0) == 0)
        bytes = mem;
#elif defined(Q_OS_UNIX)
    const long pages = sysconf(_SC_PHYS_PAGES);
    const long page_size = sysconf(_SC_PAGE_SIZE);
    if (pages > 0 && page_size > 0)
        bytes = static_cast<quint64>(pages) * static_cast<quint64>(page_size);
#endif
    constexpr quint64 kGiB = 1024ULL * 1024ULL * 1024ULL;
    return bytes == 0 ? 0 : static_cast<int>((bytes + kGiB / 2) / kGiB);
}

ClientFacts ci_build_facts() {
    ClientFacts f;
    // May run dpkg-query / rpm once (Linux package installs) — see prime().
    const services::updater::InstallContext& ctx = services::updater::install_context();
    f.app_version = ci_app_version();
    f.platform = ctx.platform_key.isEmpty() ? QStringLiteral("unknown") : ctx.platform_key;
    f.install_kind = ci_install_kind(ctx.kind);

    // "Windows 11 Version 23H2" → "Windows 11"; "macOS Sonoma (14.5)" → "macOS Sonoma".
    const QString pretty = QSysInfo::prettyProductName();
    QString name = pretty;
    if (const qsizetype paren = name.indexOf(QLatin1Char('(')); paren > 0)
        name.truncate(paren);
    if (const qsizetype ver = name.indexOf(QLatin1String(" Version ")); ver > 0)
        name.truncate(ver);
    f.os_name = name.trimmed();
#if defined(Q_OS_WIN)
    f.os_version = QSysInfo::kernelVersion(); // "10.0.22631" — productVersion() is just "11"
#else
    f.os_version = QSysInfo::productVersion();
#endif
    f.kernel = QSysInfo::kernelType() + QLatin1Char(' ') + QSysInfo::kernelVersion();
    f.arch = QSysInfo::currentCpuArchitecture();
    f.qt_version = QString::fromLatin1(qVersion());
    f.locale = QLocale::system().name();
    f.timezone = QString::fromUtf8(QTimeZone::systemTimeZoneId());
    f.cpu_cores = QThread::idealThreadCount();
    f.memory_gb = ci_memory_gb();

    const QString ua = QStringLiteral("FinceptTerminal/%1 (%2; %3; %4; Qt %5)")
                           .arg(ci_ua_token(f.app_version), ci_ua_token(f.platform), f.install_kind,
                                ci_ua_token(pretty), ci_ua_token(f.qt_version));
    f.user_agent = ua.toLatin1();
    return f;
}

const ClientFacts& ci_facts() {
    static const ClientFacts facts = ci_build_facts(); // thread-safe one-time init
    return facts;
}

} // namespace

void ClientInfo::prime() {
    QThreadPool::globalInstance()->start([]() {
        (void)ci_facts();
        (void)ClientInfo::install_id();
    });
}

QByteArray ClientInfo::user_agent() {
    return ci_facts().user_agent;
}

QString ClientInfo::install_id() {
    static const QString id = []() {
        QSettings settings = app_settings::open();
        const QString key = QStringLiteral("usage/install_id");
        const QUuid stored = QUuid::fromString(settings.value(key).toString());
        if (!stored.isNull())
            return stored.toString(QUuid::WithoutBraces);
        const QString fresh = QUuid::createUuid().toString(QUuid::WithoutBraces);
        settings.setValue(key, fresh);
        return fresh;
    }();
    return id;
}

QString ClientInfo::run_id() {
    static const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    return id;
}

void ClientInfo::apply_headers(QNetworkRequest& request) {
    request.setHeader(QNetworkRequest::UserAgentHeader, user_agent());
    request.setRawHeader("X-Fincept-Install", install_id().toLatin1());
    request.setRawHeader("X-Fincept-Run", run_id().toLatin1());
}

QJsonObject ClientInfo::client_json() {
    const ClientFacts& f = ci_facts();
    QJsonObject o{
        {QStringLiteral("app_version"), f.app_version},
        {QStringLiteral("platform"), f.platform},
        {QStringLiteral("install_kind"), f.install_kind},
        {QStringLiteral("os_name"), f.os_name},
        {QStringLiteral("os_version"), f.os_version},
        {QStringLiteral("kernel"), f.kernel},
        {QStringLiteral("arch"), f.arch},
        {QStringLiteral("qt_version"), f.qt_version},
        {QStringLiteral("locale"), f.locale},
        {QStringLiteral("timezone"), f.timezone},
        {QStringLiteral("cpu_cores"), f.cpu_cores},
    };
    if (f.memory_gb > 0)
        o.insert(QStringLiteral("memory_gb"), f.memory_gb);
    // Monitors can come and go while the terminal runs: read them now.
    if (qobject_cast<QGuiApplication*>(QCoreApplication::instance())) {
        o.insert(QStringLiteral("screens"), static_cast<int>(QGuiApplication::screens().size()));
        if (const QScreen* primary = QGuiApplication::primaryScreen()) {
            const QSize size = primary->size();
            const qreal dpr = primary->devicePixelRatio();
            o.insert(QStringLiteral("primary_resolution"),
                     QStringLiteral("%1x%2").arg(qRound(size.width() * dpr)).arg(qRound(size.height() * dpr)));
        }
    }
    return o;
}

} // namespace fincept::telemetry
