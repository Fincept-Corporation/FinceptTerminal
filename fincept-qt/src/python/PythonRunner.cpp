#include "python/PythonRunner.h"

#include "core/logging/Logger.h"
#include "python/PythonSetupManager.h"
#include "python/PythonWorker.h"
#include "storage/secure/SecureStorage.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLatin1String>
#include <QMetaObject>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QStringList>
#include <QThread>
#include <QTimer>
#include <QUuid>

#include <atomic>
#include <memory>

namespace fincept::python {

// ── yfinance_data.py CLI → daemon dispatch translation ───────────────────────
// Returns true if the CLI args were recognised and routed to the persistent
// PythonWorker daemon, avoiding the ~3s subprocess + yfinance/pandas import
// cost. Unrecognised actions return false so the caller falls through to the
// subprocess path (preserves correctness for future actions added to the CLI
// but not yet wired into the daemon dispatch table).
//
// Action names match scripts/yfinance_data.py::_daemon_dispatch.
static bool route_yfinance_to_daemon(const QStringList& args, std::function<void(PythonResult)> cb) {
    if (args.isEmpty())
        return false;
    const QString action = args[0];
    QJsonObject payload;

    auto sym = [&](int i) -> QString { return i < args.size() ? args[i] : QString(); };
    auto split_csv = [](const QString& s) {
        QJsonArray a;
        for (const QString& x : s.split(',', Qt::SkipEmptyParts))
            a.append(x.trimmed());
        return a;
    };

    if (action == "quote" || action == "info" || action == "financials" || action == "company_profile" ||
        action == "financial_ratios") {
        if (args.size() < 2)
            return false;
        payload["symbol"] = sym(1);
    } else if (action == "news") {
        if (args.size() < 2)
            return false;
        payload["symbol"] = sym(1);
        payload["count"] = args.size() > 2 ? args[2].toInt() : 20;
    } else if (action == "search") {
        if (args.size() < 2)
            return false;
        payload["query"] = sym(1);
        payload["limit"] = args.size() > 2 ? args[2].toInt() : 50;
    } else if (action == "historical_period") {
        if (args.size() < 2)
            return false;
        payload["symbol"] = sym(1);
        payload["period"] = args.size() > 2 ? args[2] : QStringLiteral("6mo");
        payload["interval"] = args.size() > 3 ? args[3] : QStringLiteral("1d");
    } else if (action == "historical") {
        if (args.size() < 4)
            return false;
        payload["symbol"] = sym(1);
        payload["start_date"] = sym(2);
        payload["end_date"] = sym(3);
        payload["interval"] = args.size() > 4 ? args[4] : QStringLiteral("1d");
    } else if (action == "multiple_ratios" || action == "multiple_profiles") {
        if (args.size() < 2)
            return false;
        payload["symbols"] = split_csv(args[1]);
    } else if (action == "batch_quotes" || action == "batch_sparklines") {
        if (args.size() < 2)
            return false;
        QJsonArray syms;
        for (int i = 1; i < args.size(); ++i)
            syms.append(args[i]);
        payload["symbols"] = syms;
    } else if (action == "batch_all") {
        // batch_all carries a JSON blob (or @file spill). Skip routing for @file
        // — daemon would need separate file-read support. Parse inline JSON.
        if (args.size() < 2 || args[1].startsWith('@'))
            return false;
        auto doc = QJsonDocument::fromJson(args[1].toUtf8());
        if (!doc.isObject())
            return false;
        payload = doc.object();
    } else {
        return false; // unknown action — let subprocess path handle it
    }

    PythonWorker::instance().submit(
        action, payload, [cb = std::move(cb), action](bool ok, QJsonObject result, QString error) {
            // Daemon may legitimately not know an action; fail cleanly so caller
            // sees a structured error instead of a silent hang.
            if (!ok) {
                cb({false, {}, error.isEmpty() ? ("daemon error for " + action) : error, -1});
                return;
            }
            PythonResult r;
            r.success = true;
            // PythonWorker wraps non-object results (e.g. the bare JSON array
            // that historical_period/historical/batch_sparklines return) under
            // "_value" so its object-callback API always hands back an object.
            // The string-output path callers (e.g. EquityResearchService) expect
            // the same raw JSON the subprocess path prints — a bare array, not a
            // {"_value":[...]} wrapper. Unwrap so both paths are byte-identical.
            if (result.size() == 1 && result.contains(QLatin1String("_value"))) {
                const QJsonValue v = result.value(QLatin1String("_value"));
                QJsonDocument doc;
                if (v.isArray())
                    doc.setArray(v.toArray());
                else if (v.isObject())
                    doc.setObject(v.toObject());
                else
                    doc = QJsonDocument::fromVariant(v.toVariant());
                r.output = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
            } else {
                r.output = QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Compact));
            }
            r.exit_code = 0;
            cb(r);
        });
    return true;
}

