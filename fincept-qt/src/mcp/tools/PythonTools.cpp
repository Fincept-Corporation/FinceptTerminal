// PythonTools.cpp — Run Python analytics scripts (Qt port)

#include "mcp/tools/PythonTools.h"

#include "core/logging/Logger.h"
#include "mcp/AsyncDispatch.h"
#include "mcp/ToolSchemaBuilder.h"
#include "mcp/tools/ThreadHelper.h"
#include "python/PythonRunner.h"

#include <QDir>
#include <QJsonDocument>
#include <QPromise>
#include <QRegularExpression>

#include <algorithm>
#include <memory>

namespace fincept::mcp::tools {

static constexpr const char* TAG = "PythonTools";

// Synchronously run a Python script. Tool handlers run on a worker thread;
// PythonRunner's QProcess lives on the main thread. We marshal the run()
// call onto the runner's thread (see mcp/tools/ThreadHelper.h).
[[maybe_unused]] static ToolResult run_script_sync(const QString& script, const QStringList& args) {
    if (!python::PythonRunner::instance().is_available())
        return ToolResult::fail("Python is not available — run setup first");

    ToolResult out;
    auto* runner = &python::PythonRunner::instance();
    detail::run_async_wait(runner, [&](auto signal_done) {
        runner->run(script, args, [&, signal_done](python::PythonResult result) {
            if (!result.success) {
                out = ToolResult::fail("Script failed: " + result.error);
            } else {
                QString json_text = python::extract_json(result.output);
                if (json_text.isEmpty())
                    json_text = result.output;

                QJsonDocument doc = QJsonDocument::fromJson(json_text.toUtf8());
                if (!doc.isNull()) {
                    if (doc.isObject())
                        out = ToolResult::ok_data(doc.object());
                    else if (doc.isArray())
                        out = ToolResult::ok_data(doc.array());
                    else
                        out = ToolResult::ok(result.output);
                } else {
                    out = ToolResult::ok(result.output);
                }
            }
            signal_done();
        });
    });

    return out;
}

std::vector<ToolDef> get_python_tools() {
    std::vector<ToolDef> tools;

    // ── run_python_script ──────────────────────────────────────────────
    // Phase 4: async exemplar. Previously used run_script_sync (which
    // blocked the worker thread for the entire script run via
    // QMutex+QWaitCondition). Now the handler returns immediately; the
    // promise resolves when PythonRunner's QProcess::finished signal
    // fires. Provider's timeout watchdog covers runaway scripts.
    {
        ToolDef t;
        t.name = "run_python_script";
        t.description = "Execute a Python analytics script by name. IMPORTANT: only pass script names returned by "
                        "list_python_scripts — do not invent or guess script names. For market data prefer "
                        "get_quote / get_candles / edgar_* tools rather than Python scripts.";
        t.category = "analytics";
        t.input_schema =
            ToolSchemaBuilder()
                .string("script", "Script name (without .py) — must be returned by list_python_scripts")
                .required()
                .pattern("^[a-zA-Z0-9_-]+$")
                .length(1, 128)
                .array("args", "Array of string arguments to pass to the script", QJsonObject{{"type", "string"}})
                .build();
        // Python scripts can take a while; allow a generous default. Override
        // per-call via _meta.timeout_ms when Phase 6 wires that through.
        t.default_timeout_ms = 60000;
        // Phase 6.3: arbitrary script execution must be gated. Even with the
        // regex pattern check on script name, the script can read/write
        // files, hit the network, etc. Always confirm.
        t.auth_required = AuthLevel::Authenticated;
        t.is_destructive = true;
        t.async_handler = [](const QJsonObject& args_obj, ToolContext ctx,
                             std::shared_ptr<QPromise<ToolResult>> promise) {
            QString script = args_obj["script"].toString().trimmed();
            // Schema + list_python_scripts expose bare names (no .py), but
            // PythonRunner::run() resolves scripts_dir_ + "/" + script with an
            // exact existence check and never appends .py — normalize here.
            if (!script.endsWith(".py"))
                script += ".py";
            QStringList script_args;
            if (args_obj.contains("args") && args_obj["args"].isArray()) {
                for (const auto& a : args_obj["args"].toArray()) {
                    // The schema says "array of strings" but items are not type-checked,
                    // and models pass numbers/booleans all the time. Numbers and bools
                    // used to fall into the object branch, where `toObject()` of a scalar
                    // is an empty object — the script silently received "{}" instead of
                    // the value. Convert each JSON type to the argv text it stands for.
                    if (a.isString())
                        script_args.append(a.toString());
                    else if (a.isBool())
                        script_args.append(a.toBool() ? QStringLiteral("true") : QStringLiteral("false"));
                    else if (a.isDouble())
                        script_args.append(QString::number(a.toDouble(), 'g', 15));
                    else if (a.isObject())
                        script_args.append(QString::fromUtf8(QJsonDocument(a.toObject()).toJson(QJsonDocument::Compact)));
                    else if (a.isArray())
                        script_args.append(QString::fromUtf8(QJsonDocument(a.toArray()).toJson(QJsonDocument::Compact)));
                    // null / undefined: no argv text to pass
                }
            }

            if (!python::PythonRunner::instance().is_available()) {
                // Resolve through the shared single-winner guard (ctx.resolve_guard), not a
                // bare addResult(): the provider's watchdog races the same flag.
                AsyncDispatch::callback_to_promise(nullptr, ctx, promise, [](auto resolve) {
                    resolve(ToolResult::fail("Python is not available — run setup first"));
                });
                return;
            }

            LOG_INFO(TAG, QString("Running script: %1 with %2 args").arg(script).arg(script_args.size()));

            auto* runner = &python::PythonRunner::instance();
            AsyncDispatch::callback_to_promise(runner, ctx, promise, [runner, script, script_args, ctx](auto resolve) {
                // Forward the script's own output as job progress. Under a job
                // this is the only thing that separates "still working" from
                // "hung": the tool is one opaque call, so its stdout is the
                // sole evidence anything is happening.
                runner->run(script, script_args, [resolve, ctx](python::PythonResult result) {
                    if (ctx.cancelled()) {
                        resolve(ToolResult::fail("cancelled"));
                        return;
                    }
                    if (!result.success) {
                        resolve(ToolResult::fail("Script failed: " + result.error));
                        return;
                    }
                    QString json_text = python::extract_json(result.output);
                    if (json_text.isEmpty())
                        json_text = result.output;

                    QJsonDocument doc = QJsonDocument::fromJson(json_text.toUtf8());
                    if (!doc.isNull()) {
                        if (doc.isObject())
                            resolve(ToolResult::ok_data(doc.object()));
                        else if (doc.isArray())
                            resolve(ToolResult::ok_data(doc.array()));
                        else
                            resolve(ToolResult::ok(result.output));
                    } else {
                        resolve(ToolResult::ok(result.output));
                    }
                }, AsyncDispatch::line_progress_bridge(ctx));
            });
        };
        tools.push_back(std::move(t));
    }

    // ── list_python_scripts ────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "list_python_scripts";
        t.description = "List the runnable Python analytics scripts in the scripts/ directory (there are hundreds — "
                        "pass `query` to filter by name; results are capped by `limit`).";
        t.category = "analytics";
        t.input_schema = ToolSchemaBuilder()
                             .string("query", "Case-insensitive substring to filter script names (e.g. 'yfinance')")
                             .default_str("")
                             .length(0, 64)
                             .integer("limit", "Max scripts to return")
                             .default_int(100)
                             .between(1, 500)
                             .build();
        t.handler = [](const QJsonObject& args) -> ToolResult {
            QString scripts_dir = python::PythonRunner::instance().scripts_dir();
            QDir dir(scripts_dir);

            if (!dir.exists())
                return ToolResult::fail("Scripts directory not found: " + scripts_dir);

            const QString needle = args["query"].toString().trimmed().toLower();
            const int limit = std::clamp(args["limit"].toInt(100), 1, 500);

            // Only offer names run_python_script will accept (its `script` pattern), so
            // the model is never handed a script the validator then refuses.
            static const QRegularExpression runnable(QStringLiteral("^[a-zA-Z0-9_-]+$"));

            QStringList py_files = dir.entryList({"*.py"}, QDir::Files, QDir::Name);
            QJsonArray result;
            int matched = 0;
            for (const auto& f : py_files) {
                QString name = f;
                name.chop(3); // remove .py
                if (!runnable.match(name).hasMatch())
                    continue;
                if (!needle.isEmpty() && !name.toLower().contains(needle))
                    continue;
                ++matched;
                if (result.size() < limit)
                    result.append(QJsonObject{{"name", name}, {"filename", f}});
            }
            // The unfiltered list is ~365 names — far over the result budget, and the
            // overflow shaper keeps the alphabetically-first ones. Say so, and tell the
            // model how to see the rest, rather than letting a clipped list read as complete.
            if (matched > result.size())
                return ToolResult::ok(QStringLiteral("Showing %1 of %2 matching scripts — pass `query` to narrow the "
                                                     "list or raise `limit` (max 500).")
                                          .arg(result.size())
                                          .arg(matched),
                                      result);
            return ToolResult::ok_data(result);
        };
        tools.push_back(std::move(t));
    }

    // ── check_python_status ────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "check_python_status";
        t.description = "Check if Python is available and what scripts directory is in use.";
        t.category = "analytics";
        t.handler = [](const QJsonObject&) -> ToolResult {
            auto& runner = python::PythonRunner::instance();
            return ToolResult::ok_data(QJsonObject{{"python_available", runner.is_available()},
                                                   {"python_path", runner.python_path()},
                                                   {"scripts_dir", runner.scripts_dir()}});
        };
        tools.push_back(std::move(t));
    }

    return tools;
}

} // namespace fincept::mcp::tools
