#include "core/logging/Logger.h"

#include "core/config/EditionIdentity.h"

#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QReadLocker>
#include <QReadWriteLock>
#include <QRegularExpression>
#include <QThread>
#include <QUrl>
#include <QWaitCondition>
#include <QWriteLocker>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <thread>

namespace fincept {

namespace {

// ── Every size and time limit of the logger, in one place ──────────────────
// fincept.log rotates by size within a session (.1 … .3) and is renamed at every
// start (.prev, .prev.1 … .prev.4 = the five previous sessions). All fincept.log*
// files together are capped (oldest deleted first; the current file never is).
constexpr qint64 kLogRotateBytes = 5LL * 1024 * 1024;
constexpr int kLogRotateKeep = 3;
constexpr int kLogSessionsKeep = 5;
constexpr qint64 kLogTotalCapBytes = 50LL * 1024 * 1024;
// audit.log is never renamed at start and never counted in the cap above:
// it rotates at 5 MB keeping .1 … .10.
constexpr qint64 kAuditRotateBytes = 5LL * 1024 * 1024;
constexpr int kAuditRotateKeep = 10;
// Writer thread: a TRACE..INFO line reaches the OS at most this long after it was
// logged (WARN / ERROR / AUDIT wake the writer at once).
constexpr int kFlushIntervalMs = 250;
constexpr qsizetype kWakeBytes = 8 * 1024;
// Queue bound (only reached when the disk stalls) and what a full queue does.
constexpr qsizetype kQueueMaxBytes = 4 * 1024 * 1024;
constexpr int kQueueFullWaitMs = 200;     // non-GUI WARN/ERROR wait this long for room
constexpr int kAuditQueueMaxLines = 10000; // GUI-thread audit lines waiting for the writer
constexpr int kAuditSyncWaitMs = 1000;     // non-GUI audit: bounded wait for the file
constexpr qint64 kRepeatFlushMs = 5000;    // "last message repeated" at the latest after this
constexpr qint64 kHousekeepingTickMs = 1000;
constexpr qint64 kWriteRetryMs = 1000; // after a failed write, retry this much later (no spinning)
constexpr int kRateLimitMaxKeys = 1024;
constexpr int kTransitionMaxKeys = 1024;
constexpr int kRecentProblemsMax = 200;
constexpr qsizetype kMaxMessageChars = 16 * 1024; // safety net; the policy budget is 500

constexpr int kAuditLevel = 6; // level index of AUDIT lines (not a LogLevel: it has no threshold)
constexpr std::array<const char*, 7> kLoggerLevelNames = {"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL", "AUDIT"};
static_assert(static_cast<int>(LogLevel::Fatal) + 2 == static_cast<int>(kLoggerLevelNames.size()),
              "kLoggerLevelNames must list every LogLevel plus AUDIT");

enum LoggerEnqueueFlag : unsigned {
    kBypassCap = 1u, // never dropped for a full queue
    kNoFold = 2u,    // never folded into "last message repeated"
};

// True while the current thread holds the file mutex (draining, rotating or
// reopening the file). A Qt warning raised by QFile/QDir in there comes straight
// back through main.cpp's message handler into Logger::warn(); this flag makes
// that nested call queue its line instead of re-locking the non-recursive file
// mutex on the same thread (deadlock).
thread_local bool t_logger_in_drain = false;

struct LoggerDrainGuard {
    bool prev;
    LoggerDrainGuard() : prev(t_logger_in_drain) { t_logger_in_drain = true; }
    ~LoggerDrainGuard() { t_logger_in_drain = prev; }
    LoggerDrainGuard(const LoggerDrainGuard&) = delete;
    LoggerDrainGuard& operator=(const LoggerDrainGuard&) = delete;
};

// Set by the atexit hook: from then on nothing here may call into Qt facilities that
// static destruction can already have torn down (time zones, QThread names).
std::atomic<bool> g_logger_at_exit{false};

qint64 logger_steady_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

// Local UTC offset, cached per wall-clock second.
//
// QDateTime::currentDateTime().offsetFromUtc() costs ~7.5 µs and goes through
// Qt's process-wide time-zone lock, so every log line from every thread used to
// serialise on it (measured: 8 threads gained no throughput over 1). The offset
// only changes at a DST transition or a system time-zone change, so it is
// recomputed at most once per second; within a second after such a change a
// line may still carry the previous offset — local time and offset are computed
// together, so the printed instant is correct either way.
//
// Packed into one atomic so a reader never pairs one second's offset with
// another second: (utc_second << 20) | (offset_sec + 2^19).
std::atomic<qint64> g_logger_offset_cache{-1};

int logger_utc_offset_sec(qint64 utc_ms) {
    const qint64 utc_sec = utc_ms >= 0 ? utc_ms / 1000 : (utc_ms - 999) / 1000;
    const qint64 packed = g_logger_offset_cache.load(std::memory_order_acquire);
    if (packed >= 0 && (packed >> 20) == utc_sec)
        return static_cast<int>(packed & 0xFFFFF) - (1 << 19);
    if (g_logger_at_exit.load(std::memory_order_relaxed)) // last known offset, no Qt call
        return packed >= 0 ? static_cast<int>(packed & 0xFFFFF) - (1 << 19) : 0;
    const int offset = QDateTime::fromMSecsSinceEpoch(utc_ms).offsetFromUtc(); // local zone
    if (utc_sec >= 0)
        g_logger_offset_cache.store((utc_sec << 20) | (qint64(offset) + (1 << 19)), std::memory_order_release);
    return offset;
}

// Days since 1970-01-01 → proleptic Gregorian civil date (H. Hinnant's algorithm).
void logger_civil_from_days(qint64 z, int& y, int& m, int& d) {
    z += 719468;
    const qint64 era = (z >= 0 ? z : z - 146096) / 146097;
    const qint64 doe = z - era * 146097;                                      // [0, 146096]
    const qint64 yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365; // [0, 399]
    const qint64 doy = doe - (365 * yoe + yoe / 4 - yoe / 100);               // [0, 365]
    const qint64 mp = (5 * doy + 2) / 153;                                    // [0, 11]
    d = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);                       // [1, 31]
    m = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);                          // [1, 12]
    y = static_cast<int>(yoe + era * 400 + (m <= 2 ? 1 : 0));
}

char* logger_put_digits(char* p, int value, int width) {
    for (int i = width - 1; i >= 0; --i) {
        p[i] = static_cast<char>('0' + value % 10);
        value /= 10;
    }
    return p + width;
}

// Length of the timestamp text logger_put_timestamp() writes.
constexpr qsizetype kLoggerTimestampChars = 29;

// P1.5 + P1.6 — `utc_ms` rendered at the local offset: the same text as
// QDateTime::toString("yyyy-MM-dd HH:mm:ss.zzz") + "+HH:MM", e.g.
// 2026-04-18 03:12:08.123+05:30. Writes 29 chars into `buf`, returns the length.
int logger_put_timestamp(char* buf, qint64 utc_ms) {
    const int offset_sec = logger_utc_offset_sec(utc_ms);
    const qint64 local_ms = utc_ms + qint64(offset_sec) * 1000;
    qint64 days = local_ms / 86400000;
    qint64 ms_of_day = local_ms % 86400000;
    if (ms_of_day < 0) {
        ms_of_day += 86400000;
        --days;
    }
    int y = 0, mo = 0, d = 0;
    logger_civil_from_days(days, y, mo, d);
    const int abs_off = std::abs(offset_sec);

    char* p = buf;
    p = logger_put_digits(p, y, 4);
    *p++ = '-';
    p = logger_put_digits(p, mo, 2);
    *p++ = '-';
    p = logger_put_digits(p, d, 2);
    *p++ = ' ';
    p = logger_put_digits(p, static_cast<int>(ms_of_day / 3600000), 2);
    *p++ = ':';
    p = logger_put_digits(p, static_cast<int>((ms_of_day / 60000) % 60), 2);
    *p++ = ':';
    p = logger_put_digits(p, static_cast<int>((ms_of_day / 1000) % 60), 2);
    *p++ = '.';
    p = logger_put_digits(p, static_cast<int>(ms_of_day % 1000), 3);
    *p++ = offset_sec >= 0 ? '+' : '-';
    p = logger_put_digits(p, abs_off / 3600, 2);
    *p++ = ':';
    p = logger_put_digits(p, (abs_off % 3600) / 60, 2);
    return static_cast<int>(p - buf);
}

// ── Thread label (policy C.8) ───────────────────────────────────────────────
// "main" for the GUI thread, else the QThread objectName (sanitised) plus a
// per-process sequence number, or "t<n>". Computed once per thread into
// trivially destructible thread-local storage.
struct LoggerThreadLabel {
    char text[40];
    int len;
    bool init;
};
thread_local LoggerThreadLabel t_logger_thread{};
std::atomic<int> g_logger_thread_seq{0};

QLatin1StringView logger_thread_label() {
    LoggerThreadLabel& t = t_logger_thread;
    if (!t.init) {
        t.init = true;
        const int seq = g_logger_thread_seq.fetch_add(1, std::memory_order_relaxed) + 1;
        QByteArray label;
        if (QThread::isMainThread()) {
            label = "main";
        } else {
            QByteArray name;
            if (!g_logger_at_exit.load(std::memory_order_relaxed)) {
                if (QThread* th = QThread::currentThread())
                    name = th->objectName().toLatin1();
            }
            for (char c : name) {
                const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                                c == '-' || c == '_' || c == '.';
                label.append(ok ? c : '_');
            }
            label.truncate(24);
            label = (label.isEmpty() ? QByteArray("t") : label + '-') + QByteArray::number(seq);
        }
        t.len = static_cast<int>(std::min<qsizetype>(label.size(), qsizetype(sizeof(t.text))));
        std::copy_n(label.constData(), t.len, t.text);
    }
    return QLatin1StringView(t.text, t.len);
}

// ── Line formatting ─────────────────────────────────────────────────────────

