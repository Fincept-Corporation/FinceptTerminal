// PythonScriptPolicy.cpp — see PythonScriptPolicy.h.

#include "mcp/core/PythonScriptPolicy.h"

#include "core/logging/LogRedact.h"
#include "mcp/server/CallApproval.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include <initializer_list>

namespace fincept::mcp::core::python_scripts {

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

/// A JSON-object argument (or stdin), parsed; {} for anything else.
QJsonObject json_object(const QString& text) {
    const QString t = text.trimmed();
    if (!t.startsWith(QLatin1Char('{')))
        return {};
    return QJsonDocument::fromJson(t.toUtf8()).object();
}

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
            const QJsonObject o = json_object(a);
            for (const QString& k : kFlags) {
                if (o.value(k).isString())
                    add(o.value(k).toString());
            }
        }
    }
    return out;
}

enum class CommandAct {
    None,
    Order, ///< places / changes / cancels an order, or moves money
    Data,  ///< deletes, resets or publishes something: asks, moves no money
};

CommandAct command_act(const QString& cmd) {
    // `\b` used to end order_rx, and '_' is a word character, so
    // place_orders_batch / cancel_order_batch slipped through as "normal".
    static const QRegularExpression order_rx(QStringLiteral(
        R"(^(place|submit|create|cancel|modify|amend|edit|update|decrease|increase|close|exit|execute|send|new|replace)[_\-]?(order|orders|trade|trades|position|positions|bet|bets)(?:[_\-]|$))"));
    static const QRegularExpression cancel_all_rx(
        QStringLiteral(R"(^cancel[_\-]?(all|market|markets|everything)(?:[_\-]|$))"));
    static const QRegularExpression money_rx(QStringLiteral(
        R"(^(buy|sell|short|cover|withdraw|transfer|deposit|send|sign|swap|redeem|stake|unstake|bridge|mint|burn)([_\-]|$))"));
    static const QRegularExpression data_rx(
        QStringLiteral(R"(^(delete|remove|drop|purge|wipe|reset|upload|publish|post)([_\-]|$))"));
    // "delete_order", "post_orders", "reset_position": a data word on an order is an order.
    static const QRegularExpression names_order(
        QStringLiteral(R"((^|[_\-])(order|trade|position|bet)s?([_\-]|$))"));
    if (order_rx.match(cmd).hasMatch() || cancel_all_rx.match(cmd).hasMatch() || money_rx.match(cmd).hasMatch())
        return CommandAct::Order;
    if (data_rx.match(cmd).hasMatch())
        return names_order.match(cmd).hasMatch() ? CommandAct::Order : CommandAct::Data;
    return CommandAct::None;
}

/// exchange/ scripts that only read — market data, or the account's balances,
/// positions and orders with its stored keys. They still ask (the keys), but
/// place nothing.
bool exchange_read(const QString& lower_base) {
    return lower_base.startsWith(QLatin1String("fetch_")) || lower_base.startsWith(QLatin1String("list_")) ||
           lower_base == QLatin1String("ws_stream");
}

/// A key whose value is never shown: credentials, signatures, sessions.
bool secret_name(const QString& name) {
    const QString n = name.toLower();
    for (const char* part : {"secret", "pass", "pwd", "private", "mnemonic", "seed", "cookie", "credential",
                             "signature", "session", "api_key", "apikey", "auth"}) {
        if (n.contains(QLatin1String(part)))
            return true;
    }
    for (const char* tail : {"key", "token", "pin", "otp"}) {
        if (n.endsWith(QLatin1String(tail)))
            return true;
    }
    return false;
}

/// A JSON value for the approval dialog: secret-named keys replaced, strings
/// scrubbed and clipped, nesting bounded.
QJsonValue shown_json(const QJsonValue& v, int depth) {
    if (depth > 6)
        return QStringLiteral("…");
    if (v.isObject()) {
        QJsonObject o = v.toObject();
        for (auto it = o.begin(); it != o.end(); ++it)
            it.value() = secret_name(it.key()) ? QJsonValue(QStringLiteral("<redacted>"))
                                               : shown_json(it.value(), depth + 1);
        return o;
    }
    if (v.isArray()) {
        QJsonArray out;
        const QJsonArray in = v.toArray();
        for (qsizetype i = 0; i < in.size() && i < 50; ++i)
            out.append(shown_json(in.at(i), depth + 1));
        return out;
    }
    if (v.isString())
        return log_scrub(v.toString().left(200));
    return v;
}