// ── Credential catalogue ─────────────────────────────────────────────────────
// Env-var names that SettingsScreen lets the user configure and store in
// SecureStorage (see SettingsScreen.cpp CRED_KEYS). build_python_env() pulls
// each from SecureStorage and injects it into the subprocess env so Python
// scripts that read os.environ.get("FRED_API_KEY") work whether or not the
// user has the variable in their shell. SecureStorage is the source of truth:
// the injected value overrides any inherited shell value.
//
// Keep this list in sync with SettingsScreen.cpp CRED_KEYS. C++-only services
// (Binance/Kraken/Polymarket) are listed too — injection is harmless when no
// Python script reads them, and it future-proofs new scripts.
static const QStringList kManagedCredentialKeys = {
    "ALPHA_VANTAGE_API_KEY", "POLYGON_API_KEY",    "DATABENTO_API_KEY", "FRED_API_KEY",       "NEWSAPI_KEY",
    "BINANCE_API_KEY",       "BINANCE_SECRET_KEY", "KRAKEN_API_KEY",    "KRAKEN_SECRET_KEY",  "IEX_CLOUD_TOKEN",
    "FINNHUB_API_KEY",       "TIINGO_API_KEY",     "QUANDL_API_KEY",    "POLYMARKET_API_KEY", "POLYMARKET_SECRET",
    "POLYMARKET_PASSPHRASE", "POLYMARKET_WALLET",
// Keyed data-connector provider keys (auto-generated; see MCP data_* tools /
// Settings › Credentials). Kept in sync with CredentialsSection.cpp CRED_KEYS
// via the same shared X-macro include.
#define FINCEPT_KEYED_CRED(KEY, NAME) KEY, // NOLINT(cppcoreguidelines-macro-usage) — X-macro list expansion
#include "config/KeyedConnectorCredentials.inc"
#undef FINCEPT_KEYED_CRED
};

// ── Sensitive shell-env stripping ────────────────────────────────────────────
// After SecureStorage injection, drop any *other* credential-shaped variable
// the user may have inherited from their shell. This prevents unrelated keys
// (e.g. a developer's GITHUB_TOKEN, AWS_SECRET_ACCESS_KEY) from leaking into
// Python subprocesses where they'd be visible via /proc/<pid>/environ on
// Linux or process inspection tools elsewhere.
//
// Suffix list narrowed vs. PR #214: we omit bare _TOKEN because legitimate
// non-credential vars use it (CSRF_TOKEN, GITHUB_TOKEN for tooling). The
// kManagedCredentialKeys allow-list is checked first so any key the user
// configured in Settings is preserved, regardless of suffix.
static void strip_unmanaged_credentials(QProcessEnvironment& env, const QStringList& managed) {
    static const QStringList kSuffixes = {
        "_API_KEY", "_SECRET", "_SECRET_KEY", "_ACCESS_TOKEN", "_AUTH_TOKEN", "_PASSWORD", "_PRIVATE_KEY",
    };
    const QStringList all_keys = env.keys();
    for (const QString& k : all_keys) {
        if (managed.contains(k))
            continue; // we just injected this one — keep it
        const QString upper = k.toUpper();
        for (const QString& sfx : kSuffixes) {
            if (upper.endsWith(sfx)) {
                env.remove(k);
                break;
            }
        }
    }
}

// ── Argument-spill opt-in ────────────────────────────────────────────────────
// start_next() can replace an oversized argv entry with "@<temp-path>" to dodge
// the Windows ~32 KB command-line limit. That only works for scripts that
// implement the convention (read the file, delete it, use its contents). It was
// applied unconditionally, so any *other* script handed a >8 KB payload — a
// portfolio config from PortfolioAnalyticsService, a backtest spec from
// BacktestingService, an option chain from OptionChainService, a Databento
// query — received the literal string "@C:\...\fincept_arg_*.json" and failed
// with "malformed JSON" on exactly the inputs that had grown past 8 KB.
//
// So spilling is opt-in: only the scripts verified below (grep `resolve_arg` /
// `startswith("@")` under scripts/) get it.
//
// PREFER stdin FOR NEW LARGE PAYLOADS. PythonRunner::run() takes `stdin_data`;
// it has no size limit, never touches disk, and is invisible to same-user
// process inspection. New scripts should take `--stdin` rather than grow another
// `@file` reader.
static bool script_supports_arg_spill(const QString& script, const QStringList& args) {
    // yfinance_data.py reads `@file` only for its batch_all action; every other
    // action takes plain scalar args and would choke on an "@..." string.
    if (script == QLatin1String("yfinance_data.py"))
        return !args.isEmpty() && args[0] == QLatin1String("batch_all");

    static const QStringList kSpillCapableScripts = {
        // scripts/*.py — resolve_arg()
        QStringLiteral("news_nlp.py"),
        QStringLiteral("news_correlation.py"),
        QStringLiteral("news_geolocation.py"),
        // scripts/Analytics/options/*.py — resolve_arg(), invoked via
        // MAAnalyticsService::run_python_json()
        QStringLiteral("Analytics/options/gex_calculator.py"),
        QStringLiteral("Analytics/options/iv_smile.py"),
        QStringLiteral("Analytics/options/iv_surface.py"),
        QStringLiteral("Analytics/options/oi_tracker.py"),
        QStringLiteral("Analytics/options/straddle_simulator.py"),
        QStringLiteral("Analytics/options/strategy_chart.py"),
        // scripts/agents/finagent_core/main.py — payload_str.startswith("@").
        // AgentService reaches it via its own stdin QProcess, not through here,
        // but the reader exists so the flag stays correct for any other caller.
        QStringLiteral("agents/finagent_core/main.py"),
    };
    return kSpillCapableScripts.contains(script);
}

// ── Watchdog budgets ─────────────────────────────────────────────────────────
// Every spawn gets a kill-timer. Without one, a script blocked on a socket
// holds one of the THREE concurrency slots forever; three such hangs wedge
// every Python-backed feature in the terminal until the app is restarted, and
// nothing is ever shown to the user because the callback simply never fires.
//
// This is a *floor* policy, not a per-script tuning knob: callers that know
// better pass RunOptions::timeout_ms (see BacktestingService). The table below
// only exists because a handful of scripts legitimately run for many minutes
// and their callers live in files this change does not touch.
static constexpr int kDefaultTimeoutMs = 300'000;  //  5 min — ordinary fetch/analytics
static constexpr int kLongRunTimeoutMs = 3'600'000; // 60 min — training / backtests

