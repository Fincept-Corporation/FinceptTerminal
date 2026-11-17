#include "python/PythonEnvRelocation.h"

#include "core/logging/Logger.h"
#include "core/privacy/LogRedact.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStringDecoder>
#include <QtConcurrent/QtConcurrent>
#include <QtEndian>

#include <cstring>
#include <utility>

#ifdef Q_OS_WIN
#    ifndef WIN32_LEAN_AND_MEAN
#        define WIN32_LEAN_AND_MEAN
#    endif
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <windows.h>
#else
#    include <unistd.h>
#endif

namespace fincept::python::relocation {
namespace {

constexpr const char* kTag = "PythonRelocate";
constexpr qint64 kMaxTextFile = 4LL * 1024 * 1024;
constexpr qint64 kMaxLauncher = 64LL * 1024 * 1024;
constexpr int kVerifyStartMs = 20 * 1000;
constexpr int kVerifyTimeoutMs = 120 * 1000;
const QByteArray kUtf8Bom("\xEF\xBB\xBF", 3);

#ifdef Q_OS_WIN
constexpr Qt::CaseSensitivity kPathCase = Qt::CaseInsensitive;
#else
constexpr Qt::CaseSensitivity kPathCase = Qt::CaseSensitive;
#endif

QString venv_bin_dir(const QString& venv_dir) {
#ifdef Q_OS_WIN
    return venv_dir + QStringLiteral("/Scripts");
#else
    return venv_dir + QStringLiteral("/bin");
#endif
}

QString venv_python(const QString& venv_dir) {
#ifdef Q_OS_WIN
    return venv_dir + QStringLiteral("/Scripts/python.exe");
#else
    return venv_dir + QStringLiteral("/bin/python3");
#endif
}

#ifdef Q_OS_WIN
/// \\?\ form for paths a plain Win32 call would truncate at MAX_PATH.
std::wstring win_path(const QString& path) {
    QString native = QDir::toNativeSeparators(QDir::cleanPath(QFileInfo(path).absoluteFilePath()));
    if (native.size() >= 240 && !native.startsWith(QLatin1String("\\\\")))
        native.prepend(QStringLiteral("\\\\?\\"));
    return native.toStdWString();
}
#endif

/// The root a venv was built under, from pyvenv.cfg: home = <root>/python/cpython-…[/bin].
QString prefix_from_pyvenv(const QString& venv_dir) {
    QFile f(venv_dir + QStringLiteral("/pyvenv.cfg"));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    static const QRegularExpression kHome(QStringLiteral(R"(^\s*home\s*=\s*(.+?)\s*$)"),
                                          QRegularExpression::MultilineOption |
                                              QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch home = kHome.match(QString::fromUtf8(f.readAll()));
    if (!home.hasMatch())
        return {};
    static const QRegularExpression kRoot(QStringLiteral(R"(^(.+?)[\\/]python[\\/]cpython-[^\\/]+(?:[\\/]bin)?[\\/]?$)"),
                                          QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch root = kRoot.match(home.captured(1));
    return root.hasMatch() ? root.captured(1) : QString();
}

bool same_path(const QString& a, const QString& b) {
    const auto norm = [](QString p) {
        p.replace(QLatin1Char('\\'), QLatin1Char('/'));
        while (p.size() > 1 && p.endsWith(QLatin1Char('/')))
            p.chop(1);
        return p;
    };
    return norm(a).compare(norm(b), kPathCase) == 0;
}

/// Replaces an old root at a path boundary: preceded by start of line, white
/// space, a quote, '=', '!' (shebang), ':' or ';' (path lists), "-L" / "-I"
/// (compiler flags); followed by a
/// separator, quote, white space or end — never by '.', so the new root
/// (old + ".community") is never matched again: rewriting twice is a no-op.
class PrefixRewriter {
  public:
    PrefixRewriter(const QStringList& old_prefixes, const QString& new_root) {
        new_fwd_ = QDir::fromNativeSeparators(new_root);
        new_back_ = new_fwd_;
        new_back_.replace(QLatin1Char('/'), QLatin1Char('\\'));
        for (const QString& p : old_prefixes) {
            QStringList parts = QDir::fromNativeSeparators(p).split(QLatin1Char('/'));
            for (QString& part : parts)
                part = QRegularExpression::escape(part);
            const QString body = parts.join(QStringLiteral("[\\\\/]"));
            QRegularExpression::PatternOptions opts = QRegularExpression::MultilineOption;
            if constexpr (kPathCase == Qt::CaseInsensitive)
                opts |= QRegularExpression::CaseInsensitiveOption;
            // "-L/-I<path>" covers the compiler flags in _sysconfigdata.
            patterns_.append(QRegularExpression(
                QStringLiteral(R"((?:^|(?<=[\s"'=!:;,(\[])|(?<=-[LI]))%1(?=[\\/"'\s:;,)\]]|$))").arg(body), opts));
            const QString last = QDir::fromNativeSeparators(p).section(QLatin1Char('/'), -1);
            needles_.append((kPathCase == Qt::CaseInsensitive ? last.toLower() : last).toUtf8());
        }
    }

    bool empty() const { return patterns_.isEmpty(); }

    /// Cheap pre-filter on raw bytes: the old roots' last component occurs.
    bool mentions(const QByteArray& bytes) const {
        const QByteArray hay = kPathCase == Qt::CaseInsensitive ? bytes.toLower() : bytes;
        for (const QByteArray& n : needles_) {
            if (!n.isEmpty() && hay.contains(n))
                return true;
        }
        return false;
    }

    QString apply(const QString& text, bool* changed) const {
        QString out = text;
        for (const QRegularExpression& re : patterns_) {
            QString next;
            qsizetype last = 0;
            auto it = re.globalMatch(out);
            bool any = false;
            while (it.hasNext()) {
                const QRegularExpressionMatch m = it.next();
                next += QStringView(out).mid(last, m.capturedStart() - last);
                next += m.captured().contains(QLatin1Char('\\')) ? new_back_ : new_fwd_;
                last = m.capturedEnd();
                any = true;
            }
            if (any) {
                next += QStringView(out).mid(last);
                out = next;
                if (changed)
                    *changed = true;
            }
        }
        return out;
    }

  private:
    QList<QRegularExpression> patterns_;
    QList<QByteArray> needles_;
    QString new_fwd_;
    QString new_back_;
};

bool write_replacing(const QString& path, const QByteArray& content, QFileDevice::Permissions perms) {
    // A new file renamed over the old one: a hard-linked original (the copy
    // path links the runtime from the old folder) is never changed through it.
    QSaveFile s(path);
    if (!s.open(QIODevice::WriteOnly) || s.write(content) != content.size() || !s.commit())
        return false;
    QFile::setPermissions(path, perms);
    return true;
}

void rewrite_text_file(const QString& path, const PrefixRewriter& rw, RelocationReport& rep) {
    const QFileInfo fi(path);
    if (fi.isSymLink() || !fi.isFile() || fi.size() > kMaxTextFile)
        return;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        rep.problems << QStringLiteral("cannot read %1").arg(path);
        return;
    }
    QByteArray bytes = f.readAll();
    f.close();
    if (bytes.left(8192).contains('\0') || !rw.mentions(bytes))
        return; // binary, or no reference to an old root
    const bool bom = bytes.startsWith(kUtf8Bom);
    if (bom)
        bytes.remove(0, kUtf8Bom.size());
    QStringDecoder decoder(QStringDecoder::Utf8);
    const QString text = decoder.decode(bytes);
    if (decoder.hasError()) {
        rep.problems << QStringLiteral("%1 is not UTF-8; left as it was").arg(path);
        return;
    }
    bool changed = false;
    const QString out = rw.apply(text, &changed);
    if (!changed)
        return;
    QByteArray content = out.toUtf8();
    if (bom)
        content.prepend(kUtf8Bom);
    if (write_replacing(path, content, fi.permissions()))
        ++rep.files_rewritten;
    else
        rep.problems << QStringLiteral("cannot rewrite %1").arg(path);
}

#ifndef Q_OS_WIN
bool make_symlink(const QString& target, const QString& link) {
    return ::symlink(QFile::encodeName(target).constData(), QFile::encodeName(link).constData()) == 0;
}
#endif

void repoint_link(const QString& path, const PrefixRewriter& rw, RelocationReport& rep) {
#ifdef Q_OS_WIN
    Q_UNUSED(path);
    Q_UNUSED(rw);
    Q_UNUSED(rep);
    // uv's Windows runtime has no symlinks (Scripts\python.exe is a launcher
    // that reads pyvenv.cfg); junctions are not created by the pinned uv 0.7.
#else
    const QFileInfo fi(path);
    if (!fi.isSymLink())
        return;
    const QString raw = fi.readSymLink();
    if (raw.isEmpty() || QDir::isRelativePath(raw))
        return; // relative links move with their folder
    bool changed = false;
    const QString target = rw.apply(raw, &changed);
    if (!changed)
        return;
    if (QFile::remove(path) && make_symlink(target, path))
        ++rep.links_repointed;
    else
        rep.problems << QStringLiteral("cannot re-point %1 to %2").arg(path, target);
#endif
}

/// uv's Windows launcher (console scripts, and python.exe in some uv versions):
/// <exe><payload><python path, UTF-8><u32 LE length><"UVSC" | "UVPY">. Only a
/// trailer that parses completely and names an old root is rebuilt.
[[maybe_unused]] // called from the Windows branch only
void patch_launcher(const QString& path, const PrefixRewriter& rw, RelocationReport& rep) {
    const QFileInfo fi(path);
    if (fi.isSymLink() || !fi.isFile() || fi.size() < 16 || fi.size() > kMaxLauncher)
        return;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return;
    const QByteArray data = f.readAll();
    f.close();
    const QByteArray magic = data.right(4);
    if (magic != QByteArrayLiteral("UVSC") && magic != QByteArrayLiteral("UVPY"))
        return;
    const quint32 len = qFromLittleEndian<quint32>(data.constData() + data.size() - 8);
    if (len == 0 || len > 32768 || qint64(len) + 8 > data.size())
        return;
    const qsizetype start = data.size() - 8 - qsizetype(len);
    QStringDecoder decoder(QStringDecoder::Utf8);
    const QString python = decoder.decode(data.mid(start, len));
    if (decoder.hasError())
        return;
    bool changed = false;
    const QString repointed = rw.apply(python, &changed);
    if (!changed)
        return;
    const QByteArray encoded = repointed.toUtf8();
    QByteArray out = data.left(start);
    out += encoded;
    char size_le[4];
    qToLittleEndian<quint32>(quint32(encoded.size()), size_le);
    out.append(size_le, 4);
    out += magic;
    if (write_replacing(path, out, fi.permissions()))
        ++rep.launchers_patched;
    else
        rep.problems << QStringLiteral("cannot rewrite launcher %1").arg(path);
}

void relocate_venv(const QString& vdir, const PrefixRewriter& rw, RelocationReport& rep) {
    rewrite_text_file(vdir + QStringLiteral("/pyvenv.cfg"), rw, rep);
    // lib64 -> lib and similar top-level links
    for (const QFileInfo& e : QDir(vdir).entryInfoList(QDir::AllEntries | QDir::System | QDir::Hidden |
                                                      QDir::NoDotAndDotDot)) {
        if (e.isSymLink())
            repoint_link(e.filePath(), rw, rep);
    }
    // Interpreter links, scripts with shebangs, activate scripts, launchers.
    for (const QFileInfo& e : QDir(venv_bin_dir(vdir))
                                  .entryInfoList(QDir::Files | QDir::System | QDir::Hidden | QDir::NoDotAndDotDot)) {
        if (e.isSymLink()) {
            repoint_link(e.filePath(), rw, rep);
            continue;
        }
#ifdef Q_OS_WIN
        if (e.suffix().compare(QLatin1String("exe"), Qt::CaseInsensitive) == 0) {
            patch_launcher(e.filePath(), rw, rep);
            continue;
        }
#endif
        rewrite_text_file(e.filePath(), rw, rep);
    }
    // .pth / .egg-link files are where site-packages keeps absolute paths.
    QStringList site_dirs;
#ifdef Q_OS_WIN
    site_dirs << vdir + QStringLiteral("/Lib/site-packages");
#else
    for (const QString& lib : QDir(vdir + QStringLiteral("/lib")).entryList({QStringLiteral("python*")}, QDir::Dirs))
        site_dirs << vdir + QStringLiteral("/lib/") + lib + QStringLiteral("/site-packages");
#endif
    for (const QString& sp : std::as_const(site_dirs)) {
        for (const QFileInfo& e :
             QDir(sp).entryInfoList({QStringLiteral("*.pth"), QStringLiteral("*.egg-link")}, QDir::Files)) {
            rewrite_text_file(e.filePath(), rw, rep);
        }
    }
}

void relocate_base_runtime(const QString& python_dir, const PrefixRewriter& rw, RelocationReport& rep) {
    if (!QFileInfo(python_dir).isDir())
        return;
    // Every file: uv writes the install prefix into _sysconfigdata*.py (and on
    // some versions pkg-config / *-config scripts). The pre-filter keeps this to
    // a read of the few MB of text the runtime holds.
    QDirIterator it(python_dir, QDir::AllEntries | QDir::System | QDir::Hidden | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        const QFileInfo fi = it.fileInfo();
        if (fi.isSymLink())
            repoint_link(path, rw, rep);
        else if (fi.isFile())
            rewrite_text_file(path, rw, rep);
    }
}

const char* const kVerifyCode = "import importlib, sys\n"
                                "bad = []\n"
                                "for m in sys.argv[1:]:\n"
                                "    try:\n"
                                "        importlib.import_module(m)\n"
                                "    except BaseException as e:\n"
                                "        bad.append('%s (%s: %s)' % (m, type(e).__name__, str(e)[:160]))\n"
                                "print('FINCEPT-VERIFY-BASE ' + sys.base_prefix)\n"
                                "print('FINCEPT-VERIFY ' + ('OK' if not bad else 'MISSING ' + '; '.join(bad)))\n";

/// The modules every installed venv must import. venv-numpy2 carries the
/// essentials (resources/requirements-numpy2-core.txt, by import name).
QStringList essentials_for(const QString& venv) {
    if (venv == QLatin1String("venv-numpy2"))
        return {QStringLiteral("numpy"),    QStringLiteral("pandas"),     QStringLiteral("yfinance"),
                QStringLiteral("requests"), QStringLiteral("websockets"), QStringLiteral("bs4"),
                QStringLiteral("certifi"),  QStringLiteral("curl_cffi")};
    return {QStringLiteral("numpy")};
}

bool under(const QString& path, const QString& dir) {
    QString p = QFileInfo(path).canonicalFilePath();
    QString d = QFileInfo(dir).canonicalFilePath();
    if (p.isEmpty() || d.isEmpty())
        return false;
    return p.compare(d, kPathCase) == 0 || p.startsWith(d + QLatin1Char('/'), kPathCase);
}

VenvVerdict verify_venv(const QString& root, const QString& venv) {
    VenvVerdict v;
    v.venv = venv;
    const QString dir = root + QLatin1Char('/') + venv;
    if (!QFileInfo(dir).isDir())
        return v; // Absent
    const QString python = venv_python(dir);
    if (!QFileInfo::exists(python)) {
        v.state = VenvVerdict::Broken;
        v.detail = QStringLiteral("its interpreter %1 does not resolve").arg(QDir::toNativeSeparators(python));
        return v;
    }
    QProcess proc;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    for (const char* key : {"PYTHONHOME", "PYTHONPATH", "VIRTUAL_ENV", "CONDA_PREFIX", "PYTHONSTARTUP", "PYTHONUSERBASE"})
        env.remove(QString::fromLatin1(key));
    proc.setProcessEnvironment(env);
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.setWorkingDirectory(root);
#ifdef Q_OS_WIN
    proc.setCreateProcessArgumentsModifier(
        [](QProcess::CreateProcessArguments* cpa) { cpa->flags |= 0x08000000; }); // CREATE_NO_WINDOW
#endif
    // -I: no user site, no PYTHON* variables; -B: writes no bytecode into the venv.
    proc.start(python, QStringList{QStringLiteral("-I"), QStringLiteral("-B"), QStringLiteral("-c"),
                                   QString::fromLatin1(kVerifyCode)} +
                           essentials_for(venv));
    if (!proc.waitForStarted(kVerifyStartMs)) {
        v.state = VenvVerdict::Broken;
        v.detail = QStringLiteral("its interpreter did not start (%1)").arg(proc.errorString());
        return v;
    }
    if (!proc.waitForFinished(kVerifyTimeoutMs)) {
        proc.kill();
        proc.waitForFinished(3000);
        v.state = VenvVerdict::Broken;
        v.detail = QStringLiteral("importing the essentials did not finish in %1 s").arg(kVerifyTimeoutMs / 1000);
        return v;
    }
    const QString out = QString::fromUtf8(proc.readAll());
    const qsizetype at = out.lastIndexOf(QLatin1String("FINCEPT-VERIFY "));
    if (proc.exitStatus() != QProcess::NormalExit || at < 0) {
        v.state = VenvVerdict::Broken;
        v.detail = QStringLiteral("its interpreter failed: %1").arg(out.right(300).simplified());
        return v;
    }
    const qsizetype base_at = out.indexOf(QLatin1String("FINCEPT-VERIFY-BASE "));
    const QString base = base_at < 0 ? QString() : out.mid(base_at + 20).section(QLatin1Char('\n'), 0, 0).trimmed();
    if (!base.isEmpty() && !under(base, root + QStringLiteral("/python"))) {
        v.state = VenvVerdict::Broken;
        v.detail = QStringLiteral("it still runs an interpreter outside the data root (%1)").arg(base);
        return v;
    }
    const QString verdict = out.mid(at + 15).section(QLatin1Char('\n'), 0, 0).trimmed();
    if (verdict == QLatin1String("OK")) {
        v.state = VenvVerdict::Ok;
        v.detail = QStringLiteral("imports %1").arg(essentials_for(venv).join(QLatin1String(", ")));
    } else {
        v.state = VenvVerdict::PackagesMissing;
        v.detail = verdict.mid(8).left(400);
    }
    return v;
}

} // namespace

