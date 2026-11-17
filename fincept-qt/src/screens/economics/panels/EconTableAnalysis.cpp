// src/screens/economics/panels/EconTableAnalysis.cpp
#include "screens/economics/panels/EconTableAnalysis.h"

#include "ui/widgets/AutoLoadPolicy.h"

#include <QJsonDocument>
#include <QMap>
#include <QRegularExpression>
#include <QSet>
#include <QTimeZone>

#include <algorithm>
#include <cmath>
#include <numeric>

namespace fincept::screens::econ_table {

// Regular expressions are thread_local: this file runs on worker threads and the GUI thread at once.

namespace {

int econ_ta_month_from_name(const QString& name) {
    static const char* const kNames[] = {"jan", "feb", "mar", "apr", "may", "jun",
                                         "jul", "aug", "sep", "oct", "nov", "dec"};
    const QString l = name.left(3).toLower();
    for (int i = 0; i < 12; ++i)
        if (l == QLatin1String(kNames[i]))
            return i + 1;
    return 0;
}

int econ_ta_full_year(int y) {
    return y < 100 ? (y < 50 ? 2000 + y : 1900 + y) : y;
}

} // namespace

Period parse_period(const QString& raw) {
    using RE = QRegularExpression;
    thread_local const RE re_day(QStringLiteral("^(\\d{4})-(\\d{1,2})-(\\d{1,2})(?:[T ].*)?$"));
    thread_local const RE re_month(QStringLiteral("^(\\d{4})[-/.](\\d{1,2})$"));
    thread_local const RE re_year(QStringLiteral("^(\\d{4})$"));
    thread_local const RE re_quarter(QStringLiteral("^(\\d{4})[-\\s]?Q([1-4])$"), RE::CaseInsensitiveOption);
    thread_local const RE re_quarter_rev(QStringLiteral("^Q([1-4])[-\\s]?(\\d{4})$"), RE::CaseInsensitiveOption);
    thread_local const RE re_half(QStringLiteral("^(\\d{4})[-\\s]?[SH]([12])$"), RE::CaseInsensitiveOption);
    thread_local const RE re_sdmx_month(QStringLiteral("^(\\d{4})-?M(\\d{1,2})$"), RE::CaseInsensitiveOption);
    thread_local const RE re_week(QStringLiteral("^(\\d{4})-?W(\\d{1,2})$"), RE::CaseInsensitiveOption);
    thread_local const RE re_d_mon_y(QStringLiteral("^(\\d{1,2})[\\s\\-/]([A-Za-z]{3,9})[\\s\\-/,]+(\\d{2,4})$"));
    thread_local const RE re_mon_y(QStringLiteral("^([A-Za-z]{3,9})[\\s\\-/,]+(\\d{4})$"));
    thread_local const RE re_y_mon(QStringLiteral("^(\\d{4})[\\s\\-]([A-Za-z]{3,9})$"));
    thread_local const RE re_slash(QStringLiteral("^(\\d{1,2})/(\\d{1,2})/(\\d{4})$"));
    // 2025年09月份 · 2025年第3季度 / 2025年第1-3季度 (cumulative → ends in Q3) · 2025年
    thread_local const RE re_cn_month(QStringLiteral("^(\\d{4})年(\\d{1,2})月"));
    thread_local const RE re_cn_quarter(QStringLiteral("^(\\d{4})年第(?:\\d-)?(\\d)季度"));
    thread_local const RE re_cn_year(QStringLiteral("^(\\d{4})年$"));

    Period p;
    const QString s = raw.trimmed();
    if (s.size() < 4)
        return p;
    auto set = [&p](int y, int m, int d, Gran g) {
        p.date = QDate(y, m, d);
        p.gran = p.date.isValid() ? g : Gran::Unknown;
    };
    QRegularExpressionMatch m;
    if ((m = re_day.match(s)).hasMatch()) {
        set(m.captured(1).toInt(), m.captured(2).toInt(), m.captured(3).toInt(), Gran::Day);
    } else if ((m = re_month.match(s)).hasMatch()) {
        set(m.captured(1).toInt(), m.captured(2).toInt(), 1, Gran::Month);
    } else if ((m = re_year.match(s)).hasMatch()) {
        set(m.captured(1).toInt(), 1, 1, Gran::Year);
    } else if ((m = re_quarter.match(s)).hasMatch()) {
        set(m.captured(1).toInt(), (m.captured(2).toInt() - 1) * 3 + 1, 1, Gran::Quarter);
    } else if ((m = re_quarter_rev.match(s)).hasMatch()) {
        set(m.captured(2).toInt(), (m.captured(1).toInt() - 1) * 3 + 1, 1, Gran::Quarter);
    } else if ((m = re_half.match(s)).hasMatch()) {
        set(m.captured(1).toInt(), (m.captured(2).toInt() - 1) * 6 + 1, 1, Gran::Half);
    } else if ((m = re_sdmx_month.match(s)).hasMatch()) {
        set(m.captured(1).toInt(), m.captured(2).toInt(), 1, Gran::Month);
    } else if ((m = re_week.match(s)).hasMatch()) {
        // ISO week: week 1 is the week holding 4 January; weeks start on Monday.
        const QDate jan4(m.captured(1).toInt(), 1, 4);
        const QDate monday = jan4.addDays(1 - jan4.dayOfWeek());
        p.date = monday.addDays((m.captured(2).toInt() - 1) * 7);
        p.gran = p.date.isValid() ? Gran::Week : Gran::Unknown;
    } else if ((m = re_d_mon_y.match(s)).hasMatch()) {
        const int mon = econ_ta_month_from_name(m.captured(2));
        if (mon > 0)
            set(econ_ta_full_year(m.captured(3).toInt()), mon, m.captured(1).toInt(), Gran::Day);
    } else if ((m = re_mon_y.match(s)).hasMatch()) {
        const int mon = econ_ta_month_from_name(m.captured(1));
        if (mon > 0)
            set(m.captured(2).toInt(), mon, 1, Gran::Month);
    } else if ((m = re_y_mon.match(s)).hasMatch()) {
        const int mon = econ_ta_month_from_name(m.captured(2));
        if (mon > 0)
            set(m.captured(1).toInt(), mon, 1, Gran::Month);
    } else if ((m = re_slash.match(s)).hasMatch()) {
        // Day-first unless that is impossible (US month-first "12/31/2024").
        const int a = m.captured(1).toInt();
        const int b = m.captured(2).toInt();
        if (b > 12)
            set(m.captured(3).toInt(), a, b, Gran::Day);
        else
            set(m.captured(3).toInt(), b, a, Gran::Day);
    } else if ((m = re_cn_month.match(s)).hasMatch()) {
        set(m.captured(1).toInt(), m.captured(2).toInt(), 1, Gran::Month);
    } else if ((m = re_cn_quarter.match(s)).hasMatch()) {
        const int q = m.captured(2).toInt();
        if (q >= 1 && q <= 4)
            set(m.captured(1).toInt(), (q - 1) * 3 + 1, 1, Gran::Quarter);
    } else if ((m = re_cn_year.match(s)).hasMatch()) {
        set(m.captured(1).toInt(), 1, 1, Gran::Year);
    }
    return p;
}

QString period_text(const QJsonValue& v) {
    if (v.isDouble()) {
        const double d = v.toDouble();
        if (d == std::floor(d) && std::abs(d) < 1e9)
            return QString::number(static_cast<qint64>(d)).rightJustified(4, QLatin1Char('0'));
        return QString::number(d, 'g', 10);
    }
    QString s = v.toString().trimmed();
    bool is_int = false;
    const int y = s.toInt(&is_int);
    if (is_int && s.size() < 4 && y >= 0)
        s = s.rightJustified(4, QLatin1Char('0'));
    return s;
}

bool to_number(const QJsonValue& v, double* out) {
    if (v.isDouble()) {
        const double d = v.toDouble();
        if (!std::isfinite(d))
            return false;
        *out = d;
        return true;
    }
    if (!v.isString())
        return false;
    QString s = v.toString().trimmed();
    if (s.endsWith(QLatin1Char('%')))
        s.chop(1);
    if (s.contains(QLatin1Char(','))) {
        thread_local const QRegularExpression kGrouped(QStringLiteral("^-?\\d{1,3}(,\\d{3})+(\\.\\d+)?$"));
        if (kGrouped.match(s).hasMatch())
            s.remove(QLatin1Char(','));
    }
    if (s.isEmpty())
        return false;
    bool ok = false;
    const double d = s.toDouble(&ok);
    if (!ok || !std::isfinite(d))
        return false;
    *out = d;
    return true;
}

const QStringList& date_keys() {
    static const QStringList kKeys = {
        QStringLiteral("date"),         QStringLiteral("Date"),           QStringLiteral("DATE"),
        QStringLiteral("period"),       QStringLiteral("Period"),         QStringLiteral("TIME_PERIOD"),
        QStringLiteral("time_period"),  QStringLiteral("obs_date"),       QStringLiteral("observation_date"),
        QStringLiteral("record_date"),  QStringLiteral("ref_date"),       QStringLiteral("report_date"),
        QStringLiteral("year"),         QStringLiteral("Year"),           QStringLiteral("YEAR"),
        QStringLiteral("month"),        QStringLiteral("week"),
        QStringLiteral("time"),         QStringLiteral("Time"),           QStringLiteral("event_datetime"),
        QStringLiteral("月份"), QStringLiteral("季度"),   QStringLiteral("日期"),
        QStringLiteral("年份"),
    };
    return kKeys;
}

bool is_date_key(const QString& key) {
    return date_keys().contains(key);
}

const QStringList& value_keys() {
    static const QStringList kKeys = {QStringLiteral("value"),     QStringLiteral("Value"),
                                      QStringLiteral("VALUE"),     QStringLiteral("val"),
                                      QStringLiteral("OBS_VALUE"), QStringLiteral("obs_value"),
                                      QStringLiteral("amount"),    QStringLiteral("rate")};
    return kKeys;
}

bool is_id_like(const QString& key) {
    const QString l = key.toLower();
    return l == QLatin1String("id") || l.endsWith(QLatin1String("_id")) || l.endsWith(QLatin1String("code")) ||
           l == QLatin1String("series") || l == QLatin1String("symbol");
}

Gran gran_from_text(const QString& text) {
    const QString l = text.trimmed().toLower();
    if (l.isEmpty())
        return Gran::Unknown;
    if (l == QLatin1String("d") || l == QLatin1String("b") || l.startsWith(QLatin1String("daily")) ||
        l.contains(QLatin1String("business day")))
        return Gran::Day;
    if (l == QLatin1String("w") || l.startsWith(QLatin1String("week")))
        return Gran::Week;
    if (l == QLatin1String("m") || l.startsWith(QLatin1String("month")))
        return Gran::Month;
    if (l == QLatin1String("q") || l.startsWith(QLatin1String("quarter")))
        return Gran::Quarter;
    if (l == QLatin1String("s") || l == QLatin1String("h") || l.startsWith(QLatin1String("semi")) ||
        l.startsWith(QLatin1String("half")))
        return Gran::Half;
    if (l == QLatin1String("a") || l == QLatin1String("y") || l.startsWith(QLatin1String("annual")) ||
        l.startsWith(QLatin1String("year")))
        return Gran::Year;
    return Gran::Unknown;
}

Gran infer_gran(const QVector<Period>& pts) {
    if (pts.isEmpty())
        return Gran::Unknown;
    // A uniform period spelling coarser than a day decides it ("2024-03", "2024-Q1", "2024").
    Gran common = pts.first().gran;
    for (const auto& p : pts) {
        if (p.gran != common) {
            common = Gran::Unknown;
            break;
        }
    }
    if (common != Gran::Unknown && common != Gran::Day)
        return common;
    // Full dates: classify the median spacing of the most recent observations.
    QVector<qint64> gaps;
    for (qsizetype i = std::max<qsizetype>(1, pts.size() - 25); i < pts.size(); ++i) {
        const qint64 g = pts[i - 1].date.daysTo(pts[i].date);
        if (g > 0)
            gaps.append(g);
    }
    if (gaps.isEmpty())
        return Gran::Unknown;
    std::sort(gaps.begin(), gaps.end());
    const qint64 med = gaps[gaps.size() / 2];
    if (med <= 4)
        return Gran::Day;
    if (med <= 10)
        return Gran::Week;
    if (med <= 45)
        return Gran::Month;
    if (med <= 120)
        return Gran::Quarter;
    if (med <= 240)
        return Gran::Half;
    return Gran::Year;
}

bool text_says_percent(const QString& text) {
    const QString l = text.toLower();
    if (l.contains(QLatin1String("per 1,000")) || l.contains(QLatin1String("per 1000")) ||
        l.contains(QLatin1String("per 100,000")) || l.contains(QLatin1String("per 100 ")))
        return false; // demographic "rates" per 1,000 people are not percentages
    if (l.contains(QLatin1Char('%')) || l.contains(QLatin1String("percent")) || l.contains(QLatin1String("per cent")) ||
        l.contains(QLatin1String("_pct")) || l.contains(QLatin1String("yield")))
        return true;
    thread_local const QRegularExpression kRateWord(QStringLiteral("\\brates?\\b"));
    return kRateWord.match(l).hasMatch() && !l.contains(QLatin1String("exchange")) && !l.contains(QLatin1String("fx")) &&
           !l.contains(QLatin1Char('/'));
}

bool text_says_bp(const QString& units) {
    const QString l = units.trimmed().toLower();
    return l == QLatin1String("bp") || l == QLatin1String("bps") || l.contains(QLatin1String("basis point"));
}

QString cell_text(const QJsonValue& v) {
    if (v.isDouble())
        return QString::number(v.toDouble(), 'g', 15);
    if (v.isBool())
        return v.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    if (v.isNull() || v.isUndefined())
        return {};
    if (v.isObject())
        return QString::fromUtf8(QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact));
    if (v.isArray())
        return QString::fromUtf8(QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact));
    return v.toString();
}