static int default_timeout_for_script(const QString& script) {
    // Prefix match on the script's directory. These trees train models, build
    // FAISS indexes, run multi-year backtests or drive multi-step LLM agents —
    // minutes is normal for them and 5 min would kill real work.
    //   vision_quant/setup_index.py  — CNN training + index build
    //   ai_quant_lab/qlib_rl.py      — RL training (streams progress for ages)
    //   ai_quant_lab/qlib_rolling_retraining.py
    //   Analytics/backtesting/*      — full backtests / walk-forward / optimise
    //   agents/*                     — LLM agent chains
    static const QStringList kLongRunPrefixes = {
        QStringLiteral("vision_quant/"),
        QStringLiteral("ai_quant_lab/"),
        QStringLiteral("agents/"),
        QStringLiteral("Analytics/backtesting/"),
    };
    for (const QString& p : kLongRunPrefixes) {
        if (script.startsWith(p))
            return kLongRunTimeoutMs;
    }
    return kDefaultTimeoutMs;
}

// ── Script-level error envelope ──────────────────────────────────────────────
// Scripts across the tree print a well-formed `{"error": "..."}` (often with
// `{"success": false}` alongside) to describe a failure. Two things went wrong
// with that before:
//   * on exit 0 the envelope was treated as a *successful* payload, so the
//     caller got success=true with data it could not use and rendered blank;
//   * on a non-zero exit the caller surfaced `result.error`, which was stderr —
//     and stderr is empty in exactly this case, so the real diagnostic sat
//     unread in `result.output`.
// Returns true and fills `*out` when the document carries a usable message.
//
// Deliberately NOT triggered by `success: false` alone: several providers emit
// `{"success": true, "data": {...}, "error": null}`, and BacktestingService
// already unwraps nested `success` flags itself with better messages.
static bool extract_error_envelope(const QJsonDocument& doc, QString* out) {
    if (!doc.isObject())
        return false;
    const QJsonObject obj = doc.object();
    const QJsonValue err = obj.value(QLatin1String("error"));
    if (err.isUndefined() || err.isNull())
        return false; // absent, or explicitly "no error"

    QString msg;
    if (err.isString())
        msg = err.toString();
    else if (err.isObject())
        msg = QString::fromUtf8(QJsonDocument(err.toObject()).toJson(QJsonDocument::Compact));
    else if (err.isArray())
        msg = QString::fromUtf8(QJsonDocument(err.toArray()).toJson(QJsonDocument::Compact));
    else
        msg = err.toVariant().toString();

    msg = msg.trimmed();
    if (msg.isEmpty() || msg == QLatin1String("{}") || msg == QLatin1String("[]"))
        return false; // `"error": ""` / `{}` is not a failure signal
    *out = msg;
    return true;
}

// Scripts that require NumPy 1.x environment
static const QStringList kNumpy1Scripts = {
    "vectorbt", "backtesting", "gluonts", "functime", "pyportfolioopt", "financepy", "ffn", "ffn_wrapper",
};

PythonRunner& PythonRunner::instance() {
    static PythonRunner s;
    return s;
}

PythonRunner::PythonRunner() {
    scripts_dir_ = find_scripts_dir();
    LOG_INFO("Python", "Scripts: " + scripts_dir_);

    // Try fast, non-blocking venv detection first
    python_path_ = find_python_sync();

    if (!python_path_.isEmpty()) {
        python_init_done_ = true;
        LOG_INFO("Python", "Python: " + python_path_);
    } else {
        // System python detection requires a process spawn — do it async
        find_python_async();
    }
}

bool PythonRunner::is_available() const {
    return !python_path_.isEmpty();
}

QString PythonRunner::python_path() const {
    return python_path_;
}
QString PythonRunner::scripts_dir() const {
    return scripts_dir_;
}

// ── Standard Python environment ──────────────────────────────────────────────
// Shared by PythonRunner::run() and external direct-QProcess callers
// (e.g., AgentService's stdin/stream paths). Anything every Python spawn needs
// — encoding pins, FINCEPT_DATA_DIR, FINAGENT_DATA_DIR, base PYTHONPATH — lives
// here. Script-specific path additions (parent-of-pkg) stay in run().
QProcessEnvironment PythonRunner::build_python_env() const {
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("PYTHONIOENCODING", "utf-8");
    env.insert("PYTHONDONTWRITEBYTECODE", "1");
    env.insert("PYTHONUNBUFFERED", "1");

    const QString install_dir = PythonSetupManager::instance().install_dir();
    env.insert("FINCEPT_DATA_DIR", install_dir);

    // FINAGENT_DATA_DIR: default to <FINCEPT_DATA_DIR>/finagent. Always overwrite
    // so we match what resources.py expects regardless of what the user shell
    // inherited — the app owns its own per-persona storage root.
    env.insert("FINAGENT_DATA_DIR", install_dir + "/finagent");

    // FINAGENT_RUNTIME_CACHE_SIZE: respect user override if already set; otherwise
    // leave unset so persona_registry.py uses its own default (8).
    // (No insert here by design.)

    // Base PYTHONPATH: scripts_dir on front, preserve whatever the user had.
#ifdef _WIN32
    const QChar kPathSep = ';';
#else
    const QChar kPathSep = ':';
#endif
    const QString existing_pypath = env.value("PYTHONPATH");
    QString new_pypath = existing_pypath.isEmpty() ? scripts_dir_ : (scripts_dir_ + kPathSep + existing_pypath);
    env.insert("PYTHONPATH", new_pypath);

    // Inject credentials stored via SettingsScreen → SecureStorage. SecureStorage
    // is the source of truth: any value found there overrides whatever the user's
    // shell happened to have. Without this, Python scripts like fred_data.py that
    // read os.environ.get("FRED_API_KEY") only worked when the user happened to
    // export the variable in their shell before launching the app.
    int injected = 0;
    for (const QString& key : kManagedCredentialKeys) {
        auto r = SecureStorage::instance().retrieve(key);
        if (r.is_ok() && !r.value().isEmpty()) {
            env.insert(key, r.value());
            ++injected;
        }
    }

    // Strip any other credential-shaped variable the user inherited from their
    // shell. The allow-list above is preserved; everything else matching the
    // sensitive suffixes is removed before the subprocess sees it.
    strip_unmanaged_credentials(env, kManagedCredentialKeys);

    if (injected > 0) {
        LOG_DEBUG("Python", QString("Injected %1 credentials from SecureStorage").arg(injected));
    }

    return env;
}

