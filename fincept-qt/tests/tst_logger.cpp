// Unit tests for src/core/logging — the logger core (Logger.{h,cpp}, LogFields.h, LogRedact.h).
//
// Every test function logs into its own directory (init() calls set_file()), so a file
// holds only that test's lines and rotation in one test cannot hide another's lines.
// The logger is a process singleton: tests restore levels / JSON mode / the writer in init().
//
// Covered: a disabled level evaluates nothing; logf formatting, quoting and escaping;
// log_url / log_path / log_mask / log_scrub against the leak patterns of the logging audit;
// the on-disk line format (the check_log_budget.py HEAD regex); repeat suppression (incl. the
// 5 s summary, flush, AUDIT never folded, N threads); LOG_*_RL and the sweep; LOG_TRANSITION
// and its bounded table; the audit sink (cannot be disabled, on disk before a worker's call
// returns, GUI-thread queue overflow reported); the full-queue policy (GUI thread drops without
// waiting, workers wait 200 ms for WARN, "Log lines dropped" marker); FATAL drains
// synchronously; WARN is not written on the calling thread; five previous session logs, the
// total size cap and size rotation; FINCEPT_LOG_LEVEL / FINCEPT_LOG_TAGS and level overrides;
// recent_problems() scrubbing; JSON mode; the thread field; flush_and_close() idempotence;
// and an 8 threads x 100,000 lines stress run (no torn, lost or reordered lines beyond the
// counted drop policy).

#include "core/logging/Logger.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#include <atomic>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using namespace fincept;

namespace {

enum PlainColor { Red = 3 };

QString qs(const char* utf8) {
    return QString::fromUtf8(utf8);
}

// The line head check_log_budget.py parses (tools/check_log_budget.py, HEAD).
const QRegularExpression& head_rx() {
    static const QRegularExpression rx(QStringLiteral(
        R"(^\[(\d{4}-\d\d-\d\d \d\d:\d\d:\d\d\.\d{3})[+-]\d\d:\d\d\] \[(TRACE|DEBUG|INFO|WARN|ERROR|FATAL|AUDIT)\] \[([^\]]*)\] ?(.*)$)"));
    return rx;
}

struct Entry {
    QString level;
    QString tag;
    QString msg;
    bool parsed = false;
    QString raw;
};

QStringList read_raw(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(f.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
}

// The current file plus its size-rotated parts, oldest first.
QStringList read_with_parts(const QString& path) {
    QStringList out;
    for (int i = 3; i >= 1; --i)
        out += read_raw(path + QLatin1Char('.') + QString::number(i));
    out += read_raw(path);
    return out;
}

QList<Entry> parse(const QStringList& lines) {
    QList<Entry> out;
    for (const QString& l : lines) {
        Entry e;
        e.raw = l;
        const QRegularExpressionMatch m = head_rx().match(l);
        if (m.hasMatch()) {
            e.parsed = true;
            e.level = m.captured(2);
            e.tag = m.captured(3);
            e.msg = m.captured(4);
        }
        out.append(e);
    }
    return out;
}

QList<Entry> with_tag(const QList<Entry>& all, const QString& tag) {
    QList<Entry> out;
    for (const Entry& e : all) {
        if (e.tag == tag)
            out.append(e);
    }
    return out;
}

QStringList msgs(const QList<Entry>& entries) {
    QStringList out;
    for (const Entry& e : entries)
        out << e.msg;
    return out;
}

int g_evals = 0;
int g_keys = 0;
int g_msgs = 0;
int count_eval() {
    return ++g_evals;
}
QString counted(const QString& s) {
    ++g_evals;
    return s;
}
QString counted_key(const QString& s) {
    ++g_keys;
    return s;
}
QString counted_msg(const QString& s) {
    ++g_msgs;
    return s;
}

// The forbidden-content patterns of check_log_budget.py.
QStringList budget_violations(const QString& text) {
    static const QList<QPair<const char*, QRegularExpression>> rules = {
        {"e-mail address", QRegularExpression(QStringLiteral(R"([\w.+-]+@[\w-]+\.[A-Za-z]{2,})"))},
        {"bearer token", QRegularExpression(QStringLiteral(R"(bearer\s+[A-Za-z0-9._\-]{8,})"),
                                            QRegularExpression::CaseInsensitiveOption)},
        {"JWT", QRegularExpression(QStringLiteral(R"(\beyJ[A-Za-z0-9_\-]{10,}\.[A-Za-z0-9_\-]{10,})"))},
        // Ordinary literal (not a raw string): moc's lexer loses track of a raw string
        // that contains a double quote, and then misses the Q_OBJECT class below.
        {"secret=value",
         QRegularExpression(QStringLiteral("(?:api[_-]?key|apikey|token|secret|password|passwd|access_token|signature)"
                                           "=(?!<)[^\\s&\"]{8,}"),
                            QRegularExpression::CaseInsensitiveOption)},
        {"home directory", QRegularExpression(QStringLiteral(R"((?:[A-Z]:\\Users\\|/home/|/Users/)[^\\/\s]+)"),
                                              QRegularExpression::CaseInsensitiveOption)},
    };
    QStringList hits;
    for (const auto& r : rules) {
        if (r.second.match(text).hasMatch())
            hits << QString::fromLatin1(r.first);
    }
    return hits;
}

} // namespace

class TstLogger : public QObject {
    Q_OBJECT

  public:
    enum class Side { Buy, Sell };
    Q_ENUM(Side)

    TstLogger() {
        // Read by the Logger constructor (first Logger::instance() is in initTestCase).
        qputenv("FINCEPT_LOG_LEVEL", "warn,EnvTag=debug");
        qputenv("FINCEPT_LOG_TAGS", "Other:error");
    }

  private slots:
    void initTestCase();
    void init();
    void cleanup();
    void cleanupTestCase();

    void environment_override_applied_at_start();
    void disabled_level_evaluates_nothing();
    void logf_formats_values();
    void logf_quotes_and_escapes();
    void text_line_format_is_parseable();
    void log_url_redacts();
    void log_path_redacts();
    void log_mask_shows_presence_and_last_four();
    void log_scrub_removes_secrets();
    void repeat_suppression();
    void repeat_summary_after_five_seconds();
    void repeat_suppression_across_threads();
    void rate_limit();
    void rate_limit_table_is_bounded();
    void transition();
    void audit_cannot_be_disabled();
    void audit_queue_overflow_is_reported();
    void queue_full_gui_thread_drops_without_waiting();
    void queue_full_worker_warn_waits_then_drops();
    void warn_is_not_written_on_the_calling_thread();
    void fatal_drains_synchronously();
    void five_previous_sessions_are_kept();
    void total_size_cap();
    void size_rotation();
    void level_spec_parsing_and_overrides();
    void recent_problems_are_scrubbed();
    void json_mode();
    void thread_field();
    void flush_and_close_is_idempotent();
    void stress_8_threads_100k_lines();