bool is_change_key(const QString& key) {
    thread_local const QRegularExpression kCamel(QStringLiteral("([a-z0-9])([A-Z])"));
    thread_local const QRegularExpression kChange(
        QStringLiteral("(^|[_\\s\\-.])(change|chg|growth|yoy|mom|qoq|wow|diff|delta)([_\\s\\-.%]|$)"));
    QString k = key;
    k.replace(kCamel, QStringLiteral("\\1_\\2"));
    return kChange.match(k.toLower()).hasMatch();
}

bool plain_number_string(const QString& s, double* out) {
    thread_local const QRegularExpression kPlain(QStringLiteral("^-?(0|[1-9]\\d*)(\\.\\d+)?$"));
    if (!kPlain.match(s).hasMatch())
        return false;
    bool ok = false;
    const double d = s.toDouble(&ok);
    if (!ok || !std::isfinite(d))
        return false;
    *out = d;
    return true;
}

Meta meta_from_json(const QJsonObject& mj) {
    Meta m;
    m.units = mj.value(QStringLiteral("units")).toString();
    m.frequency = mj.value(QStringLiteral("frequency")).toString();
    m.seasonal_adj = mj.value(QStringLiteral("seasonal_adj")).toString();
    m.source = mj.value(QStringLiteral("source")).toString();
    m.last_updated = mj.value(QStringLiteral("last_updated")).toString();
    m.note = mj.value(QStringLiteral("note")).toString();
    m.percent = mj.value(QStringLiteral("percent")).toInt(-1);
    m.value_column = mj.value(QStringLiteral("value_column")).toString();
    m.stats_cutoff = QDate::fromString(mj.value(QStringLiteral("stats_cutoff")).toString(), Qt::ISODate);
    return m;
}

