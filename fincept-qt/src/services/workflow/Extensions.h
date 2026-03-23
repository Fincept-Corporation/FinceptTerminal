#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>

namespace fincept::workflow {

/// Array extension functions for expression evaluation.
namespace ArrayExt {
    QJsonValue first(const QJsonArray& arr);
    QJsonValue last(const QJsonArray& arr);
    bool is_empty(const QJsonArray& arr);
    int length(const QJsonArray& arr);
    double sum(const QJsonArray& arr, const QString& field = {});
    double min(const QJsonArray& arr, const QString& field = {});
    double max(const QJsonArray& arr, const QString& field = {});
    double average(const QJsonArray& arr, const QString& field = {});
    QJsonArray unique(const QJsonArray& arr, const QString& field = {});
    QJsonArray pluck(const QJsonArray& arr, const QString& field);
    QJsonArray filter_by(const QJsonArray& arr, const QString& field, const QJsonValue& value);
    QJsonArray sort_by(const QJsonArray& arr, const QString& field, bool ascending = true);

    // Finance-specific
    QJsonArray returns(const QJsonArray& prices);
    double volatility(const QJsonArray& prices);
}

/// String extension functions.
namespace StringExt {
    bool is_empty(const QString& s);
    int length(const QString& s);
    QString to_lower(const QString& s);
    QString to_upper(const QString& s);
    QStringList split(const QString& s, const QString& sep);
    QString replace(const QString& s, const QString& from, const QString& to);
    bool contains(const QString& s, const QString& sub);
    bool starts_with(const QString& s, const QString& prefix);
    bool ends_with(const QString& s, const QString& suffix);
    QString trim(const QString& s);

    // Finance-specific
    bool is_ticker(const QString& s);
    QString to_symbol(const QString& s);
}

/// Number extension functions.
namespace NumberExt {
    double round(double v, int decimals = 2);
    double floor(double v);
    double ceil(double v);
    double abs(double v);
    double clamp(double v, double lo, double hi);
    QString format_currency(double v, const QString& currency = "USD");
    QString format_percent(double v, int decimals = 2);
    double calculate_return(double old_val, double new_val);
}

/// Object extension functions.
namespace ObjectExt {
    QStringList keys(const QJsonObject& obj);
    QJsonArray values(const QJsonObject& obj);
    bool is_empty(const QJsonObject& obj);
    bool has_field(const QJsonObject& obj, const QString& field);
    QJsonObject pick(const QJsonObject& obj, const QStringList& fields);
    QJsonObject omit(const QJsonObject& obj, const QStringList& fields);
    QJsonObject merge(const QJsonObject& a, const QJsonObject& b);
}

/// Dispatch an extension call from an expression like "$input.prices.sum()"
QJsonValue call_extension(const QString& method, const QJsonValue& target,
                          const QJsonValue& arg = {});

} // namespace fincept::workflow