  private:
    QString dir_for(const QString& name) const { return tmp_.path() + QLatin1Char('/') + name + QStringLiteral("/logs"); }
    QString current_log() const { return current_dir_ + QStringLiteral("/fincept.log"); }
    QString current_audit() const { return current_dir_ + QStringLiteral("/audit.log"); }
    QList<Entry> entries() {
        Logger::instance().flush();
        return parse(read_with_parts(current_log()));
    }

    QTemporaryDir tmp_;
    QString current_dir_;
};

void TstLogger::initTestCase() {
    QVERIFY(tmp_.isValid());
    current_dir_ = dir_for(QStringLiteral("start"));
    Logger::instance().set_file(current_log());
    QVERIFY(QFile::exists(current_log()));
}

void TstLogger::init() {
    auto& log = Logger::instance();
    log.set_writer_paused_for_testing(false);
    log.set_json_mode(false);
    log.set_level(LogLevel::Info);
    log.clear_all_tag_levels();
    if (qstrcmp(QTest::currentTestFunction(), "environment_override_applied_at_start") != 0)
        log.clear_level_overrides();
    current_dir_ = dir_for(QString::fromLatin1(QTest::currentTestFunction()));
    log.set_file(current_log());
    LOG_INFO("Harness", QStringLiteral("Test started name=%1").arg(QString::fromLatin1(QTest::currentTestFunction())));
}

void TstLogger::cleanup() {
    Logger::instance().set_writer_paused_for_testing(false);
    Logger::instance().flush();
}

void TstLogger::cleanupTestCase() {
    Logger::instance().flush_and_close();
}

// ── Levels ───────────────────────────────────────────────────────────────────

void TstLogger::environment_override_applied_at_start() {
    auto& log = Logger::instance();
    QVERIFY(log.global_level() == LogLevel::Warn); // FINCEPT_LOG_LEVEL=warn wins over set_level(Info) in init()
    QVERIFY(log.is_enabled(LogLevel::Debug, QStringLiteral("EnvTag")));
    QVERIFY(!log.is_enabled(LogLevel::Warn, QStringLiteral("Other")));
    QCOMPARE(log.level_overrides(),
             qs("level=Warn tags=EnvTag:Debug,Other:Error (FINCEPT_LOG_LEVEL+FINCEPT_LOG_TAGS)"));
    // Logged once, in the first file, whatever the level.
    const QList<Entry> start = parse(read_raw(dir_for(QStringLiteral("start")) + QStringLiteral("/fincept.log")));
    QCOMPARE(msgs(with_tag(start, QStringLiteral("App"))),
             QStringList{qs("Log level overridden source=FINCEPT_LOG_LEVEL+FINCEPT_LOG_TAGS level=Warn "
                            "tags=EnvTag:Debug,Other:Error")});
    log.clear_level_overrides();
    QVERIFY(log.global_level() == LogLevel::Info); // back to the settings value
    QCOMPARE(log.level_overrides(), QString());
}

void TstLogger::disabled_level_evaluates_nothing() {
    auto& log = Logger::instance();
    g_evals = 0;
    LOG_DEBUG("Lazy", counted(QStringLiteral("x")));
    LOG_TRACE("Lazy", logf(QStringLiteral("Field"), {{"n", count_eval()}}));
    LOG_DEBUG_F("Lazy", "a %1", count_eval());
    LOG_DEBUG_RL("Lazy", counted(QStringLiteral("key")), 1000, counted(QStringLiteral("msg")));
    LOG_TRANSITION("Lazy", counted(QStringLiteral("k")), 1, LogLevel::Debug, counted(QStringLiteral("m")));
    QCOMPARE(g_evals, 0);

    // An override for another tag enables nothing here.
    log.set_tag_level(QStringLiteral("Other"), LogLevel::Trace);
    LOG_DEBUG("Lazy", counted(QStringLiteral("x")));
    QCOMPARE(g_evals, 0);
    log.clear_all_tag_levels();

    LOG_INFO("Lazy", counted(QStringLiteral("evaluated once")));
    QCOMPARE(g_evals, 1);
    QCOMPARE(msgs(with_tag(entries(), QStringLiteral("Lazy"))), QStringList{qs("evaluated once")});
}

// ── Fields ───────────────────────────────────────────────────────────────────

void TstLogger::logf_formats_values() {
    const QString s = logf(QStringLiteral("Order submitted"),
                           {{"symbol", "AAPL"},
                            {"venue", QStringLiteral("paper")},
                            {"qty", 10},
                            {"big", qint64(1) << 40},
                            {"u", quint64(18446744073709551615ULL)},
                            {"neg", -7},
                            {"size", std::size_t(3)},
                            {"price", 101.25},
                            {"ratio", 0.1f},
                            {"ok", true},
                            {"no", false},
                            {"ch", 'x'},
                            {"bytes", QByteArray("raw")},
                            {"std", std::string("s")},
                            {"sv", std::string_view("v")},
                            {"lat", QLatin1StringView("l")},
                            {"view", QStringView(u"w")},
                            {"side", Side::Sell},
                            {"plain", Red},
                            {"url", QUrl(QStringLiteral("https://u:p@hooks.slack.com/services/T/B/x?token=1"))},
                            {"none", static_cast<const char*>(nullptr)}});
    QCOMPARE(s, qs("Order submitted symbol=AAPL venue=paper qty=10 big=1099511627776 u=18446744073709551615 neg=-7 "
                   "size=3 price=101.25 ratio=0.1 ok=true no=false ch=x bytes=raw std=s sv=v lat=l view=w side=Sell "
                   "plain=3 url=https://hooks.slack.com/<redacted> none=\"\""));
    QCOMPARE(logf(QStringLiteral("Big"), {{"price", 1234567.891}, {"tiny", 0.000012}}),
             qs("Big price=1234567.891 tiny=1.2e-05"));
    // Field order is the caller's; the QList form for conditional fields.
    QCOMPARE(logf(QStringLiteral("S"), {{"b", 1}, {"a", 2}}), qs("S b=1 a=2"));
    QList<LogField> fields{{"venue", "paper"}};
    fields.append(LogField("price", 1.5));
    QCOMPARE(logf(QStringLiteral("S"), fields), qs("S venue=paper price=1.5"));
}

void TstLogger::logf_quotes_and_escapes() {
    const auto one = [](const LogField& f) { return logf(QStringLiteral("E"), {f}); };
    QCOMPARE(one({"error", "Connection refused"}), qs(R"(E error="Connection refused")"));
    QCOMPARE(one({"v", ""}), qs(R"(E v="")"));
    QCOMPARE(one({"v", "a=b"}), qs(R"(E v="a=b")"));
    // Not a raw string: MSVC's preprocessor misreads \" inside a raw string in a macro argument.
    QCOMPARE(one({"v", "say \"hi\""}), qs("E v=\"say \\\"hi\\\"\""));
    QCOMPARE(one({"v", "line1\nline2\r\n"}), qs(R"(E v="line1\nline2\r\n")"));
    QCOMPARE(one({"v", "tab\there"}), qs(R"(E v="tab\there")"));
    QCOMPARE(one({"v", "C:\\Program Files\\x"}), qs(R"(E v="C:\\Program Files\\x")"));
    QCOMPARE(one({"v", "C:\\dir\\x"}), qs(R"(E v=C:\dir\x)")); // bare: nothing to quote
    QCOMPARE(one({"v", QString(QChar(0x01))}), qs(R"(E v="\x01")"));
    QCOMPARE(one({"v", QStringLiteral("x\u00A0y")}), qs("E v=\"x\xC2\xA0y\"")); // Unicode space quotes
    QCOMPARE(one({"v", QStringLiteral("caf\u00E9")}), qs("E v=caf\xC3\xA9"));

    // Cut at 200 characters with "…", inside or outside quotes.
    QCOMPARE(one({"v", QString(300, QLatin1Char('a'))}), qs("E v=") + QString(200, QLatin1Char('a')) + QChar(0x2026));
    const QString spaced = QString(150, QLatin1Char('a')) + QLatin1Char(' ') + QString(150, QLatin1Char('b'));
    QCOMPARE(one({"v", spaced}), qs("E v=\"") + spaced.left(200) + QChar(0x2026) + QLatin1Char('"'));
    // A surrogate pair is never split.
    const QString emoji = QString(199, QLatin1Char('a')) + QString::fromUtf8("\xF0\x9F\x98\x80") + QStringLiteral("zz");
    QCOMPARE(one({"v", emoji}), qs("E v=") + QString(199, QLatin1Char('a')) + QChar(0x2026));

    QCOMPARE(log_quote(u"plain"), qs("plain"));
    QCOMPARE(log_quote(u"two words"), qs("\"two words\""));
    QCOMPARE(log_quote(u"abcdef", 3), qs("abc\xE2\x80\xA6"));
}

void TstLogger::text_line_format_is_parseable() {
    LOG_INFO("Fmt", "First line\nsecond line\r\nthird\rfourth");
    LOG_WARN("Fmt", logf(QStringLiteral("HTTP request failed"), {{"status", 404}, {"error", "Not Found"}}));
    const QList<Entry> all = entries();
    for (const Entry& e : all)
        QVERIFY2(e.parsed, qPrintable(e.raw));
    const QList<Entry> fmt = with_tag(all, QStringLiteral("Fmt"));
    QCOMPARE(fmt.size(), 2);
    QCOMPARE(fmt[0].level, qs("INFO"));
    QCOMPARE(fmt[0].msg, qs(R"(First line\nsecond line\nthird\rfourth)"));
    QCOMPARE(fmt[1].level, qs("WARN"));
    QCOMPARE(fmt[1].msg, qs(R"(HTTP request failed status=404 error="Not Found")"));
}

// ── Redaction ────────────────────────────────────────────────────────────────

void TstLogger::log_url_redacts() {
    const QList<QPair<const char*, const char*>> cases = {
        {"https://api.fincept.in/market/quote?symbol=AAPL&apikey=SECRETKEY", "https://api.fincept.in/market/quote?<redacted>"},
        {"https://api.opsgenie.com/v2/alerts?apiKey=SECRETKEY", "https://api.opsgenie.com/<redacted>"},
        {"https://api.telegram.org/bot123456:SECRETTOKEN/sendMessage", "https://api.telegram.org/<redacted>"},
        {"https://hooks.slack.com/services/T0/B1/SECRETPATH", "https://hooks.slack.com/<redacted>"},
        {"http://user:SECRETPASS@proxy.corp:3128", "http://proxy.corp:3128"},
        {"http://user:SECRETPASS@proxy.corp:3128/", "http://proxy.corp:3128"},
        {"socks5://u:SECRETPASS@10.0.0.1:1080", "socks5://10.0.0.1:1080"},
        {"wss://stream.example.com/ws?token=SECRETTOKEN", "wss://stream.example.com/<redacted>"},
        {"/user/login?password=SECRETPASS", "/user/login?<redacted>"},
        {"/macro/upcoming-events", "/macro/upcoming-events"},
        {"https://api.fincept.in/x#SECRETFRAG", "https://api.fincept.in/x"},
        {"https://example.com", "https://example.com"},
        {"not a url", "<invalid url>"},
        {"user:SECRETPASS@proxy:8080", "<invalid url>"},
        {"", ""},
    };
    for (const auto& c : cases) {
        const QString out = log_url(qs(c.first));
        QCOMPARE(out, qs(c.second));
        QVERIFY2(!out.contains(QLatin1String("SECRET")), c.first);
    }
    QCOMPARE(log_url(QUrl(QStringLiteral("https://u:SECRETPASS@api.fincept.in/a?b=c"))),
             qs("https://api.fincept.in/a?<redacted>"));
    QCOMPARE(log_url(QUrl::fromLocalFile(QDir::homePath() + QStringLiteral("/x.csv"))), qs("file://~/x.csv"));

    log_url_add_first_party_host(QStringLiteral("https://data.example.org"));
    QCOMPARE(log_url(qs("https://data.example.org/v1/series?key=SECRETKEY")),
             qs("https://data.example.org/v1/series?<redacted>"));
    QCOMPARE(log_url(qs("https://other.example.org/v1/series")), qs("https://other.example.org/<redacted>"));
}

void TstLogger::log_path_redacts() {
    using log_detail::log_path_with;
    using log_detail::PathRoots;
    const PathRoots win{qs("C:/Users/Bob/AppData/Local/com.fincept.terminal.community"), qs("C:/Program Files/Fincept"),
                        qs("C:/Users/Bob"), true};
    QCOMPARE(log_path_with(qs(R"(C:\Users\Bob\AppData\Local\com.fincept.terminal.community\data\fincept.db)"), win),
             qs(R"(<data>\data\fincept.db)"));
    QCOMPARE(log_path_with(qs(R"(c:\users\bob\Documents\notes.txt)"), win), qs(R"(~\Documents\notes.txt)"));
    QCOMPARE(log_path_with(qs("C:/Program Files/Fincept/python/python.exe"), win), qs("<app>/python/python.exe"));
    QCOMPARE(log_path_with(qs(R"(C:\Users\Alice\x.txt)"), win), qs(R"(~\x.txt)")); // someone else's home
    QCOMPARE(log_path_with(qs(R"(C:\Users\Bobby\x)"), win), qs(R"(~\x)"));         // not "Bob" + "by"
    QCOMPARE(log_path_with(qs(R"(D:\ft-community-data\logs)"), win), qs(R"(D:\ft-community-data\logs)"));

    const PathRoots nix{qs("/home/bob/.local/share/com.fincept.terminal.community"), qs("/opt/fincept"),
                        qs("/home/bob"), false};
    QCOMPARE(log_path_with(qs("/home/bob/.local/share/com.fincept.terminal.community/logs/fincept.log"), nix),
             qs("<data>/logs/fincept.log"));
    QCOMPARE(log_path_with(qs("/home/bob/notes/x.ipynb"), nix), qs("~/notes/x.ipynb"));
    QCOMPARE(log_path_with(qs("/home/bob"), nix), qs("~"));
    QCOMPARE(log_path_with(qs("/home/bobby/x"), nix), qs("~/x"));
    QCOMPARE(log_path_with(qs("/Users/carol/Library/x"), nix), qs("~/Library/x"));
    QCOMPARE(log_path_with(qs("/opt/fincept/bin/FinceptTerminal"), nix), qs("<app>/bin/FinceptTerminal"));
    QCOMPARE(log_path_with(qs("/tmp/x.log"), nix), qs("/tmp/x.log"));
    QCOMPARE(log_path_with(QString(), nix), QString());

    // Live roots: this process's home, and the data root learned from set_file().
    QCOMPARE(log_path(QDir::homePath() + QStringLiteral("/sub/file.txt")), qs("~/sub/file.txt"));
    const QString data_root = QFileInfo(current_dir_).absolutePath();
    QCOMPARE(log_path(data_root + QStringLiteral("/data/fincept.db")), qs("<data>/data/fincept.db"));
}

void TstLogger::log_mask_shows_presence_and_last_four() {
    QCOMPARE(log_mask(QString()), QString());
    QCOMPARE(log_mask(qs("1234")), QString(QChar(0x2026)));
    QCOMPARE(log_mask(qs("AB12345678")), QChar(0x2026) + qs("5678"));
    QCOMPARE(log_mask(qs("AB12345678"), 0), QString(QChar(0x2026)));
    QCOMPARE(log_mask(qs("AB12345678"), 6), QChar(0x2026) + qs("345678"));
}

void TstLogger::log_scrub_removes_secrets() {
    const QString in = qs(
        "Authorization: Bearer abc.DEF-123456789 sent to user.name@example.com with api_key=SECRETVALUE1 and "
        "password: hunter2xyz at C:\\Users\\Bob\\x and /home/alice/y token "
        "eyJhbGciOiJIUzI1NiJ9.eyJzdWIiOiIxMjM0NTY3ODkwIn0.c2lnbmF0dXJl url "
        "https://hooks.slack.com/services/T0/B1/xyz?x=1 proxy http://me:pw123456@proxy:3128 id "
        "3f9a8b7c6d5e4f3a2b1c0d9e8f7a6b5c4d3e2f1a session_token=\"quoted secret\" broker_api_key=XYZSECRET12 "
        "proxy-authorization: Basic dXNlcjpwYXNzd29yZA== x-auth-token: tok_live_ABCDEFG");
    const QString out = log_scrub(in);
    for (const char* leaked : {"abc.DEF-123456789", "user.name@example.com", "SECRETVALUE1", "hunter2xyz", "\\Users\\Bob",
                               "/home/alice", "eyJhbGci", "T0/B1/xyz", "pw123456", "3f9a8b7c6d5e", "quoted secret",
                               "XYZSECRET12", "dXNlcjpwYXNz", "tok_live_ABCDEFG"})
        QVERIFY2(!out.contains(qs(leaked)), leaked);
    for (const char* kept : {"Bearer <redacted>", "<email>", "api_key=<redacted>", "<jwt>", "<token>",
                             "https://hooks.slack.com/<redacted>", "http://proxy:3128", "broker_api_key=<redacted>",
                             "Basic <redacted>", "x-auth-token: <redacted>"})
        QVERIFY2(out.contains(qs(kept)), kept);
    QCOMPARE(budget_violations(out), QStringList());
    QVERIFY(!budget_violations(in).isEmpty()); // the rules do fire on the raw text
    const QString ordinary = qs("Order accepted venue=paper qty=10 price=101.5 order_id=o-9 tokens_in=1200");
    QCOMPARE(log_scrub(ordinary), ordinary);
}

// ── Repeat suppression ───────────────────────────────────────────────────────

void TstLogger::repeat_suppression() {
    for (int i = 0; i < 5; ++i)
        LOG_INFO("Rep", "Same line");
    LOG_INFO("Rep", "Different line");
    LOG_INFO("RepA", "X"); // other tag: not folded
    LOG_INFO("RepB", "X");
    LOG_INFO("Rep", "Y"); // other level: not folded
    LOG_WARN("Rep", "Y");
    for (int i = 0; i < 3; ++i)
        LOG_ERROR("Rep", "Boom"); // the first ERROR is always written
    for (int i = 0; i < 3; ++i)
        LOG_AUDIT("RepAudit", "Same audit"); // never folded
    for (int i = 0; i < 3; ++i)
        LOG_INFO("Rep", "Tail"); // summary only at flush
    const QList<Entry> all = entries();
    QStringList got;
    for (const Entry& e : all) {
        if (e.tag.startsWith(QLatin1String("Rep")))
            got << e.level + QLatin1Char(' ') + e.tag + QLatin1Char(' ') + e.msg;
    }
    const QStringList want = {
        qs("INFO Rep Same line"),
        qs("INFO Rep last message repeated 4 times"),
        qs("INFO Rep Different line"),
        qs("INFO RepA X"),
        qs("INFO RepB X"),
        qs("INFO Rep Y"),
        qs("WARN Rep Y"),
        qs("ERROR Rep Boom"),
        qs("ERROR Rep last message repeated 2 times"),
        qs("AUDIT RepAudit Same audit"),
        qs("AUDIT RepAudit Same audit"),
        qs("AUDIT RepAudit Same audit"),
        qs("INFO Rep Tail"),
        qs("INFO Rep last message repeated 2 times"),
    };
    QCOMPARE(got, want);
    QVERIFY(Logger::instance().stats().repeats_suppressed >= 8);
}

void TstLogger::repeat_summary_after_five_seconds() {
    for (int i = 0; i < 3; ++i)
        LOG_INFO("Quiet", "Quiet repeat");
    // No flush and no other line: the writer emits the summary by itself after 5 s.
    QTRY_VERIFY_WITH_TIMEOUT(msgs(with_tag(parse(read_raw(current_log())), QStringLiteral("Quiet")))
                                 .contains(qs("last message repeated 2 times")),
                             9000);
}

void TstLogger::repeat_suppression_across_threads() {
    constexpr int kThreads = 8;
    constexpr int kPer = 2000;
    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t)
        threads.emplace_back([]() {
            for (int i = 0; i < kPer; ++i)
                LOG_INFO("RepMT", "Shared message");
        });
    for (auto& th : threads)
        th.join();
    LOG_INFO("RepMT", "After");
    int written = 0;
    qint64 folded = 0;
    const QRegularExpression rx(QStringLiteral(R"(^last message repeated (\d+) times$)"));
    for (const Entry& e : with_tag(entries(), QStringLiteral("RepMT"))) {
        QVERIFY2(e.parsed, qPrintable(e.raw));
        if (e.msg == QLatin1String("Shared message"))
            ++written;
        else if (const auto m = rx.match(e.msg); m.hasMatch())
            folded += m.captured(1).toLongLong();
    }
    QCOMPARE(written, 1);
    QCOMPARE(written + folded, qint64(kThreads) * kPer);
}

