// HarnessDetect.cpp — pure search logic for harness CLIs (see HarnessDetect.h).
// QtCore only; every file-system access goes through the DetectContext.

#include "services/harness/HarnessDetect.h"

#include "services/harness/IHarnessAdapter.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>

namespace fincept::harness {

namespace harness_detect_detail {

constexpr qint64 kMaxShimBytes = 64 * 1024; // npm shims are < 1 KB
constexpr int kMaxNvmVersions = 8;

bool is_absolute(const QString& p, bool windows) {
    if (windows) {
        return (p.size() >= 3 && p.at(0).isLetter() && p.at(1) == QLatin1Char(':') && p.at(2) == QLatin1Char('/')) ||
               p.startsWith(QLatin1String("//"));
    }
    return p.startsWith(QLatin1Char('/'));
}

QString path_key(const QString& p, bool windows) {
    return windows ? p.toLower() : p;
}

QString env_dir(const DetectContext& ctx, const char* name) {
    return normalize_detect_path(ctx.env_value(QString::fromLatin1(name)), ctx.windows);
}

QString under(const QString& base, const char* rel) {
    return base.isEmpty() ? QString() : base + QLatin1Char('/') + QLatin1String(rel);
}

/// "v20.11.1" → {20, 11, 1}; non-numeric parts count as 0.
QVector<int> version_key(const QString& name) {
    QString s = name;
    if (s.startsWith(QLatin1Char('v')) || s.startsWith(QLatin1Char('V')))
        s.remove(0, 1);
    QVector<int> out;
    for (const QString& part : s.split(QLatin1Char('.'))) {
        bool ok = false;
        const int n = part.toInt(&ok);
        out.append(ok ? n : 0);
    }
    return out;
}

/// ~/.nvm/versions/node/<v> names, newest first.
QStringList nvm_versions_newest_first(const DetectContext& ctx) {
    if (ctx.home.isEmpty() || !ctx.list_dir)
        return {};
    QStringList names = ctx.list_dir(ctx.home + QStringLiteral("/.nvm/versions/node"), QStringLiteral("v*"));
    std::stable_sort(names.begin(), names.end(), [](const QString& a, const QString& b) {
        const QVector<int> ka = version_key(a);
        const QVector<int> kb = version_key(b);
        return std::lexicographical_compare(kb.cbegin(), kb.cend(), ka.cbegin(), ka.cend()); // descending
    });
    if (names.size() > kMaxNvmVersions)
        names = names.mid(0, kMaxNvmVersions);
    return names;
}

/// The node executable an npm shim falls back to when it has none beside it.
QString find_node(const DetectContext& ctx) {
    const QString exe = ctx.windows ? QStringLiteral("node.exe") : QStringLiteral("node");
    QStringList dirs = split_search_path(ctx.env_value(QStringLiteral("PATH")), ctx.windows);
    if (ctx.windows) {
        dirs << env_dir(ctx, "NVM_SYMLINK");
        const QString pf = env_dir(ctx, "ProgramFiles");
        if (!pf.isEmpty())
            dirs << pf + QStringLiteral("/nodejs");
    }
    for (const QString& d : std::as_const(dirs)) {
        if (d.isEmpty())
            continue;
        const QString p = d + QLatin1Char('/') + exe;
        if (ctx.executable(p))
            return p;
    }
    return {};
}

bool is_node_script(const QString& path) {
    const QString lower = path.toLower();
    return lower.endsWith(QLatin1String(".js")) || lower.endsWith(QLatin1String(".cjs")) ||
           lower.endsWith(QLatin1String(".mjs"));
}

} // namespace harness_detect_detail

using namespace harness_detect_detail;

DetectContext make_system_detect_context(const QString& override_path) {
    DetectContext ctx;
    ctx.home = QDir::homePath();
    ctx.env = QProcessEnvironment::systemEnvironment();
    ctx.override_path = override_path;
    ctx.is_file = [](const QString& p) { return QFileInfo(p).isFile(); };
#ifdef Q_OS_WIN
    ctx.is_executable = [](const QString& p) { return QFileInfo(p).isFile(); };
#else
    ctx.is_executable = [](const QString& p) {
        const QFileInfo fi(p);
        return fi.isFile() && fi.isExecutable();
    };
#endif
    ctx.list_dir = [](const QString& dir, const QString& glob) {
        return QDir(dir).entryList(QStringList{glob}, QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    };
    ctx.read_file = [](const QString& path, qint64 max_bytes) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly))
            return QByteArray();
        return f.read(max_bytes);
    };
    return ctx;
}

