// src/screens/gov_data/GovDataShape.cpp
// Pure (QtCore only): see GovDataShape.h.
#include "screens/gov_data/GovDataShape.h"

#include <QStringList>
#include <QStringView>

#include <cmath>

namespace fincept::screens::gov {

QString first_string(const QJsonObject& o, std::initializer_list<const char*> keys) {
    for (const char* k : keys) {
        const QString s = text(o.value(QLatin1String(k)));
        if (!s.isEmpty())
            return s;
    }
    return {};
}

QString request_id(const QString& prefix, quint64 run) {
    return prefix + QLatin1Char('#') + QString::number(run);
}

quint64 run_of(const QString& request_id, const QString& prefix) {
    if (request_id.size() <= prefix.size() + 1 || !request_id.startsWith(prefix) ||
        request_id.at(prefix.size()) != QLatin1Char('#'))
        return 0;
    bool ok = false;
    const quint64 run = QStringView(request_id).mid(prefix.size() + 1).toULongLong(&ok);
    return ok ? run : 0;
}

QString plain_error(const QString& raw) {
    QString s = raw.trimmed();
    const qsizetype tb = s.indexOf(QLatin1String("Traceback"));
    if (tb >= 0)
        s = s.left(tb).trimmed();
    const qsizetype nl = s.indexOf(QLatin1Char('\n'));
    if (nl > 0)
        s = s.left(nl).trimmed();
    if (s.size() > 300)
        s = s.left(300) + QStringLiteral("…");
    return s;
}

QString text(const QJsonValue& v) {
    if (v.isString())
        return v.toString();
    if (v.isDouble()) {
        const double d = v.toDouble();
        if (std::isfinite(d) && d == std::floor(d) && std::fabs(d) < 1e15)
            return QString::number(static_cast<qint64>(d));
        return QString::number(d, 'g', 15);
    }
    if (v.isBool())
        return v.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    return {};
}

int integer(const QJsonValue& v, int fallback) {
    if (v.isDouble())
        return v.toInt(fallback);
    if (v.isString()) {
        bool ok = false;
        const int n = v.toString().trimmed().toInt(&ok);
        if (ok)
            return n;
        const double d = v.toString().trimmed().toDouble(&ok);
        if (ok && std::isfinite(d))
            return static_cast<int>(d);
    }
    return fallback;
}

QJsonArray list_rows(const QJsonObject& data, int* total) {
    QJsonArray rows;
    int count = -1;
    const QJsonValue raw = data.value(QStringLiteral("data"));
    if (raw.isArray()) {
        rows = raw.toArray();
    } else if (raw.isObject()) {
        const QJsonObject o = raw.toObject();
        for (const char* key : {"datasets", "results", "result"}) {
            const QJsonValue v = o.value(QLatin1String(key));
            if (v.isArray()) {
                rows = v.toArray();
                break;
            }
        }
        count = integer(o.value(QStringLiteral("count")), -1);
    }
    const int meta = integer(data.value(QStringLiteral("metadata")).toObject().value(QStringLiteral("total_count")), -1);
    if (meta > 0)
        count = meta;
    if (count < rows.size())
        count = int(rows.size());
    if (total)
        *total = count;
    return rows;
}

QJsonArray compact_resources(const QJsonArray& rows) {
    QJsonArray out;
    for (const QJsonValue& v : rows) {
        const QJsonObject o = v.toObject();
        QString name = first_string(o, {"name"});
        if (name.isEmpty())
            name = o.value(QStringLiteral("description")).toString().left(60);
        if (name.isEmpty())
            name = first_string(o, {"id"});
        QJsonObject r;
        r.insert(QStringLiteral("name"), name);
        r.insert(QStringLiteral("format"), o.value(QStringLiteral("format")).toString().toUpper());
        const QJsonValue size = o.value(QStringLiteral("size"));
        r.insert(QStringLiteral("size"), size.isDouble() ? size.toDouble() : size.toString().toDouble());
        r.insert(QStringLiteral("modified"), first_string(o, {"last_modified", "metadata_modified", "created"}).left(10));
        r.insert(QStringLiteral("url"), o.value(QStringLiteral("url")).toString());
        out.append(r);
    }
    return out;
}

QJsonArray compact_datasets(const QJsonArray& rows, bool keep_resources) {
    QJsonArray out;
    for (const QJsonValue& v : rows) {
        QJsonObject r;
        if (!v.isObject()) { // a bare name (HK datasets_list)
            const QString name = text(v);
            if (name.isEmpty())
                continue;
            r.insert(QStringLiteral("title"), name);
            r.insert(QStringLiteral("id"), name);
            r.insert(QStringLiteral("files"), -1);
            out.append(r);
            continue;
        }
        const QJsonObject o = v.toObject();
        QString title = first_string(o, {"title", "original_title", "name", "id"});
        const QString id = first_string(o, {"id", "name"});
        int files = -1;
        for (const char* k : {"num_resources", "resource_count", "resources_count"}) {
            files = integer(o.value(QLatin1String(k)), -1);
            if (files >= 0)
                break;
        }
        const QJsonArray resources = o.value(QStringLiteral("resources")).toArray();
        if (files <= 0 && !resources.isEmpty())
            files = int(resources.size());
        QStringList tags;
        for (const QJsonValue& t : o.value(QStringLiteral("tags")).toArray()) {
            const QString tag = t.isString() ? t.toString()
                                             : first_string(t.toObject(), {"display_name", "name"});
            if (!tag.isEmpty())
                tags << tag;
            if (tags.size() >= 8)
                break;
        }
        QString org;
        const QJsonValue org_v = o.value(QStringLiteral("organization"));
        if (org_v.isString())
            org = org_v.toString();
        else if (org_v.isObject())
            org = first_string(org_v.toObject(), {"title", "name"});
        QString author = o.value(QStringLiteral("author")).toString();
        if (author.isEmpty())
            author = org;

        r.insert(QStringLiteral("title"), title);
        // The title as published is shown; a full English translation (french_gov_api.py "title_en")
        // goes in the tooltip.
        const QString title_en = first_string(o, {"title_en"});
        if (!title_en.isEmpty() && title_en != title)
            r.insert(QStringLiteral("title_en"), title_en);
        r.insert(QStringLiteral("id"), id);
        r.insert(QStringLiteral("files"), files);
        r.insert(QStringLiteral("modified"),
                 first_string(o, {"metadata_modified", "modified", "last_modified", "updated_at"}).left(10));
        r.insert(QStringLiteral("tags"), QJsonArray::fromStringList(tags));
        r.insert(QStringLiteral("org"), org);
        r.insert(QStringLiteral("author"), author);
        r.insert(QStringLiteral("license"), first_string(o, {"license", "license_title", "license_id"}));
        const QString desc = first_string(o, {"notes", "description"});
        if (!desc.isEmpty())
            r.insert(QStringLiteral("desc"), desc.left(400));
        if (keep_resources)
            r.insert(QStringLiteral("resources"), compact_resources(resources));
        out.append(r);
    }
    return out;
}

QJsonObject list_payload(const QString& kind, const QJsonArray& rows, int total) {
    return QJsonObject{{QStringLiteral("kind"), kind},
                       {QStringLiteral("rows"), rows},
                       {QStringLiteral("total"), total < int(rows.size()) ? int(rows.size()) : total}};
}

} // namespace fincept::screens::gov