// ── Rate limit and transitions ───────────────────────────────────────────────

void TstLogger::rate_limit() {
    g_keys = 0;
    g_msgs = 0;
    for (int i = 0; i < 6; ++i)
        LOG_WARN_RL("RL", counted_key(QStringLiteral("topic|reason")), 400,
                    counted_msg(QStringLiteral("Topic failing topic=x")));
    QCOMPARE(g_keys, 6); // the key is evaluated (level on) ...
    QCOMPARE(g_msgs, 1); // ... the message only for the line written
    QTest::qWait(450);
    LOG_WARN_RL("RL", QStringLiteral("topic|reason"), 400, QStringLiteral("Topic failing topic=x"));

    for (int i = 0; i < 4; ++i)
        LOG_WARN_RL("RL", QStringLiteral("burst"), 300, QStringLiteral("Burst line"));
    // A burst that stops is reported by the writer's sweep once its interval ends.
    QTRY_VERIFY_WITH_TIMEOUT(msgs(with_tag(parse(read_raw(current_log())), QStringLiteral("RL")))
                                 .contains(qs("Repeated lines suppressed key=burst suppressed=3 interval_ms=300")),
                             4000);

    const QStringList got = msgs(with_tag(entries(), QStringLiteral("RL")));
    QCOMPARE(got.count(qs("Topic failing topic=x")) + got.count(qs("Topic failing topic=x (suppressed=5)")), 2);
    // The 5 suppressed lines are reported exactly once: as the suffix of the next line, or by
    // the sweep if it ran first (both are correct; which one wins is timing).
    const bool suffix = got.contains(qs("Topic failing topic=x (suppressed=5)"));
    const bool swept = got.contains(qs("Repeated lines suppressed key=topic|reason suppressed=5 interval_ms=400"));
    QVERIFY(suffix != swept);
    QCOMPARE(got.count(qs("Burst line")), 1);
}

