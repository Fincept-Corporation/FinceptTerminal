// tests/tst_backtest_protocol.cpp
//
// The host side of the backtesting provider protocol (src/services/backtesting/BacktestRunProtocol):
//   * @@BT_PROGRESS lines on stderr -> BtProgress (and nothing else is progress),
//   * a finished process -> Ok / Failed / Cancelled, where a killed, cancelled or envelope-less
//     process is NEVER an empty success (the defect of the old BacktestingService, which read a
//     missing "success" as true).
#include "services/backtesting/BacktestRunProtocol.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest/QtTest>

#include <cmath>

using namespace fincept::services::backtest;

class TestBacktestProtocol : public QObject {
    Q_OBJECT

  private slots:
    // ── progress lines ──────────────────────────────────────────────────────
    void parses_a_full_progress_line() {
        const auto p = parse_progress_line(QStringLiteral(
            "@@BT_PROGRESS {\"phase\":\"optimize\",\"done\":240,\"total\":500,\"message\":\"Optimising 240 / 500\","
            "\"best\":1.21,\"etaSec\":14.2,\"workers\":4}"));
        QVERIFY(p.has_value());
        QCOMPARE(p->phase, QStringLiteral("optimize"));
        QCOMPARE(p->done, 240);
        QCOMPARE(p->total, 500);
        QCOMPARE(p->message, QStringLiteral("Optimising 240 / 500"));
        QCOMPARE(p->best, 1.21);
        QCOMPARE(p->eta_sec, 14.2);
        QCOMPARE(p->workers, 4);
        QVERIFY(p->determinate());
        QCOMPARE(p->fraction(), 0.48);
    }

    void null_total_is_indeterminate() {
        const auto p = parse_progress_line(
            QStringLiteral("@@BT_PROGRESS {\"phase\":\"metrics\",\"done\":0,\"total\":null,\"message\":\"\"}"));
        QVERIFY(p.has_value());
        QVERIFY(!p->determinate());
        QVERIFY(std::isnan(p->fraction()));
        QVERIFY(std::isnan(p->best));
        QVERIFY(std::isnan(p->eta_sec));
        QCOMPARE(p->workers, -1);
    }

    void leading_blanks_and_unknown_keys_are_accepted() {
        const auto p = parse_progress_line(
            QStringLiteral("   @@BT_PROGRESS {\"phase\":\"data\",\"done\":1,\"total\":2,\"future\":[1,2]}\r"));
        QVERIFY(p.has_value());
        QCOMPARE(p->phase, QStringLiteral("data"));
        QCOMPARE(p->done, 1);
    }

    void other_lines_are_not_progress() {
        QVERIFY(!parse_progress_line(QStringLiteral("Traceback (most recent call last):")).has_value());
        QVERIFY(!parse_progress_line(QStringLiteral("@@BT_PROGRESS not json")).has_value());
        QVERIFY(!parse_progress_line(QStringLiteral("@@BT_PROGRESS [1,2,3]")).has_value());
        QVERIFY(!parse_progress_line(QStringLiteral("@@BT_PROGRESS {}")).has_value());
        QVERIFY(!parse_progress_line(QStringLiteral("{\"phase\":\"run\"}")).has_value());
        QVERIFY(!parse_progress_line(QString()).has_value());
    }

    void negative_eta_is_unknown() {
        const auto p = parse_progress_line(QStringLiteral("@@BT_PROGRESS {\"phase\":\"run\",\"etaSec\":-3}"));
        QVERIFY(p.has_value());
        QVERIFY(std::isnan(p->eta_sec));
    }

    void progress_text_does_not_repeat_counts() {
        BtProgress p;
        p.phase = QStringLiteral("optimize");
        p.done = 240;
        p.total = 500;
        p.message = QStringLiteral("Optimising 240 / 500");
        p.best = 1.21;
        p.eta_sec = 14;
        const QString t = progress_text(p);
        QCOMPARE(t.count(QStringLiteral("240")), 1);
        QVERIFY(t.contains(QStringLiteral("1.21")));
        QVERIFY(t.contains(QStringLiteral("14 s")));
        BtProgress q;
        q.phase = QStringLiteral("monte_carlo");
        q.done = 10;
        q.total = 1000;
        const QString u = progress_text(q);
        QVERIFY(u.startsWith(progress_phase_label(QStringLiteral("monte_carlo"))));
        QVERIFY(u.contains(QStringLiteral("10")));
    }

    void eta_formatting() {
        QCOMPARE(format_eta(14), QStringLiteral("14 s"));
        QCOMPARE(format_eta(200), QStringLiteral("3 min 20 s"));
        QCOMPARE(format_eta(3900), QStringLiteral("1 h 05 min"));
        QVERIFY(format_eta(std::nan("")).isEmpty());
        QVERIFY(format_eta(-1).isEmpty());
    }

    void stderr_tail_drops_progress_lines() {
        const QString err = QStringLiteral("@@BT_PROGRESS {\"phase\":\"run\"}\nWarning: x\n@@BT_PROGRESS {\"phase\":\"metrics\"}\n");
        QCOMPARE(strip_progress_lines(err), QStringLiteral("Warning: x"));
    }

