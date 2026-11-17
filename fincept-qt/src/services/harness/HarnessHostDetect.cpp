// HarnessHostDetect.cpp — harness detection answered by the host (see the header).

#include "services/harness/HarnessHostDetect.h"

#include "core/process/HostExec.h"
#include "services/harness/HarnessDetect.h"
#include "services/harness/IHarnessAdapter.h"

#include <QHash>
#include <QProcessEnvironment>

#include <functional>

namespace fincept::harness {

namespace harness_host_detect_detail {

namespace host = fincept::process::host;

constexpr int kHostQueryTimeoutMs = 15000;
constexpr int kMaxRounds = 4;
constexpr qint64 kMaxReadBytes = 1024 * 1024;
constexpr qint64 kMaxQueryOutputBytes = 16 * 1024 * 1024;

/// One batched question list, one argument per question; answers by index:
///   f:<path>        → "F\t<i>\t<regular file 0|1>\t<executable 0|1>\t<size> <mtime s>"  (nothing if absent)
///   d:<glob>:<dir>  → "D\t<i>\t<name>" per matching sub-directory
///   r:<max>:<path>  → "R\t<i>\t<hex of at most max bytes>"  (nothing if unreadable)
/// stat -L -c is GNU / BusyBox; without it the size is -1 (no cache, a version probe).
constexpr char kQueryScript[] = "i=0\n"
                                "for a in \"$@\"; do\n"
                                "  case $a in\n"
                                "  f:*)\n"
                                "    p=${a#f:}\n"
                                "    if [ -e \"$p\" ]; then\n"
                                "      t=0; [ -f \"$p\" ] && t=1\n"
                                "      x=0; [ -x \"$p\" ] && x=1\n"
                                "      s=$(stat -L -c '%s %Y' -- \"$p\" 2>/dev/null) || s='-1 0'\n"
                                "      printf 'F\\t%s\\t%s\\t%s\\t%s\\n' \"$i\" \"$t\" \"$x\" \"$s\"\n"
                                "    fi ;;\n"
                                "  d:*)\n"
                                "    r=${a#d:}; g=${r%%:*}; q=${r#*:}\n"
                                "    for e in \"$q\"/$g; do\n"
                                "      [ -d \"$e\" ] && printf 'D\\t%s\\t%s\\n' \"$i\" \"${e##*/}\"\n"
                                "    done ;;\n"
                                "  r:*)\n"
                                "    r=${a#r:}; m=${r%%:*}; q=${r#*:}\n"
                                "    if [ -f \"$q\" ] && [ -r \"$q\" ]; then\n"
                                "      printf 'R\\t%s\\t' \"$i\"\n"
                                "      head -c \"$m\" -- \"$q\" | od -An -v -tx1 | tr -d ' \\n'\n"
                                "      printf '\\n'\n"
                                "    fi ;;\n"
                                "  esac\n"
                                "  i=$((i+1))\n"
                                "done\n";

struct FileFact {
    bool is_file = false;
    bool executable = false;
    qint64 size = -1;
    qint64 mtime_ms = 0;
};

/// The host's answers so far, and the questions the rules asked that have none yet.
struct Snapshot {
    QHash<QString, std::optional<FileFact>> files; // nullopt = does not exist
    QHash<QString, QStringList> dirs;              // dir + '\n' + glob → sub-directory names
    QHash<QString, QByteArray> reads;              // path → first bytes (empty = unreadable)
    QStringList pending_files;
    QStringList pending_dirs;               // dir + '\n' + glob
    QHash<QString, qint64> pending_reads;   // path → max bytes

    bool any_pending() const { return !pending_files.isEmpty() || !pending_dirs.isEmpty() || !pending_reads.isEmpty(); }
    void clear_pending() {
        pending_files.clear();
        pending_dirs.clear();
        pending_reads.clear();
    }