void TstLogger::rate_limit_table_is_bounded() {
    for (int i = 0; i < 3000; ++i)
        LOG_INFO_RL("RLB", QString::number(i), 60000, QStringLiteral("Keyed line"));
    QVERIFY(Logger::instance().stats().rate_limit_keys <= 1024);
}

void TstLogger::transition() {
    auto& log = Logger::instance();
    g_msgs = 0;
    LOG_TRANSITION("Tr", QStringLiteral("feed-a"), 0, LogLevel::Warn, counted_msg(qs("healthy at start: nothing")));
    LOG_TRANSITION("Tr", QStringLiteral("feed-a"), 1, LogLevel::Warn, logf(qs("Feed failed"), {{"feed", "a"}}));
    LOG_TRANSITION("Tr", QStringLiteral("feed-a"), 1, LogLevel::Warn, counted_msg(qs("still failing")));
    LOG_TRANSITION("Tr", QStringLiteral("feed-a"), true, LogLevel::Warn, counted_msg(qs("still failing")));
    QCOMPARE(g_msgs, 0);

    LogTransition info;
    QVERIFY(log.transition(QStringLiteral("Tr"), QStringLiteral("feed-a"), 0, &info));
    QCOMPARE(info.previous_state, 1);
    QCOMPARE(info.observations_in_previous, 3);
    QVERIFY(info.ms_in_previous >= 0);
    LOG_TRANSITION("Tr", QStringLiteral("feed-a"), 0, LogLevel::Info, counted_msg(qs("already healthy")));
    QCOMPARE(g_msgs, 0);

    // Several non-healthy states.
    QVERIFY(log.transition(QStringLiteral("Tr"), QStringLiteral("feed-b"), 1));
    QVERIFY(log.transition(QStringLiteral("Tr"), QStringLiteral("feed-b"), 2));
    QVERIFY(!log.transition(QStringLiteral("Tr"), QStringLiteral("feed-b"), 2));
    QVERIFY(log.transition(QStringLiteral("Tr"), QStringLiteral("feed-b"), 0));

    // Thousands of failing symbols cannot grow the table without bound; recovered keys leave it.
    for (int i = 0; i < 5000; ++i)
        log.transition(QStringLiteral("TrBulk"), QString::number(i), 1);
    QVERIFY(log.stats().transition_keys <= 1024);
    for (int i = 0; i < 5000; ++i)
        log.transition(QStringLiteral("TrBulk"), QString::number(i), 0);
    QCOMPARE(log.stats().transition_keys, 0);
    QVERIFY(!log.transition_possible(0));

    QCOMPARE(msgs(with_tag(entries(), QStringLiteral("Tr"))), QStringList{qs("Feed failed feed=a")});
}