QString RelocationReport::summary() const {
    if (old_prefixes.isEmpty())
        return QStringLiteral("no absolute paths to relocate");
    QString s = QStringLiteral("references to %1 relocated: %2 file(s) rewritten, %3 link(s) re-pointed, %4 "
                               "launcher(s) patched")
                    .arg(old_prefixes.join(QLatin1String(", ")))
                    .arg(files_rewritten)
                    .arg(links_repointed)
                    .arg(launchers_patched);
    if (!problems.isEmpty())
        s += QStringLiteral(" (%1 problem(s): %2)").arg(problems.size()).arg(problems.mid(0, 3).join(QLatin1String("; ")));
    return s;
}

RelocationReport relocate_tree(const QString& dir, const QString& known_old_root, const QString& new_root) {
    RelocationReport rep;
    QStringList prefixes;
    const auto add = [&prefixes, &new_root](const QString& p) {
        if (p.isEmpty() || same_path(p, new_root))
            return;
        for (const QString& existing : std::as_const(prefixes)) {
            if (same_path(existing, p))
                return;
        }
        prefixes << p;
    };
    for (const char* venv : kVenvNames)
        add(prefix_from_pyvenv(dir + QLatin1Char('/') + QLatin1String(venv)));
    add(known_old_root);
    rep.old_prefixes = prefixes;
    if (prefixes.isEmpty())
        return rep;
    const PrefixRewriter rw(prefixes, new_root);
    for (const char* venv : kVenvNames) {
        const QString vdir = dir + QLatin1Char('/') + QLatin1String(venv);
        if (QFileInfo(vdir).isDir())
            relocate_venv(vdir, rw, rep);
    }
    relocate_base_runtime(dir + QStringLiteral("/python"), rw, rep);
    for (const QString& p : std::as_const(rep.problems))
        LOG_WARN(kTag, privacy::redact_text(p));
    return rep;
}

