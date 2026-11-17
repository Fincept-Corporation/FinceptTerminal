#include "core/telemetry/UsageScrub.h"

#include "screens/settings/SettingsSecretMask.h"

#include <QDir>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>
#include <QUrl>

#include <algorithm>

// Helpers are prefixed us_ (unity builds batch this file with others).

namespace fincept::telemetry {

namespace {

/// Upper-case words that are technical vocabulary, not instruments.
bool us_is_known_word(const QString& word) {
    static const QSet<QString> kKnown = {
        QStringLiteral("HTTP"),     QStringLiteral("HTTPS"),   QStringLiteral("HTTP2"),  QStringLiteral("SSL"),
        QStringLiteral("TLS"),      QStringLiteral("DNS"),     QStringLiteral("TCP"),    QStringLiteral("UDP"),
        QStringLiteral("IP"),       QStringLiteral("URL"),     QStringLiteral("URI"),    QStringLiteral("API"),
        QStringLiteral("JSON"),     QStringLiteral("XML"),     QStringLiteral("HTML"),   QStringLiteral("CSV"),
        QStringLiteral("SQL"),      QStringLiteral("UTF"),     QStringLiteral("UTF-8"),  QStringLiteral("ASCII"),
        QStringLiteral("EOF"),      QStringLiteral("OK"),      QStringLiteral("GET"),    QStringLiteral("POST"),
        QStringLiteral("PUT"),      QStringLiteral("PATCH"),   QStringLiteral("DELETE"), QStringLiteral("HEAD"),
        QStringLiteral("ID"),       QStringLiteral("OS"),      QStringLiteral("CPU"),    QStringLiteral("GPU"),
        QStringLiteral("RAM"),      QStringLiteral("IO"),      QStringLiteral("PID"),    QStringLiteral("ERROR"),
        QStringLiteral("WARNING"),  QStringLiteral("WARN"),    QStringLiteral("INFO"),   QStringLiteral("DEBUG"),
        QStringLiteral("CRITICAL"), QStringLiteral("FATAL"),   QStringLiteral("NULL"),   QStringLiteral("NONE"),
        QStringLiteral("TRUE"),     QStringLiteral("FALSE"),   QStringLiteral("NAN"),    QStringLiteral("INF"),
        QStringLiteral("UUID"),     QStringLiteral("QT"),      QStringLiteral("PDF"),    QStringLiteral("PNG"),
        QStringLiteral("ZIP"),      QStringLiteral("TIMEOUT"), QStringLiteral("PIP"),    QStringLiteral("UV"),
        QStringLiteral("MCP"),      QStringLiteral("LLM"),     QStringLiteral("AI"),     QStringLiteral("SIGSEGV"),
        QStringLiteral("SIGKILL"),  QStringLiteral("SIGTERM"), QStringLiteral("SIGABRT"), QStringLiteral("ENOENT"),
        QStringLiteral("EACCES"),   QStringLiteral("EPERM"),   QStringLiteral("ENOMEM"), QStringLiteral("ECONNREFUSED"),
        QStringLiteral("ETIMEDOUT"),
    };
    return kKnown.contains(word);
}

/// The first non-empty line of `text`, at most `max_chars` of it. Scans only as
/// far as that line (a Python stderr can be megabytes), and bounds the work of
/// every rule that runs on it afterwards.
QString us_first_line(const QString& text, qsizetype max_chars) {
    qsizetype start = 0;
    const qsizetype size = text.size();
    while (start < size) {
        qsizetype end = start;
        while (end < size && text.at(end) != QLatin1Char('\n') && text.at(end) != QLatin1Char('\r'))
            ++end;
        const QStringView line = QStringView(text).mid(start, std::min(end - start, max_chars));
        if (!line.trimmed().isEmpty())
            return line.toString();
        start = end + 1;
    }
    return {};
}

void us_replace_home(QString& text) {
#ifdef Q_OS_WIN
    constexpr Qt::CaseSensitivity kCase = Qt::CaseInsensitive;
#else
    constexpr Qt::CaseSensitivity kCase = Qt::CaseSensitive;
#endif
    const QString home = QDir::homePath();
    if (home.size() > 1) {
        text.replace(home, QStringLiteral("~"), kCase);
        const QString native = QDir::toNativeSeparators(home);
        if (native != home)
            text.replace(native, QStringLiteral("~"), kCase);
    }
    // Any other spelling of a user folder (another account, a short 8.3 name).
    static const QRegularExpression kUserDir(QStringLiteral("(?:[A-Za-z]:)?[\\\\/]+(?:Users|home)[\\\\/]+[^\\\\/\\s'\"]+"),
                                             QRegularExpression::CaseInsensitiveOption);
    text.replace(kUserDir, QStringLiteral("~"));
}

/// Upper-case words that look like a ticker ("AAPL", "RELIANCE.NS", "BTC-USD",
/// "EURUSD=X", "^GSPC") become "<sym>"; technical acronyms stay.
void us_mask_symbols(QString& text) {
    static const QRegularExpression kSymbol(
        QStringLiteral("\\^[A-Z0-9]{2,10}\\b|\\b[A-Z][A-Z0-9&]{1,11}(?:[.\\-=][A-Z0-9]{1,5})?\\b"));
    QString out;
    qsizetype last = 0;
    auto it = kSymbol.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        if (us_is_known_word(m.captured()))
            continue;
        out += QStringView(text).mid(last, m.capturedStart() - last);
        out += QStringLiteral("<sym>");
        last = m.capturedEnd();
    }
    if (last == 0)
        return;
    out += QStringView(text).mid(last);
    text = out;
}

} // namespace