// ── Audit ────────────────────────────────────────────────────────────────────

void TstLogger::audit_cannot_be_disabled() {
    auto& log = Logger::instance();
    log.set_level(LogLevel::Fatal);
    log.set_tag_level(QStringLiteral("Orders"), LogLevel::Fatal);
    LOG_AUDIT("Orders", "Order submitted",
              {{"venue", "paper"},
               {"account", log_mask(qs("AB12345678"))},
               {"mode", "paper"},
               {"symbol", "AAPL"},
               {"side", "buy"},
               {"type", "limit"},
               {"qty", 10},
               {"price", 101.5},
               {"client_id", "c-1"},
               {"source", "ticket"}});
    // Off the GUI thread the line is in audit.log before LOG_AUDIT returns.
    bool on_disk_at_return = false;
    std::thread worker([&]() {
        LOG_AUDIT("Orders", logf(qs("Order accepted"), {{"client_id", "c-1"}, {"order_id", "o-9"}, {"elapsed_ms", 42}}));
        on_disk_at_return = read_raw(current_audit()).join(QLatin1Char('\n')).contains(qs("Order accepted client_id=c-1"));
    });
    worker.join();
    QVERIFY(on_disk_at_return);
    log.set_level(LogLevel::Info);
    log.clear_all_tag_levels();

    const QString submitted =
        QStringLiteral("Order submitted venue=paper account=") + QChar(0x2026) +
        qs("5678 mode=paper symbol=AAPL side=buy type=limit qty=10 price=101.5 client_id=c-1 source=ticket");
    const QStringList want = {submitted, qs("Order accepted client_id=c-1 order_id=o-9 elapsed_ms=42")};
    QList<Entry> main_orders = with_tag(entries(), QStringLiteral("Orders"));
    QCOMPARE(msgs(main_orders), want);
    for (const Entry& e : main_orders)
        QCOMPARE(e.level, qs("AUDIT"));
    const QList<Entry> audit = parse(read_raw(current_audit()));
    QCOMPARE(msgs(audit), want);
    for (const Entry& e : audit)
        QCOMPARE(e.level, qs("AUDIT"));
    QCOMPARE(Logger::instance().audit_file(), current_audit());

    // From the GUI thread: handed to the writer, on disk within one wake-up.
    LOG_AUDIT("Orders", "Kill switch engaged scope=all");
    QTRY_VERIFY_WITH_TIMEOUT(read_raw(current_audit()).join(QLatin1Char('\n')).contains(qs("Kill switch engaged scope=all")),
                             2000);
}

