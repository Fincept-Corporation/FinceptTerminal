// MappingEngine.cpp — see MappingEngine.h.

#include "services/data_mapping/MappingEngine.h"

#include "services/data_sources/TableParsers.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QRegularExpression>
#include <QSet>
#include <QTimeZone>

#include <algorithm>
#include <cmath>

namespace fincept::services::datamapping {

namespace ds = fincept::services::datasources;

namespace {

QString me_tr(const char* text) {
    return QCoreApplication::translate("MappingEngine", text);
}

QString me_show(const QJsonValue& v) {
    QString t = ds::cell_text(v);
    if (t.size() > 40)
        t = t.left(40) + QStringLiteral("…");
    return QLatin1Char('\'') + t + QLatin1Char('\'');
}

bool me_is_null_token(const QString& text, const MappingOptions& o) {
    const QString t = text.trimmed();
    for (const QString& tok : o.null_tokens)
        if (t.compare(tok, Qt::CaseInsensitive) == 0)
            return true;
    return false;
}

bool me_has_time(const QString& fmt) {
    return fmt.contains(QLatin1Char('H')) || fmt.contains(QLatin1Char('h')) || fmt.contains(QLatin1Char('m'));
}

/// Concrete date format for an "auto" column: the first candidate that parses every sampled value.
/// When a day-first format and its month-first twin both fit (every day <= 12), the column is
/// ambiguous and nothing is guessed — each cell then says "choose a date format".
QString me_infer_date_format(const QStringList& samples) {
    if (samples.isEmpty())
        return QString();
    struct Candidate {
        const char* fmt;
        const char* twin; ///< the other day/month order, or nullptr
    };
    static const Candidate kCandidates[] = {
        {"iso", nullptr},
        {"d/M/yyyy", "M/d/yyyy"},
        {"M/d/yyyy", "d/M/yyyy"},
        {"d.M.yyyy", nullptr},
        {"d-M-yyyy", "M-d-yyyy"},
        {"M-d-yyyy", "d-M-yyyy"},
        {"yyyy/M/d", nullptr},
        {"yyyyMMdd", nullptr},
        {"d MMM yyyy", nullptr},
        {"MMM d, yyyy", nullptr},
        {"d-MMM-yyyy", nullptr},
        {"d/M/yy", "M/d/yy"},
        {"M/d/yy", "d/M/yy"},
        {"d/M/yyyy H:mm:ss", "M/d/yyyy H:mm:ss"},
        {"M/d/yyyy H:mm:ss", "d/M/yyyy H:mm:ss"},
        {"d/M/yyyy H:mm", "M/d/yyyy H:mm"},
        {"M/d/yyyy H:mm", "d/M/yyyy H:mm"},
        {"period", nullptr},
    };
    auto fits = [&samples](const QString& fmt) {
        for (const QString& s : samples) {
            bool ok = false;
            parse_datetime(QJsonValue(s), fmt, &ok);
            if (!ok)
                return false;
        }
        return true;
    };
    for (const Candidate& c : kCandidates) {
        const QString fmt = QString::fromLatin1(c.fmt);
        if (!fits(fmt))
            continue;
        if (c.twin && fits(QString::fromLatin1(c.twin)))
            return QString(); // ambiguous day/month order
        return fmt;
    }
    return QString();
}

QString me_period_from_date(const QDate& d, const QString& granularity) {
    if (granularity == QLatin1String("year"))
        return QString::number(d.year());
    if (granularity == QLatin1String("month"))
        return QStringLiteral("%1-%2").arg(d.year()).arg(d.month(), 2, 10, QLatin1Char('0'));
    if (granularity == QLatin1String("half"))
        return QStringLiteral("%1-H%2").arg(d.year()).arg(d.month() <= 6 ? 1 : 2);
    return QStringLiteral("%1-Q%2").arg(d.year()).arg((d.month() - 1) / 3 + 1);
}

QJsonValue me_apply_text_transform(const QJsonValue& v, const QString& transform) {
    const QString t = transform.trimmed().toLower();
    if (t.isEmpty())
        return v;
    if (v.isString()) {
        if (t == QLatin1String("upper"))
            return v.toString().toUpper();
        if (t == QLatin1String("lower"))
            return v.toString().toLower();
        if (t == QLatin1String("trim"))
            return v.toString().trimmed();
    }
    if (v.isDouble() && (t == QLatin1String("abs") || t == QLatin1String("abs_value")))
        return std::abs(v.toDouble());
    return v;
}

} // namespace

const CellIssue* MappingOutput::issue_at(int out_row, int column) const {
    const auto it = issue_index.constFind((qint64(out_row) << 20) | qint64(column));
    return it == issue_index.constEnd() ? nullptr : &issues[it.value()];
}

double parse_number_text(const QString& text, const QString& decimal, bool* ok) {
    *ok = false;
    QString t = text.trimmed();
    if (t.isEmpty())
        return 0;
    bool negative = false;
    if (t.startsWith(QLatin1Char('(')) && t.endsWith(QLatin1Char(')'))) {
        negative = true;
        t = t.mid(1, t.size() - 2).trimmed();
    }
    // Currency signs, percent, spaces (incl. no-break / thin spaces), apostrophes, underscores.
    static const QRegularExpression kStrip(QStringLiteral("[\\s\\x{00A0}\\x{202F}\\x{2009}$€£¥₹%'_]"));
    t.remove(kStrip);
    for (const char* code : {"USD", "EUR", "GBP", "JPY", "INR", "CHF", "CAD", "AUD", "CNY"}) {
        if (t.endsWith(QLatin1String(code), Qt::CaseInsensitive))
            t.chop(3);
        else if (t.startsWith(QLatin1String(code), Qt::CaseInsensitive))
            t = t.mid(3);
    }
    if (t.startsWith(QLatin1Char('+')))
        t = t.mid(1);
    if (t.startsWith(QLatin1Char('-'))) {
        negative = !negative;
        t = t.mid(1);
    }
    if (t.isEmpty())
        return 0;
    const int dots = int(t.count(QLatin1Char('.')));
    const int commas = int(t.count(QLatin1Char(',')));
    QString mode = decimal.toLower();
    if (mode != QLatin1String("dot") && mode != QLatin1String("comma")) {
        static const QRegularExpression kCommaThousands(QStringLiteral("^\\d{1,3}(,\\d{3})+(\\.\\d*)?$"));
        static const QRegularExpression kDotThousands(QStringLiteral("^\\d{1,3}(\\.\\d{3})+(,\\d*)?$"));
        if (dots > 0 && commas > 0)
            mode = t.lastIndexOf(QLatin1Char(',')) > t.lastIndexOf(QLatin1Char('.')) ? QStringLiteral("comma")
                                                                                     : QStringLiteral("dot");
        else if (commas > 0)
            mode = kCommaThousands.match(t).hasMatch() ? QStringLiteral("dot") : QStringLiteral("comma");
        else if (dots > 1)
            mode = kDotThousands.match(t).hasMatch() ? QStringLiteral("comma") : QStringLiteral("dot");
        else
            mode = QStringLiteral("dot");
    }
    if (mode == QLatin1String("comma")) {
        t.remove(QLatin1Char('.'));
        t.replace(QLatin1Char(','), QLatin1Char('.'));
    } else {
        t.remove(QLatin1Char(','));
    }
    static const QRegularExpression kPlain(QStringLiteral("^(\\d+\\.?\\d*|\\.\\d+)([eE][+-]?\\d{1,3})?$"));
    if (!kPlain.match(t).hasMatch())
        return 0;
    bool conv = false;
    double d = QLocale::c().toDouble(t, &conv);
    if (!conv || !std::isfinite(d))
        return 0;
    *ok = true;
    return negative ? -d : d;
}

bool parse_bool_text(const QString& text, bool* ok) {
    const QString t = text.trimmed().toLower();
    *ok = true;
    if (t == QLatin1String("true") || t == QLatin1String("yes") || t == QLatin1String("y") || t == QLatin1String("1") ||
        t == QLatin1String("on") || t == QLatin1String("t"))
        return true;
    if (t == QLatin1String("false") || t == QLatin1String("no") || t == QLatin1String("n") || t == QLatin1String("0") ||
        t == QLatin1String("off") || t == QLatin1String("f"))
        return false;
    *ok = false;
    return false;
}

Period parse_period(const QString& text) {
    Period p;
    const QString t = text.trimmed().toUpper();
    if (t.isEmpty())
        return p;
    QRegularExpressionMatch m;
    static const QRegularExpression kYear(QStringLiteral("^(?:FY\\s?)?(\\d{4})(?:\\s?FY)?$"));
    static const QRegularExpression kQuarterA(QStringLiteral("^(\\d{4})\\s?[-/ ]?\\s?Q([1-4])$"));
    static const QRegularExpression kQuarterB(QStringLiteral("^Q([1-4])\\s?[-/ ]?\\s?(\\d{4})$"));
    static const QRegularExpression kQuarterC(QStringLiteral("^(\\d{4})\\s?[-/ ]?([1-4])Q$"));
    static const QRegularExpression kHalf(QStringLiteral("^(\\d{4})\\s?[-/ ]?[HS]([12])$"));
    static const QRegularExpression kMonthA(QStringLiteral("^(\\d{4})[-/.](\\d{1,2})$"));
    static const QRegularExpression kMonthB(QStringLiteral("^(\\d{4})M(\\d{1,2})$"));
    static const QRegularExpression kMonthC(QStringLiteral("^(\\d{4})(\\d{2})$"));
    int year = 0, quarter = 0, half = 0, month = 0;
    if ((m = kYear.match(t)).hasMatch()) {
        year = m.captured(1).toInt();
    } else if ((m = kQuarterA.match(t)).hasMatch()) {
        year = m.captured(1).toInt();
        quarter = m.captured(2).toInt();
    } else if ((m = kQuarterB.match(t)).hasMatch()) {
        year = m.captured(2).toInt();
        quarter = m.captured(1).toInt();
    } else if ((m = kQuarterC.match(t)).hasMatch()) {
        year = m.captured(1).toInt();
        quarter = m.captured(2).toInt();
    } else if ((m = kHalf.match(t)).hasMatch()) {
        year = m.captured(1).toInt();
        half = m.captured(2).toInt();
    } else if ((m = kMonthA.match(t)).hasMatch() || (m = kMonthB.match(t)).hasMatch() ||
               (m = kMonthC.match(t)).hasMatch()) {
        year = m.captured(1).toInt();
        month = m.captured(2).toInt();
        if (month < 1 || month > 12)
            return p;
    } else {
        return p;
    }
    if (year < 1000 || year > 9999)
        return p;
    if (quarter > 0) {
        p.code = QStringLiteral("%1-Q%2").arg(year).arg(quarter);
        p.start = QDate(year, (quarter - 1) * 3 + 1, 1);
        p.end = p.start.addMonths(3).addDays(-1);
    } else if (half > 0) {
        p.code = QStringLiteral("%1-H%2").arg(year).arg(half);
        p.start = QDate(year, half == 1 ? 1 : 7, 1);
        p.end = p.start.addMonths(6).addDays(-1);
    } else if (month > 0) {
        p.code = QStringLiteral("%1-%2").arg(year).arg(month, 2, 10, QLatin1Char('0'));
        p.start = QDate(year, month, 1);
        p.end = p.start.addMonths(1).addDays(-1);
    } else {
        p.code = QString::number(year);
        p.start = QDate(year, 1, 1);
        p.end = QDate(year, 12, 31);
    }
    return p;
}

QDateTime parse_datetime(const QJsonValue& v, const QString& format_in, bool* ok, bool* is_date_only, QString* why) {
    *ok = false;
    if (is_date_only)
        *is_date_only = true;
    const QString fmt = format_in.trimmed();
    const QString f = fmt.toLower();
    auto fail = [&](const QString& reason) {
        if (why)
            *why = reason;
        return QDateTime();
    };
    auto from_epoch = [&](double d, bool ms) {
        if (!std::isfinite(d))
            return fail(me_tr("not a number"));
        const qint64 msec = ms ? qint64(std::llround(d)) : qint64(std::llround(d * 1000.0));
        const QDateTime dt = QDateTime::fromMSecsSinceEpoch(msec, QTimeZone::UTC);
        if (!dt.isValid() || dt.date().year() < 1900 || dt.date().year() > 2200)
            return fail(me_tr("out of range for an epoch timestamp"));
        *ok = true;
        if (is_date_only)
            *is_date_only = false;
        return dt;
    };

    if (v.isDouble()) {
        const double d = v.toDouble();
        if (f == QLatin1String("epoch_s"))
            return from_epoch(d, false);
        if (f == QLatin1String("epoch_ms"))
            return from_epoch(d, true);
        if (f == QLatin1String("epoch") || f.isEmpty() || f == QLatin1String("auto")) {
            if (f != QLatin1String("epoch") && d >= 1000 && d <= 9999 && d == std::floor(d)) {
                *ok = true;
                return QDateTime(QDate(int(d), 1, 1), QTime(0, 0), QTimeZone::UTC);
            }
            if (std::abs(d) >= 1e11)
                return from_epoch(d, true);
            if (std::abs(d) >= 1e8)
                return from_epoch(d, false);
            return fail(me_tr("a number that is neither a year nor an epoch timestamp"));
        }
        if (f == QLatin1String("year") && d >= 1000 && d <= 9999 && d == std::floor(d)) {
            *ok = true;
            return QDateTime(QDate(int(d), 1, 1), QTime(0, 0), QTimeZone::UTC);
        }
        if (f == QLatin1String("yyyymmdd")) {
            return parse_datetime(QJsonValue(QString::number(qint64(d))), QStringLiteral("yyyyMMdd"), ok, is_date_only,
                                  why);
        }
        return parse_datetime(QJsonValue(ds::cell_text(v)), fmt, ok, is_date_only, why);
    }
    if (!v.isString())
        return fail(me_tr("not a date value"));
    const QString s = v.toString().trimmed();
    if (s.isEmpty())
        return fail(me_tr("empty"));

    if (f == QLatin1String("epoch_s") || f == QLatin1String("epoch_ms") || f == QLatin1String("epoch")) {
        bool num_ok = false;
        const double d = QLocale::c().toDouble(s, &num_ok);
        if (!num_ok)
            return fail(me_tr("not an epoch number"));
        return parse_datetime(QJsonValue(d), fmt, ok, is_date_only, why);
    }
    if (f == QLatin1String("year") || f == QLatin1String("quarter") || f == QLatin1String("month") ||
        f == QLatin1String("period") || f == QLatin1String("period_start") || f == QLatin1String("period_end")) {
        const Period p = parse_period(s);
        if (!p.valid())
            return fail(me_tr("not a period such as 2024, 2024-Q1 or 2024-03"));
        *ok = true;
        return QDateTime(f == QLatin1String("period_end") ? p.end : p.start, QTime(0, 0), QTimeZone::UTC);
    }
    if (f == QLatin1String("iso") || f.isEmpty() || f == QLatin1String("auto")) {
        QDateTime dt = QDateTime::fromString(s, Qt::ISODateWithMs);
        if (!dt.isValid())
            dt = QDateTime::fromString(s, Qt::ISODate);
        if (!dt.isValid() && s.size() >= 19 && s.at(10) == QLatin1Char(' ')) {
            QString t = s;
            t[10] = QLatin1Char('T');
            dt = QDateTime::fromString(t, Qt::ISODateWithMs);
        }
        if (dt.isValid()) {
            if (s.size() > 10) {
                if (is_date_only)
                    *is_date_only = false;
                if (dt.timeSpec() == Qt::LocalTime)
                    dt.setTimeZone(QTimeZone::UTC); // a naive timestamp is taken as UTC, never machine-local
                *ok = true;
                return dt.toUTC();
            }
            *ok = true;
            return QDateTime(dt.date(), QTime(0, 0), QTimeZone::UTC);
        }
        const QDate d = QDate::fromString(s.left(10), Qt::ISODate);
        if (d.isValid() && s.size() == 10) {
            *ok = true;
            return QDateTime(d, QTime(0, 0), QTimeZone::UTC);
        }
        if (f == QLatin1String("iso"))
            return fail(me_tr("not an ISO date (YYYY-MM-DD)"));
        // auto: a period code, then the unambiguous day/month forms.
        const Period p = parse_period(s);
        if (p.valid()) {
            *ok = true;
            return QDateTime(p.start, QTime(0, 0), QTimeZone::UTC);
        }
        static const QRegularExpression kDmy(QStringLiteral("^(\\d{1,2})[/.-](\\d{1,2})[/.-](\\d{4})$"));
        const QRegularExpressionMatch m = kDmy.match(s);
        if (m.hasMatch()) {
            const int a = m.captured(1).toInt(), b = m.captured(2).toInt(), y = m.captured(3).toInt();
            if (a > 12 && b <= 12) {
                const QDate dd(y, b, a);
                if (dd.isValid()) {
                    *ok = true;
                    return QDateTime(dd, QTime(0, 0), QTimeZone::UTC);
                }
            } else if (b > 12 && a <= 12) {
                const QDate dd(y, a, b);
                if (dd.isValid()) {
                    *ok = true;
                    return QDateTime(dd, QTime(0, 0), QTimeZone::UTC);
                }
            }
            return fail(me_tr("ambiguous day/month order — choose a date format (dd/MM/yyyy or MM/dd/yyyy)"));
        }
        const QDateTime rfc = QDateTime::fromString(s, Qt::RFC2822Date);
        if (rfc.isValid()) {
            if (is_date_only)
                *is_date_only = false;
            *ok = true;
            return rfc.toUTC();
        }
        return fail(me_tr("not a recognisable date — choose a date format"));
    }
    // An explicit Qt format ("dd.MM.yyyy", "MM/dd/yyyy HH:mm"), read with C-locale month names.
    QDateTime dt = QLocale::c().toDateTime(s, fmt);
    if (!dt.isValid()) {
        const QDate d = QLocale::c().toDate(s, fmt);
        if (d.isValid())
            dt = QDateTime(d, QTime(0, 0), QTimeZone::UTC);
    }
    if (!dt.isValid())
        return fail(me_tr("does not match the date format %1").arg(fmt));
    // Two-digit years: 00-69 → 2000s, 70-99 → 1900s (Qt reads "yy" as 19xx).
    if (fmt.count(QLatin1Char('y')) == 2 && dt.date().year() < 1970)
        dt = dt.addYears(100);
    dt.setTimeZone(QTimeZone::UTC);
    if (is_date_only)
        *is_date_only = !me_has_time(fmt);
    *ok = true;
    return dt;
}

CastResult cast_cell(const QJsonValue& input, const FieldMapping& field, const MappingOptions& options) {
    CastResult r;
    QJsonValue v = input;
    if (v.isString() && options.trim_text)
        v = v.toString().trimmed();
    if (v.isUndefined() || v.isNull() || (v.isString() && me_is_null_token(v.toString(), options))) {
        r.empty = true;
        r.value = QJsonValue(QJsonValue::Null);
        return r;
    }
    v = me_apply_text_transform(v, field.transform);
    auto bad = [&](const QString& why) {
        r.value = QJsonValue(QJsonValue::Null);
        r.error = why;
        return r;
    };

    switch (field.type) {
        case TargetType::String:
            r.value = v.isString() ? v : QJsonValue(ds::cell_text(v));
            return r;
        case TargetType::Json: {
            if (v.isString()) {
                const QJsonDocument doc = QJsonDocument::fromJson(v.toString().toUtf8());
                if (doc.isObject())
                    r.value = doc.object();
                else if (doc.isArray())
                    r.value = doc.array();
                else
                    r.value = v;
            } else {
                r.value = v;
            }
            return r;
        }
        case TargetType::Bool: {
            if (v.isBool()) {
                r.value = v;
                return r;
            }
            if (v.isDouble()) {
                const double d = v.toDouble();
                if (d == 0.0 || d == 1.0) {
                    r.value = d == 1.0;
                    return r;
                }
                return bad(me_tr("%1 is not a yes/no value").arg(me_show(v)));
            }
            bool ok = false;
            const bool b = parse_bool_text(ds::cell_text(v), &ok);
            if (!ok)
                return bad(me_tr("%1 is not a yes/no value").arg(me_show(v)));
            r.value = b;
            return r;
        }
        case TargetType::Integer:
        case TargetType::Number: {
            double d = 0;
            if (v.isDouble()) {
                d = v.toDouble();
            } else if (v.isBool()) {
                d = v.toBool() ? 1 : 0;
            } else if (v.isString()) {
                bool ok = false;
                d = parse_number_text(v.toString(), options.decimal, &ok);
                if (!ok)
                    return bad(me_tr("%1 is not a number").arg(me_show(v)));
            } else {
                return bad(me_tr("an object or list is not a number"));
            }
            d *= field.scale;
            if (!std::isfinite(d))
                return bad(me_tr("%1 is out of range").arg(me_show(v)));
            if (field.type == TargetType::Integer) {
                const double rounded = std::round(d);
                if (std::abs(d - rounded) > 1e-9 * std::max(1.0, std::abs(d)))
                    return bad(me_tr("%1 is not a whole number").arg(me_show(v)));
                if (std::abs(rounded) > 9.007199254740992e15)
                    return bad(me_tr("%1 is too large for an integer").arg(me_show(v)));
                d = rounded;
            }
            r.value = d;
            return r;
        }
        case TargetType::Date:
        case TargetType::DateTime: {
            bool ok = false;
            bool date_only = true;
            QString why;
            const QDateTime dt = parse_datetime(v, field.format, &ok, &date_only, &why);
            if (!ok)
                return bad(me_tr("%1: %2").arg(me_show(v), why));
            if (field.type == TargetType::Date)
                r.value = dt.date().toString(Qt::ISODate);
            else
                r.value = dt.toUTC().toString(Qt::ISODate);
            return r;
        }
        case TargetType::Period: {
            const QString f = field.format.trimmed().toLower();
            const bool to_granularity = f == QLatin1String("year") || f == QLatin1String("quarter") ||
                                        f == QLatin1String("month") || f == QLatin1String("half");
            if (!to_granularity) {
                const Period p = parse_period(ds::cell_text(v));
                if (p.valid()) {
                    r.value = p.code;
                    return r;
                }
            }
            bool ok = false;
            QString why;
            const QDateTime dt = parse_datetime(v, to_granularity ? QStringLiteral("auto") : field.format, &ok, nullptr, &why);
            if (!ok)
                return bad(me_tr("%1 is not a period (2024, 2024-Q1, 2024-03) or a date").arg(me_show(v)));
            r.value = me_period_from_date(dt.date(), to_granularity ? f : QStringLiteral("quarter"));
            return r;
        }
    }
    return r;
}

MappingOutput apply_mapping(const ds::DataTable& input, const MappingSpec& spec, int max_rows) {
    MappingOutput out;
    const int nfields = int(spec.fields.size());
    out.table.columns.reserve(nfields);
    for (const FieldMapping& f : spec.fields) {
        out.table.columns << f.target;
        ds::ColumnType t = ds::ColumnType::String;
        switch (f.type) {
            case TargetType::String:
            case TargetType::Period:
                t = ds::ColumnType::String;
                break;
            case TargetType::Integer:
                t = ds::ColumnType::Integer;
                break;
            case TargetType::Number:
                t = ds::ColumnType::Number;
                break;
            case TargetType::Bool:
                t = ds::ColumnType::Bool;
                break;
            case TargetType::Date:
                t = ds::ColumnType::Date;
                break;
            case TargetType::DateTime:
                t = ds::ColumnType::DateTime;
                break;
            case TargetType::Json:
                t = ds::ColumnType::Object;
                break;
        }
        out.table.types << t;
    }
    out.field_stats = QVector<FieldStats>(nfields);
    out.rows_in = input.row_count();
    if (nfields == 0) {
        out.spec_problems << me_tr("No target fields are mapped yet");
        return out;
    }

    // Resolve source columns; check targets are unique.
    QVector<int> src_col(nfields, -1);
    QSet<QString> targets;
    for (int i = 0; i < nfields; ++i) {
        const FieldMapping& f = spec.fields[i];
        if (f.target.trimmed().isEmpty())
            out.spec_problems << me_tr("Field %1 has no target name").arg(i + 1);
        else if (targets.contains(f.target))
            out.spec_problems << me_tr("Target \"%1\" is mapped twice").arg(f.target);
        targets.insert(f.target);
        if (!f.source.isEmpty()) {
            src_col[i] = input.column_index(f.source);
            if (src_col[i] < 0)
                out.spec_problems << me_tr("Source column \"%1\" (for %2) is not in the data").arg(f.source, f.target);
        } else if (f.default_value.isEmpty() && f.required) {
            out.spec_problems << me_tr("Required field \"%1\" has no source column and no default").arg(f.target);
        }
    }

    // "auto" date formats: pick one format per column from a sample, so dd/MM vs MM/dd is decided
    // by the whole column, not guessed per cell.
    QVector<FieldMapping> fields = spec.fields;
    for (int i = 0; i < nfields; ++i) {
        FieldMapping& f = fields[i];
        const bool dateish = f.type == TargetType::Date || f.type == TargetType::DateTime;
        const QString fl = f.format.trimmed().toLower();
        if (!dateish || src_col[i] < 0 || !(fl.isEmpty() || fl == QLatin1String("auto")) || !f.path.isEmpty())
            continue;
        QStringList samples;
        bool numeric = false;
        for (int r = 0; r < input.row_count() && samples.size() < 500; ++r) {
            const QJsonValue v = input.cell(r, src_col[i]);
            if (v.isDouble()) {
                numeric = true;
                break;
            }
            if (v.isString() && !me_is_null_token(v.toString(), spec.options))
                samples << v.toString().trimmed();
        }
        if (numeric)
            continue;
        const QString inferred = me_infer_date_format(samples);
        if (!inferred.isEmpty())
            f.format = inferred;
    }

    const int limit = max_rows < 0 ? input.row_count() : std::min(max_rows, input.row_count());
    QHash<QString, int> seen_keys;
    QVector<int> key_cols;
    for (const QString& k : spec.options.key_fields) {
        const int idx = spec.index_of(k);
        if (idx >= 0)
            key_cols << idx;
    }
    const QString dup_mode = spec.options.duplicates.toLower();

    out.table.rows.reserve(limit);
    for (int r = 0; r < limit; ++r) {
        QJsonArray row;
        QVector<CellIssue> row_issues;
        for (int i = 0; i < nfields; ++i) {
            const FieldMapping& f = fields[i];
            QJsonValue in = src_col[i] >= 0 ? input.cell(r, src_col[i]) : QJsonValue(QJsonValue::Null);
            if (!f.path.isEmpty() && src_col[i] >= 0) {
                QJsonValue container = in;
                if (container.isString()) {
                    const QJsonDocument doc = QJsonDocument::fromJson(container.toString().toUtf8());
                    if (doc.isObject())
                        container = doc.object();
                    else if (doc.isArray())
                        container = doc.array();
                }
                in = ds::json_at_path(container, f.path);
            }
            CastResult c = cast_cell(in, f, spec.options);
            if (c.empty && !f.default_value.isEmpty()) {
                FieldMapping as_text = f;
                as_text.transform.clear();
                c = cast_cell(QJsonValue(f.default_value), as_text, spec.options);
                if (!c.error.isEmpty())
                    c.error = me_tr("default %1 — %2").arg(f.default_value, c.error);
            }
            FieldStats& st = out.field_stats[i];
            if (!c.error.isEmpty()) {
                ++st.errors;
                if (st.first_error.isEmpty())
                    st.first_error = me_tr("row %1: %2").arg(r + 1).arg(c.error);
                row_issues.append({r, i, c.error});
            } else if (c.empty || c.value.isNull()) {
                if (f.required) {
                    const QString msg = me_tr("required value is missing");
                    ++st.errors;
                    if (st.first_error.isEmpty())
                        st.first_error = me_tr("row %1: %2").arg(r + 1).arg(msg);
                    row_issues.append({r, i, msg});
                } else {
                    ++st.empty;
                }
            } else {
                ++st.ok;
            }
            row.append(c.value);
        }

        // Duplicates by key fields.
        if (!key_cols.isEmpty()) {
            QStringList parts;
            for (int kc : std::as_const(key_cols))
                parts << ds::cell_text(row.at(kc));
            const QString key = parts.join(QChar(0x1f));
            const auto it = seen_keys.constFind(key);
            if (it != seen_keys.constEnd()) {
                ++out.duplicates;
                if (dup_mode == QLatin1String("drop")) {
                    ++out.rows_dropped_duplicate;
                    continue;
                }
                if (dup_mode == QLatin1String("flag"))
                    row_issues.append({r, key_cols.first(), me_tr("duplicate of row %1").arg(it.value() + 1)});
            } else {
                seen_keys.insert(key, r);
            }
        }

        if (spec.options.drop_invalid_rows && !row_issues.isEmpty()) {
            ++out.rows_dropped_invalid;
            continue;
        }
        const int out_row = out.table.row_count();
        out.table.rows.append(row);
        out.source_rows.append(r);
        for (const CellIssue& is : std::as_const(row_issues)) {
            ++out.error_cells;
            if (out.issues.size() < MappingOutput::kMaxIssues) {
                out.issue_index.insert((qint64(out_row) << 20) | qint64(is.column), int(out.issues.size()));
                out.issues.append(is);
            }
        }
    }
    out.rows_out = out.table.row_count();
    return out;
}

} // namespace fincept::services::datamapping