    const FileFact* file(const QString& path) {
        const auto it = files.constFind(path);
        if (it != files.cend())
            return it->has_value() ? &it->value() : nullptr;
        if (!pending_files.contains(path))
            pending_files << path;
        return nullptr;
    }
    QStringList dir(const QString& d, const QString& glob) {
        const QString key = d + QLatin1Char('\n') + glob;
        const auto it = dirs.constFind(key);
        if (it != dirs.cend())
            return *it;
        if (!pending_dirs.contains(key))
            pending_dirs << key;
        return {};
    }
    QByteArray read(const QString& path, qint64 max_bytes) {
        const auto it = reads.constFind(path);
        if (it != reads.cend())
            return it->left(max_bytes);
        const qint64 want = qBound<qint64>(0, max_bytes, kMaxReadBytes);
        pending_reads.insert(path, qMax(want, pending_reads.value(path)));
        return {};
    }
};

/// The rules see the host: its HOME, its login PATH, its files. Environment
/// variable NAMES (API-key presence) stay the app's own: those are what a run
/// hands to the CLI (HostExec mirrors them).
DetectContext host_context(const host::HostProfile& profile, const QString& override_path, Snapshot* snap) {
    DetectContext ctx;
    ctx.home = profile.home;
    ctx.env = QProcessEnvironment::systemEnvironment();
    ctx.env.insert(QStringLiteral("HOME"), profile.home);
    ctx.env.insert(QStringLiteral("PATH"), profile.path);
    ctx.override_path = override_path;
    ctx.windows = false;
    ctx.is_file = [snap](const QString& p) {
        const FileFact* f = snap->file(p);
        return f && f->is_file;
    };
    ctx.is_executable = [snap](const QString& p) {
        const FileFact* f = snap->file(p);
        return f && f->is_file && f->executable;
    };
    ctx.list_dir = [snap](const QString& d, const QString& glob) { return snap->dir(d, glob); };
    ctx.read_file = [snap](const QString& p, qint64 max_bytes) { return snap->read(p, max_bytes); };
    return ctx;
}

bool askable_path(const QString& p) {
    return p.startsWith(QLatin1Char('/')) && !p.contains(QLatin1Char('\n'));
}

/// Ask the host every pending question in one query. Unaskable questions are
/// answered "absent". False (with `error`) when the host could not be asked.
bool answer_pending(Snapshot& snap, QString* error) {
    enum class Kind { File, Dir, Read };
    struct Ref {
        Kind kind;
        QString key;
    };
    QStringList items;
    QVector<Ref> refs;
    for (const QString& p : std::as_const(snap.pending_files)) {
        snap.files.insert(p, std::nullopt); // absent unless the host says otherwise
        if (!askable_path(p))
            continue;
        items << QStringLiteral("f:") + p;
        refs.append(Ref{Kind::File, p});
    }
    for (const QString& key : std::as_const(snap.pending_dirs)) {
        snap.dirs.insert(key, {});
        const qsizetype nl = key.indexOf(QLatin1Char('\n'));
        const QString d = key.left(nl);
        const QString glob = key.mid(nl + 1);
        if (!askable_path(d) || glob.isEmpty() || glob.contains(QLatin1Char(':')) || glob.contains(QLatin1Char('/')) ||
            glob.contains(QLatin1Char('\n')))
            continue;
        items << QStringLiteral("d:%1:%2").arg(glob, d);
        refs.append(Ref{Kind::Dir, key});
    }
    for (auto it = snap.pending_reads.cbegin(); it != snap.pending_reads.cend(); ++it) {
        snap.reads.insert(it.key(), QByteArray());
        if (!askable_path(it.key()) || it.value() <= 0)
            continue;
        items << QStringLiteral("r:%1:%2").arg(it.value()).arg(it.key());
        refs.append(Ref{Kind::Read, it.key()});
    }
    snap.clear_pending();
    if (items.isEmpty())
        return true;

    QStringList argv{QStringLiteral("/bin/sh"), QStringLiteral("-c"), QString::fromLatin1(kQueryScript),
                     QStringLiteral("fincept-detect")};
    argv << items;
    const host::HostRun r = host::run_blocking(argv, kHostQueryTimeoutMs, kMaxQueryOutputBytes);
    if (!r.started || r.timed_out || r.exit_code != 0) {
        if (error) {
            const QString err = QString::fromUtf8(r.err).trimmed();
            *error = !r.started    ? (r.error.isEmpty() ? QStringLiteral("the host command could not be started") : r.error)
                     : r.timed_out ? QStringLiteral("your system did not answer in time")
                     : !err.isEmpty() ? err.left(300)
                                      : QStringLiteral("exit code %1").arg(r.exit_code);
        }
        return false;
    }
    for (const QByteArray& raw : r.out.split('\n')) {
        const QList<QByteArray> f = raw.split('\t');
        if (f.size() < 3)
            continue;
        bool ok = false;
        const qsizetype idx = f.at(1).toLongLong(&ok);
        if (!ok || idx < 0 || idx >= refs.size())
            continue;
        const Ref& ref = refs.at(idx);
        if (f.at(0) == "F" && ref.kind == Kind::File && f.size() >= 5) {
            FileFact fact;
            fact.is_file = f.at(2) == "1";
            fact.executable = f.at(3) == "1";
            const QList<QByteArray> st = f.at(4).trimmed().split(' ');
            if (st.size() >= 2) {
                bool size_ok = false;
                bool time_ok = false;
                const qint64 size = st.at(0).toLongLong(&size_ok);
                const qint64 secs = st.at(1).toLongLong(&time_ok);
                if (size_ok && time_ok && size >= 0) {
                    fact.size = size;
                    fact.mtime_ms = secs * 1000;
                }
            }
            snap.files.insert(ref.key, fact);
        } else if (f.at(0) == "D" && ref.kind == Kind::Dir) {
            const QString name = QString::fromUtf8(f.at(2));
            if (!name.isEmpty() && name != QLatin1String(".") && name != QLatin1String(".."))
                snap.dirs[ref.key] << name;
        } else if (f.at(0) == "R" && ref.kind == Kind::Read) {
            snap.reads.insert(ref.key, QByteArray::fromHex(f.at(2).trimmed()));
        }
    }
    return true;
}

/// Run `rules` over the snapshot until they ask nothing new (or kMaxRounds).
bool run_rounds(Snapshot& snap, const std::function<void()>& rules, QString* error) {
    for (int round = 0; round < kMaxRounds; ++round) {
        snap.clear_pending();
        rules();
        if (!snap.any_pending())
            return true;
        if (!answer_pending(snap, error))
            return false;
    }
    snap.clear_pending();
    rules(); // the last answers in; whatever is still open counts as absent
    return true;
}

} // namespace harness_host_detect_detail

using namespace harness_host_detect_detail;

HostSearchResult search_on_host(const IHarnessAdapter& adapter, const QString& override_path) {
    HostSearchResult res;
    if (host::blocked()) {
        res.blocked = true;
        res.error = host::blocked_reason();
        return res;
    }
    const host::HostProfile profile = host::host_profile_blocking();
    if (!profile.ok) {
        res.error = QStringLiteral("Fincept Terminal could not ask your system where %1 is installed: %2")
                        .arg(adapter.descriptor().display_name, profile.error);
        return res;
    }
    Snapshot snap;
    const DetectContext ctx = host_context(profile, override_path, &snap);
    QString error;
    const bool ok = run_rounds(
        snap,
        [&]() {
            res.candidates = find_candidates(adapter, ctx);
            res.hint = adapter.auth_hint(ctx);
            res.user_mcp_server_names = adapter.user_mcp_server_names(ctx);
        },
        &error);
    if (!ok) {
        res.candidates.clear();
        res.error = QStringLiteral("Fincept Terminal could not search your system for %1: %2")
                        .arg(adapter.descriptor().display_name, error);
        return res;
    }
    for (Installation& c : res.candidates) {
        const auto it = snap.files.constFind(c.path);
        if (it != snap.files.cend() && it->has_value()) {
            c.file_size = (*it)->size;
            c.file_mtime_ms = (*it)->mtime_ms;
        }
    }
    return res;
}

std::optional<QStringList> host_user_mcp_server_names(const IHarnessAdapter& adapter) {
    if (host::blocked())
        return std::nullopt;
    const host::HostProfile profile = host::host_profile_blocking();
    if (!profile.ok)
        return std::nullopt;
    Snapshot snap;
    const DetectContext ctx = host_context(profile, QString(), &snap);
    QStringList names;
    QString error;
    if (!run_rounds(snap, [&]() { names = adapter.user_mcp_server_names(ctx); }, &error))
        return std::nullopt;
    return names;
}

} // namespace fincept::harness
