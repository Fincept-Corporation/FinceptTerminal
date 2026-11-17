// tst_python_stream_lines — the pure per-line stream rules of python::PythonRunner
// (src/python/PythonRunner.h, namespace stream_detail): which channel reaches the stream
// callback (RunOptions::stream_stdout = false -> stderr only, stdout never scanned or decoded),
// and the line splitter (LF / CRLF, lines split across reads, a partial tail kept for the next
// read and not streamed at the end). StreamSim below repeats the runner's drain loop
// (PythonRunner::start_next, drain_lines) over the same two helpers, without a process.

#include "python/PythonRunner.h"

#include <QtTest>

using namespace fincept::python;
using namespace fincept::python::stream_detail;

namespace {

struct StreamSim {
    bool has_callback = true;
    bool stream_stdout = true;
    QByteArray out, err;   // what the runner keeps for the result / error text
    int out_off = 0, err_off = 0;
    QList<QPair<QString, bool>> lines; // (line, is_stderr) as the callback saw them
    int stdout_scans = 0;  // next_line() calls on stdout = line-end scans of the result buffer

    void feed(bool is_stderr, const QByteArray& chunk) {
        (is_stderr ? err : out).append(chunk);
        drain(is_stderr);
    }
    void drain(bool is_stderr) {
        while (true) {
            if (!streams_channel(has_callback, stream_stdout, is_stderr))
                return;
            const QByteArray& buf = is_stderr ? err : out;
            int& off = is_stderr ? err_off : out_off;
            QByteArray line;
            if (!is_stderr)
                ++stdout_scans;
            if (!next_line(buf, &off, &line))
                break;
            lines.append({QString::fromUtf8(line), is_stderr});
        }
    }
    QStringList texts() const {
        QStringList t;
        for (const auto& l : lines)
            t << l.first;
        return t;
    }
};

} // namespace

class TstPythonStreamLines : public QObject {
    Q_OBJECT
  private slots:
    void options_default_streams_both_channels() {
        PythonRunner::RunOptions o;
        QVERIFY(o.stream_stdout);
        // Appended last: existing designated initialisers keep compiling.
        const PythonRunner::RunOptions quiet{.expect_json = false, .stream_stdout = false};
        QVERIFY(!quiet.expect_json);
        QVERIFY(!quiet.stream_stdout);
    }

    void channel_rule() {
        // has_callback, stream_stdout, is_stderr
        QVERIFY(streams_channel(true, true, false));
        QVERIFY(streams_channel(true, true, true));
        QVERIFY(!streams_channel(true, false, false)); // stderr-only: stdout not streamed
        QVERIFY(streams_channel(true, false, true));
        QVERIFY(!streams_channel(false, true, false)); // no callback: nothing
        QVERIFY(!streams_channel(false, true, true));
        QVERIFY(!streams_channel(false, false, false));
        QVERIFY(!streams_channel(false, false, true));
    }

    void splits_lf_and_crlf() {
        const QByteArray buf("one\ntwo\r\nthree\r\n\n\r\nlast");
        int off = 0;
        QByteArray line;
        QStringList got;
        while (next_line(buf, &off, &line))
            got << QString::fromUtf8(line);
        QCOMPARE(got, (QStringList{"one", "two", "three", "", ""}));
        QCOMPARE(off, int(buf.indexOf("last"))); // the partial tail stays
        QVERIFY(!next_line(buf, &off, &line));
        QCOMPARE(off, int(buf.indexOf("last"))); // a miss does not move the offset
    }

    void only_one_trailing_cr_removed_and_inner_cr_kept() {
        const QByteArray buf("a\rb\n50%\r\r\n");
        int off = 0;
        QByteArray line;
        QVERIFY(next_line(buf, &off, &line));
        QCOMPARE(line, QByteArray("a\rb"));      // a progress bar's carriage return inside a line stays
        QVERIFY(next_line(buf, &off, &line));
        QCOMPARE(line, QByteArray("50%\r"));     // CRLF: exactly one \r before the \n removed
        QCOMPARE(off, int(buf.size()));
    }

    void lines_split_across_reads() {
        StreamSim s;
        s.feed(true, "hel");
        QVERIFY(s.lines.isEmpty());
        s.feed(true, "lo\nwor");
        s.feed(true, "ld\r");                    // CR of a CRLF in one read, LF in the next
        QCOMPARE(s.texts(), QStringList{"hello"});
        s.feed(true, "\n");
        QCOMPARE(s.texts(), (QStringList{"hello", "world"}));
        QCOMPARE(s.err, QByteArray("hello\nworld\r\n")); // buffer intact for the error text
    }

