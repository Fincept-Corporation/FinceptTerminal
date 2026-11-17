// TabularData.cpp — see TabularData.h.

#include "services/data_sources/TabularData.h"

#include <QDate>
#include <QHash>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <utility>

namespace fincept::services::datasources {

namespace {

bool td_is_iso_date(const QString& s) {
    static const QRegularExpression kDate(QStringLiteral("^\\d{4}-\\d{2}-\\d{2}$"));
    return s.size() == 10 && kDate.match(s).hasMatch() && QDate::fromString(s, Qt::ISODate).isValid();
}

bool td_is_iso_datetime(const QString& s) {
    static const QRegularExpression kDateTime(
        QStringLiteral("^\\d{4}-\\d{2}-\\d{2}[T ]\\d{2}:\\d{2}(:\\d{2}(\\.\\d+)?)?(Z|[+-]\\d{2}:?\\d{2})?$"));
    return s.size() >= 16 && kDateTime.match(s).hasMatch();
}

ColumnType td_merge(ColumnType a, ColumnType b) {
    if (a == ColumnType::Unknown)
        return b;
    if (b == ColumnType::Unknown || a == b)
        return a;
    if ((a == ColumnType::Integer && b == ColumnType::Number) || (a == ColumnType::Number && b == ColumnType::Integer))
        return ColumnType::Number;
    if ((a == ColumnType::Date && b == ColumnType::DateTime) || (a == ColumnType::DateTime && b == ColumnType::Date))
        return ColumnType::DateTime;
    if (a == ColumnType::Object || b == ColumnType::Object)
        return ColumnType::Object;
    return ColumnType::String;
}

QString td_csv_field(const QString& s) {
    if (s.contains(QLatin1Char(',')) || s.contains(QLatin1Char('"')) || s.contains(QLatin1Char('\n')) ||
        s.contains(QLatin1Char('\r')) || s.startsWith(QLatin1Char(' ')) || s.endsWith(QLatin1Char(' '))) {
        QString q = s;
        q.replace(QLatin1Char('"'), QStringLiteral("\"\""));
        return QLatin1Char('"') + q + QLatin1Char('"');
    }
    return s;
}

QString td_number_text(double d) {
    if (!std::isfinite(d))
        return QString();
    if (d == std::floor(d) && std::abs(d) < 1e15)
        return QString::number(static_cast<qint64>(d));
    return QString::number(d, 'g', 15);
}

} // namespace

QString column_type_name(ColumnType t) {
    switch (t) {
        case ColumnType::String:
            return QStringLiteral("string");
        case ColumnType::Integer:
            return QStringLiteral("integer");
        case ColumnType::Number:
            return QStringLiteral("number");
        case ColumnType::Bool:
            return QStringLiteral("bool");
        case ColumnType::Date:
            return QStringLiteral("date");
        case ColumnType::DateTime:
            return QStringLiteral("datetime");
        case ColumnType::Object:
            return QStringLiteral("object");
        case ColumnType::Unknown:
            break;
    }
    return QString();
}

ColumnType column_type_from_name(const QString& name) {
    const QString n = name.trimmed().toLower();
    if (n == QLatin1String("string") || n == QLatin1String("text") || n == QLatin1String("str"))
        return ColumnType::String;
    if (n == QLatin1String("integer") || n == QLatin1String("int") || n == QLatin1String("int64"))
        return ColumnType::Integer;
    if (n == QLatin1String("number") || n == QLatin1String("float") || n == QLatin1String("double") ||
        n == QLatin1String("decimal") || n == QLatin1String("real"))
        return ColumnType::Number;
    if (n == QLatin1String("bool") || n == QLatin1String("boolean"))
        return ColumnType::Bool;
    if (n == QLatin1String("date"))
        return ColumnType::Date;
    if (n == QLatin1String("datetime") || n == QLatin1String("timestamp"))
        return ColumnType::DateTime;
    if (n == QLatin1String("object") || n == QLatin1String("array") || n == QLatin1String("json"))
        return ColumnType::Object;
    return ColumnType::Unknown;
}

ColumnType cell_type(const QJsonValue& v) {
    switch (v.type()) {
        case QJsonValue::Bool:
            return ColumnType::Bool;
        case QJsonValue::Double: {
            const double d = v.toDouble();
            return (std::isfinite(d) && d == std::floor(d) && std::abs(d) < 9.007199254740992e15) ? ColumnType::Integer
                                                                                                 : ColumnType::Number;
        }
        case QJsonValue::String: {
            const QString s = v.toString();
            if (td_is_iso_date(s))
                return ColumnType::Date;
            if (td_is_iso_datetime(s))
                return ColumnType::DateTime;
            return ColumnType::String;
        }
        case QJsonValue::Array:
        case QJsonValue::Object:
            return ColumnType::Object;
        case QJsonValue::Null:
        case QJsonValue::Undefined:
            break;
    }
    return ColumnType::Unknown;
}

int DataTable::column_index(const QString& column) const {
    const int i = int(columns.indexOf(column));
    if (i >= 0)
        return i;
    for (int c = 0; c < columns.size(); ++c)
        if (columns[c].compare(column, Qt::CaseInsensitive) == 0)
            return c;
    return -1;
}

QJsonValue DataTable::cell(int row, int column) const {
    if (row < 0 || row >= rows.size())
        return QJsonValue(QJsonValue::Undefined);
    const QJsonArray& r = rows[row];
    if (column < 0 || column >= r.size())
        return QJsonValue(QJsonValue::Undefined);
    return r.at(column);
}

int TabularResult::index_of(const QString& table_name) const {
    for (int i = 0; i < tables.size(); ++i)
        if (tables[i].name == table_name)
            return i;
    return -1;
}

QStringList TabularResult::table_names() const {
    QStringList out;
    for (const DataTable& t : tables)
        out << t.name;
    return out;
}

int TabularResult::total_rows() const {
    int n = 0;
    for (const DataTable& t : tables)
        n += t.row_count();
    return n;
}

bool TabularResult::is_empty() const {
    for (const DataTable& t : tables)
        if (!t.is_empty())
            return false;
    return true;
}

DataTable table_from_json_array(const QJsonArray& rows, const QString& name, int scan_rows, int max_columns) {
    DataTable t;
    t.name = name;
    if (rows.isEmpty())
        return t;
    const int n = std::min<int>(int(rows.size()), std::max(1, scan_rows));
    int objects = 0, arrays = 0;
    for (int i = 0; i < n; ++i) {
        if (rows.at(i).isObject())
            ++objects;
        else if (rows.at(i).isArray())
            ++arrays;
    }
    if (objects * 2 > n) {
        QSet<QString> seen;
        for (int i = 0; i < n && t.columns.size() < max_columns; ++i) {
            if (!rows.at(i).isObject())
                continue;
            const QJsonObject o = rows.at(i).toObject();
            for (auto it = o.constBegin(); it != o.constEnd() && t.columns.size() < max_columns; ++it) {
                if (!seen.contains(it.key())) {
                    seen.insert(it.key());
                    t.columns << it.key();
                }
            }
        }
        bool has_scalar = false;
        for (int i = 0; i < n; ++i)
            has_scalar = has_scalar || !rows.at(i).isObject();
        if (has_scalar && !seen.contains(QStringLiteral("value")))
            t.columns << QStringLiteral("value");
        const int value_col = has_scalar ? int(t.columns.indexOf(QStringLiteral("value"))) : -1;
        t.rows.reserve(rows.size());
        for (const QJsonValue& v : rows) {
            QJsonArray r;
            if (v.isObject()) {
                const QJsonObject o = v.toObject();
                for (const QString& c : std::as_const(t.columns))
                    r.append(o.contains(c) ? o.value(c) : QJsonValue(QJsonValue::Null));
            } else {
                for (int c = 0; c < t.columns.size(); ++c)
                    r.append(c == value_col ? v : QJsonValue(QJsonValue::Null));
            }
            t.rows.append(r);
        }
    } else if (arrays * 2 > n) {
        int width = 0;
        for (int i = 0; i < n; ++i)
            width = std::max<int>(width, int(rows.at(i).toArray().size()));
        width = std::min(width, max_columns);
        for (int c = 0; c < width; ++c)
            t.columns << QString::number(c);
        t.rows.reserve(rows.size());
        for (const QJsonValue& v : rows) {
            const QJsonArray a = v.toArray();
            QJsonArray r;
            for (int c = 0; c < width; ++c)
                r.append(c < a.size() ? a.at(c) : QJsonValue(QJsonValue::Null));
            t.rows.append(r);
        }
    } else {
        t.columns << QStringLiteral("value");
        t.rows.reserve(rows.size());
        for (const QJsonValue& v : rows)
            t.rows.append(QJsonArray{v});
    }
    t.types = QVector<ColumnType>(t.columns.size(), ColumnType::Unknown);
    infer_column_types(t, scan_rows);
    return t;
}

DataTable table_from_records(const QJsonArray& rows, const QStringList& columns, const QJsonObject& types,
                             const QString& name) {
    if (columns.isEmpty())
        return table_from_json_array(rows, name);
    DataTable t;
    t.name = name;
    t.columns = columns;
    t.types.reserve(columns.size());
    for (const QString& c : columns)
        t.types << column_type_from_name(types.value(c).toString());
    t.rows.reserve(rows.size());
    for (const QJsonValue& v : rows) {
        QJsonArray r;
        if (v.isObject()) {
            const QJsonObject o = v.toObject();
            for (const QString& c : columns)
                r.append(o.contains(c) ? o.value(c) : QJsonValue(QJsonValue::Null));
        } else if (v.isArray()) {
            const QJsonArray a = v.toArray();
            for (int c = 0; c < columns.size(); ++c)
                r.append(c < a.size() ? a.at(c) : QJsonValue(QJsonValue::Null));
        } else {
            r.append(v);
            for (int c = 1; c < columns.size(); ++c)
                r.append(QJsonValue(QJsonValue::Null));
        }
        t.rows.append(r);
    }
    infer_column_types(t);
    return t;
}

DataTable table_from_key_values(const QJsonObject& object, const QString& name) {
    DataTable t;
    t.name = name;
    t.columns = {QStringLiteral("key"), QStringLiteral("value")};
    t.types = {ColumnType::String, ColumnType::Unknown};
    for (auto it = object.constBegin(); it != object.constEnd(); ++it)
        t.rows.append(QJsonArray{it.key(), it.value()});
    infer_column_types(t);
    return t;
}

void infer_column_types(DataTable& table, int scan_rows) {
    if (table.types.size() != table.columns.size())
        table.types.resize(table.columns.size());
    const int n = std::min<int>(table.row_count(), std::max(1, scan_rows));
    for (int c = 0; c < table.columns.size(); ++c) {
        if (table.types[c] != ColumnType::Unknown)
            continue;
        ColumnType t = ColumnType::Unknown;
        for (int r = 0; r < n; ++r) {
            t = td_merge(t, cell_type(table.rows[r].at(c)));
            if (t == ColumnType::String || t == ColumnType::Object)
                break;
        }
        table.types[c] = t;
    }
}

QJsonObject table_to_json(const DataTable& table) {
    QJsonArray types;
    for (ColumnType t : table.types)
        types.append(column_type_name(t));
    QJsonArray rows;
    for (const QJsonArray& r : table.rows)
        rows.append(r);
    return QJsonObject{{"name", table.name},
                       {"columns", QJsonArray::fromStringList(table.columns)},
                       {"types", types},
                       {"rows", rows}};
}

DataTable table_from_json(const QJsonObject& obj) {
    DataTable t;
    t.name = obj.value(QStringLiteral("name")).toString();
    for (const QJsonValue& c : obj.value(QStringLiteral("columns")).toArray())
        t.columns << c.toString();
    const QJsonArray types = obj.value(QStringLiteral("types")).toArray();
    for (int i = 0; i < t.columns.size(); ++i)
        t.types << column_type_from_name(types.at(i).toString());
    const QJsonArray rows = obj.value(QStringLiteral("rows")).toArray();
    t.rows.reserve(rows.size());
    for (const QJsonValue& r : rows) {
        QJsonArray a = r.toArray();
        while (a.size() < t.columns.size())
            a.append(QJsonValue(QJsonValue::Null));
        t.rows.append(a);
    }
    return t;
}

QJsonObject result_to_json(const TabularResult& result) {
    QJsonArray tables;
    for (const DataTable& t : result.tables)
        tables.append(table_to_json(t));
    QJsonObject o{{"tables", tables},
                  {"source", result.source},
                  {"truncated", result.truncated},
                  {"meta", result.meta}};
    if (result.as_of.isValid())
        o.insert(QStringLiteral("as_of"), result.as_of.toUTC().toString(Qt::ISODate));
    if (!result.next.isEmpty())
        o.insert(QStringLiteral("next"), result.next);
    return o;
}

TabularResult result_from_json(const QJsonObject& obj) {
    TabularResult r;
    for (const QJsonValue& t : obj.value(QStringLiteral("tables")).toArray())
        r.tables << table_from_json(t.toObject());
    r.source = obj.value(QStringLiteral("source")).toString();
    r.as_of = QDateTime::fromString(obj.value(QStringLiteral("as_of")).toString(), Qt::ISODate);
    r.truncated = obj.value(QStringLiteral("truncated")).toBool();
    r.next = obj.value(QStringLiteral("next")).toString();
    r.meta = obj.value(QStringLiteral("meta")).toObject();
    return r;
}

QString cell_text(const QJsonValue& v) {
    switch (v.type()) {
        case QJsonValue::Null:
        case QJsonValue::Undefined:
            return QString();
        case QJsonValue::Bool:
            return v.toBool() ? QStringLiteral("true") : QStringLiteral("false");
        case QJsonValue::Double:
            return td_number_text(v.toDouble());
        case QJsonValue::String:
            return v.toString();
        case QJsonValue::Array:
            return QString::fromUtf8(QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact));
        case QJsonValue::Object:
            return QString::fromUtf8(QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact));
    }
    return QString();
}

QByteArray table_to_csv(const DataTable& table, int max_rows) {
    QByteArray out;
    QStringList header;
    for (const QString& c : table.columns)
        header << td_csv_field(c);
    out += header.join(QLatin1Char(',')).toUtf8() + "\r\n";
    const int n = max_rows < 0 ? table.row_count() : std::min(max_rows, table.row_count());
    for (int r = 0; r < n; ++r) {
        QStringList line;
        line.reserve(table.columns.size());
        for (int c = 0; c < table.columns.size(); ++c)
            line << td_csv_field(cell_text(table.rows[r].at(c)));
        out += line.join(QLatin1Char(',')).toUtf8() + "\r\n";
    }
    return out;
}

QJsonArray table_to_records(const DataTable& table) {
    QJsonArray out;
    for (const QJsonArray& r : table.rows) {
        QJsonObject o;
        for (int c = 0; c < table.columns.size(); ++c)
            o.insert(table.columns[c], r.at(c));
        out.append(o);
    }
    return out;
}

} // namespace fincept::services::datasources