// ── Analysis ────────────────────────────────────────────────────────────────

namespace {

/// The first column of `columns` (not a date or an identifier) whose present values are mostly
/// numbers.
bool econ_ta_mostly_numeric(const QJsonArray& rows, const QString& column) {
    int numeric = 0;
    int present = 0;
    for (const auto& rv : rows) {
        const QJsonValue v = rv.toObject().value(column);
        if (v.isUndefined() || v.isNull())
            continue;
        ++present;
        double x = 0.0;
        if (to_number(v, &x))
            ++numeric;
    }
    return present > 0 && numeric * 2 >= present;
}

void econ_ta_stats(Analysis& a) {
    Stats& st = a.stats;
    const QStringList& columns = a.columns;
    const Meta& meta = a.meta;
    const QString date_col = (!columns.isEmpty() && is_date_key(columns.first())) ? columns.first() : QString();

    // Column the cards summarise: the panel's choice, a conventional value key, else the first
    // mostly-numeric column that is not a date or an identifier.
    QString value_col = columns.contains(meta.value_column) ? meta.value_column : QString();
    if (value_col.isEmpty()) {
        for (const auto& vk : value_keys()) {
            if (columns.contains(vk)) {
                value_col = vk;
                break;
            }
        }
    }
    if (value_col.isEmpty()) {
        for (const auto& c : columns) {
            if (c == date_col || is_date_key(c) || is_id_like(c))
                continue;
            if (econ_ta_mostly_numeric(a.rows, c)) {
                value_col = c;
                break;
            }
        }
    }
    st.column = value_col;
    st.count = static_cast<int>(a.rows.size());
    if (date_col.isEmpty() || value_col.isEmpty())
        return;

    struct Pt {
        Period p;
        QString label;
        double v = 0.0;
    };
    QVector<Pt> pts;
    pts.reserve(a.rows.size());
    int unparsed = 0;
    for (const auto& rv : a.rows) {
        const QJsonObject o = rv.toObject();
        double x = 0.0;
        if (!to_number(o.value(value_col), &x))
            continue;
        const QString label = period_text(o.value(date_col));
        const Period p = parse_period(label);
        if (!p.date.isValid()) {
            ++unparsed;
            continue;
        }
        pts.append({p, label, x});
    }
    if (pts.isEmpty() || unparsed * 4 > pts.size())
        return;
    std::stable_sort(pts.begin(), pts.end(), [](const Pt& l, const Pt& r) { return l.p.date < r.p.date; });
    // A time series has one observation per period. Repeated dates mean several series (or a
    // cross-section) are interleaved, and LATEST / CHANGE / MIN / MAX over them would mix unrelated
    // numbers — no cards then.
    for (qsizetype i = 1; i < pts.size(); ++i)
        if (pts[i].p.date == pts[i - 1].p.date)
            return;
    // Projections (IMF WEO, EIA STEO, …) stay in the table but never become "LATEST".
    if (meta.stats_cutoff.isValid()) {
        const qsizetype before = pts.size();
        const QDate cutoff = meta.stats_cutoff;
        pts.erase(std::remove_if(pts.begin(), pts.end(), [&cutoff](const Pt& x) { return x.p.date > cutoff; }),
                  pts.end());
        st.excluded = static_cast<int>(before - pts.size());
        if (pts.isEmpty())
            return;
    }
    st.applicable = true;

    QVector<Period> periods;
    periods.reserve(pts.size());
    for (const auto& x : std::as_const(pts))
        periods.append(x.p);
    st.gran = gran_from_text(meta.frequency);
    if (st.gran == Gran::Unknown)
        st.gran = infer_gran(periods);

    st.bp = text_says_bp(meta.units);
    if (meta.percent == 1)
        st.pct = true;
    else if (meta.percent == -1 && !st.bp)
        st.pct = text_says_percent(meta.units.trimmed().isEmpty() ? a.title + QLatin1Char(' ') + value_col : meta.units);

    const Pt& last = pts.last();
    st.latest_label = last.label;
    st.latest = last.v;
    if (pts.size() >= 2) {
        const Pt& prev = pts[pts.size() - 2];
        st.has_prev = true;
        st.prev_label = prev.label;
        st.prev = prev.v;
    }
    // Year-over-year for sub-annual series: the same period one year earlier (±7 days for daily /
    // weekly data, whose calendar dates drift).
    if (st.gran != Gran::Unknown && st.gran != Gran::Year) {
        const QDate target = last.p.date.addYears(-1);
        const Pt* match = nullptr;
        for (qsizetype i = pts.size() - 1; i >= 0 && !match; --i) {
            const QDate d = pts[i].p.date;
            if (st.gran == Gran::Day || st.gran == Gran::Week) {
                if (d <= target && d >= target.addDays(-7))
                    match = &pts[i];
                else if (d < target.addDays(-7))
                    break;
            } else if (d == target) {
                match = &pts[i];
            } else if (d < target) {
                break;
            }
        }
        if (match) {
            st.has_yoy = true;
            st.yoy_label = match->label;
            st.yoy = match->v;
        }
    }
    double mn = pts.first().v;
    double mx = pts.first().v;
    double sum = 0.0;
    for (const auto& x : std::as_const(pts)) {
        mn = std::min(mn, x.v);
        mx = std::max(mx, x.v);
        sum += x.v;
    }
    st.min = mn;
    st.max = mx;
    st.avg = sum / static_cast<double>(pts.size());
    st.count = static_cast<int>(pts.size());
}

void econ_ta_chart(Analysis& a) {
    const QJsonArray& rows = a.rows;
    const QStringList& columns = a.columns;
    const QString date_col = (!columns.isEmpty() && is_date_key(columns.first())) ? columns.first() : QString();
    QVector<QDate> dates;
    int parsed = 0;
    if (!date_col.isEmpty() && rows.size() >= 2) {
        dates.resize(rows.size());
        for (qsizetype i = 0; i < rows.size(); ++i) {
            dates[i] = parse_period(period_text(rows[i].toObject().value(date_col))).date;
            if (dates[i].isValid())
                ++parsed;
        }
    }
    // A chart needs real dates on (nearly) every row — an event calendar's "8:30am" is not a series.
    if (date_col.isEmpty() || parsed < 2 || parsed * 5 < rows.size() * 4)
        return;

    // Numeric columns other than the date and identifiers.
    QStringList numeric_cols;
    for (const auto& c : columns) {
        if (c == date_col || is_date_key(c) || is_id_like(c))
            continue;
        if (econ_ta_mostly_numeric(rows, c))
            numeric_cols << c;
    }
    QString value_col = numeric_cols.contains(a.meta.value_column) ? a.meta.value_column : QString();
    if (value_col.isEmpty()) {
        for (const auto& vk : value_keys()) {
            if (numeric_cols.contains(vk)) {
                value_col = vk;
                break;
            }
        }
    }

    // One column as a line, optionally restricted to the rows of one group. `dup` reports a date
    // seen twice — the rows are then not a single series.
    auto build = [&](const QString& col, const QString& group_col, const QString& group_val, bool* dup) {
        QMap<qint64, double> by_x;
        for (qsizetype i = 0; i < rows.size(); ++i) {
            if (!dates[i].isValid())
                continue;
            const QJsonObject o = rows[i].toObject();
            if (!group_col.isEmpty() && cell_text(o.value(group_col)) != group_val)
                continue;
            double v = 0.0;
            if (!to_number(o.value(col), &v))
                continue;
            const qint64 x = dates[i].startOfDay(QTimeZone::UTC).toMSecsSinceEpoch();
            if (by_x.contains(x)) {
                *dup = true;
                return QVector<QPointF>{};
            }
            by_x.insert(x, v);
        }
        QVector<QPointF> pts;
        pts.reserve(by_x.size());
        for (auto it = by_x.cbegin(); it != by_x.cend(); ++it)
            pts.append(QPointF(static_cast<double>(it.key()), it.value()));
        return pts;
    };

    constexpr int kMaxLines = 8;
    const QString primary = !value_col.isEmpty()       ? value_col
                            : numeric_cols.size() == 1 ? numeric_cols.first()
                            : numeric_cols.contains(a.stats.column) && numeric_cols.size() > kMaxLines
                                ? a.stats.column
                                : QString();
    QVector<Line> lines;
    bool dup = false;
    if (!primary.isEmpty()) {
        const QVector<QPointF> pts = build(primary, {}, {}, &dup);
        if (!dup && pts.size() >= 2)
            lines.append({primary, pts});
    } else if (numeric_cols.size() >= 2) {
        // Wide table — one dated row, a few numeric columns (tenors, currencies): a line each.
        for (const auto& col : std::as_const(numeric_cols)) {
            const QVector<QPointF> pts = build(col, {}, {}, &dup);
            if (dup)
                break;
            if (pts.size() >= 2)
                lines.append({col, pts});
        }
        if (dup)
            lines.clear();
    }
    if (lines.isEmpty() && dup && !primary.isEmpty()) {
        // Several series interleaved by date (a "series" / "country" / … column): a line per series.
        static const QStringList kGroupKeys = {
            QStringLiteral("series"),  QStringLiteral("series_id"), QStringLiteral("country"),
            QStringLiteral("country_name"), QStringLiteral("currency"), QStringLiteral("symbol"),
            QStringLiteral("indicator"), QStringLiteral("measure"),  QStringLiteral("tenor"),
            QStringLiteral("maturity"), QStringLiteral("field"),     QStringLiteral("name")};
        for (const auto& gk : kGroupKeys) {
            if (!columns.contains(gk))
                continue;
            QStringList groups;
            for (qsizetype i = 0; i < rows.size() && groups.size() <= kMaxLines; ++i) {
                const QString g = cell_text(rows[i].toObject().value(gk));
                if (dates[i].isValid() && !g.isEmpty() && !groups.contains(g))
                    groups << g;
            }
            if (groups.size() < 2 || groups.size() > kMaxLines)
                continue;
            QVector<Line> group_lines;
            bool group_dup = false;
            for (const auto& g : std::as_const(groups)) {
                const QVector<QPointF> pts = build(primary, gk, g, &group_dup);
                if (group_dup)
                    break;
                if (pts.size() >= 2)
                    group_lines.append({g, pts});
            }
            if (!group_dup && !group_lines.isEmpty()) {
                lines = group_lines;
                break;
            }
        }
    }
    if (lines.isEmpty())
        return;

    // Same value convention as the stat cards: rates in %, bp series in bp, levels compact.
    a.chart_bp = text_says_bp(a.meta.units);
    a.chart_pct = a.meta.percent == 1;
    if (a.meta.percent == -1 && !a.chart_bp)
        a.chart_pct = text_says_percent(a.meta.units.trimmed().isEmpty()
                                            ? a.title + QLatin1Char(' ') + lines.first().name
                                            : a.meta.units);
    a.x_max = 0.0;
    for (const auto& l : std::as_const(lines))
        a.x_max = std::max(a.x_max, l.points.last().x());
    // Range presets that suit the spacing of the observations.
    const QVector<QPointF>& ref = lines.first().points;
    const double span_days = (ref.last().x() - ref.first().x()) / 86400000.0;
    const double gap_days = ref.size() > 1 ? span_days / static_cast<double>(ref.size() - 1) : 0.0;
    a.range_kind = gap_days <= 10.0 ? RangeKind::Daily : (gap_days <= 45.0 ? RangeKind::Monthly : RangeKind::Coarse);
    a.lines = lines;
}

} // namespace