// UTF-8 append with an ASCII fast path (no temporary QByteArray for plain text).
void logger_append_utf8(QByteArray& out, QStringView s) {
    const qsizetype n = s.size();
    if (n == 0)
        return;
    const char16_t* src = s.utf16();
    const qsizetype start = out.size();
    out.resize(start + n);
    char* dst = out.data() + start;
    qsizetype i = 0;
    for (; i < n; ++i) {
        const char16_t c = src[i];
        if (c >= 0x80)
            break;
        dst[i] = static_cast<char>(c);
    }
    if (i == n)
        return;
    out.truncate(start + i);
    out.append(s.sliced(i).toUtf8());
}

// The message, cut at kMaxMessageChars (never inside a surrogate pair).
QStringView logger_capped(QStringView msg, bool* cut) {
    *cut = msg.size() > kMaxMessageChars;
    if (!*cut)
        return msg;
    qsizetype n = kMaxMessageChars;
    if (msg[n - 1].isHighSurrogate())
        --n;
    return msg.first(n);
}

// Text-mode message: one physical line — CR LF / LF become the two characters \n,
// a lone CR becomes \r (policy B.2.5) — then the truncation note if it was cut.
void logger_append_message(QByteArray& out, QStringView msg) {
    bool cut = false;
    const QStringView body = logger_capped(msg, &cut);
    bool breaks = false;
    for (const QChar c : body) {
        if (c == u'\n' || c == u'\r') {
            breaks = true;
            break;
        }
    }
    if (!breaks) {
        logger_append_utf8(out, body);
    } else {
        QString one_line;
        one_line.reserve(body.size() + 16);
        for (qsizetype i = 0; i < body.size(); ++i) {
            const QChar c = body[i];
            if (c == u'\r') {
                if (i + 1 < body.size() && body[i + 1] == u'\n')
                    continue; // CR LF → \n (written for the LF)
                one_line += QLatin1StringView("\\r");
            } else if (c == u'\n') {
                one_line += QLatin1StringView("\\n");
            } else {
                one_line += c;
            }
        }
        logger_append_utf8(out, one_line);
    }
    if (cut) {
        out.append("\xE2\x80\xA6 (truncated, ");
        out.append(QByteArray::number(msg.size()));
        out.append(" chars)");
    }
}

// [2026-11-17 02:14:05.123+05:30] [INFO] [Tag] message[ thread=<label>]\n
QByteArray logger_text_line(int level, QStringView tag, QStringView msg, qint64 utc_ms, QLatin1StringView thread) {
    QByteArray out;
    out.reserve(48 + tag.size() + msg.size() + (msg.size() >> 3) + thread.size() + 8);
    char ts[40];
    const int ts_len = logger_put_timestamp(ts, utc_ms);
    out.append('[');
    out.append(ts, ts_len);
    out.append("] [", 3);
    out.append(kLoggerLevelNames[static_cast<std::size_t>(level)]);
    out.append("] [", 3);
    logger_append_utf8(out, tag);
    out.append("] ", 2);
    logger_append_message(out, msg);
    if (!thread.isEmpty()) {
        out.append(" thread=", 8);
        out.append(thread.data(), thread.size());
    }
    out.append('\n');
    return out;
}

// P3.17 — one JSON object per line: ts, level, tag, msg, thread[, repeat].
QByteArray logger_json_line(int level, QStringView tag, QStringView msg, qint64 utc_ms, QLatin1StringView thread,
                            quint64 repeat) {
    char ts[40];
    const int ts_len = logger_put_timestamp(ts, utc_ms);
    bool cut = false;
    QString text = logger_capped(msg, &cut).toString();
    if (cut)
        text += QStringLiteral("… (truncated, %1 chars)").arg(msg.size());
    QJsonObject obj{
        {"ts", QString::fromLatin1(ts, ts_len)},
        {"level", QString::fromLatin1(kLoggerLevelNames[static_cast<std::size_t>(level)])},
        {"tag", tag.toString()},
        {"msg", text},
    };
    if (!thread.isEmpty())
        obj.insert(QStringLiteral("thread"), QString(thread));
    if (repeat > 0)
        obj.insert(QStringLiteral("repeat"), static_cast<qint64>(repeat));
    QByteArray out = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    out.append('\n');
    return out;
}

// ── Field quoting (policy B.3) ──────────────────────────────────────────────
bool logger_needs_quotes(QStringView v) {
    if (v.isEmpty())
        return true;
    for (const QChar c : v) {
        const char16_t u = c.unicode();
        if (u <= 0x20 || u == u'"' || u == u'=' || u == 0x7f || c.isSpace())
            return true;
    }
    return false;
}

void logger_append_quoted(QString& out, QStringView value, int max_chars) {
    bool cut = false;
    if (max_chars > 0 && value.size() > max_chars) {
        qsizetype n = max_chars;
        if (value[n - 1].isHighSurrogate())
            --n;
        value = value.first(n);
        cut = true;
    }
    if (!logger_needs_quotes(value)) {
        out += value;
        if (cut)
            out += QChar(0x2026);
        return;
    }
    static constexpr char kHex[] = "0123456789abcdef";
    out += QLatin1Char('"');
    for (const QChar c : value) {
        const char16_t u = c.unicode();
        switch (u) {
            case u'\\':
                out += QLatin1StringView("\\\\");
                break;
            case u'"':
                out += QLatin1StringView("\\\"");
                break;
            case u'\n':
                out += QLatin1StringView("\\n");
                break;
            case u'\r':
                out += QLatin1StringView("\\r");
                break;
            case u'\t':
                out += QLatin1StringView("\\t");
                break;
            default:
                if (u < 0x20 || u == 0x7f) {
                    out += QLatin1StringView("\\x");
                    out += QLatin1Char(kHex[(u >> 4) & 0xF]);
                    out += QLatin1Char(kHex[u & 0xF]);
                } else {
                    out += c;
                }
        }
    }
    if (cut)
        out += QChar(0x2026);
    out += QLatin1Char('"');
}

QString logger_logf(const QString& sentence, const LogField* begin, const LogField* end) {
    qsizetype estimate = sentence.size();
    for (const LogField* f = begin; f != end; ++f)
        estimate += 4 + (f->key ? qsizetype(std::strlen(f->key)) : 0) + std::min<qsizetype>(f->value.size(), 210);
    QString out;
    out.reserve(estimate);
    out += sentence;
    for (const LogField* f = begin; f != end; ++f) {
        if (!f->key || !*f->key)
            continue;
        if (!out.isEmpty())
            out += QLatin1Char(' ');
        out += QLatin1StringView(f->key);
        out += QLatin1Char('=');
        logger_append_quoted(out, f->value, kLogFieldMaxChars);
    }
    return out;
}

// ── Redaction roots (never destroyed: usable during static destruction) ────
struct LoggerRedactRoots {
    QReadWriteLock lock;
    QString data_root;
    bool data_explicit = false;
    QString app_dir;
    bool app_explicit = false;
    QString home;
    bool home_known = false;
    QStringList first_party{QStringLiteral("api.fincept.in")};
};

LoggerRedactRoots& logger_roots() {
    static LoggerRedactRoots* const roots = new LoggerRedactRoots();
    return *roots;
}