void TstLogger::audit_queue_overflow_is_reported() {
    auto& log = Logger::instance();
    log.set_writer_paused_for_testing(true);
    const quint64 lost0 = log.stats().audit_lost_lines;
    for (int i = 0; i < 10000 + 5; ++i)
        LOG_AUDIT("Orders", QStringLiteral("Paused audit n=%1").arg(i));
    QCOMPARE(log.stats().audit_lost_lines - lost0, quint64(5));
    log.set_writer_paused_for_testing(false);
    const QList<Entry> main = entries();
    const QList<Entry> in_audit = parse(read_raw(current_audit()));
    int kept = 0;
    for (const Entry& e : in_audit)
        kept += e.msg.startsWith(QLatin1String("Paused audit")) ? 1 : 0;
    QCOMPARE(kept, 10000);
    QVERIFY(msgs(in_audit).contains(qs("Audit lines lost count=5 reason=queue_full")));
    bool error_in_main = false;
    for (const Entry& e : main)
        error_in_main = error_in_main || (e.level == QLatin1String("ERROR") && e.msg == qs("Audit lines lost count=5 reason=queue_full"));
    QVERIFY(error_in_main);
}

// ── Queue, I/O model ─────────────────────────────────────────────────────────

void TstLogger::queue_full_gui_thread_drops_without_waiting() {
    auto& log = Logger::instance();
    log.set_writer_paused_for_testing(true);
    const quint64 dropped0 = log.stats().dropped_lines;
    const QString pad(100, QLatin1Char('p'));
    for (int i = 0; i < 50000; ++i) // ~7.5 MB: past the 4 MB bound
        LOG_INFO("Q", QStringLiteral("fill n=%1 pad=%2").arg(i).arg(pad));
    QElapsedTimer t;
    t.start();
    for (int i = 0; i < 100; ++i)
        LOG_WARN("Q", QStringLiteral("warn while full n=%1").arg(i));
    const qint64 warn_ms = t.elapsed();
    QVERIFY2(warn_ms < 1000, qPrintable(QString::number(warn_ms))); // 100 x 200 ms if the GUI thread waited
    const quint64 dropped = log.stats().dropped_lines - dropped0;
    QVERIFY(dropped >= 100);
    log.set_writer_paused_for_testing(false);
    log.flush();
    LOG_INFO("Q", "after the stall");
    const QList<Entry> all = entries();
    QStringList markers;
    for (const Entry& e : all) {
        QVERIFY2(e.parsed, qPrintable(e.raw.left(200)));
        if (e.tag == QLatin1String("App") && e.msg.startsWith(QLatin1String("Log lines dropped")))
            markers << e.level + QLatin1Char(' ') + e.msg;
    }
    QCOMPARE(markers, QStringList{QStringLiteral("WARN Log lines dropped count=%1 reason=queue_full").arg(dropped)});
    int fills = 0;
    for (const Entry& e : with_tag(all, QStringLiteral("Q")))
        fills += e.msg.startsWith(QLatin1String("fill ")) ? 1 : 0;
    QCOMPARE(quint64(fills) + dropped, quint64(50000 + 100)); // every line written or counted
}

void TstLogger::queue_full_worker_warn_waits_then_drops() {
    auto& log = Logger::instance();
    log.set_writer_paused_for_testing(true);
    const QString pad(100, QLatin1Char('p'));
    for (int i = 0; i < 50000; ++i)
        LOG_INFO("Q", QStringLiteral("fill n=%1 pad=%2").arg(i).arg(pad));
    qint64 warn_ms = -1;
    qint64 info_ms = -1;
    std::thread worker([&]() {
        QElapsedTimer t;
        t.start();
        LOG_WARN("Q", "worker warn while full");
        warn_ms = t.elapsed();
        t.restart();
        LOG_INFO("Q", "worker info while full");
        info_ms = t.elapsed();
    });
    worker.join();
    QVERIFY2(warn_ms >= 150 && warn_ms < 2000, qPrintable(QString::number(warn_ms))); // bounded wait (200 ms)
    QVERIFY2(info_ms < 100, qPrintable(QString::number(info_ms)));                     // INFO never waits
    log.set_writer_paused_for_testing(false);
    log.flush();
    const QStringList got = msgs(with_tag(entries(), QStringLiteral("Q")));
    QVERIFY(!got.contains(qs("worker warn while full")));
    QVERIFY(!got.contains(qs("worker info while full")));
}

void TstLogger::warn_is_not_written_on_the_calling_thread() {
    auto& log = Logger::instance();
    log.flush();
    log.set_writer_paused_for_testing(true);
    LOG_WARN("Async", "warn handed to the writer");
    LOG_ERROR("Async", "error handed to the writer");
    QVERIFY(!read_raw(current_log()).join(QLatin1Char('\n')).contains(qs("handed to the writer")));
    log.set_writer_paused_for_testing(false);
    QTRY_COMPARE_WITH_TIMEOUT(msgs(with_tag(parse(read_raw(current_log())), QStringLiteral("Async"))).size(), 2, 2000);
}

void TstLogger::fatal_drains_synchronously() {
    auto& log = Logger::instance();
    log.set_writer_paused_for_testing(true); // the writer cannot help
    LOG_INFO("Fat", "queued before the fatal line");
    log.fatal(QStringLiteral("Fat"), QStringLiteral("Fatal line"));
    const QStringList on_disk = msgs(with_tag(parse(read_raw(current_log())), QStringLiteral("Fat")));
    QCOMPARE(on_disk, (QStringList{qs("queued before the fatal line"), qs("Fatal line")}));
}