    // ── run outcome ─────────────────────────────────────────────────────────
    void success_envelope_is_ok() {
        const QByteArray out = "{\"success\":true,\"data\":{\"schemaVersion\":2,\"id\":\"x\"}}";
        const BtOutcome o = classify_run_output(true, 0, QString(), out);
        QVERIFY(o.ok());
        QCOMPARE(o.data.value(QStringLiteral("id")).toString(), QStringLiteral("x"));
    }

    void failure_envelope_carries_code_message_and_details() {
        const QByteArray out =
            "{\"success\":false,\"error\":\"Market data for ZZZZ could not be loaded. Check the symbol.\","
            "\"errorCode\":\"DATA\",\"details\":{\"traceback\":\"Traceback...\",\"logs\":[\"a\",\"b\"],\"symbol\":\"ZZZZ\"}}";
        const BtOutcome o = classify_run_output(false, 0, QStringLiteral("Market data for ZZZZ could not be loaded."), out);
        QCOMPARE(o.kind, BtOutcome::Kind::Failed);
        QCOMPARE(o.error_code, QStringLiteral("DATA"));
        QVERIFY(o.message.startsWith(QStringLiteral("Market data for ZZZZ")));
        QVERIFY(o.technical.contains(QStringLiteral("Traceback...")));
        QVERIFY(o.technical.contains(QStringLiteral("a\nb")));
        QCOMPARE(o.symbol, QStringLiteral("ZZZZ"));
    }

    void cooperative_cancel_is_cancelled_not_failed() {
        const QByteArray out = "{\"success\":false,\"error\":\"Cancelled by user\",\"errorCode\":\"CANCELLED\",\"cancelled\":true}";
        const BtOutcome o = classify_run_output(false, 3, QStringLiteral("Cancelled by user"), out);
        QVERIFY(o.cancelled());
        QCOMPARE(o.error_code, QStringLiteral("CANCELLED"));
    }

    void exit_code_three_alone_is_cancelled() {
        const BtOutcome o = classify_run_output(false, 3, QString(), QByteArray());
        QVERIFY(o.cancelled());
    }

    void killed_process_is_cancelled_never_success() {
        // PythonRunner's cancel_token kill: success=false, error "Cancelled", exit -1, no stdout.
        const BtOutcome o = classify_run_output(false, -1, QStringLiteral("Cancelled"), QByteArray());
        QVERIFY(o.cancelled());
        QVERIFY(!o.ok());
    }

    void last_progress_object_on_stdout_is_not_success() {
        // A process that died after printing a progress-like object (or any object without "success").
        const BtOutcome o = classify_run_output(true, 0, QString(), "{\"phase\":\"optimize\",\"done\":3}");
        QCOMPARE(o.kind, BtOutcome::Kind::Failed);
        QCOMPARE(o.error_code, QStringLiteral("NO_ENVELOPE"));
    }

    void empty_output_is_failed() {
        const BtOutcome o = classify_run_output(false, 1, QStringLiteral("@@BT_PROGRESS {\"phase\":\"run\"}\nSegfault"),
                                                QByteArray());
        QCOMPARE(o.kind, BtOutcome::Kind::Failed);
        QCOMPARE(o.technical, QStringLiteral("Segfault"));
        const BtOutcome e = classify_run_output(true, 0, QString(), QByteArray());
        QCOMPARE(e.kind, BtOutcome::Kind::Failed);
        QCOMPARE(e.error_code, QStringLiteral("BAD_OUTPUT"));
    }

    void timeout_is_failed_with_timeout_code() {
        const BtOutcome o =
            classify_run_output(false, -1, QStringLiteral("Script timed out after 3600 s and was killed"), QByteArray());
        QCOMPARE(o.kind, BtOutcome::Kind::Failed);
        QCOMPARE(o.error_code, QStringLiteral("TIMEOUT"));
    }

    void success_with_crash_exit_is_failed() {
        const BtOutcome o = classify_run_output(false, 139, QStringLiteral("boom"), "{\"success\":true,\"data\":{}}");
        QCOMPARE(o.kind, BtOutcome::Kind::Failed);
        QCOMPARE(o.error_code, QStringLiteral("PROCESS"));
    }

    void legacy_nested_failure_is_failed() {
        const QByteArray out = "{\"success\":true,\"data\":{\"success\":false,\"error\":\"No parameter ranges\"}}";
        const BtOutcome o = classify_run_output(true, 0, QString(), out);
        QCOMPARE(o.kind, BtOutcome::Kind::Failed);
        QCOMPARE(o.message, QStringLiteral("No parameter ranges"));
    }

    void legacy_error_object_without_success_is_failed() {
        const BtOutcome o = classify_run_output(false, 0, QStringLiteral("bad"), "{\"error\":\"bad\"}");
        QCOMPARE(o.kind, BtOutcome::Kind::Failed);
        QCOMPARE(o.message, QStringLiteral("bad"));
    }

    void legacy_reply_without_data_key_uses_root() {
        const BtOutcome o = classify_run_output(true, 0, QString(), "{\"success\":true,\"indicator\":\"rsi\"}");
        QVERIFY(o.ok());
        QCOMPARE(o.data.value(QStringLiteral("indicator")).toString(), QStringLiteral("rsi"));
    }
};

QTEST_GUILESS_MAIN(TestBacktestProtocol)
#include "tst_backtest_protocol.moc"