Analysis analyse(const QJsonObject& t) {
    Analysis a;
    a.meta = meta_from_json(t.value(QStringLiteral("meta")).toObject());
    a.title = t.value(QStringLiteral("title")).toString();
    const QJsonArray rows = t.value(QStringLiteral("rows")).toArray();
    QStringList column_order;
    for (const auto& c : t.value(QStringLiteral("order")).toArray())
        column_order << c.toString();
    const bool keep_row_order = t.value(QStringLiteral("keep")).toBool();

    // Rows are expected to be objects; anything else (bare numbers / strings / nested arrays) becomes a
    // one-column "value" row, so an unexpected shape still renders as a table, never a blank grid.
    bool all_objects = true;
    for (const auto& rv : rows) {
        if (!rv.isObject()) {
            all_objects = false;
            break;
        }
    }
    if (all_objects) {
        a.rows = rows; // shared with the payload, no copy
    } else {
        for (const auto& rv : rows) {
            const QJsonValue v = rv;
            a.rows.append(v.isObject() ? v : QJsonValue(QJsonObject{{QStringLiteral("value"), v}}));
        }
    }

    // Columns in the order they first appear (a row's keys come sorted).
    QSet<QString> seen;
    for (const auto& rv : std::as_const(a.rows)) {
        const QJsonObject o = rv.toObject();
        for (auto it = o.constBegin(); it != o.constEnd(); ++it) {
            const QString k = it.key();
            if (!seen.contains(k)) {
                seen.insert(k);
                a.columns << k;
            }
        }
    }
    if (!column_order.isEmpty()) {
        QStringList ordered;
        for (const auto& c : column_order)
            if (seen.contains(c) && !ordered.contains(c))
                ordered << c;
        for (const auto& c : std::as_const(a.columns))
            if (!ordered.contains(c))
                ordered << c;
        a.columns = ordered;
    } else {
        // Date first, value second. Stop at the first key found: a later date-like key ("year"
        // after "date") must not displace the real one.
        for (const auto& dk : date_keys()) {
            const auto idx = a.columns.indexOf(dk);
            if (idx < 0)
                continue;
            if (idx > 0)
                a.columns.move(idx, 0);
            break;
        }
        for (const auto& vk : value_keys()) {
            const auto idx = a.columns.indexOf(vk);
            if (idx < 0)
                continue;
            if (idx > 1)
                a.columns.move(idx, 1);
            break;
        }
    }

    // Newest first when the first column holds real dates (a calendar's "8:30am" does not parse).
    a.order.resize(a.rows.size());
    std::iota(a.order.begin(), a.order.end(), 0);
    if (!keep_row_order && !a.columns.isEmpty() && is_date_key(a.columns.first())) {
        int parsed = 0;
        for (const auto& rv : std::as_const(a.rows))
            if (parse_period(period_text(rv.toObject().value(a.columns.first()))).date.isValid())
                ++parsed;
        if (parsed * 5 >= a.rows.size() * 4) {
            a.order = sorted_order(a.rows, a.columns.first(), false);
            a.sort_column = 0;
            a.sort_descending = true;
        }
    }

    econ_ta_stats(a);
    econ_ta_chart(a);
    a.approx_bytes = ui::autoload::approx_json_bytes(a.rows);
    return a;
}