QString scrub_sample(const QString& text, int max_chars) {
    // Four times the limit is plenty to work on: what lies past it would be cut anyway.
    QString s = us_first_line(text, std::max(max_chars, 64) * 4);
    if (s.isEmpty())
        return {};

    // Order matters: whole URLs and credentials first, so the narrower rules
    // below never leave half of one behind.
    static const QRegularExpression kUrl(QStringLiteral("\\b[A-Za-z][A-Za-z0-9+.\\-]*://[^\\s'\"<>]+"));
    s.replace(kUrl, QStringLiteral("<url>"));
    settings_secret_mask::mask_token_patterns(s);
    static const QRegularExpression kKeyValue(
        QStringLiteral("\\b(api[_-]?key|access[_-]?token|refresh[_-]?token|session[_-]?token|token|secret|password|"
                       "passwd|pwd|auth|authorization|cookie|signature)\\b(\\s*[:=]\\s*)(\"[^\"]*\"|'[^']*'|\\S+)"),
        QRegularExpression::CaseInsensitiveOption);
    s.replace(kKeyValue, QStringLiteral("\\1\\2<redacted>"));
    static const QRegularExpression kEmail(QStringLiteral("[A-Za-z0-9._%+\\-]+@[A-Za-z0-9.\\-]+\\.[A-Za-z]{2,}"));
    s.replace(kEmail, QStringLiteral("<email>"));
    us_replace_home(s);

    // Values quoted in an error ("KeyError: 'AAPL'", "file '...' not found")
    // are what the user typed or what a file holds — never sent.
    static const QRegularExpression kSingleQuoted(QStringLiteral("(^|[\\s(\\[{=:,])'[^']*'"));
    s.replace(kSingleQuoted, QStringLiteral("\\1'…'"));
    static const QRegularExpression kDoubleQuoted(QStringLiteral("\"[^\"]*\""));
    s.replace(kDoubleQuoted, QStringLiteral("\"…\""));

    // Opaque ids and tokens (long runs with a digit), then long numbers
    // (account / order / phone numbers).
    static const QRegularExpression kOpaque(QStringLiteral("(?=[A-Za-z0-9_+/=\\-]*[0-9])[A-Za-z0-9_+/=\\-]{20,}"));
    s.replace(kOpaque, QStringLiteral("<id>"));
    static const QRegularExpression kLongNumber(QStringLiteral("\\d{6,}"));
    s.replace(kLongNumber, QStringLiteral("<n>"));
    us_mask_symbols(s);

    s = s.simplified();
    if (max_chars > 1 && s.size() > max_chars) {
        s.truncate(max_chars - 1);
        if (!s.isEmpty() && s.back().isHighSurrogate())
            s.chop(1);
        s += QChar(0x2026);
    }
    return s;
}

QString scrub_route(const QUrl& url) {
    static const QRegularExpression kRouteWord(QStringLiteral("^[a-z][a-z0-9_\\-]{0,39}$"));
    const QStringList parts = url.path(QUrl::FullyDecoded).split(QLatin1Char('/'), Qt::SkipEmptyParts);
    QStringList out;
    for (const QString& part : parts) {
        int digits = 0;
        for (const QChar c : part)
            digits += c.isDigit() ? 1 : 0;
        out << ((digits <= 3 && kRouteWord.match(part).hasMatch()) ? part : QStringLiteral(":id"));
        if (out.size() >= 12)
            break;
    }
    return QLatin1Char('/') + out.join(QLatin1Char('/'));
}

QString scrub_label(const QString& text, int max_chars) {
    QString out;
    out.reserve(qMin<qsizetype>(text.size(), max_chars));
    for (const QChar c : text) {
        const char16_t u = c.unicode();
        const bool keep = (u >= u'a' && u <= u'z') || (u >= u'A' && u <= u'Z') || (u >= u'0' && u <= u'9') ||
                          u == u' ' || u == u'_' || u == u'.' || u == u':' || u == u'=' || u == u'/' || u == u'<' ||
                          u == u'>' || u == u'-';
        if (!keep)
            continue;
        out.append(c);
        if (out.size() >= max_chars)
            break;
    }
    return out.trimmed();
}

} // namespace fincept::telemetry