// ── Venv routing ─────────────────────────────────────────────────────────────
// Routes scripts to the correct venv based on their name.

static QString select_venv_for_script(const QString& script) {
    QString lower = script.toLower();
    for (const auto& keyword : kNumpy1Scripts) {
        if (lower.contains(keyword)) {
            return "venv-numpy1";
        }
    }
    return "venv-numpy2";
}

// ── Find Python ──────────────────────────────────────────────────────────────

// Fast, non-blocking detection: checks only file existence (no process spawns)
QString PythonRunner::find_python_sync() const {
    // 1. Use UV-managed venvs from PythonSetupManager (preferred)
    auto& setup = PythonSetupManager::instance();
    QString venv2_py = setup.python_path("venv-numpy2");
    if (QFileInfo::exists(venv2_py)) {
        LOG_INFO("Python", "Using UV-managed venv-numpy2: " + venv2_py);
        return venv2_py;
    }

    QString venv1_py = setup.python_path("venv-numpy1");
    if (QFileInfo::exists(venv1_py)) {
        LOG_INFO("Python", "Using UV-managed venv-numpy1: " + venv1_py);
        return venv1_py;
    }

    // 2. Check venv next to executable (dev / portable)
    QString exe_dir = QCoreApplication::applicationDirPath();
    for (const auto& venv : {"venv-numpy2", "venv-numpy1", "venv", ".venv"}) {
#ifdef _WIN32
        QString candidate = exe_dir + "/" + venv + "/Scripts/python.exe";
#else
        QString candidate = exe_dir + "/" + venv + "/bin/python3";
#endif
        if (QFileInfo::exists(candidate)) {
            return candidate;
        }
    }

    return {}; // No venv found — need async system python check
}

// Async detection: spawns `python --version` without blocking the UI thread
void PythonRunner::find_python_async() {
#ifdef _WIN32
    QString sys_python = "python";
#else
    QString sys_python = "python3";
#endif

    auto* proc = new QProcess(this);

#ifdef _WIN32
    proc->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* cpa) {
        cpa->flags |= 0x08000000; // CREATE_NO_WINDOW
    });
#endif

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this, proc, sys_python](int exit_code, QProcess::ExitStatus) {
                proc->deleteLater();
                if (exit_code == 0) {
                    python_path_ = sys_python;
                    LOG_INFO("Python", "System Python found: " + sys_python);
                } else {
                    LOG_WARN("Python", "No Python interpreter found");
                }
                python_init_done_ = true;
                if (!python_path_.isEmpty()) {
                    emit python_ready();
                }

                // Drain any requests that were queued while we were detecting
                start_next();
            });

    connect(proc, &QProcess::errorOccurred, this, [this, proc](QProcess::ProcessError) {
        proc->deleteLater();
        LOG_WARN("Python", "No Python interpreter found (process error)");
        python_init_done_ = true;

        // Drain requests queued during detection so they fail fast
        // ("Python not available") instead of sitting in the queue forever.
        start_next();
    });

    proc->start(sys_python, {"--version"});
}

// ── Find Scripts Directory ───────────────────────────────────────────────────

QString PythonRunner::find_scripts_dir() const {
    QString exe_dir = QCoreApplication::applicationDirPath();

    // Check relative to exe: scripts/, ../scripts/, etc.
    // macOS-canonical first: signed bundles must keep scripts/ in
    // Contents/Resources/ (not Contents/MacOS/) — anything other than
    // Mach-Os in MacOS/ makes codesign reject the bundle as malformed.
    QStringList candidates = {
        exe_dir + "/../Resources/scripts", // macOS canonical (.app/Contents/Resources/scripts)
        exe_dir + "/scripts",
        exe_dir + "/../scripts",
        exe_dir + "/../../scripts",
        exe_dir + "/../../../scripts",
        exe_dir + "/../../../../scripts", // deep dev: build/Release/../../scripts
        exe_dir + "/resources/scripts",
    };

    // Also check project root (for dev: fincept-qt/scripts)
    QDir dir(exe_dir);
    while (dir.cdUp()) {
        if (QFileInfo::exists(dir.filePath("scripts/yfinance_data.py"))) {
            return dir.filePath("scripts");
        }
        if (dir.isRoot())
            break;
    }

    for (const auto& c : candidates) {
        if (QFileInfo::exists(c + "/yfinance_data.py")) {
            return QDir::cleanPath(c);
        }
    }

    // Last resort: current directory
    if (QFileInfo::exists("scripts/yfinance_data.py")) {
        return QDir::cleanPath("scripts");
    }

    LOG_WARN("Python", "Scripts directory not found");
    return exe_dir + "/scripts";
}

// ── Run Script ───────────────────────────────────────────────────────────────