QVector<int> sorted_order(const QJsonArray& rows, const QString& key, bool ascending) {
    struct Item {
        int index = 0;
        bool has = false;
        bool num_ok = false;
        double num = 0.0;
        bool date_ok = false;
        qint64 jd = 0;
        QString str;
    };
    QVector<Item> items;
    items.reserve(rows.size());
    for (qsizetype i = 0; i < rows.size(); ++i) {
        Item it;
        it.index = static_cast<int>(i);
        const QJsonValue v = rows[i].toObject().value(key);
        it.has = !(v.isUndefined() || v.isNull() || (v.isString() && v.toString().trimmed().isEmpty()));
        if (it.has) {
            it.num_ok = to_number(v, &it.num);
            if (!it.num_ok) {
                const Period p = parse_period(period_text(v));
                it.date_ok = p.date.isValid();
                if (it.date_ok)
                    it.jd = p.date.toJulianDay();
            }
            it.str = cell_text(v);
        }
        items.append(std::move(it));
    }
    std::stable_sort(items.begin(), items.end(), [ascending](const Item& a, const Item& b) {
        if (a.has != b.has)
            return a.has; // blanks last in either direction
        if (!a.has)
            return false;
        int c = 0;
        if (a.date_ok && b.date_ok)
            c = a.jd < b.jd ? -1 : (a.jd > b.jd ? 1 : 0);
        else if (a.num_ok && b.num_ok)
            c = a.num < b.num ? -1 : (a.num > b.num ? 1 : 0);
        else
            c = QString::compare(a.str, b.str, Qt::CaseInsensitive);
        return ascending ? c < 0 : c > 0;
    });
    QVector<int> order;
    order.reserve(items.size());
    for (const auto& it : std::as_const(items))
        order.append(it.index);
    return order;
}