// ── Retention ────────────────────────────────────────────────────────────────

void TstLogger::five_previous_sessions_are_kept() {
    auto& log = Logger::instance();
    const QString path = dir_for(QStringLiteral("sessions")) + QStringLiteral("/fincept.log");
    const auto prev = [&path](int i) {
        return i == 0 ? path + QStringLiteral(".prev") : path + QStringLiteral(".prev.") + QString::number(i);
    };
    for (int s = 1; s <= 8; ++s) {
        log.set_file(path);
        LOG_INFO("Sess", QStringLiteral("session %1").arg(s));
        log.flush();
    }
    QVERIFY(QFile::exists(path));
    for (int i = 0; i < 5; ++i)
        QVERIFY2(QFile::exists(prev(i)), qPrintable(prev(i)));
    QVERIFY(!QFile::exists(path + QStringLiteral(".prev.5")));
    const auto session_of = [](const QString& file) { return msgs(with_tag(parse(read_raw(file)), QStringLiteral("Sess"))); };
    QCOMPARE(session_of(path), QStringList{qs("session 8")});
    QCOMPARE(session_of(prev(0)), QStringList{qs("session 7")});
    QCOMPARE(session_of(prev(4)), QStringList{qs("session 3")});
    log.set_file(current_log());
}

void TstLogger::total_size_cap() {
    auto& log = Logger::instance();
    const QString dir = dir_for(QStringLiteral("cap"));
    QVERIFY(QDir().mkpath(dir));
    const QString path = dir + QStringLiteral("/fincept.log");
    const auto prev = [&path](int i) {
        return i == 0 ? path + QStringLiteral(".prev") : path + QStringLiteral(".prev.") + QString::number(i);
    };
    constexpr qint64 k12MB = 12LL * 1024 * 1024;
    for (const QString& f : {path, prev(0), prev(1), prev(2), prev(3)}) {
        QFile file(f);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.resize(k12MB));
    }
    log.set_file(path); // shift: 5 previous sessions x 12 MB = 60 MB > 50 MB cap
    LOG_INFO("Cap", "new session");
    log.flush();
    QVERIFY(!QFile::exists(prev(4))); // the oldest went
    QVERIFY(QFile::exists(prev(3)));
    qint64 total = 0;
    const QFileInfoList files = QDir(dir).entryInfoList({QStringLiteral("fincept.log*")}, QDir::Files);
    for (const QFileInfo& fi : files)
        total += fi.size();
    QVERIFY2(total <= 50LL * 1024 * 1024, qPrintable(QString::number(total)));
    log.set_file(current_log());
    for (const QFileInfo& fi : files)
        QFile::remove(fi.absoluteFilePath());
}

void TstLogger::size_rotation() {
    const QString pad(100, QLatin1Char('r'));
    for (int i = 0; i < 45000; ++i) // ~6 MB
        LOG_INFO("Rot", QStringLiteral("rotation n=%1 pad=%2").arg(i).arg(pad));
    Logger::instance().flush();
    QVERIFY(QFile::exists(current_log() + QStringLiteral(".1")));
    QVERIFY(QFileInfo(current_log()).size() < 5LL * 1024 * 1024);
    int seen = 0;
    for (const Entry& e : with_tag(parse(read_with_parts(current_log())), QStringLiteral("Rot")))
        seen += e.msg.startsWith(QLatin1String("rotation ")) ? 1 : 0;
    QCOMPARE(seen, 45000);
}

void TstLogger::level_spec_parsing_and_overrides() {
    auto& log = Logger::instance();
    QVERIFY(Logger::parse_level(u"WARNING") == LogLevel::Warn);
    QVERIFY(Logger::parse_level(u" 3 ") == LogLevel::Warn);
    QVERIFY(Logger::parse_level(u"0") == LogLevel::Trace);
    QVERIFY(Logger::parse_level(u"err") == LogLevel::Error);
    QVERIFY(Logger::parse_level(u"critical") == LogLevel::Fatal);
    QVERIFY(!Logger::parse_level(u"6"));
    QVERIFY(!Logger::parse_level(QStringView()));
    QVERIFY(!Logger::parse_level(u"verbose"));
    QCOMPARE(Logger::level_name(LogLevel::Warn), qs("Warn"));

    const LogLevelSpec spec = Logger::parse_level_spec(u"debug,DataHub=trace;HTTP:warn  bogus X=nope 7");
    QVERIFY(spec.global == LogLevel::Debug);
    QCOMPARE(spec.tags.size(), 2);
    QVERIFY(spec.tags.value(qs("DataHub")) == LogLevel::Trace);
    QVERIFY(spec.tags.value(qs("HTTP")) == LogLevel::Warn);
    QCOMPARE(spec.invalid, (QStringList{qs("bogus"), qs("X=nope"), qs("7")}));

    log.apply_level_overrides(Logger::parse_level_spec(u"error,Env:debug,nonsense"), qs("--log-level"));
    log.set_level(LogLevel::Trace); // a settings change does not undo the override
    QVERIFY(log.global_level() == LogLevel::Error);
    log.set_tag_level(qs("Env"), LogLevel::Error);
    QVERIFY(log.is_enabled(LogLevel::Debug, qs("Env")));
    QVERIFY(log.tag_levels().value(qs("Env")) == LogLevel::Debug);
    QCOMPARE(log.level_overrides(), qs("level=Error tags=Env:Debug (--log-level)"));
    const QStringList app = msgs(with_tag(entries(), QStringLiteral("App")));
    QVERIFY(app.contains(qs("Log level overridden source=--log-level level=Error tags=Env:Debug")));
    QVERIFY(app.contains(qs("Log level setting ignored source=--log-level value=nonsense")));
    log.clear_level_overrides();
    QVERIFY(log.global_level() == LogLevel::Trace); // the settings value comes back
    QVERIFY(log.tag_levels().value(qs("Env")) == LogLevel::Error);
}

void TstLogger::recent_problems_are_scrubbed() {
    auto& log = Logger::instance();
    LOG_WARN("Probe", qs("Upload failed url=https://x.example.com/a?token=SECRET123 path=C:\\Users\\Bob\\file "
                         "api_key=abcdef123456 to bob@example.com"));
    LOG_INFO("Probe", "Info lines are not problems");
    LOG_ERROR("Probe", "Second problem");
    const QStringList rp = log.recent_problems(2);
    QCOMPARE(rp.size(), 2);
    QVERIFY(rp[0].contains(qs("[WARN] [Probe] Upload failed")));
    for (const char* leaked : {"SECRET123", "\\Users\\Bob", "abcdef123456", "bob@example.com"})
        QVERIFY2(!rp[0].contains(qs(leaked)), leaked);
    QVERIFY(rp[1].contains(qs("[ERROR] [Probe] Second problem")));
    QCOMPARE(log.recent_problems(1).size(), 1);
}

