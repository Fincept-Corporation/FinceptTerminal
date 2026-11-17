// src/services/backtesting/BacktestRunProtocol.cpp
#include "services/backtesting/BacktestRunProtocol.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonValue>
#include <QLocale>
#include <QStringList>

#include <cmath>

namespace fincept::services::backtest {

namespace {

QString bt_proto_tr(const char* text) {
    return QCoreApplication::translate("fincept::services::backtest::BtProgress", text);
}

qint64 bt_proto_int(const QJsonValue& v, qint64 fallback) {
    if (!v.isDouble())
        return fallback;
    const double d = v.toDouble();
    if (!std::isfinite(d))
        return fallback;
    return static_cast<qint64>(std::llround(d));
}

double bt_proto_num(const QJsonValue& v) {
    if (!v.isDouble())
        return std::numeric_limits<double>::quiet_NaN();
    const double d = v.toDouble();
    return std::isfinite(d) ? d : std::numeric_limits<double>::quiet_NaN();
}

QString bt_proto_first_line(const QString& text) {
    const QString t = text.trimmed();
    const qsizetype nl = t.indexOf(QLatin1Char('\n'));
    return (nl < 0 ? t : t.left(nl)).trimmed().left(400);
}

/// details{traceback, logs[], symbol} of a failure envelope -> technical text.
QString bt_proto_details_text(const QJsonObject& root) {
    QStringList parts;
    const QJsonValue dv = root.value(QStringLiteral("details"));
    if (dv.isObject()) {
        const QJsonObject d = dv.toObject();
        const QString tb = d.value(QStringLiteral("traceback")).toString();
        if (!tb.trimmed().isEmpty())
            parts << tb.trimmed();
        const QJsonArray logs = d.value(QStringLiteral("logs")).toArray();
        QStringList log_lines;
        for (const QJsonValue& l : logs)
            if (l.isString() && !l.toString().trimmed().isEmpty())
                log_lines << l.toString().trimmed();
        if (!log_lines.isEmpty())
            parts << log_lines.join(QLatin1Char('\n'));
    } else if (dv.isString() && !dv.toString().trimmed().isEmpty()) {
        parts << dv.toString().trimmed();
    }
    // Legacy replies put the traceback at the top level.
    const QString tb = root.value(QStringLiteral("traceback")).toString();
    if (!tb.trimmed().isEmpty() && !parts.contains(tb.trimmed()))
        parts << tb.trimmed();
    return parts.join(QStringLiteral("\n\n"));
}

QString bt_proto_symbol(const QJsonObject& root) {
    const QJsonValue dv = root.value(QStringLiteral("details"));
    if (dv.isObject()) {
        const QString s = dv.toObject().value(QStringLiteral("symbol")).toString();
        if (!s.isEmpty())
            return s;
    }
    return root.value(QStringLiteral("symbol")).toString();
}

bool bt_proto_is_cancel_text(const QString& runner_error) {
    const QString e = runner_error.trimmed();
    return e.compare(QLatin1String("Cancelled"), Qt::CaseInsensitive) == 0 ||
           e.startsWith(QLatin1String("Cancelled by user"), Qt::CaseInsensitive);
}

bool bt_proto_is_timeout_text(const QString& runner_error) {
    return runner_error.contains(QLatin1String("timed out"), Qt::CaseInsensitive);
}

BtOutcome bt_proto_failed(const QString& code, const QString& message, const QString& technical) {
    BtOutcome o;
    o.kind = BtOutcome::Kind::Failed;
    o.error_code = code;
    o.message = message;
    o.technical = technical;
    return o;
}

} // namespace

double BtProgress::fraction() const {
    if (!determinate())
        return std::numeric_limits<double>::quiet_NaN();
    const double f = static_cast<double>(done) / static_cast<double>(total);
    return f < 0 ? 0.0 : (f > 1 ? 1.0 : f);
}

QString progress_prefix() {
    return QStringLiteral("@@BT_PROGRESS ");
}

std::optional<BtProgress> parse_progress_line(const QString& line) {
    static const QString prefix = progress_prefix();
    qsizetype start = 0;
    while (start < line.size() && line.at(start).isSpace())
        ++start;
    if (line.size() - start < prefix.size() || QStringView(line).mid(start, prefix.size()) != prefix)
        return std::nullopt;
    const QByteArray json = QStringView(line).mid(start + prefix.size()).trimmed().toUtf8();
    QJsonParseError pe{};
    const QJsonDocument doc = QJsonDocument::fromJson(json, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject())
        return std::nullopt;
    const QJsonObject o = doc.object();
    BtProgress p;
    p.phase = o.value(QStringLiteral("phase")).toString();
    p.done = bt_proto_int(o.value(QStringLiteral("done")), -1);
    p.total = bt_proto_int(o.value(QStringLiteral("total")), -1);
    if (p.total <= 0)
        p.total = -1;
    p.message = o.value(QStringLiteral("message")).toString().simplified().left(200);
    p.best = bt_proto_num(o.value(QStringLiteral("best")));
    p.eta_sec = bt_proto_num(o.value(QStringLiteral("etaSec")));
    if (std::isfinite(p.eta_sec) && p.eta_sec < 0)
        p.eta_sec = std::numeric_limits<double>::quiet_NaN();
    p.workers = static_cast<int>(bt_proto_int(o.value(QStringLiteral("workers")), -1));
    if (p.phase.isEmpty() && p.done < 0 && p.message.isEmpty())
        return std::nullopt; // an object, but nothing a progress reader can use
    return p;
}

QString progress_phase_label(const QString& phase) {
    if (phase == QLatin1String("data"))
        return bt_proto_tr("Loading market data");
    if (phase == QLatin1String("run"))
        return bt_proto_tr("Running the strategy");
    if (phase == QLatin1String("optimize"))
        return bt_proto_tr("Optimising");
    if (phase == QLatin1String("walk_forward"))
        return bt_proto_tr("Walk-forward");
    if (phase == QLatin1String("monte_carlo"))
        return bt_proto_tr("Simulating paths");
    if (phase == QLatin1String("sweep"))
        return bt_proto_tr("Sweeping parameters");
    if (phase == QLatin1String("metrics"))
        return bt_proto_tr("Computing metrics");
    return phase;
}

QString format_eta(double seconds) {
    if (!std::isfinite(seconds) || seconds < 0)
        return {};
    const qint64 s = static_cast<qint64>(std::llround(seconds));
    if (s < 60)
        return bt_proto_tr("%1 s").arg(s);
    if (s < 3600)
        return bt_proto_tr("%1 min %2 s").arg(s / 60).arg(s % 60, 2, 10, QLatin1Char('0'));
    return bt_proto_tr("%1 h %2 min").arg(s / 3600).arg((s % 3600) / 60, 2, 10, QLatin1Char('0'));
}

QString progress_text(const BtProgress& p) {
    const QLocale loc;
    QString text = p.message.isEmpty() ? progress_phase_label(p.phase) : p.message;
    if (p.determinate()) {
        const QString counts = QStringLiteral("%1 / %2").arg(loc.toString(p.done), loc.toString(p.total));
        // Engines often put the counts in their message already ("Optimising 240 / 500").
        const QString plain = QStringLiteral("%1 / %2").arg(p.done).arg(p.total);
        if (!text.contains(counts) && !text.contains(plain))
            text += QStringLiteral(" · ") + counts;
    }
    if (std::isfinite(p.best))
        text += QStringLiteral(" · ") + bt_proto_tr("best %1").arg(loc.toString(p.best, 'f', 2));
    const QString eta = format_eta(p.eta_sec);
    if (!eta.isEmpty())
        text += QStringLiteral(" · ") + bt_proto_tr("ETA %1").arg(eta);
    if (p.workers > 1)
        text += QStringLiteral(" · ") + bt_proto_tr("%1 workers").arg(p.workers);
    return text;
}

QString strip_progress_lines(const QString& stderr_text) {
    if (!stderr_text.contains(QLatin1String("@@BT_PROGRESS")))
        return stderr_text.trimmed();
    QStringList keep;
    const QStringList lines = stderr_text.split(QLatin1Char('\n'));
    for (const QString& l : lines) {
        if (l.trimmed().startsWith(QLatin1String("@@BT_PROGRESS")))
            continue;
        keep << l;
    }
    return keep.join(QLatin1Char('\n')).trimmed();
}

BtOutcome classify_run_output(bool runner_success, int exit_code, const QString& runner_error,
                              const QByteArray& stdout_json) {
    const QString diag = strip_progress_lines(runner_error);
    QJsonObject root;
    bool parsed = false;
    if (!stdout_json.trimmed().isEmpty()) {
        QJsonParseError pe{};
        const QJsonDocument doc = QJsonDocument::fromJson(stdout_json, &pe);
        if (pe.error == QJsonParseError::NoError && doc.isObject()) {
            root = doc.object();
            parsed = true;
        }
    }
    const bool cancel_exit = exit_code == kCancelExitCode;

    if (parsed && root.contains(QStringLiteral("success"))) {
        const bool success = root.value(QStringLiteral("success")).toBool(false);
        if (!success) {
            BtOutcome o;
            o.envelope = root;
            o.error_code = root.value(QStringLiteral("errorCode")).toString();
            o.symbol = bt_proto_symbol(root);
            o.technical = bt_proto_details_text(root);
            if (o.technical.isEmpty())
                o.technical = diag;
            o.message = root.value(QStringLiteral("error")).toString().trimmed();
            if (o.message.isEmpty())
                o.message = root.value(QStringLiteral("message")).toString().trimmed();
            const bool cancelled = o.error_code == QLatin1String("CANCELLED") ||
                                   root.value(QStringLiteral("cancelled")).toBool(false) || cancel_exit;
            if (cancelled) {
                o.kind = BtOutcome::Kind::Cancelled;
                o.error_code = QStringLiteral("CANCELLED");
                if (o.message.isEmpty())
                    o.message = bt_proto_tr("Cancelled");
                return o;
            }
            o.kind = BtOutcome::Kind::Failed;
            if (o.error_code.isEmpty())
                o.error_code = QStringLiteral("ENGINE");
            if (o.message.isEmpty())
                o.message = bt_proto_tr("The engine reported a failure without a message.");
            return o;
        }
        // success: true — only from a process that ended normally.
        if (cancel_exit || bt_proto_is_cancel_text(runner_error)) {
            BtOutcome o;
            o.kind = BtOutcome::Kind::Cancelled;
            o.error_code = QStringLiteral("CANCELLED");
            o.message = bt_proto_tr("Cancelled");
            o.envelope = root;
            return o;
        }
        if (exit_code != 0 && !runner_success) {
            BtOutcome o = bt_proto_failed(QStringLiteral("PROCESS"),
                                          bt_proto_tr("The engine process ended with exit code %1 after printing "
                                                      "its result; the result was discarded.")
                                              .arg(exit_code),
                                          diag);
            o.envelope = root;
            return o;
        }
        const QJsonValue dv = root.value(QStringLiteral("data"));
        QJsonObject data = dv.isObject() ? dv.toObject() : root;
        // Old optimise / walk-forward replies nested the failure one level down.
        if (data.contains(QStringLiteral("success")) && data.value(QStringLiteral("success")).isBool() &&
            !data.value(QStringLiteral("success")).toBool()) {
            BtOutcome o;
            o.kind = BtOutcome::Kind::Failed;
            o.envelope = root;
            o.error_code = data.value(QStringLiteral("errorCode")).toString(QStringLiteral("ENGINE"));
            o.message = data.value(QStringLiteral("error")).toString().trimmed();
            if (o.message.isEmpty())
                o.message = data.value(QStringLiteral("message")).toString(bt_proto_tr("The command failed.")).trimmed();
            o.technical = bt_proto_details_text(data);
            if (o.technical.isEmpty())
                o.technical = diag;
            return o;
        }
        BtOutcome o;
        o.kind = BtOutcome::Kind::Ok;
        o.envelope = root;
        o.data = data;
        return o;
    }

    // No envelope.
    if (cancel_exit || bt_proto_is_cancel_text(runner_error)) {
        BtOutcome o;
        o.kind = BtOutcome::Kind::Cancelled;
        o.error_code = QStringLiteral("CANCELLED");
        o.message = bt_proto_tr("Cancelled");
        o.envelope = root;
        return o;
    }
    if (bt_proto_is_timeout_text(runner_error)) {
        BtOutcome o = bt_proto_failed(QStringLiteral("TIMEOUT"), bt_proto_first_line(runner_error), diag);
        o.envelope = root;
        return o;
    }
    if (parsed) {
        // A legacy {"error": "..."} reply, or something that is not an envelope at all (e.g. the last
        // progress object of a process that died) — never an empty success.
        const QJsonValue err = root.value(QStringLiteral("error"));
        if (err.isString() && !err.toString().trimmed().isEmpty()) {
            BtOutcome o = bt_proto_failed(root.value(QStringLiteral("errorCode")).toString(QStringLiteral("ENGINE")),
                                          err.toString().trimmed(), bt_proto_details_text(root));
            if (o.technical.isEmpty())
                o.technical = diag;
            o.envelope = root;
            return o;
        }
        BtOutcome o = bt_proto_failed(QStringLiteral("NO_ENVELOPE"),
                                      bt_proto_tr("The engine ended without reporting a result (no success / error "
                                                  "envelope on its output)."),
                                      diag);
        o.envelope = root;
        return o;
    }
    if (!runner_success && !diag.isEmpty()) {
        // Prefer the runner's own sentence (an import problem, "Python is not set up yet…") when it is one.
        return bt_proto_failed(QStringLiteral("PROCESS"), bt_proto_first_line(diag), diag);
    }
    return bt_proto_failed(QStringLiteral("BAD_OUTPUT"),
                           exit_code > 0 ? bt_proto_tr("The engine process ended with exit code %1 and printed no "
                                                       "result.")
                                               .arg(exit_code)
                                         : bt_proto_tr("The engine printed no result."),
                           diag);
}

} // namespace fincept::services::backtest