bool logger_is_first_party(const QString& host) {
    LoggerRedactRoots& r = logger_roots();
    QReadLocker lock(&r.lock);
    for (const QString& h : r.first_party) {
        if (host.compare(h, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

log_detail::PathRoots logger_current_roots() {
    LoggerRedactRoots& r = logger_roots();
    log_detail::PathRoots roots;
    bool need_home = false;
    bool need_app = false;
    {
        QReadLocker lock(&r.lock);
        roots.data_root = r.data_root;
        roots.app_dir = r.app_dir;
        roots.home = r.home;
        need_home = !r.home_known;
        need_app = r.app_dir.isEmpty() && !r.app_explicit;
    }
    const bool at_exit = g_logger_at_exit.load(std::memory_order_relaxed);
    if ((need_home || need_app) && !at_exit) {
        const QString home = need_home ? QDir::homePath() : QString();
        const QString app =
            (need_app && QCoreApplication::instance()) ? QCoreApplication::applicationDirPath() : QString();
        QWriteLocker lock(&r.lock);
        if (need_home && !r.home_known) {
            r.home = home;
            r.home_known = true;
        }
        if (!app.isEmpty() && r.app_dir.isEmpty())
            r.app_dir = app;
        roots.home = r.home;
        roots.app_dir = r.app_dir;
        roots.data_root = r.data_root;
    }
#ifdef Q_OS_WIN
    roots.case_insensitive = true;
#endif
    return roots;
}

// ── Session log retention ───────────────────────────────────────────────────
QString logger_prev_name(const QString& target, int i) {
    return i == 0 ? target + QStringLiteral(".prev") : target + QStringLiteral(".prev.") + QString::number(i);
}

// Delete the oldest fincept.log* files until all of them fit kLogTotalCapBytes.
void logger_enforce_total_cap(const QString& target) {
    QStringList oldest_first;
    for (int i = kLogSessionsKeep - 1; i >= 0; --i)
        oldest_first << logger_prev_name(target, i);
    for (int i = kLogRotateKeep; i >= 1; --i)
        oldest_first << target + QLatin1Char('.') + QString::number(i);
    qint64 total = QFileInfo(target).size();
    for (const QString& f : oldest_first)
        total += QFileInfo(f).size();
    for (const QString& f : oldest_first) {
        if (total <= kLogTotalCapBytes)
            break;
        const qint64 size = QFileInfo(f).size();
        if (size > 0 && QFile::remove(f))
            total -= size;
    }
}

// Start of a session: fincept.log → .prev → .prev.1 … → .prev.4 (five previous sessions).
void logger_shift_session_logs(const QString& target) {
    QFile::remove(logger_prev_name(target, kLogSessionsKeep - 1));
    for (int i = kLogSessionsKeep - 2; i >= 0; --i) {
        const QString src = logger_prev_name(target, i);
        if (QFile::exists(src))
            QFile::rename(src, logger_prev_name(target, i + 1));
    }
    if (QFile::exists(target))
        QFile::rename(target, logger_prev_name(target, 0));
    logger_enforce_total_cap(target);
}

struct LoggerRateEntry {
    QString tag;
    QString key;
    qint64 window_start = 0; // steady ms of the last line written for the key
    qint64 last_seen = 0;
    int interval_ms = 0;
    int suppressed = 0;
    int level = static_cast<int>(LogLevel::Info);
};

struct LoggerRateSummary {
    int level;
    QString tag;
    QString key;
    int count;
    int interval_ms;
};

struct LoggerTransitionEntry {
    int state = 0;
    int observations = 0;
    qint64 since = 0;
    qint64 last_seen = 0;
};

QString logger_table_key(const QString& tag, const QString& key) {
    QString k;
    k.reserve(tag.size() + 1 + key.size());
    k += tag;
    k += QChar(0x1f);
    k += key;
    return k;
}

} // namespace

// ── Field and redaction helpers (LogFields.h / LogRedact.h) ────────────────

LogField::LogField(const char* k, const QUrl& v) : key(k), value(log_url(v)) {}

QString log_quote(QStringView value, int max_chars) {
    QString out;
    logger_append_quoted(out, value, max_chars);
    return out;
}

QString logf(const QString& sentence, std::initializer_list<LogField> fields) {
    return logger_logf(sentence, fields.begin(), fields.end());
}

QString logf(const QString& sentence, const QList<LogField>& fields) {
    return logger_logf(sentence, fields.constData(), fields.constData() + fields.size());
}

QString log_url(const QUrl& url) {
    if (url.isEmpty())
        return {};
    if (url.isLocalFile())
        return QStringLiteral("file://") + log_path(url.toLocalFile());
    const QString host = url.host();
    if (host.isEmpty()) {
        // A relative Fincept API path ("/user/login?x=1"); anything else without a
        // host is not a URL we can reason about — never echo it.
        const QString path = url.path(QUrl::FullyEncoded);
        if (!url.scheme().isEmpty() || !path.startsWith(QLatin1Char('/')))
            return QStringLiteral("<invalid url>");
        return url.hasQuery() ? path + QStringLiteral("?<redacted>") : path;
    }
    QString out = url.adjusted(QUrl::RemoveUserInfo | QUrl::RemovePath | QUrl::RemoveQuery | QUrl::RemoveFragment)
                      .toString(QUrl::FullyEncoded);
    const QString path = url.path(QUrl::FullyEncoded);
    if (logger_is_first_party(host)) {
        out += path;
        if (url.hasQuery())
            out += QStringLiteral("?<redacted>");
    } else if ((!path.isEmpty() && path != QLatin1String("/")) || url.hasQuery()) {
        // Third-party: the path itself can be the credential (webhooks, /bot<token>/).
        out += QStringLiteral("/<redacted>");
    }
    return out;
}

QString log_url(const QString& url) {
    const QString t = url.trimmed();
    if (t.isEmpty())
        return {};
    if (t.startsWith(QLatin1Char('/')) && !t.startsWith(QLatin1String("//"))) {
        qsizetype end = t.indexOf(QLatin1Char('#'));
        if (end < 0)
            end = t.size();
        const qsizetype q = t.indexOf(QLatin1Char('?'));
        if (q >= 0 && q < end)
            return t.left(q) + QStringLiteral("?<redacted>");
        return t.left(end);
    }
    const QUrl parsed(t, QUrl::TolerantMode);
    if (!parsed.isValid() || parsed.scheme().isEmpty())
        return QStringLiteral("<invalid url>");
    return log_url(parsed);
}

void log_url_add_first_party_host(const QString& host) {
    QString h = host.trimmed();
    if (h.contains(QLatin1String("://")))
        h = QUrl(h).host();
    if (h.isEmpty())
        return;
    LoggerRedactRoots& r = logger_roots();
    QWriteLocker lock(&r.lock);
    for (const QString& known : r.first_party) {
        if (known.compare(h, Qt::CaseInsensitive) == 0)
            return;
    }
    r.first_party.append(h);
}

QString log_path(const QString& path) {
    if (path.isEmpty())
        return path;
    return log_detail::log_path_with(path, logger_current_roots());
}

void set_log_path_roots(const QString& data_root, const QString& app_dir) {
    LoggerRedactRoots& r = logger_roots();
    QWriteLocker lock(&r.lock);
    if (!data_root.isEmpty()) {
        r.data_root = data_root;
        r.data_explicit = true;
    }
    if (!app_dir.isEmpty()) {
        r.app_dir = app_dir;
        r.app_explicit = true;
    }
}

QString log_mask(const QString& id, int keep_last) {
    if (id.isEmpty())
        return {};
    const QChar ellipsis(0x2026);
    if (keep_last <= 0 || id.size() <= keep_last)
        return QString(ellipsis);
    return ellipsis + id.right(keep_last);
}

QString log_scrub(const QString& text) {
    if (text.isEmpty())
        return text;
    struct Patterns {
        QRegularExpression url{QStringLiteral(R"(\b[A-Za-z][A-Za-z0-9+.\-]*://[^\s"'<>]+)")};
        QRegularExpression bearer{QStringLiteral(R"(\b(Bearer|Basic|Token)\s+[A-Za-z0-9._~+/=\-]{6,})"),
                                  QRegularExpression::CaseInsensitiveOption};
        QRegularExpression jwt{QStringLiteral(R"(\beyJ[A-Za-z0-9_\-]{5,}\.[A-Za-z0-9_\-]{5,}(?:\.[A-Za-z0-9_\-]+)?)")};
        // Any key ENDING in a secret word (broker_api_key=, x-auth-token:), not only the bare word.
        QRegularExpression secret{
            QStringLiteral(R"(\b([A-Za-z0-9_\-]*(?:api[_\-]?key|apikey|token|secret|password|passwd|pwd|passphrase|)"
                           R"(pin|otp|totp|signature|private[_\-]?key|seed|mnemonic|cookie|authorization)\s*[=:]\s*))"
                           R"((?!<redacted>|(?:bearer|basic|token) <redacted>)("[^"]*"|'[^']*'|[^\s&,;"']+))"),
            QRegularExpression::CaseInsensitiveOption};
        QRegularExpression email{QStringLiteral(R"([A-Za-z0-9._%+\-]+@[A-Za-z0-9\-]+(?:\.[A-Za-z0-9\-]+)*\.[A-Za-z]{2,})")};
        QRegularExpression win_home{QStringLiteral(R"(\b[A-Za-z]:[\\/]+(?:Users|Documents and Settings)[\\/]+[^\\/\s"':]+)"),
                                    QRegularExpression::CaseInsensitiveOption};
        QRegularExpression nix_home{QStringLiteral(R"((?<![\w.~])/(?:home|Users)/[^/\s"':]+)")};
        QRegularExpression token{QStringLiteral(R"((?<![A-Za-z0-9_\-])[A-Za-z0-9_\-]{32,}(?![A-Za-z0-9_\-]))")};
    };
    static const Patterns* const rx = new Patterns(); // never destroyed

    QString out;
    {
        // URLs first, so a query or credential inside one is handled by log_url().
        qsizetype last = 0;
        auto it = rx->url.globalMatch(text);
        while (it.hasNext()) {
            const QRegularExpressionMatch m = it.next();
            out += QStringView(text).sliced(last, m.capturedStart() - last);
            out += log_url(m.captured());
            last = m.capturedEnd();
        }
        out += QStringView(text).sliced(last);
    }
    out.replace(rx->jwt, QStringLiteral("<jwt>")); // before the auth schemes: "Bearer eyJ…" → "Bearer <jwt>"
    out.replace(rx->bearer, QStringLiteral("\\1 <redacted>"));
    out.replace(rx->secret, QStringLiteral("\\1<redacted>"));
    out.replace(rx->email, QStringLiteral("<email>"));
    const log_detail::PathRoots roots = logger_current_roots();
    if (roots.home.size() > 1) {
        const Qt::CaseSensitivity cs = roots.case_insensitive ? Qt::CaseInsensitive : Qt::CaseSensitive;
        QString fwd = roots.home;
        fwd.replace(QLatin1Char('\\'), QLatin1Char('/'));
        QString back = fwd;
        back.replace(QLatin1Char('/'), QLatin1Char('\\'));
        out.replace(fwd, QStringLiteral("~"), cs);
        out.replace(back, QStringLiteral("~"), cs);
    }
    out.replace(rx->win_home, QStringLiteral("~"));
    out.replace(rx->nix_home, QStringLiteral("~"));
    {
        QString res;
        qsizetype last = 0;
        auto it = rx->token.globalMatch(out);
        while (it.hasNext()) {
            const QRegularExpressionMatch m = it.next();
            const QStringView tok = m.capturedView();
            bool digit = false;
            bool alpha = false;
            for (const QChar c : tok) {
                digit = digit || c.isDigit();
                alpha = alpha || c.isLetter();
            }
            if (!digit || !alpha)
                continue;
            res += QStringView(out).sliced(last, m.capturedStart() - last);
            res += QStringLiteral("<token>");
            last = m.capturedEnd();
        }
        if (last > 0) {
            res += QStringView(out).sliced(last);
            out = res;
        }
    }
    return out;
}

namespace log_detail {

QString log_path_with(const QString& path, const PathRoots& roots) {
    if (path.isEmpty())
        return path;
    QString norm = path;
    norm.replace(QLatin1Char('\\'), QLatin1Char('/'));
    const Qt::CaseSensitivity cs = roots.case_insensitive ? Qt::CaseInsensitive : Qt::CaseSensitive;

    struct Candidate {
        QString root;
        const char* token;
    };
    std::array<Candidate, 3> candidates{{{roots.data_root, "<data>"}, {roots.app_dir, "<app>"}, {roots.home, "~"}}};
    for (Candidate& c : candidates) {
        c.root.replace(QLatin1Char('\\'), QLatin1Char('/'));
        while (c.root.size() > 1 && c.root.endsWith(QLatin1Char('/')))
            c.root.chop(1);
    }
    // Longest root first: the data root usually lies inside the home directory.
    std::stable_sort(candidates.begin(), candidates.end(),
                     [](const Candidate& a, const Candidate& b) { return a.root.size() > b.root.size(); });
    for (const Candidate& c : candidates) {
        if (c.root.size() < 2)
            continue; // "" or "/" would match everything
        if (norm.startsWith(c.root, cs) && (norm.size() == c.root.size() || norm.at(c.root.size()) == u'/')) {
            QString out = QLatin1StringView(c.token);
            out += QStringView(path).sliced(c.root.size());
            return out;
        }
    }
    // Some user's home directory that is not this process's.
    static const QRegularExpression* const other_home = new QRegularExpression(
        QStringLiteral(R"(^(?:[A-Za-z]:/+(?:Users|Documents and Settings)/+[^/]+|/home/[^/]+|/Users/[^/]+|/root(?=/|$)))"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch m = other_home->match(norm);
    if (m.hasMatch()) {
        QString out = QStringLiteral("~");
        out += QStringView(path).sliced(m.capturedLength());
        return out;
    }
    return path;
}

void set_implied_data_root(const QString& data_root) {
    LoggerRedactRoots& r = logger_roots();
    QWriteLocker lock(&r.lock);
    if (!r.data_explicit)
        r.data_root = data_root;
}

} // namespace log_detail

// ── Shared state ────────────────────────────────────────────────────────────
//
// Lock order: file_mutex -> queue_mutex, never the reverse. tag_lock, rl_mutex
// and tr_mutex are each taken alone (never while holding another logger lock).
struct Logger::State {
    Logger* owner = nullptr;

    // Levels — read-mostly, so a read/write lock (level checks never queue behind disk writes).
    mutable QReadWriteLock tag_lock;
    QHash<QString, LogLevel> tag_levels; // effective: settings_tags overlaid by override_tags
    QHash<QString, LogLevel> settings_tags;
    LogLevel settings_global = LogLevel::Info;
    std::optional<LogLevel> override_global;
    QHash<QString, LogLevel> override_tags;
    QStringList override_invalid;
    QString override_source;
    bool override_announced = true;

    // Producer side: formatted lines waiting for the writer.
    QMutex queue_mutex;
    QWaitCondition queue_cv; // the writer waits on it
    QWaitCondition space_cv; // non-GUI WARN/ERROR wait on it while the queue is full
    QByteArray pending;
    qsizetype pending_lines = 0;
    QByteArray audit_pending;
    int audit_pending_lines = 0;
    bool drain_requested = false; // writer: skip the rest of the batching window
    bool stop_requested = false;  // writer: exit
    bool paused = false;          // tests: hold the writer
    int space_waiters = 0;
    quint64 dropped_unreported = 0;
    quint64 audit_lost_unreported = 0;
    // Repeat suppression: the last line accepted into the queue.
    // The last line accepted, as level + length + 64-bit hash of its text without the
    // timestamp: scalars only, so the critical section touches no string data (a shared
    // string here cost 8 producer threads ~40 % of their throughput). last_tag is taken
    // when a repeat starts — the summary line needs it.
    int last_level = -1;
    qsizetype last_len = 0;
    size_t last_hash = 0;
    QString last_tag;
    quint64 repeat_count = 0;
    qint64 repeat_first_steady = 0;
    qint64 repeat_last_wall = 0;
    // Recent WARN+ lines for recent_problems().
    QList<QByteArray> recent;
    int recent_next = 0;
    quint64 st_dropped = 0;
    quint64 st_repeats = 0;
    std::atomic<quint64> st_lost{0};
    std::atomic<quint64> st_audit_lost{0};
    std::atomic<quint64> st_rate_limited{0};

    // File side.
    QMutex file_mutex;
    QFile log_file;
    qint64 bytes_written = 0;
    qint64 rotate_at = kLogRotateBytes;
    QString audit_path;
    bool audit_path_explicit = false;
    bool audit_dir_ready = false;
    bool write_failing = false;
    qint64 retry_at = 0;
    quint64 lost_unreported = 0;
    bool last_write_partial = false;
    bool audit_failing = false;
    quint64 audit_lost_write = 0;

    std::thread writer;
    // false => no writer thread (not started yet, stopping, or failed to start):
    // every line is drained synchronously by the thread that logs it.
    std::atomic<bool> writer_running{false};
    // A file is open; lines may be queued. Lines logged while false are dropped,
    // exactly as they were before the file was opened / after it was closed.
    std::atomic<bool> accepting{false};
    std::atomic<bool> degraded{false}; // stderr has been told about the current write failure

    QMutex rl_mutex;
    QHash<QString, LoggerRateEntry> rl;
    std::atomic<int> rl_pending{0}; // entries holding a suppressed count

    QMutex tr_mutex;
    QHash<QString, LoggerTransitionEntry> tr;

    QByteArray format(int level, const QString& tag, const QString& msg, qint64 wall) const {
        return owner->json_mode_.load(std::memory_order_relaxed) ? logger_json_line(level, tag, msg, wall, {}, 0)
                                                                 : logger_text_line(level, tag, msg, wall, {});
    }
    void append_repeat_summary_locked();         // queue_mutex held
    void append_dropped_marker_locked(qint64 wall); // queue_mutex held
    QByteArray remember_problem_locked(const QByteArray& line); // returns the line it displaced
    bool drain_locked(bool from_writer); // file_mutex held; false = writer in write-failure back-off
    void write_main_locked(QByteArray& batch, qsizetype lines, qint64 wall);
    void fail_main_locked(qsizetype lines, qint64 written, const QString& error);
    void write_audit_locked(QByteArray& batch, int lines, qint64 wall);
    void fail_audit_locked(int lines, const QString& error, qint64 wall);
    void rotate_if_needed_locked(qint64 wall);
    void rotate_audit_if_needed_locked();
    void start_writer();
    void stop_writer();
    void writer_loop();
};

void Logger::State::append_repeat_summary_locked() {
    if (repeat_count == 0 || last_level < 0)
        return;
    const bool json = owner->json_mode_.load(std::memory_order_relaxed);
    const QByteArray line =
        json ? logger_json_line(last_level, last_tag, u"last message repeated", repeat_last_wall, {}, repeat_count)
             : logger_text_line(last_level, last_tag,
                                QStringLiteral("last message repeated %1 times").arg(repeat_count),
                                repeat_last_wall, {});
    pending.append(line);
    ++pending_lines;
    repeat_count = 0;
}

void Logger::State::append_dropped_marker_locked(qint64 wall) {
    if (dropped_unreported == 0)
        return;
    pending.append(format(static_cast<int>(LogLevel::Warn), QStringLiteral("App"),
                          logf(QStringLiteral("Log lines dropped"),
                               {{"count", dropped_unreported}, {"reason", "queue_full"}}),
                          wall));
    ++pending_lines;
    dropped_unreported = 0;
}

QByteArray Logger::State::remember_problem_locked(const QByteArray& line) {
    if (recent.size() < kRecentProblemsMax) {
        recent.append(line);
        recent_next = static_cast<int>(recent.size()) % kRecentProblemsMax;
        return {};
    }
    QByteArray displaced = std::exchange(recent[recent_next], line);
    recent_next = (recent_next + 1) % kRecentProblemsMax;
    return displaced;
}

bool Logger::State::drain_locked(bool from_writer) {
    if (from_writer && write_failing && logger_steady_ms() < retry_at)
        return false;
    QByteArray batch;
    QByteArray abatch;
    qsizetype lines = 0;
    int alines = 0;
    quint64 audit_lost = 0;
    {
        QMutexLocker q(&queue_mutex);
        batch.swap(pending);
        lines = std::exchange(pending_lines, 0);
        abatch.swap(audit_pending);
        alines = std::exchange(audit_pending_lines, 0);
        audit_lost = std::exchange(audit_lost_unreported, 0);
        drain_requested = false;
        if (space_waiters > 0)
            space_cv.wakeAll();
    }
    if (batch.isEmpty() && abatch.isEmpty() && audit_lost == 0)
        return true;

    LoggerDrainGuard guard;
    const qint64 wall = QDateTime::currentMSecsSinceEpoch();
    if (audit_lost > 0) {
        // Never silent: audit.log, fincept.log and stderr all say so.
        const QString msg = logf(QStringLiteral("Audit lines lost"), {{"count", audit_lost}, {"reason", "queue_full"}});
        abatch.prepend(logger_text_line(kAuditLevel, u"App", msg, wall, {}));
        ++alines;
        batch.append(format(static_cast<int>(LogLevel::Error), QStringLiteral("App"), msg, wall));
        ++lines;
        fprintf(stderr, "[Logger] %llu audit line(s) lost: the audit queue was full\n",
                static_cast<unsigned long long>(audit_lost));
    }
    write_main_locked(batch, lines, wall);
    if (!abatch.isEmpty())
        write_audit_locked(abatch, alines, wall);
    return true;
}

void Logger::State::write_main_locked(QByteArray& batch, qsizetype lines, qint64 wall) {
    if (batch.isEmpty())
        return;
    if (!log_file.isOpen()) {
        // Reopen after a failed rotation reopen; nothing to do once the file was closed on purpose.
        if (log_file.fileName().isEmpty() || !accepting.load(std::memory_order_acquire) ||
            !log_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Unbuffered)) {
            fail_main_locked(lines, 0, log_file.errorString());
            return;
        }
        bytes_written = log_file.size();
    }
    if (write_failing) {
        QByteArray head;
        if (last_write_partial)
            head.append('\n'); // end the torn line
        head.append(format(static_cast<int>(LogLevel::Warn), QStringLiteral("App"),
                           logf(QStringLiteral("Logging resumed"), {{"lost_lines", lost_unreported}}), wall));
        batch.prepend(head);
    }
    const qint64 n = log_file.write(batch);
    if (n == batch.size()) {
        if (write_failing) {
            write_failing = false;
            lost_unreported = 0;
            last_write_partial = false;
            degraded.store(false, std::memory_order_relaxed);
        }
        bytes_written += n;
        rotate_if_needed_locked(wall);
    } else {
        fail_main_locked(lines, n, log_file.errorString());
    }
}

void Logger::State::fail_main_locked(qsizetype lines, qint64 written, const QString& error) {
    st_lost.fetch_add(static_cast<quint64>(lines), std::memory_order_relaxed);
    lost_unreported += static_cast<quint64>(lines);
    if (written > 0)
        last_write_partial = true;
    write_failing = true;
    retry_at = logger_steady_ms() + kWriteRetryMs;
    // Once per failure episode, via stderr directly (qWarning would re-enter the
    // logger through main.cpp's Qt message handler).
    if (!degraded.exchange(true))
        fprintf(stderr, "[Logger] write failed, %lld line(s) lost so far, retrying every %lld ms: %s\n",
                static_cast<long long>(lost_unreported), static_cast<long long>(kWriteRetryMs),
                qUtf8Printable(error));
}

void Logger::State::rotate_if_needed_locked(qint64 wall) {
    // P1.8 — size-based rotation, checked after every write. file_mutex held.
    if (bytes_written < rotate_at || !log_file.isOpen())
        return;
    const QString base = log_file.fileName();
    log_file.close();

    const auto part = [&base](int i) { return base + QLatin1Char('.') + QString::number(i); };
    QString error;
    if (QFile::exists(part(kLogRotateKeep))) {
        QFile oldest(part(kLogRotateKeep));
        if (!oldest.remove())
            error = oldest.errorString();
    }
    for (int i = kLogRotateKeep - 1; i >= 1; --i) {
        if (!QFile::exists(part(i)))
            continue;
        QFile f(part(i));
        if (!f.rename(part(i + 1)) && error.isEmpty())
            error = f.errorString();
    }
    bool moved = true;
    {
        QFile current(base);
        if (current.exists() && !current.rename(part(1))) {
            moved = false;
            if (error.isEmpty())
                error = current.errorString();
        }
    }

    log_file.setFileName(base);
    if (!log_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Unbuffered)) {
        // stderr, not qWarning(): file_mutex is held. The next write retries the open.
        fprintf(stderr, "[Logger] rotate: reopen failed for %s error: %s\n", qUtf8Printable(log_path(base)),
                qUtf8Printable(log_file.errorString()));
        return;
    }
    bytes_written = log_file.size();
    if (moved) {
        rotate_at = kLogRotateBytes;
        return;
    }
    // A rename failed (file locked on Windows): keep writing to the same file and try
    // again 5 MB later, instead of resetting the counter and never rotating.
    rotate_at = bytes_written + kLogRotateBytes;
    const QByteArray line =
        format(static_cast<int>(LogLevel::Warn), QStringLiteral("App"),
               logf(QStringLiteral("Log rotation failed"), {{"path", log_path(base)}, {"error", error}}), wall);
    if (log_file.write(line) == line.size())
        bytes_written += line.size();
}

void Logger::State::rotate_audit_if_needed_locked() {
    const QFileInfo info(audit_path);
    if (!info.exists() || info.size() < kAuditRotateBytes)
        return;
    const auto part = [this](int i) { return audit_path + QLatin1Char('.') + QString::number(i); };
    QFile::remove(part(kAuditRotateKeep));
    for (int i = kAuditRotateKeep - 1; i >= 1; --i) {
        if (QFile::exists(part(i)))
            QFile::rename(part(i), part(i + 1));
    }
    QFile::rename(audit_path, part(1)); // on failure: keep appending, retried at the next write
}

void Logger::State::write_audit_locked(QByteArray& batch, int lines, qint64 wall) {
    if (audit_path.isEmpty()) {
        fail_audit_locked(lines, QStringLiteral("no audit file configured"), wall);
        return;
    }
    if (!audit_dir_ready) {
        QDir().mkpath(QFileInfo(audit_path).absolutePath());
        audit_dir_ready = true;
    }
    rotate_audit_if_needed_locked();
    // Opened per batch: audit volume is tiny, and a handle that is never held open
    // cannot end up writing into a file someone else deleted or renamed.
    QFile f(audit_path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Unbuffered)) {
        fail_audit_locked(lines, f.errorString(), wall);
        return;
    }
    if (audit_failing)
        batch.prepend(logger_text_line(kAuditLevel, u"App",
                                       logf(QStringLiteral("Audit log resumed"), {{"lost_lines", audit_lost_write}}),
                                       wall, {}));
    const qint64 n = f.write(batch);
    const QString error = f.errorString();
    f.close();
    if (n != batch.size()) {
        fail_audit_locked(lines, error, wall);
        return;
    }
    audit_failing = false;
    audit_lost_write = 0;
}

void Logger::State::fail_audit_locked(int lines, const QString& error, qint64 wall) {
    st_audit_lost.fetch_add(static_cast<quint64>(lines), std::memory_order_relaxed);
    audit_lost_write += static_cast<quint64>(lines);
    if (audit_failing)
        return;
    audit_failing = true;
    fprintf(stderr, "[Logger] audit log unavailable (%s): %s\n", qUtf8Printable(log_path(audit_path)),
            qUtf8Printable(error));
    // One ERROR in fincept.log (the audit lines themselves are there too, as AUDIT).
    if (log_file.isOpen()) {
        const QByteArray line = format(
            static_cast<int>(LogLevel::Error), QStringLiteral("App"),
            logf(QStringLiteral("Audit log unavailable"), {{"path", log_path(audit_path)}, {"error", error}}), wall);
        if (log_file.write(line) == line.size())
            bytes_written += line.size();
    }
}

void Logger::State::start_writer() {
    QMutexLocker q(&queue_mutex);
    if (writer.joinable())
        return;
    stop_requested = false;
    try {
        writer = std::thread([this]() { writer_loop(); });
        writer_running.store(true, std::memory_order_release);
    } catch (const std::exception& e) {
        // Logging stays fully functional, just synchronous (the pre-async behaviour).
        writer_running.store(false, std::memory_order_release);
        fprintf(stderr, "[Logger] could not start the writer thread (%s) — logging synchronously\n", e.what());
    }
}

void Logger::State::stop_writer() {
    std::thread t;
    {
        QMutexLocker q(&queue_mutex);
        if (!writer.joinable())
            return;
        if (writer.get_id() == std::this_thread::get_id())
            return; // never join ourselves
        // From here on every new line drains synchronously; nothing waits on the writer.
        writer_running.store(false, std::memory_order_release);
        stop_requested = true;
        queue_cv.wakeAll();
        space_cv.wakeAll();
        t = std::move(writer);
    }
    t.join();
}

void Logger::State::writer_loop() {
    QMutexLocker q(&queue_mutex);
    qint64 last_sweep = 0;
    for (;;) {
        // Sleep until there is something to write or a housekeeping tick is due.
        for (;;) {
            if (stop_requested)
                return; // the remaining queue is drained by flush_and_close() after join()
            const qint64 now = logger_steady_ms();
            const bool lines = !paused && (!pending.isEmpty() || !audit_pending.isEmpty() || audit_lost_unreported);
            const bool repeat_due = repeat_count > 0 && now - repeat_first_steady >= kRepeatFlushMs;
            const bool sweep_due = rl_pending.load(std::memory_order_relaxed) > 0 && now - last_sweep >= kHousekeepingTickMs;
            if (lines || repeat_due || sweep_due)
                break;
            if (repeat_count > 0 || rl_pending.load(std::memory_order_relaxed) > 0 || paused)
                queue_cv.wait(&queue_mutex, QDeadlineTimer(kHousekeepingTickMs));
            else
                queue_cv.wait(&queue_mutex);
        }

        // Batching window: let lines accumulate so one write covers many, unless a
        // WARN+ / AUDIT line or a full buffer asked for an immediate drain.
        if (!paused && !drain_requested && !pending.isEmpty() && audit_pending.isEmpty()) {
            const QDeadlineTimer deadline(kFlushIntervalMs);
            while (!stop_requested && !drain_requested && audit_pending.isEmpty() && !deadline.hasExpired())
                queue_cv.wait(&queue_mutex, deadline);
            if (stop_requested)
                return;
        }

        const qint64 now = logger_steady_ms();
        if (repeat_count > 0 && now - repeat_first_steady >= kRepeatFlushMs)
            append_repeat_summary_locked();
        const bool sweep = rl_pending.load(std::memory_order_relaxed) > 0 && now - last_sweep >= kHousekeepingTickMs;
        if (sweep)
            last_sweep = now;
        const bool write_now = !paused;

        q.unlock(); // lock order: file_mutex before queue_mutex; rl_mutex alone
        if (sweep)
            owner->sweep_rate_limits(false);
        bool backoff = false;
        if (write_now) {
            QMutexLocker f(&file_mutex);
            backoff = !drain_locked(true);
        }
        q.relock();

        if (backoff) { // a write failed recently: wait (no spinning), lines keep queueing up to the cap
            const QDeadlineTimer deadline(kWriteRetryMs);
            while (!stop_requested && !deadline.hasExpired())
                queue_cv.wait(&queue_mutex, deadline);
        }
    }
}

// ── Logger ──────────────────────────────────────────────────────────────────

Logger::Logger() : s_(std::make_unique<State>()) {
    s_->owner = this;
    // FINCEPT_LOG_LEVEL / FINCEPT_LOG_TAGS: a start-up override that needs no UI and
    // wins over the saved settings (applied whenever set_level()/set_tag_level() run).
    const QString env_level = qEnvironmentVariable("FINCEPT_LOG_LEVEL");
    const QString env_tags = qEnvironmentVariable("FINCEPT_LOG_TAGS");
    if (!env_level.trimmed().isEmpty() || !env_tags.trimmed().isEmpty()) {
        LogLevelSpec spec = parse_level_spec(env_level);
        const LogLevelSpec tags = parse_level_spec(env_tags);
        if (tags.global && !spec.global)
            spec.global = tags.global;
        for (auto it = tags.tags.cbegin(); it != tags.tags.cend(); ++it)
            spec.tags.insert(it.key(), it.value());
        spec.invalid += tags.invalid;
        QStringList sources;
        if (!env_level.trimmed().isEmpty())
            sources << QStringLiteral("FINCEPT_LOG_LEVEL");
        if (!env_tags.trimmed().isEmpty())
            sources << QStringLiteral("FINCEPT_LOG_TAGS");
        apply_level_overrides(spec, sources.join(QLatin1Char('+')));
    }
}

Logger::~Logger() = default; // never runs — instance() is deliberately immortal

Logger& Logger::instance() {
    // Immortal: a LOG_* from a static destructor that runs after main() returns
    // must never touch a destroyed logger. Its file is drained and closed by the
    // post-routine / atexit hook below, not by a destructor.
    static Logger* const s = new Logger();

    // Flush-on-exit hook — runs before static destruction, while Qt is still alive.
    // Registered only once a QCoreApplication exists (qAddPostRoutine is a no-op
    // without one); the old guard flipped to "registered" on the first call even
    // when no application existed yet, so a LOG before QApplication meant the hook
    // was never installed.
    static std::atomic<bool> post_routine_registered{false};
    if (!post_routine_registered.load(std::memory_order_acquire) && QCoreApplication::instance()) {
        bool expected = false;
        if (post_routine_registered.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
            qAddPostRoutine([]() { Logger::instance().flush_and_close(); });
    }
    return *s;
}

void Logger::recompute_thresholds_locked() {
    State& s = *s_;
    const LogLevel global = s.override_global.value_or(s.settings_global);
    min_level_.store(global, std::memory_order_relaxed);
    s.tag_levels = s.settings_tags;
    for (auto it = s.override_tags.cbegin(); it != s.override_tags.cend(); ++it)
        s.tag_levels.insert(it.key(), it.value());
    int lowest = static_cast<int>(global);
    for (auto it = s.tag_levels.cbegin(); it != s.tag_levels.cend(); ++it)
        lowest = std::min(lowest, static_cast<int>(it.value()));
    lowest_enabled_.store(lowest, std::memory_order_relaxed);
    has_tag_overrides_.store(!s.tag_levels.isEmpty(), std::memory_order_release);
}

void Logger::set_level(LogLevel level) {
    QWriteLocker lock(&s_->tag_lock);
    s_->settings_global = level;
    recompute_thresholds_locked();
}

void Logger::set_tag_level(const QString& tag, LogLevel level) {
    QWriteLocker lock(&s_->tag_lock);
    s_->settings_tags[tag] = level;
    recompute_thresholds_locked();
}

void Logger::clear_tag_level(const QString& tag) {
    QWriteLocker lock(&s_->tag_lock);
    s_->settings_tags.remove(tag);
    recompute_thresholds_locked();
}

void Logger::clear_all_tag_levels() {
    QWriteLocker lock(&s_->tag_lock);
    s_->settings_tags.clear();
    recompute_thresholds_locked();
}

void Logger::set_json_mode(bool enabled) {
    json_mode_.store(enabled, std::memory_order_relaxed);
}

bool Logger::json_mode() const {
    return json_mode_.load(std::memory_order_relaxed);
}

QHash<QString, LogLevel> Logger::tag_levels() const {
    QReadLocker lock(&s_->tag_lock);
    return s_->tag_levels;
}

bool Logger::is_enabled(LogLevel level, const QString& tag) const {
    if (static_cast<int>(level) < lowest_enabled_.load(std::memory_order_relaxed))
        return false;
    LogLevel effective = min_level_.load(std::memory_order_relaxed);
    if (has_tag_overrides_.load(std::memory_order_acquire)) {
        QReadLocker lock(&s_->tag_lock);
        const auto it = s_->tag_levels.constFind(tag);
        if (it != s_->tag_levels.cend())
            effective = it.value();
    }
    return level >= effective;
}

QString Logger::level_name(LogLevel level) {
    switch (level) {
        case LogLevel::Trace:
            return QStringLiteral("Trace");
        case LogLevel::Debug:
            return QStringLiteral("Debug");
        case LogLevel::Info:
            return QStringLiteral("Info");
        case LogLevel::Warn:
            return QStringLiteral("Warn");
        case LogLevel::Error:
            return QStringLiteral("Error");
        case LogLevel::Fatal:
            return QStringLiteral("Fatal");
    }
    return QStringLiteral("Info");
}

std::optional<LogLevel> Logger::parse_level(QStringView text) {
    const QStringView t = text.trimmed();
    if (t.isEmpty())
        return std::nullopt;
    bool numeric = false;
    const int n = t.toInt(&numeric);
    if (numeric) {
        if (n >= static_cast<int>(LogLevel::Trace) && n <= static_cast<int>(LogLevel::Fatal))
            return static_cast<LogLevel>(n);
        return std::nullopt;
    }
    const auto is = [&t](const char16_t* name) { return t.compare(QStringView(name), Qt::CaseInsensitive) == 0; };
    if (is(u"trace"))
        return LogLevel::Trace;
    if (is(u"debug"))
        return LogLevel::Debug;
    if (is(u"info"))
        return LogLevel::Info;
    if (is(u"warn") || is(u"warning"))
        return LogLevel::Warn;
    if (is(u"error") || is(u"err"))
        return LogLevel::Error;
    if (is(u"fatal") || is(u"critical"))
        return LogLevel::Fatal;
    return std::nullopt;
}

LogLevelSpec Logger::parse_level_spec(QStringView spec) {
    LogLevelSpec out;
    const auto separator = [](QChar c) { return c == u',' || c == u';' || c.isSpace(); };
    qsizetype i = 0;
    while (i < spec.size()) {
        while (i < spec.size() && separator(spec[i]))
            ++i;
        const qsizetype start = i;
        while (i < spec.size() && !separator(spec[i]))
            ++i;
        if (i == start)
            continue;
        const QStringView item = spec.sliced(start, i - start);
        qsizetype pos = item.indexOf(u'=');
        const qsizetype colon = item.indexOf(u':');
        if (pos < 0 || (colon >= 0 && colon < pos))
            pos = colon;
        if (pos < 0) {
            if (const auto level = parse_level(item))
                out.global = level;
            else
                out.invalid << item.toString();
            continue;
        }
        const QStringView tag = item.first(pos).trimmed();
        const auto level = parse_level(item.sliced(pos + 1));
        if (!tag.isEmpty() && level)
            out.tags.insert(tag.toString(), *level);
        else
            out.invalid << item.toString();
    }
    return out;
}

void Logger::apply_level_overrides(const LogLevelSpec& spec, const QString& source) {
    {
        QWriteLocker lock(&s_->tag_lock);
        if (spec.global)
            s_->override_global = spec.global;
        for (auto it = spec.tags.cbegin(); it != spec.tags.cend(); ++it)
            s_->override_tags.insert(it.key(), it.value());
        s_->override_invalid += spec.invalid;
        s_->override_source = source;
        s_->override_announced = false;
        recompute_thresholds_locked();
    }
    announce_overrides();
}

void Logger::clear_level_overrides() {
    QWriteLocker lock(&s_->tag_lock);
    s_->override_global.reset();
    s_->override_tags.clear();
    s_->override_invalid.clear();
    s_->override_source.clear();
    s_->override_announced = true;
    recompute_thresholds_locked();
}

QString Logger::level_overrides() const {
    QReadLocker lock(&s_->tag_lock);
    if (!s_->override_global && s_->override_tags.isEmpty())
        return {};
    QStringList parts;
    if (s_->override_global)
        parts << QStringLiteral("level=") + level_name(*s_->override_global);
    if (!s_->override_tags.isEmpty()) {
        QStringList tags;
        for (auto it = s_->override_tags.cbegin(); it != s_->override_tags.cend(); ++it)
            tags << it.key() + QLatin1Char(':') + level_name(it.value());
        tags.sort();
        parts << QStringLiteral("tags=") + tags.join(QLatin1Char(','));
    }
    return parts.join(QLatin1Char(' ')) + QStringLiteral(" (") + s_->override_source + QLatin1Char(')');
}

// Writes the override once, when a file is open — whatever the level, so a log
// recorded with an override always says so.
void Logger::announce_overrides() {
    State& s = *s_;
    if (!s.accepting.load(std::memory_order_acquire))
        return;
    QString source;
    QString level;
    QStringList tags;
    QStringList invalid;
    {
        QWriteLocker lock(&s.tag_lock);
        if (s.override_announced)
            return;
        s.override_announced = true;
        source = s.override_source;
        if (s.override_global)
            level = level_name(*s.override_global);
        for (auto it = s.override_tags.cbegin(); it != s.override_tags.cend(); ++it)
            tags << it.key() + QLatin1Char(':') + level_name(it.value());
        invalid = std::exchange(s.override_invalid, {});
    }
    tags.sort();
    if (!level.isEmpty() || !tags.isEmpty()) {
        QList<LogField> fields{{"source", source}};
        if (!level.isEmpty())
            fields.append(LogField("level", level));
        if (!tags.isEmpty())
            fields.append(LogField("tags", tags.join(QLatin1Char(','))));
        enqueue(static_cast<int>(LogLevel::Info), QStringLiteral("App"), logf(QStringLiteral("Log level overridden"), fields),
                kNoFold);
    }
    if (!invalid.isEmpty())
        enqueue(static_cast<int>(LogLevel::Warn), QStringLiteral("App"),
                logf(QStringLiteral("Log level setting ignored"),
                     {{"source", source}, {"value", invalid.join(QLatin1Char(','))}}),
                kNoFold);
}

void Logger::set_file(const QString& path) {
    // Drain-on-exit for every path that never destroys the QApplication: exit() from
    // deep inside the app and the headless --smoke-test / --selftest-* returns.
    // flush_and_close() is idempotent, so running after the post-routine already
    // closed the file is harmless; g_logger_at_exit keeps it off torn-down Qt state.
    static const bool atexit_hooked = []() {
        std::atexit([]() {
            g_logger_at_exit.store(true, std::memory_order_relaxed);
            Logger::instance().flush_and_close();
        });
        return true;
    }();
    Q_UNUSED(atexit_hooked)

    bool opened = false;
    {
        QMutexLocker lock(&s_->file_mutex);
        LoggerDrainGuard guard;
        s_->drain_locked(false); // P1.10 — queued lines belong to the previous file
        if (s_->log_file.isOpen())
            s_->log_file.close();

        // Use fprintf to stderr for these diagnostics — qWarning would recurse
        // through our own Qt message handler installed in main.cpp.
        const auto try_open = [this](const QString& target) -> bool {
            const QString parent = QFileInfo(target).absolutePath();
            if (!parent.isEmpty() && !QDir().mkpath(parent)) {
                fprintf(stderr, "[Logger] mkpath failed for parent dir: %s\n", qUtf8Printable(log_path(parent)));
            }

            // set_file is a fresh-start event: keep the five previous sessions.
            logger_shift_session_logs(target);

            s_->log_file.setFileName(target);
            // P1.7 — no QIODevice::Text so \n stays \n on all platforms; UTF-8 is
            // implicit because we write QByteArray directly. Unbuffered: every batch
            // is one write() to the OS, and its result is the real result.
            if (s_->log_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Unbuffered)) {
                s_->bytes_written = s_->log_file.size();
                s_->rotate_at = kLogRotateBytes;
                s_->write_failing = false;
                s_->lost_unreported = 0;
                s_->last_write_partial = false;
                s_->degraded.store(false, std::memory_order_relaxed);
                return true;
            }
            fprintf(stderr, "[Logger] failed to open log file: %s error: %s\n", qUtf8Printable(log_path(target)),
                    qUtf8Printable(s_->log_file.errorString()));
            return false;
        };

        opened = try_open(path);
        if (!opened) {
            // Fallback: system temp dir so we always leave a breadcrumb.
            // Cross-platform: %TEMP% (Windows), /tmp or $TMPDIR (Linux/macOS).
            // Edition-specific name: the private build falls back to fincept-boot.log.
            const QString fallback = QDir(QDir::tempPath())
                                         .filePath(QStringLiteral("fincept-%1-boot.log")
                                                       .arg(QLatin1String(fincept::edition::kId)));
            if (fallback != path) {
                fprintf(stderr, "[Logger] falling back to temp path: %s\n", qUtf8Printable(log_path(fallback)));
                opened = try_open(fallback);
            }
        }
        if (!s_->audit_path_explicit) {
            s_->audit_path = QFileInfo(path).absolutePath() + QStringLiteral("/audit.log");
            s_->audit_dir_ready = false;
            s_->audit_failing = false;
        }
    }

    // log_path(): <root>/logs/fincept.log → the data root is <root>.
    QDir dir = QFileInfo(path).absoluteDir();
    if (dir.dirName() == QLatin1String("logs") && dir.cdUp())
        log_detail::set_implied_data_root(dir.absolutePath());

    if (opened)
        s_->start_writer();
    s_->accepting.store(opened, std::memory_order_release);
    if (opened)
        announce_overrides();
}

void Logger::set_audit_file(const QString& path) {
    QMutexLocker lock(&s_->file_mutex);
    s_->audit_path = path;
    s_->audit_path_explicit = !path.isEmpty();
    s_->audit_dir_ready = false;
    s_->audit_failing = false;
}

QString Logger::audit_file() const {
    QMutexLocker lock(&s_->file_mutex);
    return s_->audit_path;
}

void Logger::flush() {
    State& s = *s_;
    if (t_logger_in_drain)
        return; // reached from inside a drain on this thread
    {
        QMutexLocker q(&s.queue_mutex);
        s.append_repeat_summary_locked();
    }
    QMutexLocker f(&s.file_mutex);
    s.drain_locked(false);
}

void Logger::flush_and_close() {
    State& s = *s_;
    if (t_logger_in_drain) {
        s.accepting.store(false, std::memory_order_release);
        return; // reached from inside a drain on this thread (e.g. a Qt fatal raised by QFile)
    }
    // Pending summaries first, while lines are still accepted.
    sweep_rate_limits(true);
    {
        QMutexLocker q(&s.queue_mutex);
        s.append_repeat_summary_locked();
    }
    s.accepting.store(false, std::memory_order_release);

    s.stop_writer();

    QMutexLocker lock(&s.file_mutex);
    LoggerDrainGuard guard;
    s.drain_locked(false);
    if (s.log_file.isOpen())
        s.log_file.close();
}

void Logger::set_writer_paused_for_testing(bool paused) {
    QMutexLocker q(&s_->queue_mutex);
    s_->paused = paused;
    s_->queue_cv.wakeAll();
}

void Logger::trace(const QString& tag, const QString& msg) {
    write(LogLevel::Trace, tag, msg);
}
void Logger::debug(const QString& tag, const QString& msg) {
    write(LogLevel::Debug, tag, msg);
}
void Logger::info(const QString& tag, const QString& msg) {
    write(LogLevel::Info, tag, msg);
}
void Logger::warn(const QString& tag, const QString& msg) {
    write(LogLevel::Warn, tag, msg);
}
void Logger::error(const QString& tag, const QString& msg) {
    write(LogLevel::Error, tag, msg);
}
void Logger::fatal(const QString& tag, const QString& msg) {
    write(LogLevel::Fatal, tag, msg);
}
void Logger::log(LogLevel level, const QString& tag, const QString& msg) {
    write(level, tag, msg);
}

void Logger::write(LogLevel level, const QString& tag, const QString& msg) {
    // Level check is also done by LOG_* macros, but re-check here for direct callers.
    if (!is_enabled(level, tag))
        return;
    enqueue(static_cast<int>(level), tag, msg, 0);
}

void Logger::enqueue(int level, const QString& tag, const QString& msg, unsigned flags) {
    State& s = *s_;
    if (!s.accepting.load(std::memory_order_acquire))
        return; // no file open — dropped, as before

    // Format on the calling thread with no lock held; the timestamp is the call's.
    const qint64 wall = QDateTime::currentMSecsSinceEpoch();
    const bool json = json_mode_.load(std::memory_order_relaxed);
    QLatin1StringView thread;
    if (json)
        thread = logger_thread_label();
    else if (static_cast<int>(min_level_.load(std::memory_order_relaxed)) <= static_cast<int>(LogLevel::Debug) &&
             !QThread::isMainThread())
        thread = logger_thread_label();
    const QByteArray line =
        json ? logger_json_line(level, tag, msg, wall, thread, 0) : logger_text_line(level, tag, msg, wall, thread);

    const bool fatal = level == static_cast<int>(LogLevel::Fatal);
    const bool foldable = !(flags & kNoFold) && level < static_cast<int>(LogLevel::Fatal);
    const bool problem = level >= static_cast<int>(LogLevel::Warn) && level <= static_cast<int>(LogLevel::Fatal);
    // We intentionally do NOT echo via qDebug(): main.cpp installs a Qt message
    // handler that routes qDebug back into this logger, so an echo here would recurse.
    //
    // Strings displaced from the repeat state / problem ring are released after the
    // lock is dropped: freeing another thread's buffer inside the critical section
    // serialised the producers (measured: 8 threads 60 % slower).
    QString released_tag;
    QByteArray released_problem;
    // What makes two lines "the same": the formatted line after its fixed-width timestamp
    // ("[LEVEL] [Tag] message"), or level + tag + message in JSON mode — compared by level,
    // length and a 64-bit hash (folding two different lines would need a hash collision
    // between consecutive lines of equal length).
    QByteArray json_key;
    QByteArrayView body;
    if (json) {
        json_key = QByteArray(kLoggerLevelNames[static_cast<std::size_t>(level)]) + '\x1f' + tag.toUtf8() + '\x1f' +
                   msg.toUtf8();
        body = json_key;
    } else {
        body = QByteArrayView(line).sliced(kLoggerTimestampChars + 3); // "[" ts "] "
    }
    const size_t body_hash = qHash(body, size_t(0x2545F4914F6CDD1DULL));
    {
        QMutexLocker q(&s.queue_mutex);

        // Repeat suppression: same level, tag and text as the line before it.
        if (foldable && s.last_level == level && s.last_len == body.size() && s.last_hash == body_hash) {
            if (s.repeat_count++ == 0) {
                released_tag = std::exchange(s.last_tag, tag);
                s.repeat_first_steady = logger_steady_ms();
                s.queue_cv.wakeOne(); // the writer starts its 5 s summary clock
            }
            s.repeat_last_wall = wall;
            ++s.st_repeats;
            return;
        }

        // Full queue (the disk is not keeping up): the GUI thread never waits.
        if (!(flags & kBypassCap) && !fatal && s.pending.size() >= kQueueMaxBytes) {
            if (level >= static_cast<int>(LogLevel::Warn) && !t_logger_in_drain && !QThread::isMainThread()) {
                const QDeadlineTimer deadline(kQueueFullWaitMs);
                ++s.space_waiters;
                while (s.pending.size() >= kQueueMaxBytes && !s.stop_requested && !deadline.hasExpired())
                    s.space_cv.wait(&s.queue_mutex, deadline);
                --s.space_waiters;
            }
            if (s.pending.size() >= kQueueMaxBytes) {
                ++s.dropped_unreported;
                ++s.st_dropped;
                if (problem)
                    released_problem = s.remember_problem_locked(line);
                return;
            }
        }

        s.append_repeat_summary_locked();
        s.append_dropped_marker_locked(wall);
        if (foldable) {
            s.last_level = level;
            s.last_len = body.size();
            s.last_hash = body_hash;
        } else {
            s.last_level = -1;
        }
        const bool was_empty = s.pending.isEmpty();
        s.pending.append(line);
        ++s.pending_lines;
        if (problem) {
            released_problem = s.remember_problem_locked(line);
            s.drain_requested = true; // WARN+ : write within one writer wake-up
        }
        if (s.pending.size() >= kWakeBytes)
            s.drain_requested = true;
        if (was_empty || s.drain_requested)
            s.queue_cv.wakeOne(); // first line of a batch starts the writer's window
    }

    // FATAL (the process is about to die) and the no-writer fallback drain here.
    if ((fatal || !s.writer_running.load(std::memory_order_acquire)) && !t_logger_in_drain) {
        QMutexLocker f(&s.file_mutex);
        s.drain_locked(false);
    }
}

void Logger::audit(const QString& tag, const QString& sentence, std::initializer_list<LogField> fields) {
    audit(tag, logf(sentence, fields));
}

void Logger::audit(const QString& tag, const QString& msg) {
    State& s = *s_;
    const qint64 wall = QDateTime::currentMSecsSinceEpoch();
    // audit.log is always text, whatever the JSON setting of fincept.log.
    const QByteArray line = logger_text_line(kAuditLevel, tag, msg, wall, {});
    if (!s.accepting.load(std::memory_order_acquire)) {
        // No log file (before set_file() / after shutdown): never silently.
        fwrite(line.constData(), 1, static_cast<size_t>(line.size()), stderr);
        s.st_audit_lost.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    const QByteArray mirror = json_mode_.load(std::memory_order_relaxed)
                                  ? logger_json_line(kAuditLevel, tag, msg, wall, logger_thread_label(), 0)
                                  : line;
    // Off the GUI thread the line is written before audit() returns; on the GUI
    // thread it is handed to the writer (no disk I/O on the GUI thread).
    const bool can_write_here = !QThread::isMainThread() && !t_logger_in_drain;
    {
        QMutexLocker q(&s.queue_mutex);
        s.append_repeat_summary_locked();
        s.append_dropped_marker_locked(wall);
        s.last_level = -1; // never folded, and nothing folds across it
        s.pending.append(mirror);
        ++s.pending_lines;
        if (s.audit_pending_lines >= kAuditQueueMaxLines && !can_write_here) {
            ++s.audit_lost_unreported; // reported in audit.log, fincept.log and stderr
            s.st_audit_lost.fetch_add(1, std::memory_order_relaxed);
        } else {
            s.audit_pending.append(line);
            ++s.audit_pending_lines;
        }
        s.drain_requested = true;
        s.queue_cv.wakeOne();
    }
    if (t_logger_in_drain)
        return;
    if (can_write_here || !s.writer_running.load(std::memory_order_acquire)) {
        // Bounded wait for the file; if the disk is stalled the line stays queued
        // (never dropped) and the writer writes it when the disk accepts writes.
        if (s.file_mutex.tryLock(kAuditSyncWaitMs)) {
            s.drain_locked(false);
            s.file_mutex.unlock();
        }
    }
}

bool Logger::rate_limit_pass(const QString& tag, const QString& key, int interval_ms, int* suppressed,
                             LogLevel level) {
    if (suppressed)
        *suppressed = 0;
    if (interval_ms <= 0)
        return true;
    State& s = *s_;
    const QString k = logger_table_key(tag, key);
    const qint64 now = logger_steady_ms();
    bool pass = true;
    bool wake = false;
    std::optional<LoggerRateSummary> evicted;
    {
        QMutexLocker lock(&s.rl_mutex);
        auto it = s.rl.find(k);
        if (it == s.rl.end()) {
            if (s.rl.size() >= kRateLimitMaxKeys) {
                auto oldest = s.rl.begin();
                for (auto j = s.rl.begin(); j != s.rl.end(); ++j) {
                    if (j->last_seen < oldest->last_seen)
                        oldest = j;
                }
                if (oldest->suppressed > 0) {
                    evicted = LoggerRateSummary{oldest->level, oldest->tag, oldest->key, oldest->suppressed,
                                                oldest->interval_ms};
                    s.rl_pending.fetch_sub(1, std::memory_order_relaxed);
                }
                s.rl.erase(oldest);
            }
            LoggerRateEntry e;
            e.tag = tag;
            e.key = key;
            e.window_start = now;
            e.last_seen = now;
            e.interval_ms = interval_ms;
            e.level = static_cast<int>(level);
            s.rl.insert(k, e);
        } else {
            LoggerRateEntry& e = it.value();
            e.last_seen = now;
            e.interval_ms = interval_ms;
            e.level = static_cast<int>(level);
            if (now - e.window_start < interval_ms) {
                if (e.suppressed++ == 0)
                    wake = s.rl_pending.fetch_add(1, std::memory_order_relaxed) == 0;
                pass = false;
            } else {
                if (suppressed)
                    *suppressed = e.suppressed;
                if (e.suppressed > 0)
                    s.rl_pending.fetch_sub(1, std::memory_order_relaxed);
                e.suppressed = 0;
                e.window_start = now;
            }
        }
    }
    if (!pass)
        s.st_rate_limited.fetch_add(1, std::memory_order_relaxed);
    if (wake) { // the writer starts its sweep clock
        QMutexLocker q(&s.queue_mutex);
        s.queue_cv.wakeOne();
    }
    if (evicted)
        enqueue(evicted->level, evicted->tag,
                logf(QStringLiteral("Repeated lines suppressed"),
                     {{"key", evicted->key}, {"suppressed", evicted->count}, {"interval_ms", evicted->interval_ms}}),
                0);
    return pass;
}

void Logger::sweep_rate_limits(bool all) {
    State& s = *s_;
    if (s.rl_pending.load(std::memory_order_acquire) == 0)
        return;
    QList<LoggerRateSummary> due;
    const qint64 now = logger_steady_ms();
    {
        QMutexLocker lock(&s.rl_mutex);
        for (auto it = s.rl.begin(); it != s.rl.end(); ++it) {
            LoggerRateEntry& e = it.value();
            if (e.suppressed > 0 && (all || now - e.window_start >= e.interval_ms)) {
                due.append(LoggerRateSummary{e.level, e.tag, e.key, e.suppressed, e.interval_ms});
                e.suppressed = 0;
                s.rl_pending.fetch_sub(1, std::memory_order_relaxed);
            }
        }
    }
    for (const LoggerRateSummary& m : due)
        enqueue(m.level, m.tag,
                logf(QStringLiteral("Repeated lines suppressed"),
                     {{"key", m.key}, {"suppressed", m.count}, {"interval_ms", m.interval_ms}}),
                0);
}

bool Logger::transition(const QString& tag, const QString& key, int state, LogTransition* info) {
    if (info)
        *info = LogTransition{};
    if (state == 0 && transition_keys_.load(std::memory_order_acquire) == 0)
        return false;
    State& s = *s_;
    const QString k = logger_table_key(tag, key);
    const qint64 now = logger_steady_ms();
    QMutexLocker lock(&s.tr_mutex);
    auto it = s.tr.find(k);
    if (it == s.tr.end()) {
        if (state == 0)
            return false; // healthy and never seen otherwise: nothing to say
        if (s.tr.size() >= kTransitionMaxKeys) {
            auto oldest = s.tr.begin();
            for (auto j = s.tr.begin(); j != s.tr.end(); ++j) {
                if (j->last_seen < oldest->last_seen)
                    oldest = j;
            }
            s.tr.erase(oldest);
        }
        s.tr.insert(k, LoggerTransitionEntry{state, 1, now, now});
        transition_keys_.store(static_cast<int>(s.tr.size()), std::memory_order_release);
        return true;
    }
    LoggerTransitionEntry& e = it.value();
    if (e.state == state) {
        ++e.observations;
        e.last_seen = now;
        return false;
    }
    if (info) {
        info->previous_state = e.state;
        info->observations_in_previous = e.observations;
        info->ms_in_previous = now - e.since;
    }
    if (state == 0) {
        s.tr.erase(it); // healthy keys are not stored
        transition_keys_.store(static_cast<int>(s.tr.size()), std::memory_order_release);
    } else {
        e = LoggerTransitionEntry{state, 1, now, now};
    }
    return true;
}

QStringList Logger::recent_problems(int max_lines) const {
    QList<QByteArray> lines;
    {
        QMutexLocker q(&s_->queue_mutex);
        const int n = static_cast<int>(s_->recent.size());
        lines.reserve(n);
        for (int i = 0; i < n; ++i)
            lines.append(s_->recent[(s_->recent_next + i) % n]);
    }
    if (max_lines >= 0 && lines.size() > max_lines)
        lines = lines.mid(lines.size() - max_lines);
    QStringList out;
    out.reserve(lines.size());
    for (const QByteArray& l : lines) {
        QString text = QString::fromUtf8(l);
        if (text.endsWith(QLatin1Char('\n')))
            text.chop(1);
        out << log_scrub(text);
    }
    return out;
}

LogStats Logger::stats() const {
    LogStats st;
    {
        QMutexLocker q(&s_->queue_mutex);
        st.dropped_lines = s_->st_dropped;
        st.repeats_suppressed = s_->st_repeats;
        st.queued_bytes = s_->pending.size();
    }
    st.lost_lines = s_->st_lost.load(std::memory_order_relaxed);
    st.audit_lost_lines = s_->st_audit_lost.load(std::memory_order_relaxed);
    st.rate_limited = s_->st_rate_limited.load(std::memory_order_relaxed);
    {
        QMutexLocker lock(&s_->rl_mutex);
        st.rate_limit_keys = static_cast<int>(s_->rl.size());
    }
    {
        QMutexLocker lock(&s_->tr_mutex);
        st.transition_keys = static_cast<int>(s_->tr.size());
    }
    return st;
}

} // namespace fincept