void TstLogger::json_mode() {
    auto& log = Logger::instance();
    log.flush();
    log.set_json_mode(true);
    LOG_INFO("Json", "Hello \"world\"\nline2");
    for (int i = 0; i < 3; ++i)
        LOG_INFO("Json", "Rep");
    log.flush();
    log.set_json_mode(false);
    QList<QJsonObject> objs;
    for (const QString& l : read_raw(current_log())) {
        if (!l.startsWith(QLatin1Char('{')))
            continue;
        QJsonParseError err{};
        const QJsonDocument doc = QJsonDocument::fromJson(l.toUtf8(), &err);
        QVERIFY2(err.error == QJsonParseError::NoError, qPrintable(l));
        objs << doc.object();
    }
    QCOMPARE(objs.size(), 3);
    QCOMPARE(objs[0].value(QStringLiteral("msg")).toString(), qs("Hello \"world\"\nline2"));
    QCOMPARE(objs[0].value(QStringLiteral("level")).toString(), qs("INFO"));
    QCOMPARE(objs[0].value(QStringLiteral("tag")).toString(), qs("Json"));
    QCOMPARE(objs[0].value(QStringLiteral("thread")).toString(), qs("main"));
    QVERIFY(QRegularExpression(QStringLiteral(R"(^\d{4}-\d\d-\d\d \d\d:\d\d:\d\d\.\d{3}[+-]\d\d:\d\d$)"))
                .match(objs[0].value(QStringLiteral("ts")).toString())
                .hasMatch());
    QCOMPARE(objs[1].value(QStringLiteral("msg")).toString(), qs("Rep"));
    QCOMPARE(objs[2].value(QStringLiteral("msg")).toString(), qs("last message repeated"));
    QCOMPARE(objs[2].value(QStringLiteral("repeat")).toInt(), 2);
}

void TstLogger::thread_field() {
    auto& log = Logger::instance();
    log.set_level(LogLevel::Debug);
    LOG_DEBUG("Thr", "from gui");
    std::thread([]() { LOG_DEBUG("Thr", "from worker"); }).join();
    log.set_level(LogLevel::Info);
    std::thread([]() { LOG_INFO("Thr", "worker at info"); }).join();
    const QStringList got = msgs(with_tag(entries(), QStringLiteral("Thr")));
    QCOMPARE(got.size(), 3);
    QCOMPARE(got[0], qs("from gui"));
    QVERIFY2(QRegularExpression(QStringLiteral(R"(^from worker thread=t\d+$)")).match(got[1]).hasMatch(),
             qPrintable(got[1]));
    QCOMPARE(got[2], qs("worker at info"));
}

void TstLogger::flush_and_close_is_idempotent() {
    auto& log = Logger::instance();
    LOG_INFO("Close", "before close");
    for (int i = 0; i < 3; ++i)
        LOG_INFO("Close", "rep");
    log.flush_and_close();
    log.flush_and_close();
    LOG_INFO("Close", "after close"); // dropped: no file open
    LOG_AUDIT("Close", "audit after close goes to stderr, never silently");
    QCOMPARE(msgs(with_tag(parse(read_raw(current_log())), QStringLiteral("Close"))),
             (QStringList{qs("before close"), qs("rep"), qs("last message repeated 2 times")}));
    log.set_file(current_log()); // the next test reopens as well; prove it works here
    LOG_INFO("Close", "reopened");
    QCOMPARE(msgs(with_tag(entries(), QStringLiteral("Close"))), QStringList{qs("reopened")});
}

// ── Stress ───────────────────────────────────────────────────────────────────

void TstLogger::stress_8_threads_100k_lines() {
    auto& log = Logger::instance();
    constexpr int kThreads = 8;
    constexpr int kPerThread = 100000;
    constexpr int kRounds = 8; // a fresh file per round keeps every line within the rotated parts
    constexpr int kPerRound = kPerThread / kRounds;
    const QRegularExpression rx(QStringLiteral(R"(^w=(\d+) seq=(\d+) pad=abcdefgh$)"));
    const QRegularExpression drop_rx(QStringLiteral(R"(^Log lines dropped count=(\d+) reason=queue_full$)"));
    qint64 present = 0;
    qint64 dropped = 0;
    qint64 torn = 0;
    qint64 out_of_order = 0;
    QElapsedTimer timer;
    timer.start();
    qint64 producer_ms = 0;
    for (int r = 0; r < kRounds; ++r) {
        const QString path = dir_for(QStringLiteral("stress%1").arg(r)) + QStringLiteral("/fincept.log");
        log.set_file(path);
        QElapsedTimer pt;
        pt.start();
        std::vector<std::thread> threads;
        for (int t = 0; t < kThreads; ++t) {
            threads.emplace_back([t, r]() {
                for (int i = r * kPerRound; i < (r + 1) * kPerRound; ++i) {
                    if (i % 10 == 0)
                        LOG_WARN("Stress", QStringLiteral("w=%1 seq=%2 pad=abcdefgh").arg(t).arg(i));
                    else
                        LOG_INFO("Stress", QStringLiteral("w=%1 seq=%2 pad=abcdefgh").arg(t).arg(i));
                }
            });
        }
        for (auto& th : threads)
            th.join();
        producer_ms += pt.elapsed();
        LOG_INFO("StressEnd", "round end"); // carries a pending "dropped" marker, if any
        log.flush();
        std::vector<int> last(kThreads, -1);
        for (const Entry& e : parse(read_with_parts(path))) {
            if (!e.parsed) {
                ++torn;
                continue;
            }
            if (e.tag == QLatin1String("Stress")) {
                const QRegularExpressionMatch m = rx.match(e.msg);
                if (!m.hasMatch()) {
                    ++torn;
                    continue;
                }
                const int w = m.captured(1).toInt();
                const int seq = m.captured(2).toInt();
                if (seq <= last[size_t(w)])
                    ++out_of_order;
                last[size_t(w)] = seq;
                ++present;
            } else if (e.tag == QLatin1String("App")) {
                if (const auto m = drop_rx.match(e.msg); m.hasMatch())
                    dropped += m.captured(1).toLongLong();
            }
        }
    }
    const qint64 total_ms = timer.elapsed();
    qInfo("stress: %d threads x %d lines: present=%lld dropped=%lld torn=%lld out_of_order=%lld producers=%lld ms "
          "total=%lld ms",
          kThreads, kPerThread, present, dropped, torn, out_of_order, producer_ms, total_ms);
    QCOMPARE(torn, 0);
    QCOMPARE(out_of_order, 0);
    QCOMPARE(present + dropped, qint64(kThreads) * kPerThread);
    log.set_file(current_log());
}

QTEST_GUILESS_MAIN(TstLogger)
#include "tst_logger.moc"
