#pragma once
// ReportTableFormat.h — formatted tables of the Report Builder: column types and number
// formats (number / percent / currency / date), alignment, zebra rows, a totals row,
// header repeat across pages, column widths and conditional colour scales.
//
// Stored next to the cells (config["csv"], ReportTableCodec.h) in the component's string
// map, so files, library rows and the cloud JSON stay compatible — and a table without
// these keys renders exactly as before (every column "auto": the cell text is printed as
// typed, numbers right-aligned, zebra from 5 rows, header row repeated):
//
//   config["col_fmt"]     JSON array, one object per column (missing / shorter = auto):
//       {"type":"auto|text|number|percent|currency|date",
//        "dec": -1..8        decimals (-1 = as typed for numbers, 2 for currency / percent),
//        "ccy": "USD"        currency of a currency column (empty = the table's currency),
//        "neg": "minus|brackets",
//        "div": 1|1000|1e6|1e9   divide before printing (the unit belongs in the note),
//        "align": "auto|left|center|right",
//        "width": 0..100     percent of the table width (0 = share what is left),
//        "heat": "none|green_red|red_green|blue"   conditional colour scale over the body,
//        "date_fmt": "d MMM yyyy"}
//   config["table_style"] JSON {"zebra":"auto|on|off", "totals":"none|sum|last",
//        "header_repeat": true, "currency": "USD"}
//
// format_table() turns the cells into what every output prints (page view, PDF, HTML,
// .odt, Markdown) so all of them agree. Money formatting is injected (NumberStyle): the
// page view passes the terminal's formatter (ui/formatting) on the GUI thread; exports
// running on a worker receive tables formatted on the GUI thread when the export started.
// Qt Core only (colours are "#rrggbb" strings), header-only.

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QDate>
#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QVector>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>

namespace fincept::report::tablefmt {

inline constexpr char kColFmtKey[] = "col_fmt";
inline constexpr char kStyleKey[] = "table_style";

struct ColumnFormat {
    QString type = QStringLiteral("auto"); ///< auto | text | number | percent | currency | date
    int decimals = -1;
    QString currency;
    bool brackets = false; ///< negatives as (1,234)
    double divide = 1;
    QString align = QStringLiteral("auto"); ///< auto | left | center | right
    double width = 0;                       ///< percent of the table width, 0 = auto
    QString heat = QStringLiteral("none");  ///< none | green_red | red_green | blue
    QString date_format;

    bool is_default() const {
        return type == QLatin1String("auto") && decimals < 0 && currency.isEmpty() && !brackets && divide == 1 &&
               align == QLatin1String("auto") && width <= 0 && heat == QLatin1String("none") && date_format.isEmpty();
    }
    bool operator==(const ColumnFormat& o) const {
        return type == o.type && decimals == o.decimals && currency == o.currency && brackets == o.brackets &&
               divide == o.divide && align == o.align && width == o.width && heat == o.heat &&
               date_format == o.date_format;
    }
    bool operator!=(const ColumnFormat& o) const { return !(*this == o); }