    void utf8_split_across_reads_decodes_whole_line() {
        StreamSim s;
        const QByteArray cafe = QStringLiteral("café €").toUtf8();
        const int cut = int(cafe.indexOf('f')) + 2; // inside the two-byte e-acute
        s.feed(false, cafe.left(cut));
        s.feed(false, cafe.mid(cut) + "\n");
        QCOMPARE(s.lines.size(), 1);
        QCOMPARE(s.lines.at(0).first, QStringLiteral("café €"));
    }

    void trailing_partial_line_not_streamed_at_finish() {
        StreamSim s;
        s.feed(false, "row 1\n{\"ok\":tr");
        s.feed(false, "ue}");                    // process ends without a final newline
        QCOMPARE(s.texts(), QStringList{"row 1"});
        QCOMPARE(s.out_off, 6);
        // The result is the whole buffer, tail included (what finished() decodes).
        QCOMPARE(s.out, QByteArray("row 1\n{\"ok\":true}"));
    }

    void default_mode_streams_both_in_order() {
        StreamSim s;
        s.feed(true, "PROGRESS 10\n");
        s.feed(false, "{\"a\":1}\n");
        s.feed(true, "PROGRESS 100\n");
        QCOMPARE(s.lines.size(), 3);
        QCOMPARE(s.lines.at(0), (QPair<QString, bool>{"PROGRESS 10", true}));
        QCOMPARE(s.lines.at(1), (QPair<QString, bool>{"{\"a\":1}", false}));
        QCOMPARE(s.lines.at(2), (QPair<QString, bool>{"PROGRESS 100", true}));
    }

    void stderr_only_mode_drops_stdout_from_stream_keeps_result() {
        StreamSim s;
        s.stream_stdout = false;
        s.feed(false, "warming up\n");
        s.feed(true, "PROGRESS 50\r\n");
        s.feed(false, "{\"rows\":[1,2,3]}\n");
        s.feed(true, "PROGRESS 100\n");
        QCOMPARE(s.texts(), (QStringList{"PROGRESS 50", "PROGRESS 100"}));
        for (const auto& l : s.lines)
            QVERIFY(l.second); // stderr only
        QCOMPARE(s.stdout_scans, 0);
        QCOMPARE(s.out_off, 0);
        QCOMPARE(s.out, QByteArray("warming up\n{\"rows\":[1,2,3]}\n")); // result bytes unchanged
    }

    void large_stdout_line_not_scanned_in_stderr_only_mode() {
        // One ~4 MB result line arriving in 64 KB reads, interleaved with stderr progress.
        QByteArray big("{\"series\":[");
        while (big.size() < 4 * 1024 * 1024)
            big.append("123456.789,");
        big.append("0]}\n");

        StreamSim quiet;
        quiet.stream_stdout = false;
        QStringList ticks;
        for (qsizetype i = 0; i < big.size(); i += 64 * 1024) {
            quiet.feed(false, big.mid(i, 64 * 1024));
            if (i % (1024 * 1024) == 0) {
                quiet.feed(true, "tick\n");
                ticks << QStringLiteral("tick");
            }
        }
        QCOMPARE(ticks.size(), 5);               // reads at 0, 1, 2, 3 and 4 MB
        QCOMPARE(quiet.stdout_scans, 0);         // never scanned for '\n', never decoded
        QCOMPARE(quiet.texts(), ticks);
        QCOMPARE(quiet.out, big);                // delivered whole to the result

        StreamSim loud;                          // today's default, for contrast
        for (qsizetype i = 0; i < big.size(); i += 64 * 1024)
            loud.feed(false, big.mid(i, 64 * 1024));
        QCOMPARE(loud.lines.size(), 1);          // one line, decoded once it completed
        QCOMPARE(loud.lines.at(0).first.size(), big.size() - 1);
        QVERIFY(loud.stdout_scans > 60);         // the cost stderr-only mode avoids
    }

    void no_callback_streams_nothing() {
        StreamSim s;
        s.has_callback = false;
        s.feed(false, "a\n");
        s.feed(true, "b\n");
        QVERIFY(s.lines.isEmpty());
        QCOMPARE(s.out_off, 0);
        QCOMPARE(s.err_off, 0);
    }
};

QTEST_GUILESS_MAIN(TstPythonStreamLines)
#include "tst_python_stream_lines.moc"
