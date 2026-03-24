#include "services/workflow/Extensions.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace fincept::workflow {

// ── Array Extensions ───────────────────────────────────────────────────

namespace ArrayExt {

static double extract_double(const QJsonValue& v, const QString& field) {
    if (field.isEmpty())
        return v.toDouble();
    if (v.isObject())
        return v.toObject().value(field).toDouble();
    return 0.0;
}

QJsonValue first(const QJsonArray& arr) {
    return arr.isEmpty() ? QJsonValue{} : arr.first();
}
QJsonValue last(const QJsonArray& arr) {
    return arr.isEmpty() ? QJsonValue{} : arr.last();
}
bool is_empty(const QJsonArray& arr) {
    return arr.isEmpty();
}
int length(const QJsonArray& arr) {
    return arr.size();
}

double sum(const QJsonArray& arr, const QString& field) {
    double s = 0;
    for (const auto& v : arr)
        s += extract_double(v, field);
    return s;
}

double min(const QJsonArray& arr, const QString& field) {
    double m = std::numeric_limits<double>::max();
    for (const auto& v : arr)
        m = std::min(m, extract_double(v, field));
    return arr.isEmpty() ? 0.0 : m;
}

double max(const QJsonArray& arr, const QString& field) {
    double m = std::numeric_limits<double>::lowest();
    for (const auto& v : arr)
        m = std::max(m, extract_double(v, field));
    return arr.isEmpty() ? 0.0 : m;
}

double average(const QJsonArray& arr, const QString& field) {
    return arr.isEmpty() ? 0.0 : sum(arr, field) / arr.size();
}

QJsonArray unique(const QJsonArray& arr, const QString& field) {
    QJsonArray result;
    QSet<QString> seen;
    for (const auto& v : arr) {
        QString key = field.isEmpty() ? (v.isString() ? v.toString() : QString::number(v.toDouble()))
                                      : (v.isObject() ? v.toObject().value(field).toString() : v.toString());
        if (!seen.contains(key)) {
            seen.insert(key);
            result.append(v);
        }
    }
    return result;
}

QJsonArray pluck(const QJsonArray& arr, const QString& field) {
    QJsonArray result;
    for (const auto& v : arr) {
        if (v.isObject())
            result.append(v.toObject().value(field));
    }
    return result;
}

QJsonArray filter_by(const QJsonArray& arr, const QString& field, const QJsonValue& value) {
    QJsonArray result;
    for (const auto& v : arr) {
        if (v.isObject() && v.toObject().value(field) == value)
            result.append(v);
    }
    return result;
}

QJsonArray sort_by(const QJsonArray& arr, const QString& field, bool ascending) {
    QVector<QJsonValue> vec;
    for (const auto& v : arr)
        vec.append(v);
    std::sort(vec.begin(), vec.end(), [&](const QJsonValue& a, const QJsonValue& b) {
        double da = extract_double(a, field);
        double db = extract_double(b, field);
        return ascending ? da < db : da > db;
    });
    QJsonArray result;
    for (const auto& v : vec)
        result.append(v);
    return result;
}

QJsonArray returns(const QJsonArray& prices) {
    QJsonArray result;
    for (int i = 1; i < prices.size(); ++i) {
        double prev = prices[i - 1].toDouble();
        double curr = prices[i].toDouble();
        result.append(prev != 0.0 ? (curr - prev) / prev : 0.0);
    }
    return result;
}

double volatility(const QJsonArray& prices) {
    QJsonArray rets = returns(prices);
    if (rets.size() < 2)
        return 0.0;
    double mean = average(rets, {});
    double sum_sq = 0;
    for (const auto& r : rets) {
        double diff = r.toDouble() - mean;
        sum_sq += diff * diff;
    }
    return std::sqrt(sum_sq / (rets.size() - 1)) * std::sqrt(252.0); // annualized
}

} // namespace ArrayExt

// ── String Extensions ──────────────────────────────────────────────────

namespace StringExt {

bool is_empty(const QString& s) {
    return s.isEmpty();
}
int length(const QString& s) {
    return s.length();
}
QString to_lower(const QString& s) {
    return s.toLower();
}
QString to_upper(const QString& s) {
    return s.toUpper();
}
QStringList split(const QString& s, const QString& sep) {
    return s.split(sep);
}
QString replace(const QString& s, const QString& from, const QString& to) {
    QString r = s;
    r.replace(from, to);
    return r;
}
bool contains(const QString& s, const QString& sub) {
    return s.contains(sub);
}
bool starts_with(const QString& s, const QString& prefix) {
    return s.startsWith(prefix);
}
bool ends_with(const QString& s, const QString& suffix) {
    return s.endsWith(suffix);
}
QString trim(const QString& s) {
    return s.trimmed();
}

bool is_ticker(const QString& s) {
    if (s.isEmpty() || s.length() > 10)
        return false;
    static QRegularExpression re("^[A-Z]{1,5}(\\.[A-Z]{1,2})?$");
    return re.match(s.toUpper()).hasMatch();
}

QString to_symbol(const QString& s) {
    return s.toUpper().trimmed().replace(" ", "");
}

} // namespace StringExt

// ── Number Extensions ──────────────────────────────────────────────────