    QJsonObject to_json() const {
        QJsonObject o;
        if (type != QLatin1String("auto"))
            o.insert(QStringLiteral("type"), type);
        if (decimals >= 0)
            o.insert(QStringLiteral("dec"), decimals);
        if (!currency.isEmpty())
            o.insert(QStringLiteral("ccy"), currency);
        if (brackets)
            o.insert(QStringLiteral("neg"), QStringLiteral("brackets"));
        if (divide != 1)
            o.insert(QStringLiteral("div"), divide);
        if (align != QLatin1String("auto"))
            o.insert(QStringLiteral("align"), align);
        if (width > 0)
            o.insert(QStringLiteral("width"), width);
        if (heat != QLatin1String("none"))
            o.insert(QStringLiteral("heat"), heat);
        if (!date_format.isEmpty())
            o.insert(QStringLiteral("date_fmt"), date_format);
        return o;
    }
    static ColumnFormat from_json(const QJsonObject& o) {
        static const QStringList kTypes = {"auto", "text", "number", "percent", "currency", "date"};
        static const QStringList kAligns = {"auto", "left", "center", "right"};
        static const QStringList kHeats = {"none", "green_red", "red_green", "blue"};
        ColumnFormat f;
        const QString t = o.value(QStringLiteral("type")).toString(QStringLiteral("auto"));
        f.type = kTypes.contains(t) ? t : QStringLiteral("auto");
        f.decimals = std::clamp(o.value(QStringLiteral("dec")).toInt(-1), -1, 8);
        f.currency = o.value(QStringLiteral("ccy")).toString().trimmed().left(8);
        f.brackets = o.value(QStringLiteral("neg")).toString() == QLatin1String("brackets");
        const double d = o.value(QStringLiteral("div")).toDouble(1);
        f.divide = (d == 1000 || d == 1e6 || d == 1e9) ? d : 1;
        const QString a = o.value(QStringLiteral("align")).toString(QStringLiteral("auto"));
        f.align = kAligns.contains(a) ? a : QStringLiteral("auto");
        f.width = std::clamp(o.value(QStringLiteral("width")).toDouble(0), 0.0, 100.0);
        const QString h = o.value(QStringLiteral("heat")).toString(QStringLiteral("none"));
        f.heat = kHeats.contains(h) ? h : QStringLiteral("none");
        f.date_format = o.value(QStringLiteral("date_fmt")).toString().left(40);
        return f;
    }
};

struct TableStyle {
    QString zebra = QStringLiteral("auto"); ///< auto (from 5 rows) | on | off
    QString totals = QStringLiteral("none"); ///< none | sum (computed row) | last (last row is a total)
    bool header_repeat = true;               ///< header row on every page the table runs onto
    QString currency;                        ///< the table's currency (currency columns without their own)

