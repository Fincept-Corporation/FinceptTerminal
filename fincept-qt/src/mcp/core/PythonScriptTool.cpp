// PythonScriptTool.cpp — the `python` core tool. See PythonScriptTool.h.

#include "mcp/core/PythonScriptTool.h"

#include "core/logging/Logger.h"
#include "mcp/AsyncDispatch.h"
#include "mcp/JobRegistry.h"
#include "mcp/ResultStore.h"
#include "mcp/ToolSchemaBuilder.h"
#include "mcp/core/CoreApproval.h"
#include "mcp/core/CoreTools.h"
#include "mcp/core/CoreToolsCommon.h"
#include "mcp/server/CallApproval.h"
#include "python/PythonRunner.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QSet>
#include <QVector>

#include <algorithm>
#include <atomic>
#include <functional>
#include <memory>
#include <optional>

namespace fincept::mcp::core {

namespace python_scripts {

namespace pyscript_detail {

constexpr const char* kPyToolTag = "PythonTool";

// Budgets — everything the model reads is bounded.
constexpr int kMaxFileBytes = 512 * 1024; // read at most this much of a script when indexing
constexpr int kDocMaxChars = 2500;        // docstring in `info`
constexpr int kSummaryMaxChars = 160;
constexpr int kMainExcerptMaxChars = 2500; // the argv dispatch block in `info`
constexpr int kInlineOutputChars = 4000;   // larger outputs: a preview inline, the rest parked in the ResultStore
constexpr int kMaxArgs = 64;
constexpr int kMaxArgChars = 256 * 1024;
constexpr int kMaxTotalArgChars = 1024 * 1024; // beyond this use stdin
constexpr int kMaxStdinBytes = 8 * 1024 * 1024;
constexpr int kMaxWaitSeconds = 25; // inline wait / long-poll ceiling (below every client's tool timeout)
constexpr int kDefaultWaitSeconds = 20;
constexpr int kMinTimeoutSeconds = 5;
// Python jobs one caller may have running or queued at once. PythonRunner runs
// three processes at a time for the WHOLE terminal (screens included), so an
// agent looping `run` would otherwise starve every Python-backed screen.
constexpr int kMaxRunningPythonJobs = 4;
// Stdout a single agent-started script may print before it is stopped. Results
// past the inline size are parked anyway; this guards memory and the GUI thread
// that parses them.
constexpr qint64 kMaxScriptOutputBytes = qint64{32} * 1024 * 1024;
// Kept clear of the tool's 29 s budget when an approval prompt is shown.
constexpr int kApprovalBudgetMarginMs = 4000;
constexpr int kMaxTimeoutSeconds = 3600;

struct ScriptInfo {
    QString key;     // "Analytics/corporateFinance/lbo/lbo_model"
    QString file;    // key + ".py", relative to the scripts dir
    QString summary; // first docstring line
    QString doc;     // docstring, bounded
    QString main_excerpt;
    QStringList commands; // dispatch keys
    QStringList options;  // argparse flags
    QStringList env_keys; // environment variables the script reads (credential hints)
    qint64 bytes = 0;
    bool runs_code = false; // calls exec()/eval() on its input: arbitrary code with the user's credentials
    /// Every CONSTANT_NAME the script and the local modules it imports mention.
    /// An agent-started run gets only the stored credentials named here, so a
    /// data connector never sees exchange secrets it has no use for.
    QStringList names;
};

struct ScriptIndex {
    QString scripts_dir;
    QVector<ScriptInfo> scripts;
    QHash<QString, int> by_key_lower;
};

// ── Parsing ──────────────────────────────────────────────────────────────────

const QSet<QString>& excluded_dir_names() {
    static const QSet<QString> k = {
        QStringLiteral("__pycache__"), QStringLiteral(".venv"),        QStringLiteral("venv"),
        QStringLiteral(".git"),        QStringLiteral("node_modules"), QStringLiteral("site-packages"),
        QStringLiteral(".firecrawl"),  QStringLiteral("tests"),        QStringLiteral("test"),
        QStringLiteral("_tools"),      QStringLiteral("i18n"),
    };
    return k;
}

bool excluded_file_name(const QString& base) {
    return base == QLatin1String("__init__") || base == QLatin1String("conftest") || base == QLatin1String("setup") ||
           base.startsWith(QLatin1Char('_')) || base.startsWith(QLatin1String("test_")) ||
           base.endsWith(QLatin1String("_test"));
}

/// Module docstring (or the leading comment block when there is none).
QString extract_docstring(const QString& src) {
    const QStringList lines = src.left(qsizetype{64} * 1024).split(QLatin1Char('\n'));
    int i = 0;
    QStringList comment_block;
    // Skip shebang / encoding / blank lines; remember a leading comment block.
    for (; i < lines.size(); ++i) {
        const QString t = lines[i].trimmed();
        if (t.isEmpty())
            continue;
        if (t.startsWith(QLatin1Char('#'))) {
            if (!t.startsWith(QLatin1String("#!")) && !t.contains(QLatin1String("coding")))
                comment_block << t.mid(1).trimmed();
            continue;
        }
        break;
    }
    if (i >= lines.size())
        return comment_block.join(QLatin1Char('\n'));
    QString first = lines[i].trimmed();
    if (first.startsWith(QLatin1Char('r')) || first.startsWith(QLatin1Char('u')) ||
        first.startsWith(QLatin1Char('R')) || first.startsWith(QLatin1Char('U')))
        first = first.mid(1);
    QString quote;
    if (first.startsWith(QLatin1String("\"\"\"")))
        quote = QStringLiteral("\"\"\"");
    else if (first.startsWith(QLatin1String("'''")))
        quote = QStringLiteral("'''");
    if (quote.isEmpty())
        return comment_block.join(QLatin1Char('\n'));

    QString body = first.mid(3);
    const int close_same_line = body.indexOf(quote);
    if (close_same_line >= 0)
        return body.left(close_same_line).trimmed();
    QStringList out{body};
    for (++i; i < lines.size(); ++i) {
        const int close = lines[i].indexOf(quote);
        if (close >= 0) {
            out << lines[i].left(close);
            break;
        }
        out << lines[i];
        if (out.size() > 400)
            break;
    }
    // Dedent.
    int min_indent = 1000;
    for (int k = 1; k < out.size(); ++k) {
        const QString& l = out[k];
        if (l.trimmed().isEmpty())
            continue;
        int n = 0;
        while (n < l.size() && l[n] == QLatin1Char(' '))
            ++n;
        min_indent = std::min(min_indent, n);
    }
    if (min_indent == 1000)
        min_indent = 0;
    for (int k = 1; k < out.size(); ++k)
        out[k] = out[k].mid(std::min(min_indent, static_cast<int>(out[k].size())));
    return out.join(QLatin1Char('\n')).trimmed();
}

void add_unique(QStringList& list, const QString& value, int cap) {
    if (value.isEmpty() || list.size() >= cap || list.contains(value))
        return;
    list << value;
}

void extract_commands(const QString& src, ScriptInfo& info) {
    // command == "x" / action == 'x' / elif cmd == "x"
    static const QRegularExpression eq_rx(QStringLiteral(
        R"(\b(?:command|cmd|action|operation|op|mode|method|func|function|endpoint|task|kind|query_type|data_type|subcommand)\s*==\s*['"]([A-Za-z0-9_\-:.]{1,48})['"])"));
    // command in ("a", "b") / in ["a", "b"]
    static const QRegularExpression in_rx(QStringLiteral(
        R"(\b(?:command|cmd|action|operation|op|mode|method|subcommand)\s+in\s*[\(\[\{]([^\)\]\}]{1,400})[\)\]\}])"));
    static const QRegularExpression quoted_rx(QStringLiteral(R"(['"]([A-Za-z0-9_\-:.]{1,48})['"])"));
    // argparse sub-commands and options
    static const QRegularExpression parser_rx(QStringLiteral(R"(add_parser\(\s*['"]([A-Za-z0-9_\-]{1,48})['"])"));
    static const QRegularExpression option_rx(QStringLiteral(R"(add_argument\(\s*['"](--?[A-Za-z0-9_\-]{1,48})['"])"));
    // dict dispatch tables: COMMANDS = { "quote": fn, ... }
    static const QRegularExpression table_rx(
        QStringLiteral(
            R"(^\s*[A-Za-z_]*(?:command|handler|dispatch|action|router|routes|method|func)[A-Za-z_]*\s*(?::[^=]*)?=\s*\{)"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::MultilineOption);
    static const QRegularExpression key_rx(QStringLiteral(R"(^\s*['"]([A-Za-z0-9_\-:.]{1,48})['"]\s*:)"),
                                           QRegularExpression::MultilineOption);
    // environment variables (credential hints)
    static const QRegularExpression env_rx(
        QStringLiteral(R"(os\.(?:environ(?:\.get)?|getenv)\s*[\(\[]\s*['"]([A-Z][A-Z0-9_]{2,63})['"])"));

    for (auto it = eq_rx.globalMatch(src); it.hasNext();)
        add_unique(info.commands, it.next().captured(1), 60);
    for (auto it = in_rx.globalMatch(src); it.hasNext();) {
        const QString inner = it.next().captured(1);
        for (auto q = quoted_rx.globalMatch(inner); q.hasNext();)
            add_unique(info.commands, q.next().captured(1), 60);
    }
    for (auto it = parser_rx.globalMatch(src); it.hasNext();)
        add_unique(info.commands, it.next().captured(1), 60);
    for (auto it = table_rx.globalMatch(src); it.hasNext();) {
        const auto m = it.next();
        const int start = m.capturedEnd(0);
        const int end = src.indexOf(QLatin1Char('}'), start);
        if (end < 0 || end - start > 6000)
            continue;
        const QString body = src.mid(start, end - start);
        for (auto k = key_rx.globalMatch(body); k.hasNext();)
            add_unique(info.commands, k.next().captured(1), 60);
    }
    for (auto it = option_rx.globalMatch(src); it.hasNext();)
        add_unique(info.options, it.next().captured(1), 40);
    for (auto it = env_rx.globalMatch(src); it.hasNext();) {
        const QString key = it.next().captured(1);
        // Only names that look like configuration, not PYTHONPATH & co.
        if (key.startsWith(QLatin1String("PYTHON")) || key == QLatin1String("PATH") || key == QLatin1String("HOME"))
            continue;
        add_unique(info.env_keys, key, 12);
    }
}

/// The block that turns argv into work: `def main` (when called from the
/// __main__ guard) or the __main__ block itself.
QString extract_main_excerpt(const QString& src) {
    static const QRegularExpression main_guard(QStringLiteral(R"(^if\s+__name__\s*==\s*['"]__main__['"])"),
                                               QRegularExpression::MultilineOption);
    static const QRegularExpression def_main(QStringLiteral(R"(^def\s+main\s*\()"),
                                             QRegularExpression::MultilineOption);
    int start = -1;
    const auto dm = def_main.match(src);
    if (dm.hasMatch())
        start = dm.capturedStart(0);
    else {
        const auto mg = main_guard.match(src);
        if (mg.hasMatch())
            start = mg.capturedStart(0);
    }
    if (start < 0)
        return {};
    QString excerpt = src.mid(start, kMainExcerptMaxChars + 200);
    if (excerpt.size() > kMainExcerptMaxChars) {
        const int cut = excerpt.lastIndexOf(QLatin1Char('\n'), kMainExcerptMaxChars);
        excerpt = excerpt.left(cut > 0 ? cut : kMainExcerptMaxChars) + QStringLiteral("\n    # … (truncated)");
    }
    return excerpt;
}

/// UPPER_SNAKE names in a source text — the vocabulary credentials are read by
/// (os.environ["FRED_API_KEY"], KEY = "FINNHUB_API_KEY", {"kraken": "KRAKEN_SECRET_KEY"}).
void collect_names(const QString& src, QSet<QString>& out) {
    static const QRegularExpression rx(QStringLiteral(R"(\b[A-Z][A-Z0-9]*(?:_[A-Z0-9]+)+\b)"));
    for (auto it = rx.globalMatch(src); it.hasNext();)
        out.insert(it.next().captured(0));
}

/// The local modules a script imports (one level): `import a.b`, `from a.b import c`,
/// `from .x import y`, resolved against the script's folder and the scripts root.
QStringList local_imports(const QString& src, const QString& script_dir, const QString& scripts_root) {
    static const QRegularExpression rx(QStringLiteral(R"(^\s*(?:from\s+(\.*[\w.]*)\s+import|import\s+([\w.]+)))"),
                                       QRegularExpression::MultilineOption);
    QStringList out;
    for (auto it = rx.globalMatch(src); it.hasNext();) {
        const auto m = it.next();
        QString mod = m.captured(1).isEmpty() ? m.captured(2) : m.captured(1);
        if (mod.isEmpty())
            continue;
        QStringList bases;
        if (mod.startsWith(QLatin1Char('.'))) {
            int dots = 0;
            while (dots < mod.size() && mod.at(dots) == QLatin1Char('.'))
                ++dots;
            QDir d(script_dir);
            for (int i = 1; i < dots; ++i)
                d.cdUp();
            mod = mod.mid(dots);
            bases << d.absolutePath();
        } else {
            bases << script_dir << scripts_root;
        }
        const QString rel = QString(mod).replace(QLatin1Char('.'), QLatin1Char('/'));
        for (const QString& b : bases) {
            const QString file =
                rel.isEmpty() ? b + QStringLiteral("/__init__.py") : b + QLatin1Char('/') + rel + QStringLiteral(".py");
            const QString pkg = b + QLatin1Char('/') + rel + QStringLiteral("/__init__.py");
            if (QFileInfo::exists(file)) {
                out << file;
                break;
            }
            if (!rel.isEmpty() && QFileInfo::exists(pkg)) {
                out << pkg;
                break;
            }
        }
    }
    return out;
}

bool is_entry_point(const QString& src) {
    static const QRegularExpression guard(QStringLiteral(R"(__name__\s*==\s*['"]__main__['"])"));
    return guard.match(src).hasMatch();
}

std::shared_ptr<const ScriptIndex> build_index(const QString& scripts_dir) {
    auto index = std::make_shared<ScriptIndex>();
    index->scripts_dir = scripts_dir;
    const QDir root(scripts_dir);
    if (scripts_dir.isEmpty() || !root.exists())
        return index;
    // Names per imported module, shared by every script that imports it.
    QHash<QString, QSet<QString>> module_names;
    auto names_of_module = [&module_names](const QString& path) -> const QSet<QString>& {
        auto it = module_names.find(path);
        if (it == module_names.end()) {
            QSet<QString> names;
            QFile mf(path);
            if (mf.open(QIODevice::ReadOnly))
                collect_names(QString::fromUtf8(mf.read(kMaxFileBytes)), names);
            it = module_names.insert(path, names);
        }
        return it.value();
    };

    QDirIterator it(scripts_dir, {QStringLiteral("*.py")}, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString abs = it.next();
        const QString rel = root.relativeFilePath(abs);
        const QStringList parts = rel.split(QLatin1Char('/'));
        bool skip = false;
        for (int p = 0; p + 1 < parts.size(); ++p) {
            if (excluded_dir_names().contains(parts[p]) || parts[p].startsWith(QLatin1Char('.'))) {
                skip = true;
                break;
            }
        }
        if (skip)
            continue;
        const QString base = QFileInfo(rel).completeBaseName();
        if (excluded_file_name(base))
            continue;

        QFile f(abs);
        if (!f.open(QIODevice::ReadOnly))
            continue;
        const QString src = QString::fromUtf8(f.read(kMaxFileBytes));
        f.close();
        if (!is_entry_point(src))
            continue; // a library module, not something `run` can start

        ScriptInfo info;
        info.file = rel;
        info.key = rel.left(rel.size() - 3);
        info.bytes = QFileInfo(abs).size();
        const QString doc = extract_docstring(src);
        info.doc = doc.size() > kDocMaxChars ? doc.left(kDocMaxChars) + QStringLiteral("\n… (truncated)") : doc;
        for (const QString& line : doc.split(QLatin1Char('\n'))) {
            const QString t = line.trimmed();
            if (!t.isEmpty() && !t.startsWith(QLatin1String("===")) && !t.startsWith(QLatin1String("---"))) {
                info.summary = common::clip(t, kSummaryMaxChars);
                break;
            }
        }
        extract_commands(src, info);
        info.main_excerpt = extract_main_excerpt(src);
        // A script that exec()s / eval()s what it is given (strategy "code" in the
        // backtesters) runs arbitrary code with every stored credential in its
        // environment — an agent must not get that without the user's say-so.
        static const QRegularExpression exec_rx(QStringLiteral(R"((?:^|[^.\w])(?:exec|eval)\s*\()"));
        info.runs_code = exec_rx.match(src).hasMatch();
        {
            QSet<QString> names;
            collect_names(src, names);
            for (const QString& mod : local_imports(src, QFileInfo(abs).absolutePath(), root.absolutePath()))
                names.unite(names_of_module(mod));
            info.names = QStringList(names.cbegin(), names.cend());
        }
        index->by_key_lower.insert(info.key.toLower(), static_cast<int>(index->scripts.size()));
        index->scripts.append(std::move(info));
    }
    std::sort(index->scripts.begin(), index->scripts.end(),
              [](const ScriptInfo& a, const ScriptInfo& b) { return a.key.toLower() < b.key.toLower(); });
    index->by_key_lower.clear();
    for (int i = 0; i < index->scripts.size(); ++i)
        index->by_key_lower.insert(index->scripts[i].key.toLower(), i);
    return index;
}

/// The catalogue, built once per scripts directory on first use (off the GUI
/// thread when the caller is a worker; a few hundred small files, ~100 ms).
std::shared_ptr<const ScriptIndex> script_index() {
    static QMutex mutex;
    static std::shared_ptr<const ScriptIndex> cached;
    const QString dir = python::PythonRunner::instance().scripts_dir();
    QMutexLocker lock(&mutex);
    if (!cached || cached->scripts_dir != dir) {
        cached = build_index(dir);
        LOG_INFO(kPyToolTag,
                 QStringLiteral("Indexed %1 runnable scripts under %2").arg(cached->scripts.size()).arg(dir));
    }
    return cached;
}

const ScriptInfo* find_script(const ScriptIndex& index, const QString& key) {
    const auto it = index.by_key_lower.constFind(key.toLower());
    return it == index.by_key_lower.constEnd() ? nullptr : &index.scripts[it.value()];
}

// ── Search ───────────────────────────────────────────────────────────────────

QStringList query_tokens(const QString& query) {
    static const QRegularExpression split_rx(QStringLiteral("[^A-Za-z0-9]+"));
    QStringList out;
    for (const QString& t : query.toLower().split(split_rx, Qt::SkipEmptyParts)) {
        if (t.size() >= 2 && !out.contains(t))
            out << t;
    }
    return out;
}

int score_script(const ScriptInfo& s, const QStringList& tokens) {
    const QString path = s.key.toLower();
    const QString summary = s.summary.toLower();
    const QString doc = s.doc.left(1500).toLower();
    const QString cmds = s.commands.join(QLatin1Char(' ')).toLower();
    const QString env = s.env_keys.join(QLatin1Char(' ')).toLower();
    int score = 0;
    int matched = 0;
    for (const QString& t : tokens) {
        int ts = 0;
        if (path.contains(t))
            ts += 4;
        if (cmds.contains(t))
            ts += 3;
        if (summary.contains(t))
            ts += 2;
        if (doc.contains(t))
            ts += 1;
        if (env.contains(t))
            ts += 1;
        if (ts > 0)
            ++matched;
        score += ts;
    }
    // Every word should count somewhere; a partial match ranks well below a full one.
    if (matched == 0)
        return 0;
    if (matched < tokens.size())
        score = score * matched / (2 * static_cast<int>(tokens.size()));
    return std::max(score, matched < tokens.size() ? 0 : 1);
}

QString script_line(const ScriptInfo& s) {
    QString line = s.key;
    if (!s.summary.isEmpty())
        line += QStringLiteral(" — ") + s.summary;
    if (!s.commands.isEmpty()) {
        const QStringList shown = s.commands.mid(0, 8);
        line += QStringLiteral(" · commands: ") + shown.join(QStringLiteral(", "));
        if (s.commands.size() > shown.size())
            line += QStringLiteral(" (+%1)").arg(s.commands.size() - shown.size());
    }
    QStringList keys;
    for (const QString& e : s.env_keys) {
        if (e.endsWith(QLatin1String("_KEY")) || e.endsWith(QLatin1String("_TOKEN")) ||
            e.endsWith(QLatin1String("_SECRET")))
            keys << e;
    }
    if (!keys.isEmpty())
        line += QStringLiteral(" · needs ") + keys.mid(0, 3).join(QStringLiteral(", "));
    return line;
}

// ── Results ──────────────────────────────────────────────────────────────────

/// Bound an output for the transcript; the full text stays readable through
/// ui_read(ref=<result id>).
QString fit_output(const QString& key, const QString& text) {
    if (text.size() <= kInlineOutputChars)
        return text;
    const QString id = ResultStore::instance().put(QStringLiteral("python:") + key, QJsonValue(text));
    return text.left(kInlineOutputChars) +
           QStringLiteral("\n… [first %1 of %2 characters — the rest: ui_read(ref=\"%3\", offset=%1)]")
               .arg(kInlineOutputChars)
               .arg(text.size())
               .arg(id);
}

/// One-line outline of a JSON value: what it holds, not the values —
/// {data: array[1250] of {date, open, close}, meta: {symbol, source}}.
QString json_shape(const QJsonValue& v, int depth = 0) {
    if (v.isObject()) {
        const QJsonObject o = v.toObject();
        if (depth >= 2)
            return QStringLiteral("{%1 keys}").arg(o.size());
        QStringList parts;
        for (auto it = o.constBegin(); it != o.constEnd(); ++it) {
            if (parts.size() >= 12) {
                parts << QStringLiteral("+%1 more").arg(o.size() - 12);
                break;
            }
            const QJsonValue c = it.value();
            parts << ((c.isObject() || c.isArray()) ? it.key() + QStringLiteral(": ") + json_shape(c, depth + 1)
                                                    : it.key());
        }
        return QLatin1Char('{') + parts.join(QStringLiteral(", ")) + QLatin1Char('}');
    }
    if (v.isArray()) {
        const QJsonArray a = v.toArray();
        if (a.isEmpty())
            return QStringLiteral("array[0]");
        const QJsonValue first = a.at(0);
        return QStringLiteral("array[%1] of %2")
            .arg(a.size())
            .arg(first.isObject() || first.isArray() ? json_shape(first, depth + 1)
                 : first.isString()                  ? QStringLiteral("strings")
                 : first.isDouble()                  ? QStringLiteral("numbers")
                                                     : QStringLiteral("values"));
    }
    return {};
}

/// A JSON result too big to send whole: its outline, a structure-keeping
/// preview (long arrays show their first items and say how many follow), and
/// the stored id with the ways to page it.
QString fit_json_output(const QString& key, const QJsonValue& value, const QString& compact) {
    if (compact.size() <= kInlineOutputChars)
        return compact;
    const QString id = ResultStore::instance().put(QStringLiteral("python:") + key, value);
    QJsonValue preview = value;
    shrink_json(preview, kInlineOutputChars - 600);
    const QString preview_text =
        preview.isObject()  ? QString::fromUtf8(QJsonDocument(preview.toObject()).toJson(QJsonDocument::Compact))
        : preview.isArray() ? QString::fromUtf8(QJsonDocument(preview.toArray()).toJson(QJsonDocument::Compact))
                            : compact.left(kInlineOutputChars);
    return QStringLiteral("shape: %1\npreview: %2\n[%3 characters in full, stored as %4. Page it with "
                          "ui_read(ref=\"%4\", path=\"<key or key.sub>\", offset=0, limit=20) — arrays come back "
                          "item by item; no path shows the outline.]")
        .arg(json_shape(value), preview_text)
        .arg(compact.size())
        .arg(id);
}

/// Mask credential values a script echoed (an HTTP error carrying
/// `?apikey=…`, a repr of its config) before the text reaches a transcript.
QString redact_secrets(QString text) {
    static const QRegularExpression rx(
        QStringLiteral(
            R"(((?:api[_\-]?key|apikey|access[_\-]?key|secret|token|password|passwd)["']?\s*[=:]\s*["']?)([^&\s"',}]{4,}))"),
        QRegularExpression::CaseInsensitiveOption);
    text.replace(rx, QStringLiteral("\\1***"));
    return text;
}

/// The end of a long failure text, newlines kept: a Python traceback puts the
/// actual exception on its LAST line, which a head-clip used to cut off.
QString tail_clip(const QString& text, int max_chars) {
    if (text.size() <= max_chars)
        return text;
    return QStringLiteral("…[%1 earlier characters omitted]\n").arg(text.size() - max_chars) + text.right(max_chars);
}

ToolResult to_tool_result(const QString& key, const python::PythonResult& r) {
    const QString out = r.output.trimmed();
    if (!r.success) {
        QString msg = r.error.trimmed();
        if (msg.isEmpty())
            msg = QStringLiteral("Script failed (exit code %1).").arg(r.exit_code);
        if (!out.isEmpty() && !msg.contains(out.left(200)))
            msg += QStringLiteral("\n--- output ---\n") + out.right(1500);
        return ToolResult::fail(QStringLiteral("%1 failed: %2").arg(key, redact_secrets(tail_clip(msg, 6000))));
    }
    if (out.isEmpty())
        return ToolResult::ok(QStringLiteral("%1 finished with no output.").arg(key));

    QJsonParseError pe{};
    QJsonDocument doc = QJsonDocument::fromJson(out.toUtf8(), &pe);
    if (pe.error != QJsonParseError::NoError) {
        // Log lines before the envelope: take the last balanced value, but only
        // if it really is JSON — "[done]" or a markdown "[link](url)" is not, and
        // the whole text is then passed through instead of a bracketed fragment.
        const QString tail = python::extract_json(out).trimmed();
        if (!tail.isEmpty() && tail.size() < out.size()) {
            QJsonParseError pe2{};
            const QJsonDocument doc2 = QJsonDocument::fromJson(tail.toUtf8(), &pe2);
            if (pe2.error == QJsonParseError::NoError && !doc2.isNull()) {
                doc = doc2;
                pe = pe2;
            }
        }
    }
    if (pe.error == QJsonParseError::NoError && !doc.isNull()) {
        if (doc.isObject()) {
            const QJsonObject o = doc.object();
            const QJsonValue err = o.value(QStringLiteral("error"));
            const bool has_payload = o.contains(QStringLiteral("data")) || o.contains(QStringLiteral("result")) ||
                                     o.contains(QStringLiteral("results"));
            if (err.isString() && !err.toString().trimmed().isEmpty() && !has_payload &&
                o.value(QStringLiteral("success")).toBool(false) == false)
                return ToolResult::fail(QStringLiteral("%1 reported an error: %2")
                                            .arg(key, redact_secrets(tail_clip(err.toString(), 4000))));
        }
        const QString compact =
            QString::fromUtf8(doc.isObject() ? QJsonDocument(doc.object()).toJson(QJsonDocument::Compact)
                                             : QJsonDocument(doc.array()).toJson(QJsonDocument::Compact));
        const QJsonValue value = doc.isObject() ? QJsonValue(doc.object()) : QJsonValue(doc.array());
        return ToolResult::ok(fit_json_output(key, value, compact));
    }
    return ToolResult::ok(fit_output(key, out));
}

QString receipt_text(const QString& job_id, const QString& key, int waited_s) {
    return QStringLiteral("%1 is still running after %2 s — continuing in the background as %3. Wait for it with "
                          "python(action=\"status\", job_id=\"%3\", wait_s=25): it returns the result when the script "
                          "finishes. Do not start it again; cancel with python(action=\"cancel\", job_id=\"%3\").")
        .arg(key)
        .arg(waited_s)
        .arg(job_id);
}

QStringList to_argv(const QJsonArray& arr, QString* error) {
    QStringList out;
    qint64 total = 0;
    for (const QJsonValue& a : arr) {
        QString s;
        if (a.isString())
            s = a.toString();
        else if (a.isBool())
            s = a.toBool() ? QStringLiteral("true") : QStringLiteral("false");
        else if (a.isDouble())
            s = QString::number(a.toDouble(), 'g', 15);
        else if (a.isObject())
            s = QString::fromUtf8(QJsonDocument(a.toObject()).toJson(QJsonDocument::Compact));
        else if (a.isArray())
            s = QString::fromUtf8(QJsonDocument(a.toArray()).toJson(QJsonDocument::Compact));
        else
            continue; // null: nothing to pass
        // "@path" is PythonRunner's spill-file convention: several scripts read
        // that file AND DELETE IT (resolve_arg). An agent must not point it at the
        // user's files. A bare handle such as "@elonmusk" stays allowed.
        static const QRegularExpression path_chars(QStringLiteral(R"([/\\:.~])"));
        if (s.startsWith(QLatin1Char('@')) && s.mid(1).contains(path_chars)) {
            *error = QStringLiteral("Arguments starting with '@' are file references and are not accepted — pass the "
                                    "content itself, or large payloads in `stdin`.");
            return {};
        }
        if (s.size() > kMaxArgChars) {
            *error = QStringLiteral("An argument is %1 characters — pass large payloads with `stdin` instead.")
                         .arg(s.size());
            return {};
        }
        total += s.size();
        out << s;
    }
    if (out.size() > kMaxArgs) {
        *error = QStringLiteral("Too many arguments (%1; at most %2).").arg(out.size()).arg(kMaxArgs);
        return {};
    }
    if (total > kMaxTotalArgChars) {
        *error = QStringLiteral("Arguments total %1 characters — pass large payloads with `stdin` instead.").arg(total);
        return {};
    }
    return out;
}

// ── Actions ──────────────────────────────────────────────────────────────────

ToolResult do_search(const QJsonObject& args) {
    const auto index = script_index();
    if (index->scripts.isEmpty())
        return ToolResult::fail(QStringLiteral("No runnable scripts found under %1.").arg(index->scripts_dir));
    const QString query = args.value(QStringLiteral("query")).toString().trimmed();
    const QString folder = args.value(QStringLiteral("folder")).toString().trimmed().toLower();
    const int limit = std::clamp(args.value(QStringLiteral("limit")).toInt(15), 1, 40);
    const QStringList tokens = query_tokens(query);

    struct Hit {
        int score;
        int idx;
    };
    QVector<Hit> hits;
    QHash<QString, int> folders;
    for (int i = 0; i < index->scripts.size(); ++i) {
        const ScriptInfo& s = index->scripts[i];
        const QString top = s.key.contains(QLatin1Char('/')) ? s.key.section(QLatin1Char('/'), 0, 0) : QString();
        folders[top.isEmpty() ? QStringLiteral("(top level)") : top]++;
        if (!folder.isEmpty() && !s.key.toLower().startsWith(folder))
            continue;
        const int score = tokens.isEmpty() ? 1 : score_script(s, tokens);
        if (score > 0)
            hits.append({score, i});
    }
    std::stable_sort(hits.begin(), hits.end(), [&index](const Hit& a, const Hit& b) {
        if (a.score != b.score)
            return a.score > b.score;
        return index->scripts[a.idx].key.size() < index->scripts[b.idx].key.size();
    });

    QStringList lines;
    if (tokens.isEmpty() && folder.isEmpty()) {
        QStringList f;
        for (auto it = folders.constBegin(); it != folders.constEnd(); ++it)
            f << QStringLiteral("%1 (%2)").arg(it.key()).arg(it.value());
        f.sort();
        lines << QStringLiteral("%1 runnable scripts. Folders: %2. Pass `query` (e.g. \"fred gdp\", \"black scholes\", "
                                "\"portfolio optimization\") or `folder` to narrow.")
                     .arg(index->scripts.size())
                     .arg(f.join(QStringLiteral(", ")));
    }
    if (hits.isEmpty()) {
        lines << QStringLiteral("No script matches \"%1\". Try fewer or different words.").arg(query);
        return ToolResult::ok(lines.join(QLatin1Char('\n')));
    }
    const int shown = std::min(limit, static_cast<int>(hits.size()));
    lines << QStringLiteral("%1 match%2%3 (showing %4). python(action=\"info\", script=…) gives a script's commands "
                            "and arguments.")
                 .arg(hits.size())
                 .arg(hits.size() == 1 ? QString() : QStringLiteral("es"))
                 .arg(query.isEmpty() ? QString() : QStringLiteral(" for \"%1\"").arg(query))
                 .arg(shown);
    for (int i = 0; i < shown; ++i)
        lines << script_line(index->scripts[hits[i].idx]);
    return ToolResult::ok(lines.join(QLatin1Char('\n')));
}

ToolResult do_info(const QJsonObject& args) {
    QString error;
    const QString key = normalize_script_key(args.value(QStringLiteral("script")).toString(), &error);
    if (key.isEmpty())
        return ToolResult::fail(error);
    const auto index = script_index();
    const ScriptInfo* s = find_script(*index, key);
    if (!s)
        return ToolResult::fail(QStringLiteral("Unknown script \"%1\". Find scripts with python(action=\"search\", "
                                               "query=…).")
                                    .arg(key));
    const ScriptRisk risk = classify(s->key, {}, s->runs_code);
    QStringList asks;
    for (const QString& c : s->commands) {
        bool money = false;
        if (classify(s->key, {c}, s->runs_code, &money) == ScriptRisk::Sensitive && money)
            asks << c;
    }
    QStringList lines;
    lines << QStringLiteral("script: %1   (scripts/%2, %3 KB)")
                 .arg(s->key, s->file)
                 .arg(qMax<qint64>(1, s->bytes / 1024));
    if (!s->summary.isEmpty())
        lines << QStringLiteral("summary: ") + s->summary;
    if (!s->commands.isEmpty())
        lines << QStringLiteral("commands (usually the first argument): ") + s->commands.join(QStringLiteral(", "));
    if (!s->options.isEmpty())
        lines << QStringLiteral("options: ") + s->options.join(QStringLiteral(", "));
    if (!s->env_keys.isEmpty())
        lines << QStringLiteral("reads environment: %1 (API keys come from Settings › Credentials)")
                     .arg(s->env_keys.join(QStringLiteral(", ")));
    lines << QStringLiteral("risk: %1")
                 .arg(risk == ScriptRisk::Blocked ? QStringLiteral("blocked — cannot be run from here")
                      : risk == ScriptRisk::Sensitive
                          ? QStringLiteral("sensitive — the user is asked to approve each run")
                          : QStringLiteral("normal"));
    if (risk == ScriptRisk::Normal && !asks.isEmpty())
        lines << QStringLiteral("asks the user first for: %1 (orders / money moves)")
                     .arg(asks.join(QStringLiteral(", ")));
    if (s->runs_code)
        lines << QStringLiteral("runs code it is given (exec/eval) — every run asks the user first");
    if (!s->doc.isEmpty())
        lines << QStringLiteral("--- docstring ---\n") + s->doc;
    if (!s->main_excerpt.isEmpty())
        lines << QStringLiteral("--- entry point ---\n") + s->main_excerpt;
    lines << QStringLiteral(
                 "run: python(action=\"run\", script=\"%1\", args=[…]) — JSON arguments are passed as compact "
                 "JSON text; large payloads go in `stdin`.")
                 .arg(s->key);
    return ToolResult::ok(lines.join(QLatin1Char('\n')));
}

QString job_status_line(const JobSnapshot& snap) {
    QString line = QStringLiteral("%1 %2 · %3 s elapsed")
                       .arg(snap.id, QString::fromLatin1(job_state_str(snap.state)))
                       .arg(snap.elapsed_ms() / 1000);
    if (snap.state == JobState::Running && snap.timeout_ms > 0)
        line += QStringLiteral(" of a %1 s budget").arg(snap.timeout_ms / 1000);
    if (snap.progress > 0.0)
        line += QStringLiteral(" · %1%").arg(qRound(snap.progress * 100));
    if (!snap.message.isEmpty())
        line += QStringLiteral(" · last output: ") + common::clip(snap.message, 160);
    return line;
}

/// Jobs belong to the caller that started them: another agent (or the in-app
/// chat) must not read, wait on or stop them. Unknown and foreign ids get the
/// same answer, so ids reveal nothing about other callers' work.
bool owns_job(const JobSnapshot& snap, const QString& origin) {
    return snap.owner.isEmpty() || snap.owner == origin;
}

std::optional<JobSnapshot> own_job(const QString& id, const QString& origin) {
    auto snap = JobRegistry::instance().status(id);
    if (!snap || !owns_job(*snap, origin))
        return std::nullopt;
    return snap;
}

/// The finished job's result with a one-line header, or nullopt while running.
std::optional<ToolResult> collect_finished(const QString& job_id) {
    auto& jobs = JobRegistry::instance();
    const auto snap = jobs.status(job_id);
    if (!snap || !snap->is_finished())
        return std::nullopt;
    auto result = jobs.take_result(job_id);
    if (!result)
        return ToolResult::fail(
            QStringLiteral("%1 finished but its result was already collected or expired.").arg(job_id));
    const QString header = QStringLiteral("%1 %2 after %3 s.\n")
                               .arg(job_id, QString::fromLatin1(job_state_str(snap->state)))
                               .arg(snap->elapsed_ms() / 1000);
    ToolResult out = *result;
    if (out.success)
        out.message = header + out.message;
    else
        out.error = header + out.error;
    return out;
}

} // namespace pyscript_detail

using namespace pyscript_detail;

QString normalize_script_key(const QString& raw, QString* error) {
    auto fail = [error](const QString& why) {
        if (error)
            *error = why;
        return QString();
    };
    QString s = raw.trimmed();
    s.replace(QLatin1Char('\\'), QLatin1Char('/'));
    if (s.isEmpty())
        return fail(QStringLiteral("`script` is required — find one with python(action=\"search\")."));
    if (s.startsWith(QLatin1String("scripts/")))
        s = s.mid(8);
    if (s.endsWith(QLatin1String(".py"), Qt::CaseInsensitive))
        s.chop(3);
    static const QRegularExpression allowed(QStringLiteral("^[A-Za-z0-9_][A-Za-z0-9_\\-./]{0,199}$"));
    if (!allowed.match(s).hasMatch() || s.contains(QLatin1String("//")) || s.endsWith(QLatin1Char('/')))
        return fail(QStringLiteral("\"%1\" is not a script name. Use a name from python(action=\"search\"), e.g. "
                                   "\"fred_data\" or \"Analytics/corporateFinance/lbo/lbo_model\".")
                        .arg(raw.left(120)));
    for (const QString& part : s.split(QLatin1Char('/'))) {
        if (part.isEmpty() || part == QLatin1String(".") || part == QLatin1String("..") ||
            part.startsWith(QLatin1Char('.')))
            return fail(QStringLiteral("\"%1\" must be a plain path inside the scripts folder.").arg(raw.left(120)));
    }
    return s;
}

namespace {

/// The words that name what a call DOES: the first argument, the value after an
/// --action / --command / --operation flag (or `--action=x`), and the
/// action / command / operation keys of a JSON-object argument. Commands hidden
/// in any of those places used to be classified as "normal".
QStringList command_words(const QStringList& args) {
    static const QStringList kFlags = {QStringLiteral("action"),    QStringLiteral("command"), QStringLiteral("cmd"),
                                       QStringLiteral("operation"), QStringLiteral("op"),      QStringLiteral("method"),
                                       QStringLiteral("mode"),      QStringLiteral("side")};
    QStringList out;
    auto add = [&out](QString w) {
        w = w.trimmed().toLower();
        while (w.startsWith(QLatin1Char('-')))
            w.remove(0, 1);
        if (!w.isEmpty() && w.size() <= 80 && !out.contains(w))
            out << w;
    };
    if (!args.isEmpty())
        add(args.first());
    for (int i = 0; i < args.size(); ++i) {
        const QString a = args.at(i).trimmed();
        if (a.startsWith(QLatin1Char('-'))) {
            QString flag = a;
            while (flag.startsWith(QLatin1Char('-')))
                flag.remove(0, 1);
            const qsizetype eq = flag.indexOf(QLatin1Char('='));
            const QString name = (eq >= 0 ? flag.left(eq) : flag).toLower();
            if (kFlags.contains(name)) {
                if (eq >= 0)
                    add(flag.mid(eq + 1));
                else if (i + 1 < args.size())
                    add(args.at(i + 1));
            }
        } else if (a.startsWith(QLatin1Char('{'))) {
            const QJsonObject o = QJsonDocument::fromJson(a.toUtf8()).object();
            for (const QString& k : kFlags) {
                if (o.value(k).isString())
                    add(o.value(k).toString());
            }
        }
    }
    return out;
}

} // namespace

ScriptRisk classify(const QString& key, const QStringList& args, bool runs_code, bool* moves_money) {
    if (moves_money)
        *moves_money = false;
    const QString lower = key.toLower();
    const QString base = lower.section(QLatin1Char('/'), -1);
    // Never from here: arbitrary code, maintenance/packaging tools, tests.
    if (base == QLatin1String("notebook_cell_runner") || lower.startsWith(QLatin1String("agents/_tools/")) ||
        lower.startsWith(QLatin1String("i18n/")) || lower.contains(QLatin1String("/tests/")) ||
        lower.startsWith(QLatin1String("tests/")) || base.startsWith(QLatin1String("test_")) ||
        base.endsWith(QLatin1String("_test")) || base.startsWith(QLatin1Char('_')))
        return ScriptRisk::Blocked;

    // Ask first: exchange credentials and orders, LLM agent runners (spend the
    // user's model credits), live strategy runners, microphone / speaker.
    // Ask first for order / money-moving / destructive commands on any script.
    // (Checked before the folder rules so `moves_money` is set for exchange/ too.)
    // `\b` used to end order_rx, and '_' is a word character, so
    // place_orders_batch / cancel_order_batch slipped through as "normal".
    static const QRegularExpression order_rx(QStringLiteral(
        R"(^(place|submit|create|cancel|modify|amend|edit|update|decrease|increase|close|exit|execute|send|new|replace)[_\-]?(order|orders|trade|trades|position|positions|bet|bets)(?:[_\-]|$))"));
    static const QRegularExpression cancel_all_rx(
        QStringLiteral(R"(^cancel[_\-]?(all|market|markets|everything)(?:[_\-]|$))"));
    static const QRegularExpression money_rx(QStringLiteral(
        R"(^(buy|sell|short|cover|withdraw|transfer|deposit|send|sign|swap|redeem|stake|unstake|bridge|mint|burn|delete|remove|drop|purge|wipe|reset|upload|publish|post)([_\-]|$))"));
    bool money = false;
    for (const QString& cmd : command_words(args)) {
        if (order_rx.match(cmd).hasMatch() || cancel_all_rx.match(cmd).hasMatch() || money_rx.match(cmd).hasMatch()) {
            money = true;
            break;
        }
    }
    if (moves_money)
        *moves_money = money || lower.startsWith(QLatin1String("exchange/"));
    if (money)
        return ScriptRisk::Sensitive;

    if (lower.startsWith(QLatin1String("exchange/")) || lower.startsWith(QLatin1String("agents/")) ||
        lower.startsWith(QLatin1String("voice/")) ||
        (lower.startsWith(QLatin1String("strategies/")) && lower.contains(QLatin1String("live"))) ||
        lower.contains(QLatin1String("wallet")) || lower.contains(QLatin1String("broker")))
        return ScriptRisk::Sensitive;

    // Arbitrary code (a strategy's `code`, an expression) runs with every stored
    // credential in the environment; reads and writes any file the user can.
    if (runs_code || base == QLatin1String("spreadsheet"))
        return ScriptRisk::Sensitive;
    return ScriptRisk::Normal;
}

} // namespace python_scripts

// ═════════════════════════════════════════════════════════════════════════════
// The tool
// ═════════════════════════════════════════════════════════════════════════════

namespace pytool_impl {

using namespace python_scripts;
using namespace python_scripts::pyscript_detail;

/// Start a script under a job and answer inline if it finishes within the wait.
void start_run(const ScriptInfo& script, const QStringList& argv, const QByteArray& stdin_data, int timeout_ms,
               int wait_ms, const common::Resolver& resolve, std::function<bool()> is_cancelled,
               std::shared_ptr<std::atomic<bool>> call_answered, const QString& owner) {
    auto& jobs = JobRegistry::instance();
    const QString key = script.key;
    const int budget = timeout_ms > 0 ? timeout_ms : 300000;
    const QString job_id = jobs.create(QStringLiteral("python:") + key, budget, owner);

    python::PythonRunner::RunOptions opts;
    opts.expect_json = false; // the tool parses JSON itself and passes text through untouched
    opts.timeout_ms = timeout_ms > 0 ? timeout_ms : python::PythonRunner::kTimeoutFromScript;
    opts.stdin_data = stdin_data;
    opts.cancel_token = jobs.cancel_flag(job_id);
    // Only the credentials this script (or a local module it imports) names, and
    // a hard ceiling on what it may print: an agent's run is not the terminal's.
    opts.credential_allowlist = script.names;
    opts.max_output_bytes = kMaxScriptOutputBytes;

    auto on_line = [job_id](const QString& line, bool /*is_stderr*/) {
        const QString t = line.trimmed();
        if (!t.isEmpty())
            JobRegistry::instance().set_progress(job_id, AsyncDispatch::parse_progress_fraction(t), t.left(160));
    };

    // Both the completion and the wait timer run on the GUI thread, so a plain
    // flag decides which one answers the call. Whatever does not answer leaves
    // the job's result in place for python(action="status").
    auto answered = std::make_shared<bool>(false);

    python::PythonRunner::instance().run_with_options(
        script.file, argv, opts,
        [job_id, key, resolve, answered, call_answered](python::PythonResult r) {
            JobRegistry::instance().complete(job_id, to_tool_result(key, r));
            if (*answered)
                return; // a receipt already went out — status collects it
            *answered = true;
            // The call was already answered by the watchdog or a cancel: leave the
            // result in the registry for status instead of consuming it here.
            if (call_answered && call_answered->load())
                return;
            if (auto done = collect_finished(job_id))
                resolve(*done);
        },
        on_line);

    // Inline wait: answer with a receipt after wait_ms, unless the caller gives up
    // first — then the script is stopped, because a caller that never received the
    // job id could neither collect nor cancel it.
    const int waited_s = wait_ms / 1000;
    common::poll_until(
        250, std::max(0, wait_ms), [answered, is_cancelled]() { return *answered || (is_cancelled && is_cancelled()); },
        [job_id, key, resolve, waited_s, answered, is_cancelled](bool) {
            if (*answered)
                return;
            *answered = true;
            if (is_cancelled && is_cancelled()) {
                JobRegistry::instance().request_cancel(job_id);
                LOG_INFO(kPyToolTag, QStringLiteral("%1 stopped: the caller gave up on the call").arg(job_id));
                resolve(ToolResult::fail(QStringLiteral("Cancelled — %1 was stopped.").arg(key)));
                return;
            }
            resolve(ToolResult::ok(receipt_text(job_id, key, waited_s)));
        });
}

} // namespace pytool_impl

ToolDef make_python_tool() {
    using namespace python_scripts;
    using namespace python_scripts::pyscript_detail;

    ToolDef t;
    t.name = QStringLiteral("python");
    t.category = QString::fromLatin1(kCategoryAnalytics);
    t.description = QStringLiteral(
        "The terminal's bundled Python analytics scripts (~490: market, macro and alternative data connectors, "
        "valuation, portfolio, derivatives, risk, backtesting, ML). action=search finds scripts by words; "
        "info shows one script's commands, options and docs; run starts it (returns the result, or a job_id when it "
        "runs longer than wait_s); status waits for a job and returns its result; cancel stops it. Sensitive scripts "
        "(exchange orders, agents) ask the user first.");
    t.input_schema = ToolSchemaBuilder()
                         .string("action", "search | info | run | status | cancel")
                         .required()
                         .enums({QStringLiteral("search"), QStringLiteral("info"), QStringLiteral("run"),
                                 QStringLiteral("status"), QStringLiteral("cancel")})
                         .string("query", "search: words describing what you need")
                         .string("folder", "search: limit to a folder, e.g. Analytics")
                         .integer("limit", "search: most results (default 15)")
                         .between(1, 40)
                         .string("script", "info/run: script name from search, e.g. fred_data")
                         .array("args", "run: command-line arguments (first is usually the command)",
                                QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}})
                         .integer("timeout_s", "run: kill after this many seconds (default: the script's own budget)")
                         .between(kMinTimeoutSeconds, kMaxTimeoutSeconds)
                         .integer("wait_s", "run/status: seconds to wait before answering with the job state (≤25)")
                         .between(0, kMaxWaitSeconds)
                         .string("job_id", "status/cancel: the job id from run")
                         .build();
    // Any JSON type: a string, or an object / array sent as compact JSON.
    t.input_schema.properties.insert(
        QStringLiteral("stdin"), QJsonObject{{QStringLiteral("description"),
                                              QStringLiteral("run: text or JSON piped to stdin (large payloads)")}});
    // Below the 30 s job threshold on purpose: this tool runs its own jobs (the
    // provider must never background it a second time), and every inline wait
    // is capped at 25 s.
    t.default_timeout_ms = 29000;

    t.async_handler = [](const QJsonObject& args, ToolContext ctx, std::shared_ptr<QPromise<ToolResult>> promise) {
        const common::Resolver resolve = common::make_resolver(ctx, promise);
        const QString action = args.value(QStringLiteral("action")).toString();
        const QString origin = ScopedCallOrigin::current();

        if (action == QLatin1String("search") || action == QLatin1String("info")) {
            // Indexing reads files: never on the GUI thread. The calling thread is a
            // worker on every path that reaches here (MCP pool, chat loop, bridge).
            resolve(action == QLatin1String("search") ? do_search(args) : do_info(args));
            return;
        }

        if (action == QLatin1String("cancel")) {
            const QString id = args.value(QStringLiteral("job_id")).toString().trimmed();
            if (id.isEmpty()) {
                resolve(ToolResult::fail(QStringLiteral("cancel needs `job_id`.")));
                return;
            }
            if (!own_job(id, origin) || !JobRegistry::instance().request_cancel(id)) {
                resolve(ToolResult::fail(QStringLiteral("%1 is unknown or already finished.").arg(id)));
                return;
            }
            resolve(ToolResult::ok(QStringLiteral("Stopping %1 — its script is killed within a second. "
                                                  "python(action=\"status\", job_id=\"%1\") confirms.")
                                       .arg(id)));
            return;
        }

        if (action == QLatin1String("status")) {
            const QString id = args.value(QStringLiteral("job_id")).toString().trimmed();
            const int wait_ms =
                std::clamp(args.value(QStringLiteral("wait_s")).toInt(kDefaultWaitSeconds), 0, kMaxWaitSeconds) * 1000;
            if (id.isEmpty()) {
                // Your jobs: running ones, and finished ones whose result you have
                // not collected yet (a lost job id is recoverable from here).
                QStringList lines;
                for (const auto& snap : JobRegistry::instance().list(true)) {
                    if (!snap.tool.startsWith(QLatin1String("python:")) || !owns_job(snap, origin))
                        continue;
                    if (snap.is_finished() && snap.result_collected)
                        continue;
                    lines << QStringLiteral("%1 — %2%3")
                                 .arg(snap.tool.mid(7), job_status_line(snap),
                                      snap.is_finished()
                                          ? QStringLiteral(" — result waiting: status(job_id=\"%1\")").arg(snap.id)
                                          : QString());
                }
                resolve(ToolResult::ok(lines.isEmpty() ? QStringLiteral("You have no Python jobs running or waiting.")
                                                       : lines.join(QLatin1Char('\n'))));
                return;
            }
            if (!own_job(id, origin)) {
                resolve(ToolResult::fail(
                    QStringLiteral("Unknown job %1 (finished jobs are kept for 15 minutes).").arg(id)));
                return;
            }
            common::on_gui_thread([id, wait_ms, resolve]() {
                common::poll_until(
                    250, wait_ms,
                    [id]() {
                        const auto snap = JobRegistry::instance().status(id);
                        return !snap || snap->is_finished();
                    },
                    [id, resolve](bool) {
                        if (auto done = collect_finished(id)) {
                            resolve(*done);
                            return;
                        }
                        const auto snap = JobRegistry::instance().status(id);
                        resolve(snap ? ToolResult::ok(job_status_line(*snap) +
                                                      QStringLiteral(" — still running; call status again to keep "
                                                                     "waiting."))
                                     : ToolResult::fail(QStringLiteral("Job %1 is gone.").arg(id)));
                    });
            });
            return;
        }

        if (action != QLatin1String("run")) {
            resolve(ToolResult::fail(
                QStringLiteral("Unknown action \"%1\" — use search, info, run, status or cancel.").arg(action)));
            return;
        }

        // ── run ──────────────────────────────────────────────────────────────
        QString error;
        const QString key = normalize_script_key(args.value(QStringLiteral("script")).toString(), &error);
        if (key.isEmpty()) {
            resolve(ToolResult::fail(error));
            return;
        }
        const auto index = script_index();
        const ScriptInfo* found = find_script(*index, key);
        if (!found) {
            resolve(
                ToolResult::fail(QStringLiteral("Unknown script \"%1\" (only runnable scripts in the bundled scripts "
                                                "folder can be run). Find one with python(action=\"search\").")
                                     .arg(key)));
            return;
        }
        // Defence in depth: the catalogue only holds files found under the
        // scripts dir, but resolve symlinks before handing the path to Python.
        const QString canonical_root = QFileInfo(index->scripts_dir).canonicalFilePath();
        const QString canonical_file =
            QFileInfo(index->scripts_dir + QLatin1Char('/') + found->file).canonicalFilePath();
        if (canonical_root.isEmpty() || canonical_file.isEmpty() ||
            !canonical_file.startsWith(canonical_root + QLatin1Char('/'))) {
            resolve(ToolResult::fail(QStringLiteral("\"%1\" does not resolve inside the scripts folder.").arg(key)));
            return;
        }

        const QStringList argv = to_argv(args.value(QStringLiteral("args")).toArray(), &error);
        if (!error.isEmpty()) {
            resolve(ToolResult::fail(error));
            return;
        }
        QByteArray stdin_data;
        const QJsonValue stdin_v = args.value(QStringLiteral("stdin"));
        if (stdin_v.isString())
            stdin_data = stdin_v.toString().toUtf8();
        else if (stdin_v.isObject())
            stdin_data = QJsonDocument(stdin_v.toObject()).toJson(QJsonDocument::Compact);
        else if (stdin_v.isArray())
            stdin_data = QJsonDocument(stdin_v.toArray()).toJson(QJsonDocument::Compact);
        if (stdin_data.size() > kMaxStdinBytes) {
            resolve(ToolResult::fail(
                QStringLiteral("stdin is %1 MB; at most 8 MB.").arg(stdin_data.size() / (qsizetype{1024} * 1024))));
            return;
        }

        // Commands passed on stdin as JSON ({"action": "place_order", …}) count too.
        QStringList classify_args = argv;
        if (stdin_data.trimmed().startsWith('{'))
            classify_args << QString::fromUtf8(stdin_data.left(qsizetype{64} * 1024));
        bool moves_money = false;
        const ScriptRisk risk = classify(found->key, classify_args, found->runs_code, &moves_money);
        if (risk == ScriptRisk::Blocked) {
            resolve(
                ToolResult::fail(QStringLiteral("\"%1\" cannot be run from here (arbitrary-code, maintenance or test "
                                                "script).")
                                     .arg(found->key)));
            return;
        }
        if (risk == ScriptRisk::Sensitive && ScopedPaperRun::active()) {
            // A PAPER workflow run (read on the calling thread, like origin): a script
            // that needs the user's approval — orders, money moves, agents — is refused
            // outright rather than offered for approval.
            resolve(
                ToolResult::fail(QStringLiteral("Refused: \"%1\" needs the user's approval (it can place orders, move "
                                                "money or act on its own), and this workflow runs in PAPER mode. Arm "
                                                "LIVE in the workflow's SAVE & RUN to allow it.")
                                     .arg(found->key)));
            return;
        }
        if (!python::PythonRunner::instance().is_available()) {
            resolve(ToolResult::fail(QStringLiteral("Python is not set up yet — the user can set it up in Settings › "
                                                    "Python Env.")));
            return;
        }

        {
            // The cap counts every caller's jobs (PythonRunner's slots are shared by
            // the whole terminal); the answer names only the caller's own ids.
            int running = 0;
            QStringList mine;
            for (const auto& snap : JobRegistry::instance().list(false)) {
                if (!snap.tool.startsWith(QLatin1String("python:")) || snap.is_finished())
                    continue;
                ++running;
                if (owns_job(snap, origin))
                    mine << QStringLiteral("%1 (%2)").arg(snap.id, snap.tool.mid(7));
            }
            if (running >= kMaxRunningPythonJobs) {
                resolve(ToolResult::fail(
                    QStringLiteral("%1 Python jobs are already running in the terminal%2. Wait for one with "
                                   "python(action=\"status\", job_id=…) or stop one of yours with action=\"cancel\" "
                                   "before starting another.")
                        .arg(running)
                        .arg(mine.isEmpty() ? QString()
                                            : QStringLiteral(" (yours: %1)").arg(mine.join(QStringLiteral(", "))))));
                return;
            }
        }

        const int timeout_s = args.value(QStringLiteral("timeout_s")).toInt(0);
        const int timeout_ms = timeout_s > 0 ? std::clamp(timeout_s, kMinTimeoutSeconds, kMaxTimeoutSeconds) * 1000 : 0;
        const int wait_ms =
            std::clamp(args.value(QStringLiteral("wait_s")).toInt(kDefaultWaitSeconds), 0, kMaxWaitSeconds) * 1000;
        const ScriptInfo script = *found; // copy: the index may be replaced

        if (risk == ScriptRisk::Sensitive) {
            ToolApprovalRequest req;
            req.client = origin;
            req.tool = QStringLiteral("python: ") + script.key;
            req.description = script.summary.isEmpty()
                                  ? QStringLiteral("Run the bundled script %1").arg(script.key)
                                  : QStringLiteral("Run the bundled script %1 — %2").arg(script.key, script.summary);
            QJsonArray shown_args;
            for (const QString& a : argv)
                shown_args.append(a.left(400));
            req.args = QJsonObject{{QStringLiteral("script"), script.key}, {QStringLiteral("args"), shown_args}};
            req.destructive = true;
            // Any order / money command, not only exchange/ scripts: no blanket
            // 15-minute grant is offered for those (Polymarket, Kalshi …).
            req.live_trading = moves_money;
            // The approval must fit inside this call's budget. A verdict that came
            // after the watchdog had already answered "timed out" used to start the
            // script anyway — an order the agent was told never ran, and retried.
            const int approval_ms = std::max(10000, ctx.timeout_ms - kApprovalBudgetMarginMs);
            const auto is_cancelled = ctx.is_cancelled;
            const auto call_answered = ctx.resolve_guard;
            ask_user_approval(
                req,
                [script, argv, stdin_data, timeout_ms, wait_ms, resolve, is_cancelled, call_answered,
                 origin](ToolApprovalVerdict v) {
                    if ((call_answered && call_answered->load()) || (is_cancelled && is_cancelled())) {
                        LOG_WARN(kPyToolTag, QStringLiteral("Not running %1: the call ended before the approval "
                                                            "arrived")
                                                 .arg(script.key));
                        return;
                    }
                    if (v != ToolApprovalVerdict::Approved) {
                        resolve(ToolResult::fail(approval_refusal_text(v, QStringLiteral("run %1").arg(script.key))));
                        return;
                    }
                    pytool_impl::start_run(script, argv, stdin_data, timeout_ms, wait_ms, resolve, is_cancelled,
                                           call_answered, origin);
                },
                approval_ms);
            return;
        }

        const auto is_cancelled = ctx.is_cancelled;
        const auto call_answered = ctx.resolve_guard;
        common::on_gui_thread(
            [script, argv, stdin_data, timeout_ms, wait_ms, resolve, is_cancelled, call_answered, origin]() {
                pytool_impl::start_run(script, argv, stdin_data, timeout_ms, wait_ms, resolve, is_cancelled,
                                       call_answered, origin);
            });
    };
    return t;
}

} // namespace fincept::mcp::core