QVector<QPointF> decimate(const QVector<QPointF>& points, double from, double to, int px) {
    px = std::max(px, 16);
    const qsizetype n = points.size();
    if (n <= 2 * qsizetype(px) + 2)
        return points;
    const double lo = points.first().x();
    const double hi = points.last().x();
    if (!(hi > lo))
        return points;
    if (!(to > from)) {
        from = lo;
        to = hi;
    }
    from = std::clamp(from, lo, hi);
    to = std::clamp(to, lo, hi);

    QVector<QPointF> out;
    out.reserve(4 * px + 8);
    out.append(points.first());
    // Buckets of `width` starting at `start`; one pass over [begin, end).
    auto emit_range = [&](qsizetype begin, qsizetype end, double start, double width) {
        qsizetype i = begin;
        while (i < end) {
            const qint64 bucket = width > 0 ? static_cast<qint64>((points[i].x() - start) / width) : 0;
            qsizetype mn = i, mx = i;
            qsizetype j = i + 1;
            for (; j < end; ++j) {
                const qint64 b = width > 0 ? static_cast<qint64>((points[j].x() - start) / width) : 0;
                if (b != bucket)
                    break;
                // NaN gaps: keep them (the chart draws them as breaks).
                if (std::isnan(points[j].y())) {
                    continue;
                }
                if (std::isnan(points[mn].y()) || points[j].y() < points[mn].y())
                    mn = j;
                if (std::isnan(points[mx].y()) || points[j].y() > points[mx].y())
                    mx = j;
            }
            const qsizetype first = std::min(mn, mx);
            const qsizetype second = std::max(mn, mx);
            if (first > 0 && first < n - 1)
                out.append(points[first]);
            if (second != first && second > 0 && second < n - 1)
                out.append(points[second]);
            i = j;
        }
    };
    const auto lower = [&](double x) {
        return std::lower_bound(points.cbegin(), points.cend(), x,
                                [](const QPointF& p, double v) { return p.x() < v; }) -
               points.cbegin();
    };
    const qsizetype a = lower(from);
    const qsizetype b = std::upper_bound(points.cbegin(), points.cend(), to,
                                         [](double v, const QPointF& p) { return v < p.x(); }) -
                        points.cbegin();
    const int coarse = std::max(4, px / 4);
    if (a > 0)
        emit_range(0, a, lo, (from - lo) / coarse);
    emit_range(a, b, from, (to - from) / px);
    if (b < n)
        emit_range(b, n, to, (hi - to) / coarse);
    out.append(points.last());
    return out;
}