    bool is_default() const {
        return zebra == QLatin1String("auto") && totals == QLatin1String("none") && header_repeat && currency.isEmpty();
    }
    bool operator==(const TableStyle& o) const {
        return zebra == o.zebra && totals == o.totals && header_repeat == o.header_repeat && currency == o.currency;
    }
    QJsonObject to_json() const {
        QJsonObject o;
        if (zebra != QLatin1String("auto"))
            o.insert(QStringLiteral("zebra"), zebra);
        if (totals != QLatin1String("none"))
            o.insert(QStringLiteral("totals"), totals);
        if (!header_repeat)
            o.insert(QStringLiteral("header_repeat"), false);
        if (!currency.isEmpty())
            o.insert(QStringLiteral("currency"), currency);
        return o;
    }
    static TableStyle from_json(const QJsonObject& o) {
        TableStyle s;
        const QString z = o.value(QStringLiteral("zebra")).toString(QStringLiteral("auto"));
        s.zebra = (z == QLatin1String("on") || z == QLatin1String("off")) ? z : QStringLiteral("auto");
        const QString t = o.value(QStringLiteral("totals")).toString(QStringLiteral("none"));
        s.totals = (t == QLatin1String("sum") || t == QLatin1String("last")) ? t : QStringLiteral("none");
        s.header_repeat = o.value(QStringLiteral("header_repeat")).toBool(true);
        s.currency = o.value(QStringLiteral("currency")).toString().trimmed().left(8);
        return s;
    }
};

// ── Reading / writing the config keys ───────────────────────────────────────

inline QVector<ColumnFormat> columns_from_text(const QString& text) {
    QVector<ColumnFormat> out;
    if (text.trimmed().isEmpty())
        return out;
    const QJsonArray arr = QJsonDocument::fromJson(text.toUtf8()).array();
    for (const QJsonValue& v : arr)
        out.append(ColumnFormat::from_json(v.toObject()));
    return out;
}

/// Compact JSON; empty when every column is the default (the key is then removed).
inline QString columns_to_text(QVector<ColumnFormat> cols) {
    while (!cols.isEmpty() && cols.last().is_default())
        cols.removeLast();
    if (cols.isEmpty())
        return {};
    QJsonArray arr;
    for (const ColumnFormat& c : cols)
        arr.append(c.to_json());
    return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

inline TableStyle style_from_text(const QString& text) {
    if (text.trimmed().isEmpty())
        return {};
    return TableStyle::from_json(QJsonDocument::fromJson(text.toUtf8()).object());
}

inline QString style_to_text(const TableStyle& s) {
    if (s.is_default())
        return {};
    return QString::fromUtf8(QJsonDocument(s.to_json()).toJson(QJsonDocument::Compact));
}

// ── Numbers in cells ────────────────────────────────────────────────────────

inline double nan_value() {
    return std::numeric_limits<double>::quiet_NaN();
}

/// The number a cell shows: "1,234.5", "(1,234)", "-1.2", "−1.2", "12.3%", "$1,234",
/// "€ 98", "1.2B", "3.4M", "5K", "2.1T", "12.3x", "+0.45". NaN for text, dates and
/// placeholders ("—", "-", "n/a"). `percent` (optional) tells whether a '%' was written.
/// Separators: with both ',' and '.' the last one is the decimal point; a lone ',' is a
/// thousands separator when exactly three digits follow it (else a decimal comma).
inline double parse_number(const QString& raw, bool* percent = nullptr) {
    if (percent)
        *percent = false;
    QString s = raw.trimmed();
    if (s.isEmpty())
        return nan_value();
    bool negative = false;
    if (s.startsWith(QLatin1Char('(')) && s.endsWith(QLatin1Char(')'))) {
        negative = true;
        s = s.mid(1, s.size() - 2).trimmed();
    }
    // Currency symbols / codes and spacing around the number.
    static const QString kStrip = QStringLiteral("$€£¥₹₩₽¢   ");
    QString t;
    t.reserve(s.size());
    for (QChar ch : s)
        if (!kStrip.contains(ch))
            t += ch;
    // A leading ISO code ("USD 1,234") or a trailing one ("1,234 USD") — exactly three
    // capitals next to the number, so "Q3 2024" or "FY2025" stay text.
    auto is_code = [](const QString& s, int at) {
        for (int i = at; i < at + 3; ++i)
            if (i < 0 || i >= s.size() || !(s.at(i) >= QLatin1Char('A') && s.at(i) <= QLatin1Char('Z')))
                return false;
        return true;
    };
    auto numeric_start = [](QChar ch) {
        return ch.isDigit() || ch == QLatin1Char('-') || ch == QLatin1Char('.') || ch == QChar(0x2212);
    };
    if (t.size() > 3 && is_code(t, 0) && numeric_start(t.at(3)))
        t.remove(0, 3);
    if (t.size() > 3 && is_code(t, static_cast<int>(t.size()) - 3) &&
        (t.at(t.size() - 4).isDigit() || t.at(t.size() - 4) == QLatin1Char('%')))
        t.chop(3);
    if (t.endsWith(QLatin1Char('%'))) {
        if (percent)
            *percent = true;
        t.chop(1);
    }
    double mult = 1;
    if (!t.isEmpty()) {
        const QChar last = t.at(t.size() - 1).toUpper();
        if (last == QLatin1Char('K')) {
            mult = 1e3;
            t.chop(1);
        } else if (last == QLatin1Char('M')) {
            mult = 1e6;
            t.chop(1);
        } else if (last == QLatin1Char('B')) {
            mult = 1e9;
            t.chop(1);
        } else if (last == QLatin1Char('T')) {
            mult = 1e12;
            t.chop(1);
        } else if (last == QLatin1Char('X')) {
            t.chop(1); // a multiple: "12.3x"
        }
    }
    t.replace(QChar(0x2212), QLatin1Char('-')); // unicode minus
    if (t.startsWith(QLatin1Char('+')))
        t.remove(0, 1);
    if (t.startsWith(QLatin1Char('-'))) {
        negative = !negative;
        t.remove(0, 1);
    }
    if (t.isEmpty())
        return nan_value();
    for (QChar ch : t)
        if (!ch.isDigit() && ch != QLatin1Char(',') && ch != QLatin1Char('.'))
            return nan_value();
    const int last_dot = t.lastIndexOf(QLatin1Char('.'));
    const int last_comma = t.lastIndexOf(QLatin1Char(','));
    if (last_dot >= 0 && last_comma >= 0) {
        const QChar dec = last_dot > last_comma ? QLatin1Char('.') : QLatin1Char(',');
        const QChar grp = dec == QLatin1Char('.') ? QLatin1Char(',') : QLatin1Char('.');
        t.remove(grp);
        t.replace(dec, QLatin1Char('.'));
    } else if (last_comma >= 0) {
        const int commas = static_cast<int>(t.count(QLatin1Char(',')));
        const int after = static_cast<int>(t.size()) - last_comma - 1;
        if (commas > 1 || after == 3)
            t.remove(QLatin1Char(','));
        else
            t.replace(QLatin1Char(','), QLatin1Char('.'));
    } else if (t.count(QLatin1Char('.')) > 1) {
        t.remove(QLatin1Char('.')); // "1.234.567" grouping
    }
    bool ok = false;
    double v = QLocale::c().toDouble(t, &ok);
    if (!ok || !std::isfinite(v))
        return nan_value();
    v *= mult;
    return negative ? -v : v;
}

/// Decimals the cell was typed with ("1,234.50" -> 2, "12" -> 0); -1 when not a number.
inline int typed_decimals(const QString& raw) {
    QString t = raw.trimmed();
    if (std::isnan(parse_number(t)))
        return -1;
    while (!t.isEmpty() && !t.at(t.size() - 1).isDigit())
        t.chop(1);
    const int dot = t.lastIndexOf(QLatin1Char('.'));
    const int comma = t.lastIndexOf(QLatin1Char(','));
    const int sep = std::max(dot, comma);
    if (sep < 0)
        return 0;
    const int after = static_cast<int>(t.size()) - sep - 1;
    if (sep == comma && dot < 0 && after == 3)
        return 0; // "1,234": grouping
    return after;
}

/// The legacy "looks numeric" test the renderer has always used for right-alignment.
inline bool looks_numeric(const QString& t) {
    if (t.isEmpty())
        return false;
    static const QString kAllowed = QStringLiteral(".,-+%$() BMKTx—−€£¥");
    for (QChar ch : t)
        if (!ch.isDigit() && !kAllowed.contains(ch))
            return false;
    return true;
}

// ── Formatting ──────────────────────────────────────────────────────────────

/// How numbers are printed. Defaults: '.' decimal point, ',' grouping, ISO code or a
/// common symbol for money. The page view injects the terminal's formatter.
struct NumberStyle {
    std::function<QString(double v, int decimals)> number;                       ///< grouped, |v|
    std::function<QString(double v, const QString& ccy, int decimals)> money;    ///< |v| with its unit
};

inline QString default_grouped(double v, int decimals) {
    QLocale c = QLocale::c();
    c.setNumberOptions(QLocale::DefaultNumberOptions); // grouping on
    return c.toString(v, 'f', std::clamp(decimals, 0, 8));
}

inline QString default_money(double v, const QString& ccy, int decimals) {
    const QString n = default_grouped(v, decimals);
    const QString c = ccy.trimmed().toUpper();
    if (c == QLatin1String("USD"))
        return QStringLiteral("$") + n;
    if (c == QLatin1String("EUR"))
        return QStringLiteral("€") + n;
    if (c == QLatin1String("GBP"))
        return QStringLiteral("£") + n;
    if (c == QLatin1String("JPY"))
        return QStringLiteral("¥") + n;
    if (c == QLatin1String("INR"))
        return QStringLiteral("₹") + n;
    if (c.isEmpty())
        return n;
    return c + QLatin1Char(' ') + n;
}

inline NumberStyle default_style() {
    return NumberStyle{&default_grouped, &default_money};
}

/// One cell as printed.
struct Cell {
    QString text;
    QString align = QStringLiteral("left"); ///< left | center | right
    bool numeric = false;
    double value = nan_value();
    QString background; ///< "#rrggbb" from a colour scale, empty = none (zebra is the writer's)
    bool total = false; ///< part of the totals row (bold, rule above)
};

struct FormattedTable {
    QVector<QVector<Cell>> rows; ///< row 0 = header when header_rows == 1
    int columns = 0;
    int header_rows = 1;          ///< 0 when the table has a single row
    bool header_repeat = true;
    bool zebra = false;
    int totals_row = -1;          ///< index in rows, -1 = none
    QVector<double> widths;       ///< percent per column (sums to 100); empty = writer's default
};

/// Format one value of column `f`. `raw` is the cell text; for "auto" and "text" columns,
/// and cells that are not numbers, the text is returned unchanged.
inline QString format_value(const QString& raw, const ColumnFormat& f, const QString& table_ccy, const NumberStyle& ns,
                            double* value_out = nullptr) {
    if (value_out)
        *value_out = nan_value();
    const QString& type = f.type;
    if (type == QLatin1String("auto") || type == QLatin1String("text"))
        return raw;
    if (type == QLatin1String("date")) {
        const QString t = raw.trimmed();
        QDate d = QDate::fromString(t.left(10), Qt::ISODate);
        if (!d.isValid())
            d = QDate::fromString(t, QStringLiteral("d MMM yyyy"));
        if (!d.isValid())
            d = QDate::fromString(t, QStringLiteral("dd/MM/yyyy"));
        if (!d.isValid())
            d = QDate::fromString(t, QStringLiteral("MM/dd/yyyy"));
        if (!d.isValid())
            return raw;
        return QLocale::c().toString(d, f.date_format.isEmpty() ? QStringLiteral("d MMM yyyy") : f.date_format);
    }
    bool had_percent = false;
    double v = parse_number(raw, &had_percent);
    if (std::isnan(v))
        return raw;
    if (f.divide != 1 && f.divide > 0)
        v /= f.divide;
    if (value_out)
        *value_out = v;
    const NumberStyle style = ns.number && ns.money ? ns : default_style();
    const double a = std::abs(v);
    QString printed;
    if (type == QLatin1String("percent")) {
        const int dec = f.decimals >= 0 ? f.decimals : 2;
        printed = style.number(a, dec) + QLatin1Char('%');
    } else if (type == QLatin1String("currency")) {
        const int dec = f.decimals >= 0 ? f.decimals : 2;
        printed = style.money(a, f.currency.isEmpty() ? table_ccy : f.currency, dec);
    } else { // number
        int dec = f.decimals;
        if (dec < 0)
            dec = f.divide != 1 ? 1 : std::max(0, typed_decimals(raw));
        printed = style.number(a, dec);
    }
    // A value that rounds to zero prints unsigned ("0.00", never "-0.00").
    bool all_zero = true;
    for (QChar ch : printed)
        if (ch.isDigit() && ch != QLatin1Char('0'))
            all_zero = false;
    if (v >= 0 || all_zero)
        return printed;
    return f.brackets ? QLatin1Char('(') + printed + QLatin1Char(')') : QLatin1Char('-') + printed;
}

/// "#rrggbb" for position t (0..1) of a colour scale, blended onto `paper_hex` so it works
/// on light and dark paper. Empty for "none".
inline QString heat_color(double t, const QString& scheme, const QString& paper_hex) {
    if (scheme == QLatin1String("none") || std::isnan(t))
        return {};
    t = std::clamp(t, 0.0, 1.0);
    auto hex = [](const QString& h, int i) {
        bool ok = false;
        const int v = h.mid(1 + 2 * i, 2).toInt(&ok, 16);
        return ok ? v : 255;
    };
    const QString paper = paper_hex.size() == 7 && paper_hex.startsWith(QLatin1Char('#')) ? paper_hex : QStringLiteral("#ffffff");
    const int pr = hex(paper, 0), pg = hex(paper, 1), pb = hex(paper, 2);
    int r, g, b;
    double strength;
    if (scheme == QLatin1String("blue")) {
        r = 59, g = 130, b = 246;
        strength = t;
    } else {
        // Two-sided: 0 = "bad" end, 1 = "good" end for green_red (high is green).
        const bool high_good = scheme == QLatin1String("green_red");
        const double good = high_good ? t : 1.0 - t;
        if (good >= 0.5) {
            r = 22, g = 163, b = 74; // green ink
            strength = (good - 0.5) * 2.0;
        } else {
            r = 220, g = 38, b = 38; // red ink
            strength = (0.5 - good) * 2.0;
        }
    }
    const double alpha = 0.10 + 0.32 * strength; // a tint: ink stays readable on it
    auto mix = [alpha](int paper_c, int ink_c) {
        return std::clamp(static_cast<int>(std::lround(paper_c * (1.0 - alpha) + ink_c * alpha)), 0, 255);
    };
    return QStringLiteral("#%1%2%3")
        .arg(mix(pr, r), 2, 16, QLatin1Char('0'))
        .arg(mix(pg, g), 2, 16, QLatin1Char('0'))
        .arg(mix(pb, b), 2, 16, QLatin1Char('0'));
}

/// The table as printed. `grid` = the parsed cells (row 0 the header when there are two or
/// more rows); `max_rows` / `max_cols` cap what is printed (the renderer's 200 x 20).
inline FormattedTable format_table(const QVector<QStringList>& grid, const TableStyle& style,
                                   const QVector<ColumnFormat>& cols, const NumberStyle& ns,
                                   const QString& paper_hex = QStringLiteral("#ffffff"), int max_rows = 200,
                                   int max_cols = 20) {
    FormattedTable out;
    int ncols = 0;
    for (const QStringList& r : grid)
        ncols = std::max(ncols, static_cast<int>(r.size()));
    ncols = std::clamp(ncols, 1, std::max(1, max_cols));
    out.columns = ncols;
    const int nrows = std::clamp(static_cast<int>(grid.size()), 1, std::max(1, max_rows));
    out.header_rows = nrows > 1 ? 1 : 0;
    out.header_repeat = style.header_repeat && out.header_rows > 0;
    auto fmt_of = [&cols](int c) { return c < cols.size() ? cols[c] : ColumnFormat(); };

    for (int r = 0; r < nrows; ++r) {
        QVector<Cell> row;
        row.reserve(ncols);
        const QStringList src = r < grid.size() ? grid[r] : QStringList();
        for (int c = 0; c < ncols; ++c) {
            const QString raw = c < src.size() ? src[c] : QString();
            const ColumnFormat f = fmt_of(c);
            Cell cell;
            if (r < out.header_rows) {
                cell.text = raw;
                cell.align = c > 0 ? QStringLiteral("right") : QStringLiteral("left"); // as always
            } else {
                double v = nan_value();
                cell.text = format_value(raw, f, style.currency, ns, &v);
                if (std::isnan(v) && f.type != QLatin1String("date") && f.type != QLatin1String("text"))
                    v = parse_number(raw);
                cell.value = v;
                cell.numeric = !std::isnan(v) && f.type != QLatin1String("text") && f.type != QLatin1String("date");
                const bool legacy_right = c > 0 && looks_numeric(cell.text);
                cell.align = (f.type == QLatin1String("auto") ? legacy_right : (cell.numeric && c > 0))
                                 ? QStringLiteral("right")
                                 : QStringLiteral("left");
            }
            if (f.align != QLatin1String("auto"))
                cell.align = f.align;
            row.append(cell);
        }
        out.rows.append(row);
    }

    const int body_first = out.header_rows;
    int body_last = static_cast<int>(out.rows.size()) - 1; // inclusive
    if (style.totals == QLatin1String("last") && body_last >= body_first) {
        out.totals_row = body_last;
        for (Cell& c : out.rows[body_last])
            c.total = true;
        --body_last; // colour scales leave the total out
    } else if (style.totals == QLatin1String("sum") && body_last >= body_first) {
        QVector<Cell> total(ncols);
        for (int c = 0; c < ncols; ++c) {
            const ColumnFormat f = fmt_of(c);
            Cell& t = total[c];
            t.total = true;
            t.align = out.rows[body_first][c].align;
            if (c == 0) {
                t.text = QStringLiteral("Total");
                t.align = f.align == QLatin1String("auto") ? QStringLiteral("left") : f.align;
                continue;
            }
            if (f.type == QLatin1String("percent") || f.type == QLatin1String("text") || f.type == QLatin1String("date"))
                continue;
            double sum = 0;
            int n = 0, dec = 0;
            for (int r = body_first; r <= body_last; ++r) {
                const Cell& cell = out.rows[r][c];
                if (!cell.numeric)
                    continue;
                sum += cell.value;
                ++n;
                const int td = typed_decimals(r < grid.size() && c < grid[r].size() ? grid[r][c] : QString());
                dec = std::max(dec, td);
            }
            if (n == 0)
                continue;
            ColumnFormat tf = f;
            if (tf.type == QLatin1String("auto")) {
                tf.type = QStringLiteral("number");
                tf.decimals = dec;
            }
            tf.divide = 1; // the cells were divided already
            t.value = sum;
            t.numeric = true;
            t.text = format_value(QLocale::c().toString(sum, 'f', 8), tf, style.currency, ns);
            t.align = f.align == QLatin1String("auto") ? QStringLiteral("right") : f.align;
        }
        out.rows.append(total);
        out.totals_row = static_cast<int>(out.rows.size()) - 1;
    }

    // Conditional colour scales over the body (header and totals excluded).
    for (int c = 0; c < ncols; ++c) {
        const ColumnFormat f = fmt_of(c);
        if (f.heat == QLatin1String("none"))
            continue;
        double lo = std::numeric_limits<double>::max(), hi = std::numeric_limits<double>::lowest();
        for (int r = body_first; r <= body_last; ++r) {
            const Cell& cell = out.rows[r][c];
            if (!cell.numeric)
                continue;
            lo = std::min(lo, cell.value);
            hi = std::max(hi, cell.value);
        }
        if (lo > hi)
            continue;
        for (int r = body_first; r <= body_last; ++r) {
            Cell& cell = out.rows[r][c];
            if (!cell.numeric)
                continue;
            const double t = hi > lo ? (cell.value - lo) / (hi - lo) : 0.5;
            cell.background = heat_color(t, f.heat, paper_hex);
        }
    }

    const int body_rows = static_cast<int>(out.rows.size()) - out.header_rows - (out.totals_row >= 0 ? 1 : 0);
    out.zebra = style.zebra == QLatin1String("on") ||
                (style.zebra == QLatin1String("auto") && static_cast<int>(out.rows.size()) > 4 && body_rows > 0);

    // Widths: explicit percentages, the rest shared; none set -> the writer's default.
    bool any_width = false;
    double fixed = 0;
    int free_cols = 0;
    for (int c = 0; c < ncols; ++c) {
        const double w = fmt_of(c).width;
        if (w > 0) {
            any_width = true;
            fixed += w;
        } else {
            ++free_cols;
        }
    }
    if (any_width) {
        const double scale = fixed > 100 ? 100.0 / fixed : 1.0;
        const double rest = std::max(0.0, 100.0 - fixed * scale);
        for (int c = 0; c < ncols; ++c) {
            const double w = fmt_of(c).width;
            out.widths.append(w > 0 ? w * scale : (free_cols > 0 ? rest / free_cols : 0));
        }
    }
    return out;
}

/// The default column widths the page has always used: a wider first column from three
/// columns on, the rest equal (percent).
inline QVector<double> default_widths(int cols) {
    QVector<double> w;
    if (cols <= 0)
        return w;
    if (cols >= 3) {
        const double first = std::min(36.0, 100.0 / cols * 1.8);
        w.append(first);
        for (int c = 1; c < cols; ++c)
            w.append((100.0 - first) / (cols - 1));
    } else {
        for (int c = 0; c < cols; ++c)
            w.append(100.0 / cols);
    }
    return w;
}

} // namespace fincept::report::tablefmt