void PythonRunner::run(const QString& script, const QStringList& args, Callback cb, StreamCallback on_line,
                       const QByteArray& stdin_data) {
    RunOptions opts;
    opts.stdin_data = stdin_data;
    run_with_options(script, args, opts, std::move(cb), std::move(on_line));
}

void PythonRunner::run_with_options(const QString& script, const QStringList& args, const RunOptions& opts, Callback cb,
                                    StreamCallback on_line) {
    // Thread-affinity guard. PythonRunner is a QObject singleton living on
    // whatever thread first called instance() — in practice the main thread,
    // because main.cpp warms it at startup. But run() is invoked from
    // anywhere: UI code, services, and (critically) MCP tool handlers that
    // execute on worker threads. If a worker calls into start_next() it ends
    // up doing `new QProcess(this)` from a thread that's NOT this->thread(),
    // which Qt warns about and which leaves the QProcess with bad thread
    // affinity. Destruction of that QProcess later corrupts the heap and the
    // app crashes silently (no WER, no Qt fatal). Marshal back to our own
    // thread so the queue + QProcess creation always happens here.
    if (QThread::currentThread() != this->thread()) {
        Callback cb_copy = std::move(cb);
        StreamCallback on_line_copy = std::move(on_line);
        QMetaObject::invokeMethod(
            this,
            [this, script, args, opts, cb_copy = std::move(cb_copy),
             on_line_copy = std::move(on_line_copy)]() mutable {
                run_with_options(script, args, opts, std::move(cb_copy), std::move(on_line_copy));
            },
            Qt::QueuedConnection);
        return;
    }

    if (python_init_done_ && python_path_.isEmpty()) {
        cb({false, {}, "Python not available — run first-time setup from the app", -1});
        return;
    }

    QString script_path = scripts_dir_ + "/" + script;
    if (!QFileInfo::exists(script_path)) {
        cb({false, {}, "Script not found: " + script_path, -1});
        return;
    }

    // Daemon fast-path: yfinance_data.py runs as a long-lived worker
    // (PythonWorker). Routing single-shot calls through the daemon avoids the
    // ~3s subprocess + import cost and bypasses the max_concurrent_=3 queue
    // bottleneck. Only used when no stream callback is requested — the daemon
    // doesn't surface intermediate stdout lines. Falls through to subprocess
    // if the action isn't supported by the daemon dispatcher.
    if (!on_line && script == QLatin1String("yfinance_data.py")) {
        if (route_yfinance_to_daemon(args, cb))
            return;
    }

    // Queue the request and start if under concurrency limit.
    // kTimeoutFromScript defers to the per-tree table; an explicit 0 disables the
    // watchdog, which is why the sentinel is negative rather than 0.
    const int resolved_timeout =
        (opts.timeout_ms == kTimeoutFromScript) ? default_timeout_for_script(script) : opts.timeout_ms;

    queue_.enqueue({script, args, std::move(cb), std::move(on_line), opts.stdin_data,
                    script_supports_arg_spill(script, args), opts.expect_json, resolved_timeout});
    start_next();
}

/// Run arbitrary Python code (for notebook/colab cells).
/// Creates a temp file, executes it, returns output.
void PythonRunner::run_code(const QString& code, Callback cb) {
    // Same thread-affinity guard as run(). See the comment there.
    if (QThread::currentThread() != this->thread()) {
        Callback cb_copy = std::move(cb);
        QMetaObject::invokeMethod(
            this, [this, code, cb_copy = std::move(cb_copy)]() mutable { run_code(code, std::move(cb_copy)); },
            Qt::QueuedConnection);
        return;
    }

    if (python_init_done_ && python_path_.isEmpty()) {
        cb({false, {}, "Python not available — run first-time setup from the app", -1});
        return;
    }

    // Write code to a temp file. The filename must be unique even when several
    // run_code() calls fire within the same millisecond (e.g. on portfolio load
    // fetch_correlation + benchmark history + risk-free rate all kick off at
    // once). A bare millisecond timestamp collided, so one cell's completion
    // would delete the temp file out from under another still-pending cell
    // ("can't open file … No such file or directory"). A UUID guarantees
    // uniqueness regardless of timing.
    QString temp_dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString temp_path = temp_dir + "/fincept_cell_" + QString::number(QDateTime::currentMSecsSinceEpoch()) + "_" +
                        QUuid::createUuid().toString(QUuid::Id128) + ".py";

    QFile file(temp_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        cb({false, {}, "Failed to create temp script file", -1});
        return;
    }
    file.write(code.toUtf8());
    file.close();

    // Queue as a special request — use the temp file path directly
    queue_.enqueue({"__code__:" + temp_path, {}, std::move(cb), {}, {}, false});
    start_next();
}

