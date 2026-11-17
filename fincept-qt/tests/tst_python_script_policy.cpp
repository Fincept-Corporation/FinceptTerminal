// tst_python_script_policy.cpp — what the `python` MCP tool may do with one script call
// (src/mcp/core/PythonScriptPolicy.cpp): scripts that move money ask with kind order, LIVE,
// naming the venue the script sends the order straight to; every other script that asks is
// kind code; neither is grantable; a PAPER run refuses both outright; the dialog shows the
// arguments and input with credentials removed.

#include "mcp/core/PythonScriptPolicy.h"
#include "mcp/server/ApprovalPolicy.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QtTest>

using namespace fincept::mcp;
using namespace fincept::mcp::core::python_scripts;

namespace {

const char kSecret[] = "Zq9vKx3SecretValue";

RunCall order_call() {
    RunCall c;
    c.key = QStringLiteral("exchange/place_order");
    c.summary = QStringLiteral("Place an order on an exchange");
    c.argv = {QStringLiteral("binance"), QStringLiteral("BTC/USDT"), QStringLiteral("buy"), QStringLiteral("limit"),
              QStringLiteral("0.01"), QStringLiteral("50000")};
    c.stdin_data = QByteArrayLiteral("{\"api_key\":\"AK1234\",\"secret\":\"") + QByteArray(kSecret) +
                   QByteArrayLiteral("\"}");
    c.origin = QStringLiteral("Codex");
    c.client_id = QStringLiteral("codex");
    c.run_id = QStringLiteral("run-00aa11bb22cc33dd");
    return c;
}

QString everything_shown(const ToolApprovalRequest& r) {
    return r.description + QLatin1Char(' ') + QString::fromUtf8(QJsonDocument(r.args).toJson());
}

} // namespace

class TestPythonScriptPolicy : public QObject {
    Q_OBJECT

  private slots:
    void exchange_order_is_a_live_order_straight_to_the_exchange() {
        const RunGate g = gate_run(order_call());
        QCOMPARE(g.risk, ScriptRisk::Sensitive);
        QVERIFY(g.moves_money);
        QVERIFY(g.refusal.isEmpty());
        const ToolApprovalRequest& r = g.request;
        QCOMPARE(r.kind, QStringLiteral("order"));
        QCOMPARE(r.trading_mode, ApprovalTradingMode::Live);
        QVERIFY(r.live_trading);
        QVERIFY(r.destructive);
        QCOMPARE(r.tool, QStringLiteral("python: exchange/place_order"));
        QCOMPARE(r.client, QStringLiteral("Codex"));
        QCOMPARE(r.client_id, QStringLiteral("codex"));
        QCOMPARE(r.run_id, QStringLiteral("run-00aa11bb22cc33dd"));
        QVERIFY2(r.description.contains(QStringLiteral("sends the order straight to binance")), qPrintable(r.description));
        QVERIFY(r.description.contains(QStringLiteral("not through the terminal's trading engine")));
        QVERIFY(r.description.contains(QStringLiteral("binance BTC/USDT buy limit 0.01 50000")));
        QCOMPARE(r.args.value(QStringLiteral("sent_to")).toString(), QStringLiteral("binance"));
        // Credentials on stdin never reach the dialog.
        const QString shown = everything_shown(r);
        QVERIFY2(!shown.contains(QLatin1String(kSecret)), qPrintable(shown));
        QVERIFY(!shown.contains(QStringLiteral("AK1234")));
    }

    void prediction_market_orders_name_the_market() {
        RunCall c;
        c.key = QStringLiteral("prediction_polymarket");
        c.argv = {QStringLiteral("place_order"), QStringLiteral("--stdin")};
        c.stdin_data = QByteArrayLiteral("{\"private_key\":\"0xdeadbeefcafe\",\"api_passphrase\":\"pp-77\","
                                         "\"token_id\":\"7123\",\"price\":0.65,\"size\":100,\"side\":\"BUY\"}");
        RunGate g = gate_run(c);
        QCOMPARE(g.request.kind, QStringLiteral("order"));
        QVERIFY(g.request.description.contains(QStringLiteral("straight to Polymarket")));
        // The order itself is shown (price, size, side); the keys are not.
        const QString shown = everything_shown(g.request);
        QVERIFY2(shown.contains(QStringLiteral("0.65")) && shown.contains(QStringLiteral("BUY")), qPrintable(shown));
        QVERIFY(!shown.contains(QStringLiteral("deadbeefcafe")));
        QVERIFY(!shown.contains(QStringLiteral("pp-77")));

        // A command passed only on stdin counts too.
        c.key = QStringLiteral("prediction_kalshi");
        c.argv = {QStringLiteral("--stdin")};
        c.stdin_data = QByteArrayLiteral("{\"action\":\"place_orders_batch\",\"orders\":[]}");
        g = gate_run(c);
        QVERIFY(g.moves_money);
        QCOMPARE(g.request.kind, QStringLiteral("order"));
        QVERIFY(g.request.description.contains(QStringLiteral("straight to Kalshi")));

        // A read command of the same script runs straight away.
        c.argv = {QStringLiteral("get_orders")};
        c.stdin_data.clear();
        g = gate_run(c);
        QCOMPARE(g.risk, ScriptRisk::Normal);
        QVERIFY(!g.moves_money);
    }