/// The arguments as the dialog shows them: the value after a secret-named flag
/// (`--api-key X`, `--token=X`) and secret keys of JSON arguments redacted,
/// everything else scrubbed of anything that looks like a key or token.
QStringList shown_args(const QStringList& argv) {
    QStringList out;
    bool hide_next = false;
    for (const QString& raw : argv) {
        const QString a = raw.left(400);
        if (hide_next) {
            hide_next = false;
            if (!a.startsWith(QLatin1Char('-'))) {
                out << QStringLiteral("<redacted>");
                continue;
            }
        }
        if (a.startsWith(QLatin1Char('-'))) {
            QString flag = a;
            while (flag.startsWith(QLatin1Char('-')))
                flag.remove(0, 1);
            const qsizetype eq = flag.indexOf(QLatin1Char('='));
            const QString name = eq >= 0 ? flag.left(eq) : flag;
            if (secret_name(name)) {
                if (eq >= 0) {
                    out << a.left(a.indexOf(QLatin1Char('=')) + 1) + QStringLiteral("<redacted>");
                    continue;
                }
                hide_next = true;
            }
            out << log_scrub(a);
            continue;
        }
        if (a.trimmed().startsWith(QLatin1Char('{'))) {
            const QJsonDocument doc = QJsonDocument::fromJson(raw.trimmed().toUtf8());
            if (doc.isObject()) {
                out << QString::fromUtf8(QJsonDocument(shown_json(doc.object(), 0).toObject())
                                             .toJson(QJsonDocument::Compact))
                           .left(400);
                continue;
            }
        }
        out << log_scrub(a);
    }
    return out;
}

/// stdin as the dialog shows it: a JSON object through shown_json(), anything
/// else only by size.
QString shown_stdin(const QByteArray& data) {
    if (data.trimmed().isEmpty())
        return {};
    const QJsonDocument doc = QJsonDocument::fromJson(data.trimmed());
    if (doc.isObject())
        return QString::fromUtf8(QJsonDocument(shown_json(doc.object(), 0).toObject()).toJson(QJsonDocument::Compact))
            .left(300);
    return QStringLiteral("(%1 bytes of input)").arg(data.size());
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

    // Ask first for order / money-moving / destructive commands on any script.
    // (Checked before the folder rules so `moves_money` is set for exchange/ too.)
    bool money = false;
    bool data = false;
    for (const QString& cmd : command_words(args)) {
        const CommandAct act = command_act(cmd);
        if (act == CommandAct::Order) {
            money = true;
            break;
        }
        if (act == CommandAct::Data)
            data = true;
    }
    const bool exchange = lower.startsWith(QLatin1String("exchange/"));
    if (moves_money)
        *moves_money = money || (exchange && !exchange_read(base));
    if (money || data)
        return ScriptRisk::Sensitive;

    // Ask first: exchange credentials and orders, LLM agent runners (spend the
    // user's model credits), live strategy runners, microphone / speaker.
    if (exchange || lower.startsWith(QLatin1String("agents/")) || lower.startsWith(QLatin1String("voice/")) ||
        (lower.startsWith(QLatin1String("strategies/")) && lower.contains(QLatin1String("live"))) ||
        lower.contains(QLatin1String("wallet")) || lower.contains(QLatin1String("broker")))
        return ScriptRisk::Sensitive;

    // Arbitrary code (a strategy's `code`, an expression) runs with every stored
    // credential in the environment; reads and writes any file the user can.
    if (runs_code || base == QLatin1String("spreadsheet"))
        return ScriptRisk::Sensitive;
    return ScriptRisk::Normal;
}

QString order_venue(const QString& key, const QStringList& args) {
    static const QRegularExpression name_rx(QStringLiteral(R"(^[A-Za-z][A-Za-z0-9_.\-]{1,31}$)"));
    const QString lower = key.toLower();
    const QString base = lower.section(QLatin1Char('/'), -1);
    if (base.startsWith(QLatin1String("prediction_"))) {
        QString market = base.mid(11).section(QLatin1Char('_'), 0, 0);
        if (name_rx.match(market).hasMatch()) {
            market[0] = market.at(0).toUpper();
            return market; // "Polymarket", "Kalshi"
        }
    }
    if (lower.startsWith(QLatin1String("exchange/"))) {
        // The exchange id is the first positional argument of every exchange/ script.
        for (const QString& raw : args) {
            const QString a = raw.trimmed();
            if (a.startsWith(QLatin1Char('-')) || a.startsWith(QLatin1Char('{')))
                continue;
            if (name_rx.match(a).hasMatch())
                return a;
            break;
        }
    }
    for (const QString& a : args) {
        const QJsonObject o = json_object(a);
        for (const char* k : {"exchange", "exchange_id", "broker", "venue"}) {
            const QString v = o.value(QLatin1String(k)).toString().trimmed();
            if (name_rx.match(v).hasMatch())
                return v;
        }
    }
    return {};
}