void PythonRunner::start_next() {
    // Don't start if Python detection is still in progress
    if (!python_init_done_)
        return;

    while (active_count_ < max_concurrent_ && !queue_.isEmpty()) {
        auto req = queue_.dequeue();

        if (python_path_.isEmpty()) {
            // Python became unavailable — fail the request
            req.cb({false, {}, "Python not available", -1});
            continue;
        }

        ++active_count_;

        // Determine if this is inline code or a script file
        bool is_code = req.script.startsWith("__code__:");
        QString script_path;
        QString python_exe = python_path_;
        QString temp_file;

        if (is_code) {
            // Inline code execution (notebook cells)
            temp_file = req.script.mid(9); // strip "__code__:" prefix
            script_path = temp_file;
            // Always use venv-numpy2 for notebook code
            QString venv2 = PythonSetupManager::instance().python_path("venv-numpy2");
            if (QFileInfo::exists(venv2))
                python_exe = venv2;
        } else {
            script_path = scripts_dir_ + "/" + req.script;
            // Route to correct venv based on script name
            QString venv_name = select_venv_for_script(req.script);
            QString venv_py = PythonSetupManager::instance().python_path(venv_name);
            if (QFileInfo::exists(venv_py))
                python_exe = venv_py;
        }

        auto* proc = new QProcess(this);
        QStringList full_args;

        // If the script is inside a sub-package (contains '/'), use -m <module>
        // so Python can resolve relative imports (from .core import ...). This
        // only works if every directory along the path has an `__init__.py` —
        // otherwise the package's `__init__.py` may rely on absolute imports
        // (e.g. `from finagent_core.x import ...`) that resolve against the
        // top-level `sys.path` entry the bare-script invocation sets up via
        // its own `sys.path.insert(...)` bootstrap. When any ancestor dir is
        // missing `__init__.py`, fall back to direct script invocation so the
        // script's bootstrap can run before the failing import.
        bool use_module_form = false;
        if (!is_code && req.script.contains('/')) {
            use_module_form = true;
            QString check_path = scripts_dir_;
            QStringList parts = req.script.split('/');
            parts.removeLast(); // drop the file itself
            for (const QString& p : parts) {
                check_path += "/" + p;
                if (!QFileInfo::exists(check_path + "/__init__.py")) {
                    use_module_form = false;
                    break;
                }
            }
        }
        if (use_module_form) {
            QString module = req.script;
            module.remove(".py");
            module.replace('/', '.');
            full_args << "-m" << module;
        } else {
            full_args << script_path;
        }
        if (!is_code) {
            // Spill large args to temp files to avoid Windows 32KB command-line limit.
            // Python scripts support "@/path/to/file" — they read and delete the file.
            // OPT-IN ONLY: see script_supports_arg_spill(). Spilling for a script
            // that can't read "@file" turns a working call into a parse error.
            static constexpr int kArgSpillThreshold = 8192; // 8 KB
            QStringList spilled_files;
            QStringList safe_args;
            for (const QString& arg : req.args) {
                if (arg.size() > kArgSpillThreshold && req.supports_arg_spill) {
                    QString temp_dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
                    QString temp_path =
                        temp_dir + "/fincept_arg_" + QUuid::createUuid().toString(QUuid::WithoutBraces) + ".json";
                    QFile tf(temp_path);
                    if (tf.open(QIODevice::WriteOnly | QIODevice::Text)) {
                        // 0600 BEFORE the write. TempLocation is /tmp on Linux and
                        // macOS, where the default umask leaves the file 0644 —
                        // world-readable. Spilled payloads routinely carry LLM
                        // provider API keys, the Fincept session key and the MCP
                        // bridge token, so any local user could read them off disk
                        // for the lifetime of the file.
                        tf.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
                        tf.write(arg.toUtf8());
                        tf.close();
                        safe_args << ("@" + temp_path);
                        spilled_files << temp_path;
                    } else {
                        safe_args << arg; // fallback: pass as-is (may fail, but don't crash)
                    }
                } else {
                    if (arg.size() > kArgSpillThreshold) {
                        // Not spillable. Windows caps the whole command line at
                        // ~32 KB, so this can still fail to start — the fix is to
                        // move the payload to run()'s `stdin_data`, not to spill.
                        LOG_WARN("Python", QString("Script %1 passed a %2 KB argument on argv (no @file support) — "
                                                   "move this payload to stdin")
                                               .arg(req.script)
                                               .arg(arg.size() / 1024));
                    }
                    safe_args << arg;
                }
            }
            full_args.append(safe_args);
            // Store spilled paths so BOTH the finished and errorOccurred handlers
            // can sweep them. Scripts are supposed to delete the file in
            // resolve_arg(), but we must not depend on that — see the comment in
            // the finished handler.
            if (!spilled_files.isEmpty()) {
                // Attach to process via dynamic property for cleanup in both handlers
                proc->setProperty("spilled_files", QVariant::fromValue(spilled_files));
            }
        }

        // Start from the shared base env (encoding pins, FINCEPT_DATA_DIR,
        // FINAGENT_DATA_DIR, base PYTHONPATH = scripts_dir_).
        QProcessEnvironment env = build_python_env();

        // NOTE: do NOT prepend parent-of-pkg (e.g. <scripts>/mcp) to PYTHONPATH
        // for sub-package scripts. scripts_dir_ is already on PYTHONPATH (see
        // build_python_env line 141), so `python -m mcp.edgar.main` resolves
        // both the package itself AND its relative imports correctly.
        //
        // Adding parent-of-pkg used to *shadow* third-party libraries: if a
        // local sub-package shares a name with a venv package (e.g. our
        // `<scripts>/mcp/edgar/` vs the `edgartools` PyPI library which
        // exports module `edgar`), Python's `from edgar import Company` would
        // resolve to the local empty package and fail with `ImportError:
        // cannot import name 'Company' from 'edgar'`. Removing this block
        // fixes the SEC EDGAR tools — and any other tool whose Python deps
        // share a top-level name with a local package.

        proc->setProcessEnvironment(env);
        proc->setWorkingDirectory(scripts_dir_);

#ifdef _WIN32
        proc->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* cpa) {
            cpa->flags |= 0x08000000; // CREATE_NO_WINDOW
        });