namespace NumberExt {

double round(double v, int decimals) {
    double factor = std::pow(10.0, decimals);
    return std::round(v * factor) / factor;
}

double floor(double v) {
    return std::floor(v);
}
double ceil(double v) {
    return std::ceil(v);
}
double abs(double v) {
    return std::abs(v);
}

double clamp(double v, double lo, double hi) {
    return std::max(lo, std::min(hi, v));
}

QString format_currency(double v, const QString& currency) {
    QString sign = v < 0 ? "-" : "";
    double abs_v = std::abs(v);
    if (currency == "USD" || currency == "$")
        return sign + "$" + QString::number(abs_v, 'f', 2);
    if (currency == "EUR")
        return sign + QString::number(abs_v, 'f', 2) + " EUR";
    if (currency == "INR")
        return sign + "Rs." + QString::number(abs_v, 'f', 2);
    return sign + QString::number(abs_v, 'f', 2) + " " + currency;
}

QString format_percent(double v, int decimals) {
    return QString::number(v * 100.0, 'f', decimals) + "%";
}

double calculate_return(double old_val, double new_val) {
    return old_val != 0.0 ? (new_val - old_val) / old_val : 0.0;
}

} // namespace NumberExt

// ── Object Extensions ──────────────────────────────────────────────────

namespace ObjectExt {

QStringList keys(const QJsonObject& obj) {
    return obj.keys();
}

QJsonArray values(const QJsonObject& obj) {
    QJsonArray arr;
    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it)
        arr.append(it.value());
    return arr;
}

bool is_empty(const QJsonObject& obj) {
    return obj.isEmpty();
}
bool has_field(const QJsonObject& obj, const QString& field) {
    return obj.contains(field);
}

QJsonObject pick(const QJsonObject& obj, const QStringList& fields) {
    QJsonObject result;
    for (const auto& f : fields) {
        if (obj.contains(f))
            result[f] = obj.value(f);
    }
    return result;
}

QJsonObject omit(const QJsonObject& obj, const QStringList& fields) {
    QJsonObject result = obj;
    for (const auto& f : fields)
        result.remove(f);
    return result;
}

QJsonObject merge(const QJsonObject& a, const QJsonObject& b) {
    QJsonObject result = a;
    for (auto it = b.constBegin(); it != b.constEnd(); ++it)
        result[it.key()] = it.value();
    return result;
}

} // namespace ObjectExt

// ── Extension Dispatcher ───────────────────────────────────────────────

QJsonValue call_extension(const QString& method, const QJsonValue& target, const QJsonValue& arg) {
    // Array methods
    if (target.isArray()) {
        QJsonArray arr = target.toArray();
        QString field = arg.toString();
        if (method == "first")
            return ArrayExt::first(arr);
        if (method == "last")
            return ArrayExt::last(arr);
        if (method == "isEmpty")
            return ArrayExt::is_empty(arr);
        if (method == "length")
            return ArrayExt::length(arr);
        if (method == "sum")
            return ArrayExt::sum(arr, field);
        if (method == "min")
            return ArrayExt::min(arr, field);
        if (method == "max")
            return ArrayExt::max(arr, field);
        if (method == "average")
            return ArrayExt::average(arr, field);
        if (method == "returns")
            return QJsonValue(ArrayExt::returns(arr));
        if (method == "volatility")
            return ArrayExt::volatility(arr);
        if (method == "unique")
            return QJsonValue(ArrayExt::unique(arr, field));
        if (method == "pluck")
            return QJsonValue(ArrayExt::pluck(arr, field));
    }

    // String methods
    if (target.isString()) {
        QString s = target.toString();
        if (method == "isEmpty")
            return StringExt::is_empty(s);
        if (method == "length")
            return StringExt::length(s);
        if (method == "toLower")
            return StringExt::to_lower(s);
        if (method == "toUpper")
            return StringExt::to_upper(s);
        if (method == "trim")
            return StringExt::trim(s);
        if (method == "isTicker")
            return StringExt::is_ticker(s);
        if (method == "toSymbol")
            return StringExt::to_symbol(s);
        if (method == "contains")
            return StringExt::contains(s, arg.toString());
        if (method == "startsWith")
            return StringExt::starts_with(s, arg.toString());
        if (method == "endsWith")
            return StringExt::ends_with(s, arg.toString());
    }

    // Number methods
    if (target.isDouble()) {
        double v = target.toDouble();
        if (method == "round")
            return NumberExt::round(v, arg.toInt(2));
        if (method == "floor")
            return NumberExt::floor(v);
        if (method == "ceil")
            return NumberExt::ceil(v);
        if (method == "abs")
            return NumberExt::abs(v);
        if (method == "formatCurrency")
            return NumberExt::format_currency(v, arg.toString("USD"));
        if (method == "formatPercent")
            return NumberExt::format_percent(v, arg.toInt(2));
    }

    // Object methods
    if (target.isObject()) {
        QJsonObject obj = target.toObject();
        if (method == "keys")
            return QJsonValue(QJsonArray::fromStringList(ObjectExt::keys(obj)));
        if (method == "values")
            return QJsonValue(ObjectExt::values(obj));
        if (method == "isEmpty")
            return ObjectExt::is_empty(obj);
        if (method == "hasField")
            return ObjectExt::has_field(obj, arg.toString());
    }

    return {}; // unknown method
}

} // namespace fincept::workflow