QStringList foreign_roots(const QString& root) {
    QStringList out;
    for (const char* venv : kVenvNames) {
        const QString p = prefix_from_pyvenv(root + QLatin1Char('/') + QLatin1String(venv));
        if (!p.isEmpty() && !same_path(p, root) && !out.contains(p, kPathCase))
            out << p;
    }
    return out;
}

QList<VenvVerdict> verify_and_prepare(const QString& root) {
    QList<VenvVerdict> out;
    bool repair = false;
    for (const char* name : kVenvNames) {
        VenvVerdict v = verify_venv(root, QLatin1String(name));
        if (v.state == VenvVerdict::Broken) {
            // Unusable as it is: out of the way, so the background repair builds
            // a fresh one; its files are removed off the GUI thread.
            const QString dir = root + QLatin1Char('/') + v.venv;
            const QString aside =
                dir + QStringLiteral(".unusable-") + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMddhhmmss"));
            if (QDir().rename(dir, aside)) {
                (void)QtConcurrent::run([aside]() { QDir(aside).removeRecursively(); });
                v.detail += QStringLiteral("; moved aside to be rebuilt");
            } else {
                v.detail += QStringLiteral("; could not be moved aside");
            }
        }
        if (v.state == VenvVerdict::Broken || v.state == VenvVerdict::PackagesMissing)
            repair = true;
        out << v;
    }
    const QString flag = repair_flag_path(root);
    if (repair) {
        QSaveFile f(flag);
        if (!f.open(QIODevice::WriteOnly) || f.write((describe(out) + QLatin1Char('\n')).toUtf8()) < 0 || !f.commit())
            LOG_WARN(kTag, "Could not write the repair flag path=" + privacy::redact_home(flag));
    } else {
        QFile::remove(flag);
    }
    return out;
}