#endif

        // Initialize incremental output buffers (carry stream callback from the request)
        proc_buffers_[proc] = {};
        proc_buffers_[proc].on_line = req.on_line;
        proc_buffers_[proc].start_ms = QDateTime::currentMSecsSinceEpoch();

        // Helper: drain complete \n-terminated lines from a buffer starting at `offset`,
        // invoke `on_line(line, is_stderr)` per line, advance `offset`. Trailing
        // partial line (no \n yet) stays in the buffer for the next read.
        auto drain_lines = [this, proc](bool is_stderr) {
            auto& bufs = proc_buffers_[proc];
            if (!bufs.on_line)
                return;
            QByteArray& buf = is_stderr ? bufs.stderr_buf : bufs.stdout_buf;
            int& off = is_stderr ? bufs.stderr_streamed : bufs.stdout_streamed;
            while (true) {
                int nl = buf.indexOf('\n', off);
                if (nl < 0)
                    break;
                // Line is buf[off..nl) — exclude the trailing \n; also trim \r for CRLF
                QByteArray line = buf.mid(off, nl - off);
                if (line.endsWith('\r'))
                    line.chop(1);
                off = nl + 1;
                bufs.on_line(QString::fromUtf8(line), is_stderr);
            }
        };

        // Buffer stdout/stderr incrementally. After each append, drain complete lines
        // to the optional stream callback. The full buffer is still available for the
        // `finished` handler's authoritative parse.
        connect(proc, &QProcess::readyReadStandardOutput, this, [this, proc, drain_lines]() {
            proc_buffers_[proc].stdout_buf.append(proc->readAllStandardOutput());
            drain_lines(false);
        });
        connect(proc, &QProcess::readyReadStandardError, this, [this, proc, drain_lines]() {
            proc_buffers_[proc].stderr_buf.append(proc->readAllStandardError());
            drain_lines(true);
        });

        // Share cb across both slots and gate on a once-flag so exactly one of
        // finished/errorOccurred runs cleanup + cb. (cb was previously MOVED into
        // the finished lambda, leaving errorOccurred holding an empty std::function
        // → std::bad_function_call → app crash on any subprocess FailedToStart or
        // Crash, e.g. a broken venv python or a segfault in numpy/torch/faiss.)
        auto cb = std::make_shared<Callback>(std::move(req.cb));
        auto handled = std::make_shared<std::atomic_bool>(false);
        auto script_name = std::move(req.script);

        connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
                [this, proc, cb, handled, script_name, is_code, temp_file, expect_json = req.expect_json](
                    int exit_code, QProcess::ExitStatus) {
                    if (handled->exchange(true))
                        return; // errorOccurred already handled this proc
                    // Collect any remaining buffered data
                    auto& bufs = proc_buffers_[proc];
                    bufs.stdout_buf.append(proc->readAllStandardOutput());
                    bufs.stderr_buf.append(proc->readAllStandardError());

                    const qint64 duration_ms =
                        bufs.start_ms > 0 ? QDateTime::currentMSecsSinceEpoch() - bufs.start_ms : 0;

                    QString stdout_str = QString::fromUtf8(bufs.stdout_buf);
                    QString stderr_str = QString::fromUtf8(bufs.stderr_buf);

                    proc_buffers_.remove(proc);

                    // Clean up any spilled arg temp files. The @file convention says
                    // the script deletes them, but that only holds when the script
                    // actually reached its resolve_arg() (it may have exited early,
                    // crashed, or been handed the file for an argument it ignores).
                    // Only errorOccurred used to sweep these, so every *successful*
                    // spilling call leaked one temp file — containing the payload —
                    // into TempLocation for the life of the machine.
                    for (const QString& f : proc->property("spilled_files").toStringList())
                        QFile::remove(f);

                    proc->deleteLater();

                    // Clean up temp file for inline code
                    if (is_code && !temp_file.isEmpty()) {
                        QFile::remove(temp_file);
                    }

                    PythonResult result;
                    result.exit_code = exit_code;

                    if (is_code) {
                        // For notebook cells: return raw stdout, not JSON-extracted
                        result.success = (exit_code == 0);
                        result.output = std::move(stdout_str);
                        result.error = std::move(stderr_str);
                    } else {
                        // For scripts: extract JSON from output.
                        //
                        // Success used to be `exit_code == 0 && !json_out.isEmpty()`,
                        // which laundered two distinct failures into empty success:
                        //   * a script printing {"error": "rate limited"} and exiting 0
                        //   * extract_json's fallback returning an UNBALANCED fragment
                        //     (first bracket → EOF) that never parses
                        // Callers then saw success-with-no-rows and rendered a blank
                        // screen instead of falling back to cached data.
                        QString json_out = extract_json(stdout_str);
                        result.output = json_out.isEmpty() ? stdout_str : json_out;
                        result.error = std::move(stderr_str);
                        result.success = (exit_code == 0);

                        if (expect_json) {
                            QJsonParseError pe{};
                            const QJsonDocument doc = QJsonDocument::fromJson(json_out.toUtf8(), &pe);
                            const bool parsed = !json_out.isEmpty() && pe.error == QJsonParseError::NoError;

                            if (result.success && !parsed) {
                                result.success = false;
                                if (result.error.isEmpty()) {
                                    result.error = json_out.isEmpty()
                                                       ? QStringLiteral("script produced no JSON payload")
                                                       : QStringLiteral("script produced malformed JSON: %1")
                                                             .arg(pe.errorString());
                                }
                            }

                            // A script-level {"error": ...} envelope is a failure even
                            // on exit 0 — and on a NON-zero exit it is the only useful
                            // diagnostic, because result.error is stderr and these
                            // scripts write their message to stdout. ~412 sites do this.
                            QString envelope;
                            if (parsed && extract_error_envelope(doc, &envelope)) {
                                result.success = false;
                                result.error = envelope;
                            }
                        }
                    }

                    if (!result.success && !is_code) {
                        LOG_ERROR("Python", QString("Script %1 failed in %2ms (exit=%3): %4")
                                                .arg(script_name)
                                                .arg(duration_ms)
                                                .arg(exit_code)
                                                .arg(result.error.left(200)));
                    } else if (!is_code) {
                        LOG_DEBUG("Python", QString("Script %1 finished in %2ms").arg(script_name).arg(duration_ms));
                    }

                    (*cb)(std::move(result));

                    --active_count_;
                    start_next(); // drain queue
                });

        connect(proc, &QProcess::errorOccurred, this,
                [this, proc, cb, handled, is_code, temp_file](QProcess::ProcessError) {
                    if (handled->exchange(true))
                        return; // finished already handled this proc
                    QString error_msg = proc->errorString();
                    proc_buffers_.remove(proc);
                    // Clean up any spilled arg temp files
                    auto spilled = proc->property("spilled_files").toStringList();
                    for (const QString& f : spilled)
                        QFile::remove(f);
                    proc->deleteLater();
                    if (is_code && !temp_file.isEmpty())
                        QFile::remove(temp_file);
                    (*cb)({false, {}, "Process error: " + error_msg, -1});

                    --active_count_;
                    start_next(); // drain queue
                });

        LOG_INFO("Python", QString("Running (%1/%2 active, %3 queued): %4 %5")
                               .arg(active_count_)
                               .arg(max_concurrent_)
                               .arg(queue_.size())
                               .arg(python_exe)
                               .arg(script_path));
        proc->start(python_exe, full_args);

        // Secret / large payloads arrive on stdin instead of argv. QProcess
        // buffers the write until the channel is actually open, so this is safe
        // immediately after start() and needs no waitForStarted() (P1).
        //
        // The close is GATED on non-empty stdin_data on purpose. Every existing
        // caller passes nothing and none of them writes to the child's stdin, so
        // leaving the channel untouched in that case keeps the streaming
        // (on_line) and daemon-fallback paths byte-for-byte as they are. Only a
        // caller that opts into stdin gets the EOF it is waiting for.
        if (!req.stdin_data.isEmpty()) {
            proc->write(req.stdin_data);
            proc->closeWriteChannel();
        }

        // Runaway-kill watchdog.
        //
        // This used to be deliberately absent, on the reasoning that several
        // scripts legitimately run for minutes. True — but the consequence was
        // that a script blocked on a socket held one of only max_concurrent_ (3)
        // slots FOREVER, so three hangs silently killed every Python-backed
        // feature in the terminal until restart, with no error surfaced. The
        // supposed cover in mcp/tools/PythonTools.cpp resolves the promise, never
        // the QProcess, so the slot stayed occupied either way.
        //
        // The objection is answered by the budget being per-script-tree rather
        // than blanket (default_timeout_for_script: 60 min for training/backtest/
        // agent trees, 5 min otherwise), and by callers being able to override or
        // disable it via RunOptions::timeout_ms.
        //
        // kill() drives the normal errorOccurred/finished path, so the slot is
        // released and the caller gets a real failure instead of silence.
        if (req.timeout_ms > 0) {
            auto* watchdog = new QTimer(proc);
            watchdog->setSingleShot(true);
            watchdog->setInterval(req.timeout_ms);
            const QString wd_script = req.script;
            const int wd_budget = req.timeout_ms;
            connect(watchdog, &QTimer::timeout, proc, [proc, wd_script, wd_budget]() {
                if (proc->state() == QProcess::NotRunning)
                    return;
                LOG_ERROR("Python", QString("Killing %1 — exceeded %2 ms watchdog budget").arg(wd_script).arg(wd_budget));
                proc->kill();
            });
            watchdog->start();
        }
    }
}

