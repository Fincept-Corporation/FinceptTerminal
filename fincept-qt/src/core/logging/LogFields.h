#pragma once
// Key=value fields for log lines — policy B.3 ("Plain sentence key=value key=\"v w\"").
//
//   LOG_WARN("HTTP", logf("HTTP request failed", {{"status", status}, {"url", url}, {"error", err}}));
//   → [ts] [WARN] [HTTP] HTTP request failed status=404 url=https://api.fincept.in/macro/x error="Not Found"
//
// logf() sits inside the LOG_* macro's message argument, so nothing here runs (no field is
// converted, no string is built) when the level is off. Pure functions, no state, thread-safe.
//
// Quoting (log_quote): a value is written bare when it is non-empty and has no whitespace,
// no '"', no '=' and no control character; otherwise it is written in double quotes with
// \\ \" \n \r \t escaped (other control characters as \xHH). Every value is cut at 200
// characters with "…". An empty value is written as "". So a line never spans lines.
//
// Value types: QString, QStringView, QLatin1StringView, const char* (UTF-8, nullptr = ""),
// QByteArray (UTF-8), std::string, std::string_view, char (one character), bool (true|false),
// every integer type (decimal), float / double ('g', 7 / 10 significant digits), enums (the
// Q_ENUM key when the enum is registered with Q_ENUM / Q_ENUM_NS, else the number), and
// QUrl — written through log_url(), so a URL field can never leak a query or credentials.
// Any other pointer is a compile error (it would otherwise print "true").
//
// Keys are lower_snake_case string literals from the standard list in policy B.3
// (symbol exchange venue account mode side type qty price order_id client_id topic source
// feed url status error reason attempt retry_in_s duration_ms count bytes script exit run
// model tokens_in tokens_out tool client path key version pid). Order is the caller's:
// identifiers first, quantities next, error= last.

#include <QByteArray>
#include <QList>
#include <QMetaEnum>
#include <QString>
#include <QStringView>

#include <initializer_list>
#include <string>
#include <string_view>
#include <type_traits>

QT_FORWARD_DECLARE_CLASS(QUrl)

namespace fincept {

/// Longest value logf() writes; longer values are cut and end with "…".
inline constexpr int kLogFieldMaxChars = 200;

namespace log_detail {
template <typename T>
inline constexpr bool kLogFieldTextPointee = std::is_same_v<std::remove_cv_t<T>, char> ||
                                              std::is_same_v<std::remove_cv_t<T>, char16_t> ||
                                              std::is_same_v<std::remove_cv_t<T>, wchar_t> ||
                                              std::is_same_v<std::remove_cv_t<T>, QChar>;
} // namespace log_detail

struct LogField {
    const char* key;
    QString value;

    LogField(const char* k, const QString& v) : key(k), value(v) {}
    LogField(const char* k, QStringView v) : key(k), value(v.toString()) {}
    LogField(const char* k, QLatin1StringView v) : key(k), value(v) {}
    LogField(const char* k, const char* v) : key(k), value(QString::fromUtf8(v)) {}
    LogField(const char* k, char* v) : key(k), value(QString::fromUtf8(v)) {}
    LogField(const char* k, const QByteArray& v) : key(k), value(QString::fromUtf8(v)) {}
    LogField(const char* k, const std::string& v) : key(k), value(QString::fromStdString(v)) {}
    LogField(const char* k, std::string_view v)
        : key(k), value(QString::fromUtf8(v.data(), static_cast<qsizetype>(v.size()))) {}
    LogField(const char* k, const QUrl& v); // log_url(v) — defined in Logger.cpp
    LogField(const char* k, bool v) : key(k), value(v ? QStringLiteral("true") : QStringLiteral("false")) {}
    LogField(const char* k, char v) : key(k), value(QChar::fromLatin1(v)) {}

    template <typename T,
              std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool> && !std::is_same_v<T, char>, int> = 0>
    LogField(const char* k, T v) : key(k), value(QString::number(v)) {}

    template <typename T, std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
    LogField(const char* k, T v)
        : key(k), value(QString::number(static_cast<double>(v), 'g', std::is_same_v<T, float> ? 7 : 10)) {}

    template <typename T, std::enable_if_t<std::is_enum_v<T>, int> = 0>
    LogField(const char* k, T v) : key(k), value(enum_text(v)) {}

    // A pointer that is not a string: log the object's identity, not its address.
    template <typename T, std::enable_if_t<!log_detail::kLogFieldTextPointee<T>, int> = 0>
    LogField(const char* k, T* v) = delete;

  private:
    template <typename E>
    static QString enum_text(E v) {
        if constexpr (QtPrivate::IsQEnumHelper<E>::Value) {
            if (const char* name = QMetaEnum::fromType<E>().valueToKey(static_cast<int>(v)))
                return QString::fromLatin1(name);
        }
        using U = std::underlying_type_t<E>;
        if constexpr (std::is_signed_v<U>)
            return QString::number(static_cast<qint64>(v));
        else
            return QString::number(static_cast<quint64>(v));
    }
};

/// `value` as one field value: bare, or quoted and escaped (rules above), cut at `max_chars`.
QString log_quote(QStringView value, int max_chars = kLogFieldMaxChars);

/// "Sentence k=v k2=\"v w\"". The sentence is constant text (it is the template tools count);
/// every variable goes into a field.
QString logf(const QString& sentence, std::initializer_list<LogField> fields);
/// Same, for a field list built up conditionally. Build the list inside the LOG_* argument
/// (or behind should_log()) — a list built before the macro is built even when the level is off.
QString logf(const QString& sentence, const QList<LogField>& fields);

namespace detail {
/// LOG_*_RL helper: "msg (suppressed=N)" when N > 0.
inline QString log_with_suppressed(const QString& msg, int suppressed) {
    if (suppressed <= 0)
        return msg;
    QString out = msg;
    out += QStringLiteral(" (suppressed=");
    out += QString::number(suppressed);
    out += QLatin1Char(')');
    return out;
}
} // namespace detail

} // namespace fincept