QString normalize_detect_path(const QString& path, bool windows) {
    QString s = path.trimmed();
    if (s.size() >= 2 && s.startsWith(QLatin1Char('"')) && s.endsWith(QLatin1Char('"')))
        s = s.mid(1, s.size() - 2).trimmed();
    if (s.isEmpty())
        return {};
    if (windows)
        s.replace(QLatin1Char('\\'), QLatin1Char('/'));
    return QDir::cleanPath(s);
}

QStringList split_search_path(const QString& value, bool windows) {
    QStringList out;
    QSet<QString> seen;
    const QStringList parts = value.split(windows ? QLatin1Char(';') : QLatin1Char(':'), Qt::SkipEmptyParts);
    for (const QString& raw : parts) {
        const QString dir = normalize_detect_path(raw, windows);
        if (dir.isEmpty() || !is_absolute(dir, windows))
            continue; // a relative PATH entry would resolve against our own working directory
        const QString key = path_key(dir, windows);
        if (seen.contains(key))
            continue;
        seen.insert(key);
        out << dir;
    }
    return out;
}

QStringList executable_file_names(const QString& name, const DetectContext& ctx) {
    if (!ctx.windows)
        return {name};
    const QString lower = name.toLower();
    for (const char* ext : {".exe", ".cmd", ".bat"}) {
        if (lower.endsWith(QLatin1String(ext)))
            return {name};
    }
    QStringList pathext;
    for (const QString& e : ctx.env_value(QStringLiteral("PATHEXT")).split(QLatin1Char(';'), Qt::SkipEmptyParts))
        pathext << e.trimmed().toLower();
    QStringList out{name + QStringLiteral(".exe")}; // a native binary always comes first
    for (const char* ext : {".cmd", ".bat"}) {
        if (pathext.isEmpty() || pathext.contains(QLatin1String(ext)))
            out << name + QLatin1String(ext);
    }
    return out;
}

QStringList shared_search_dirs(const DetectContext& ctx) {
    QStringList dirs;
    auto add = [&dirs](const QString& d) {
        if (!d.isEmpty())
            dirs << d;
    };
    const QString home = normalize_detect_path(ctx.home, ctx.windows);
    if (ctx.windows) {
        const QString appdata = env_dir(ctx, "APPDATA");
        const QString local = env_dir(ctx, "LOCALAPPDATA");
        const QString scoop = env_dir(ctx, "SCOOP");
        add(under(home, ".local/bin"));
        add(under(appdata, "npm"));
        add(under(local, "pnpm"));
        add(under(home, ".bun/bin"));
        add(under(local, "Microsoft/WinGet/Links"));
        add(under(scoop.isEmpty() ? under(home, "scoop") : scoop, "shims"));
        add(under(local, "Volta/bin"));
        add(env_dir(ctx, "NVM_SYMLINK"));
        return dirs;
    }
    add(under(home, ".local/bin"));
    add(QStringLiteral("/opt/homebrew/bin"));
    add(QStringLiteral("/usr/local/bin"));
    add(QStringLiteral("/home/linuxbrew/.linuxbrew/bin"));
    add(under(home, ".npm-global/bin"));
    add(under(home, ".bun/bin"));
    add(under(home, ".volta/bin"));
    for (const QString& v : nvm_versions_newest_first(ctx))
        add(home + QStringLiteral("/.nvm/versions/node/") + v + QStringLiteral("/bin"));
    add(under(home, ".local/share/fnm/aliases/default/bin"));
    add(under(home, ".local/share/mise/shims"));
    add(under(home, ".asdf/shims"));
    add(QStringLiteral("/snap/bin"));
    add(QStringLiteral("/usr/bin"));
    return dirs;
}

bool is_cmd_shim_path(const QString& path) {
    return path.endsWith(QLatin1String(".cmd"), Qt::CaseInsensitive) ||
           path.endsWith(QLatin1String(".bat"), Qt::CaseInsensitive);
}