// ── Extract JSON ─────────────────────────────────────────────────────────────

QString extract_json(const QString& output) {
    // Return the LAST complete top-level JSON value in the output.
    //
    // Script/agent stdout is frequently polluted with leading log lines — some
    // of which contain braces (e.g. a Python dict repr like
    // "ERROR ... {'error': {...}, 'status': 401}" leaked onto stdout by a
    // third-party logger). The old "first '{' to end" logic grabbed that dict
    // repr and QJsonDocument::fromJson() then returned null → the caller saw a
    // spurious "exit=0" failure. Our scripts always print their real JSON
    // envelope LAST, so we scan forward tracking brace/bracket depth (respecting
    // double-quoted strings) and keep the final value whose depth returns to 0.
    int best_start = -1, best_end = -1; // [start, end) of the last balanced value
    int cur_start = -1, depth = 0;
    bool in_str = false, esc = false;
    for (int i = 0; i < output.size(); ++i) {
        const QChar c = output[i];
        if (in_str) {
            if (esc)
                esc = false;
            else if (c == '\\')
                esc = true;
            else if (c == '"')
                in_str = false;
            continue;
        }
        if (c == '"') {
            in_str = true;
            continue;
        }
        if (depth == 0) {
            if (c == '{' || c == '[') {
                cur_start = i;
                depth = 1;
            }
        } else {
            if (c == '{' || c == '[') {
                ++depth;
            } else if (c == '}' || c == ']') {
                if (--depth == 0) {
                    best_start = cur_start;
                    best_end = i + 1;
                }
            }
        }
    }

    if (best_start >= 0)
        return output.mid(best_start, best_end - best_start).trimmed();

    // Fallback: original heuristic (first bracket to end) for partial output.
    int brace = output.indexOf('{');
    int bracket = output.indexOf('[');
    int start = -1;
    if (brace >= 0 && bracket >= 0)
        start = qMin(brace, bracket);
    else if (brace >= 0)
        start = brace;
    else if (bracket >= 0)
        start = bracket;
    if (start < 0)
        return {};
    return output.mid(start).trimmed();
}

} // namespace fincept::python