    // A paper-only run token: every script that would ask is refused outright —
    // nothing is offered for approval, nothing starts.
    void paper_runs_refuse_orders_and_code() {
        RunCall c = order_call();
        c.paper_run = true;
        RunGate g = gate_run(c);
        QVERIFY(g.moves_money);
        QVERIFY2(g.refusal.contains(QStringLiteral("PAPER")), qPrintable(g.refusal));
        QVERIFY(g.refusal.contains(QStringLiteral("binance")));
        QVERIFY(g.request.kind.isEmpty()); // nothing to ask

        // The caller label of a paper run counts even without the flag.
        c = order_call();
        c.origin = QStringLiteral("Workflow agent (paper run)");
        QVERIFY(!gate_run(c).refusal.isEmpty());

        // Agents (code) too.
        RunCall agent;
        agent.key = QStringLiteral("agents/deepagents/cli");
        agent.paper_run = true;
        g = gate_run(agent);
        QCOMPARE(g.risk, ScriptRisk::Sensitive);
        QVERIFY(!g.refusal.isEmpty());

        // A normal data script still runs in a paper run.
        RunCall data;
        data.key = QStringLiteral("fred_data");
        data.argv = {QStringLiteral("series"), QStringLiteral("GDP")};
        data.paper_run = true;
        g = gate_run(data);
        QCOMPARE(g.risk, ScriptRisk::Normal);
        QVERIFY(g.refusal.isEmpty());
    }

    void other_sensitive_scripts_are_code_one_run_at_a_time() {
        RunCall c;
        c.key = QStringLiteral("agents/deepagents/cli");
        c.argv = {QStringLiteral("--model"), QStringLiteral("x"), QStringLiteral("--api-key"), QLatin1String(kSecret),
                  QStringLiteral("--token=") + QLatin1String(kSecret)};
        RunGate g = gate_run(c);
        QCOMPARE(g.request.kind, QStringLiteral("code"));
        QVERIFY(!g.request.live_trading);
        QCOMPARE(g.request.trading_mode, ApprovalTradingMode::Unknown);
        QVERIFY(g.request.description.contains(QStringLiteral("agents/deepagents/cli with --model x --api-key")));
        QVERIFY2(!everything_shown(g.request).contains(QLatin1String(kSecret)), qPrintable(everything_shown(g.request)));
        QVERIFY(!g.request.description.contains(QStringLiteral("straight to")));

        // An exchange read uses the stored keys (asks) but places nothing.
        c = RunCall{};
        c.key = QStringLiteral("exchange/fetch_balance");
        c.argv = {QStringLiteral("binance")};
        g = gate_run(c);
        QCOMPARE(g.risk, ScriptRisk::Sensitive);
        QVERIFY(!g.moves_money);
        QCOMPARE(g.request.kind, QStringLiteral("code"));

        // A delete-style command asks, as code; on an order it is an order.
        c = RunCall{};
        c.key = QStringLiteral("some_connector");
        c.argv = {QStringLiteral("delete_cache")};
        g = gate_run(c);
        QCOMPARE(g.request.kind, QStringLiteral("code"));
        c.argv = {QStringLiteral("delete_order"), QStringLiteral("42")};
        g = gate_run(c);
        QCOMPARE(g.request.kind, QStringLiteral("order"));
        QVERIFY(g.request.description.contains(QStringLiteral("the exchange or broker it connects to")));

        // Neither kind is ever covered by a timed grant.
        QVERIFY(!approval::timed_grant_allowed(QStringLiteral("order")));
        QVERIFY(!approval::timed_grant_allowed(QStringLiteral("code")));
    }

    void blocked_scripts_are_refused_and_normal_ones_run() {
        RunCall c;
        c.key = QStringLiteral("notebook_cell_runner");
        RunGate g = gate_run(c);
        QCOMPARE(g.risk, ScriptRisk::Blocked);
        QVERIFY(!g.refusal.isEmpty());
        c.key = QStringLiteral("fred_data");
        g = gate_run(c);
        QCOMPARE(g.risk, ScriptRisk::Normal);
        QVERIFY(g.refusal.isEmpty());
        QVERIFY(g.request.kind.isEmpty());
    }

    void venue_names() {
        QCOMPARE(order_venue(QStringLiteral("exchange/cancel_order"),
                             {QStringLiteral("--prepare-only"), QStringLiteral("kraken"), QStringLiteral("1")}),
                 QStringLiteral("kraken"));
        QCOMPARE(order_venue(QStringLiteral("exchange/exchange_daemon"),
                             {QStringLiteral("{\"method\":\"place_order\",\"exchange\":\"okx\"}")}),
                 QStringLiteral("okx"));
        QCOMPARE(order_venue(QStringLiteral("exchange/place_order"), {QStringLiteral("not a name!")}), QString());
        QCOMPARE(order_venue(QStringLiteral("prediction_kalshi"), {}), QStringLiteral("Kalshi"));
    }
};

QTEST_GUILESS_MAIN(TestPythonScriptPolicy)
#include "tst_python_script_policy.moc"