std::optional<Installation> resolve_npm_shim(const QString& shim_path, const DetectContext& ctx) {
    if (!ctx.read_file || !ctx.is_file)
        return std::nullopt;
    const QString shim = normalize_detect_path(shim_path, /*windows=*/true);
    const QByteArray raw = ctx.read_file(shim, kMaxShimBytes);
    if (raw.isEmpty())
        return std::nullopt;
    // npm (cmd-shim) and pnpm both end the launch line with
    //   "<node>"  "%dp0%\node_modules\<pkg>\cli.js" %*      (or %~dp0)
    // — the last quoted %dp0%-relative path before %* is the script. Only the
    // ASCII part of the file matters, so Latin-1 is a lossless enough decode.
    static const QRegularExpression rx(QStringLiteral(R"re("(%~dp0|%dp0%)([^"\r\n]*)"\s*%\*)re"),
                                       QRegularExpression::CaseInsensitiveOption);
    const QString text = QString::fromLatin1(raw);
    QString rel;
    auto it = rx.globalMatch(text);
    while (it.hasNext())
        rel = it.next().captured(2);
    if (rel.trimmed().isEmpty())
        return std::nullopt;
    const qsizetype slash = shim.lastIndexOf(QLatin1Char('/'));
    if (slash <= 0)
        return std::nullopt;
    const QString dir = shim.left(slash);
    const QString script = normalize_detect_path(dir + QLatin1Char('/') + rel, /*windows=*/true);
    if (script.endsWith(QLatin1String(".exe"), Qt::CaseInsensitive)) {
        // A package whose bin is a native binary: the shim just runs it.
        if (!ctx.executable(script))
            return std::nullopt;
        Installation native;
        native.path = script;
        native.via = Installation::Native;
        return native;
    }
    if (!is_node_script(script) || !ctx.is_file(script))
        return std::nullopt;
    QString node = dir + QStringLiteral("/node.exe");
    if (!ctx.executable(node))
        node = find_node(ctx);
    if (node.isEmpty())
        return std::nullopt;
    Installation inst;
    inst.path = shim;
    inst.via = Installation::NodeScript;
    inst.node_path = node;
    inst.script_path = script;
    return inst;
}

QVector<Installation> find_candidates(const IHarnessAdapter& adapter, const DetectContext& ctx) {
    const bool win = ctx.windows;
    auto make = [&ctx, win](const QString& file, bool overridden) {
        Installation inst;
        inst.path = file;
        inst.path_overridden = overridden;
        if (win && is_cmd_shim_path(file)) {
            if (auto resolved = resolve_npm_shim(file, ctx)) {
                resolved->path_overridden = overridden;
                return *resolved;
            }
            inst.via = Installation::CmdShim; // not an npm shim we understand: cmd.exe, last resort
        }
        return inst;
    };

    // 1. The user's override wins when it exists. A stale override falls back
    //    to auto-detection (the settings dialog keeps showing what was typed).
    const QString override_path = normalize_detect_path(ctx.override_path, win);
    if (!override_path.isEmpty() && ctx.is_file && ctx.is_file(override_path))
        return {make(override_path, true)};

    QStringList files;
    QSet<QString> seen;
    auto add = [&](const QString& candidate) {
        const QString p = normalize_detect_path(candidate, win);
        if (p.isEmpty() || !is_absolute(p, win))
            return;
        const QString key = path_key(p, win);
        if (seen.contains(key))
            return;
        seen.insert(key);
        if (ctx.executable(p))
            files << p;
    };
    const QStringList names = adapter.descriptor().executable_names;
    auto add_dir = [&](const QString& dir) {
        for (const QString& name : names) {
            for (const QString& file : executable_file_names(name, ctx))
                add(dir + QLatin1Char('/') + file);
        }
    };

    // 2. PATH, 3. the adapter's own locations, 4. the shared per-OS list.
    for (const QString& dir : split_search_path(ctx.env_value(QStringLiteral("PATH")), win))
        add_dir(dir);
    for (const QString& c : adapter.candidate_paths(ctx))
        add(c);
    for (const QString& dir : shared_search_dirs(ctx))
        add_dir(dir);

    // Native binaries first (no node, no cmd.exe), then npm shims run as
    // node + script, then shims that need cmd.exe — each group in search order.
    QVector<Installation> native, node, cmd;
    QSet<QString> native_seen; // a shim that runs a binary found directly is not listed twice
    for (const QString& f : std::as_const(files)) {
        Installation inst = make(f, false);
        if (inst.via == Installation::Native) {
            const QString key = path_key(inst.path, win);
            if (native_seen.contains(key))
                continue;
            native_seen.insert(key);
        }
        if (inst.via == Installation::NodeScript)
            node.append(inst);
        else if (inst.via == Installation::CmdShim)
            cmd.append(inst);
        else
            native.append(inst);
    }
    return native + node + cmd;
}

VersionVerdict check_version(const IHarnessAdapter& adapter, Installation& inst, int exit_code, bool ran,
                             const QString& output) {
    QString first_line;
    for (const QString& line : output.split(QLatin1Char('\n'))) {
        const QString t = line.trimmed();
        if (t.isEmpty())
            continue;
        if (first_line.isEmpty())
            first_line = t;
        const QString v = adapter.parse_version(t);
        if (!v.isEmpty()) {
            inst.version = v;
            inst.version_line = t.left(200);
            return VersionVerdict::Accepted;
        }
    }
    if (!ran || first_line.isEmpty() || exit_code != 0)
        return VersionVerdict::Unverified; // an error message is not an identity
    inst.version.clear();
    inst.version_line = first_line.left(200);
    return VersionVerdict::Rejected;
}

} // namespace fincept::harness