QString describe(const QList<VenvVerdict>& verdicts) {
    QStringList parts;
    for (const VenvVerdict& v : verdicts) {
        switch (v.state) {
            case VenvVerdict::Absent:
                break;
            case VenvVerdict::Ok:
                parts << QStringLiteral("%1 works (%2)").arg(v.venv, v.detail);
                break;
            case VenvVerdict::PackagesMissing:
                parts << QStringLiteral("%1 lacks libraries (%2)").arg(v.venv, v.detail);
                break;
            case VenvVerdict::Broken:
                parts << QStringLiteral("%1 does not run (%2)").arg(v.venv, v.detail);
                break;
        }
    }
    return parts.isEmpty() ? QStringLiteral("no Python environment installed") : parts.join(QLatin1String("; "));
}

QString repair_flag_path(const QString& root) {
    return root + QStringLiteral("/.python_repair_pending");
}

bool link_or_copy(const QString& from, const QString& to, bool allow_copy) {
    const QFileInfo fi(from);
    QDir().mkpath(QFileInfo(to).absolutePath());
    if (fi.isSymLink()) {
#ifdef Q_OS_WIN
        return false; // not created by uv on Windows; never followed
#else
        const QString raw = fi.readSymLink();
        QFile::remove(to);
        return !raw.isEmpty() && make_symlink(raw, to);
#endif
    }
    QFile::remove(to);
#ifdef Q_OS_WIN
    if (CreateHardLinkW(win_path(to).c_str(), win_path(from).c_str(), nullptr))
        return true;
#else
    if (::link(QFile::encodeName(from).constData(), QFile::encodeName(to).constData()) == 0)
        return true;
#endif
    return allow_copy && QFile::copy(from, to);
}

} // namespace fincept::python::relocation