RunGate gate_run(const RunCall& call) {
    RunGate g;
    // Commands passed on stdin as JSON ({"action": "place_order", …}) count too —
    // all of it: a command past a size window must not read as "normal".
    QStringList classify_args = call.argv;
    if (call.stdin_data.trimmed().startsWith('{'))
        classify_args << QString::fromUtf8(call.stdin_data);
    g.risk = classify(call.key, classify_args, call.runs_code, &g.moves_money);
    if (g.risk == ScriptRisk::Blocked) {
        g.moves_money = false;
        g.refusal = QStringLiteral("\"%1\" cannot be run from here (arbitrary-code, maintenance or test script).")
                        .arg(call.key);
        return g;
    }
    if (g.risk != ScriptRisk::Sensitive) {
        g.moves_money = false;
        return g;
    }

    QString venue = g.moves_money ? order_venue(call.key, classify_args) : QString();
    if (g.moves_money && venue.isEmpty())
        venue = QStringLiteral("the exchange or broker it connects to");

    if (call.paper_run || is_paper_run_origin(call.origin)) {
        // A PAPER run (a paper-only run token): nothing it does may place, change
        // or cancel a real order, so a script that asks is refused, not offered.
        g.refusal =
            g.moves_money
                ? QStringLiteral("Refused: \"%1\" sends orders straight to %2 (LIVE, real money), and this run is "
                                 "PAPER-only — it may not place, change or cancel a real order. Nothing was sent. Hint: "
                                 "in a LIVE run (a workflow armed LIVE in SAVE & RUN) the user is asked to approve it.")
                      .arg(call.key, venue)
                : QStringLiteral("Refused: \"%1\" needs the user's approval for each run (it runs code with your stored "
                                 "keys or acts on its own), and this run is PAPER-only, so it is not offered. Nothing "
                                 "ran. Hint: in a LIVE run (a workflow armed LIVE in SAVE & RUN) the user is asked to "
                                 "approve it.")
                      .arg(call.key);
        return g;
    }

    // The script and what it is given, as it will run — scrubbed of anything
    // that looks like a key, token or password.
    const QStringList args_shown = shown_args(call.argv);
    const QString stdin_shown = shown_stdin(call.stdin_data);
    QString text = QStringLiteral("Run the bundled script %1").arg(call.key);
    if (!args_shown.isEmpty())
        text += QStringLiteral(" with %1").arg(args_shown.join(QLatin1Char(' ')).left(300));
    if (!stdin_shown.isEmpty())
        text += (args_shown.isEmpty() ? QStringLiteral(" with input %1") : QStringLiteral(" and input %1"))
                    .arg(stdin_shown);
    const QString summary = call.summary.trimmed();
    if (!summary.isEmpty())
        text += QStringLiteral(" — %1").arg(summary.left(200));
    if (!text.endsWith(QLatin1Char('.')))
        text += QLatin1Char('.');
    if (g.moves_money)
        text += QStringLiteral(" It sends the order straight to %1 (LIVE): the script talks to the exchange itself, "
                               "not through the terminal's trading engine or its order approvals.")
                    .arg(venue);

    ToolApprovalRequest& req = g.request;
    req.client = call.origin;
    req.tool = QStringLiteral("python: ") + call.key;
    req.description = text;
    QJsonArray arg_list;
    for (const QString& a : args_shown)
        arg_list.append(a);
    req.args = QJsonObject{{QStringLiteral("script"), call.key}, {QStringLiteral("args"), arg_list}};
    if (!stdin_shown.isEmpty())
        req.args.insert(QStringLiteral("input"), stdin_shown);
    if (g.moves_money)
        req.args.insert(QStringLiteral("sent_to"), venue);
    req.destructive = true;
    // Orders and money moves: kind order, LIVE (the script reaches the venue
    // directly). Everything else that asks runs code on this computer. Neither
    // kind is ever covered by a timed grant: each run is approved on its own.
    req.live_trading = g.moves_money;
    req.kind = g.moves_money ? QStringLiteral("order") : QStringLiteral("code");
    req.trading_mode = g.moves_money ? ApprovalTradingMode::Live : ApprovalTradingMode::Unknown;
    req.client_id = call.client_id;
    req.run_id = call.run_id;
    return g;
}

} // namespace fincept::mcp::core::python_scripts