QString rows_text(const QJsonArray& rows, const QStringList& columns, const QVector<int>& order, bool csv) {
    auto quote = [](QString v) {
        if (v.contains(QLatin1Char(',')) || v.contains(QLatin1Char('"')) || v.contains(QLatin1Char('\n')))
            v = QLatin1Char('"') + v.replace(QLatin1Char('"'), QStringLiteral("\"\"")) + QLatin1Char('"');
        return v;
    };
    auto clean = [](QString s) {
        return s.replace(QLatin1Char('\t'), QLatin1Char(' ')).replace(QLatin1Char('\n'), QLatin1Char(' '));
    };
    const QChar sep = csv ? QLatin1Char(',') : QLatin1Char('\t');
    QString out;
    QStringList cells;
    cells.reserve(columns.size());
    for (const auto& c : columns)
        cells << (csv ? quote(c) : clean(c));
    out += cells.join(sep);
    out += QLatin1Char('\n');
    const qsizetype count = order.isEmpty() ? rows.size() : order.size();
    for (qsizetype k = 0; k < count; ++k) {
        const qsizetype i = order.isEmpty() ? k : order[k];
        if (i < 0 || i >= rows.size())
            continue;
        const QJsonObject obj = rows[i].toObject();
        cells.clear();
        // 'g' with 15 significant digits round-trips a double without the UI formatter's rounding.
        for (const auto& col : columns) {
            const QString t = cell_text(obj.value(col));
            cells << (csv ? quote(t) : clean(t));
        }
        out += cells.join(sep);
        out += QLatin1Char('\n');
    }
    return out;
}

} // namespace fincept::screens::econ_table
