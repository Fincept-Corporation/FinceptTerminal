#pragma once
// ReportBlocks.h — build Report Builder blocks from a neutral payload.
//
// Any tab (or service) that wants to put a result into a report fills a
// ReportPayload — title, source, as-of, KPIs, tables, charts, text / Markdown, an
// echo of the inputs and an optional refresh recipe — and hands it to
// services::ReportInbox::append_blocks() (services/report_builder/ReportInbox.h).
// The builders below map it onto the component types the canvas already renders
// (heading, text, stats_block, table, chart, image, ...); ids are 0 (the service
// assigns them). Header-only, Qt Core only, no UI: safe to include anywhere.
//
// Component keys written here (all inside ReportComponent::config, so files and the
// cloud JSON stay compatible):
//   heading     content; config level (1..3)
//   text        content (Markdown when config format=markdown); config style=caption for small print
//   stats_block config title, data ("Label: Value" per line), tones (comma list: positive|negative|warning|)
//   table       config title, csv (ReportTableCodec), rows, cols, note
//   chart       config chart_type (line|bar|pie|donut|candle), title, data, labels, series (JSON
//               [{name, values[]}] for several series), ohlc (JSON [[o,h,l,c],...] for candle), width
//   image       config path (set once the bytes are stored), caption, width, align
//   every data block: as_of, source_label, and bind (ReportBinding.h) when refreshable

#include "core/report/ReportBinding.h"
#include "core/report/ReportDocument.h"
#include "core/report/ReportTableCodec.h"

#include <QByteArray>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPair>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QVector>

#include <cmath>

namespace fincept::report {

// ── Neutral payload ──────────────────────────────────────────────────────────

struct PayloadKpi {
    QString label;
    QString value; ///< already formatted by the sender ("1,234.5", "12.3")
    QString unit;  ///< "%", "x", "USD", "bp" ... ("%" is attached without a space)
    QString tone;  ///< "" | "positive" | "negative" | "warning" — printed ink for the value
};

struct PayloadTable {
    QString title;
    QStringList columns;       ///< header row
    QVector<QStringList> rows; ///< the same shape ChartExport::save_csv takes
    QString note;              ///< small print under the table (units, "showing ...")
    Binding bind;              ///< optional refresh recipe for this table
};

struct PayloadSeries {
    QString name;
    QStringList x;     ///< category / date labels
    QVector<double> y; ///< values (close for candles)
    QVector<double> open, high, low; ///< candles only (same length as y)
};

struct PayloadChart {
    QString title;
    QString kind = QStringLiteral("line"); ///< line | bar | candle | donut
    QByteArray image;                      ///< EITHER encoded image bytes (PNG / JPEG) ...
    QString image_format = QStringLiteral("png");
    QVector<PayloadSeries> series;         ///< ... OR native series the report draws itself
    QString caption;
    int width = 640; ///< px on a 96-dpi page (max ~680 on A4 portrait)
    Binding bind;    ///< optional refresh recipe (e.g. history_binding)
};

struct ReportPayload {
    QString title;     ///< heading above the blocks (empty = none)
    QString source;    ///< "Derivatives", "Chat Mode" ... — provenance line + undo text
    QDateTime as_of;   ///< when the numbers were computed (invalid = now)
    QString markdown;  ///< text / Markdown; "#" headings, pipe tables and ``` fences become their own blocks
    QString kpi_title; ///< title of the KPI block (default "Key figures")
    QVector<PayloadKpi> kpis;
    Binding kpi_bind; ///< optional refresh recipe for the KPI block
    QVector<PayloadTable> tables;
    QVector<PayloadChart> charts;
    QVector<QPair<QString, QString>> inputs; ///< "inputs echo": the assumptions behind the result
    QString notes;                           ///< trailing text (disclaimer, method note)
    /// Recipe applied to the first data block that has none of its own (KPI, chart, table).
    Binding refresh;

    bool is_empty() const {
        return title.trimmed().isEmpty() && markdown.trimmed().isEmpty() && kpis.isEmpty() && tables.isEmpty() &&
               charts.isEmpty() && inputs.isEmpty() && notes.trimmed().isEmpty();
    }
};

struct BuildOptions {
    bool heading = true;           ///< payload title as a heading
    bool provenance = true;        ///< "Source: <source> · as of <time>" caption at the end
    bool page_break_before = false;
    int max_table_rows = 500;      ///< longer tables are cut with a visible note
};

/// An image block whose bytes still have to be stored (ReportInbox writes them into
/// the report assets folder off the GUI thread, then sets config["path"]).
struct PendingImage {
    int block_index = -1;
    QByteArray bytes;
    QString format; ///< "png" | "jpg"
};

struct BuiltBlocks {
    QVector<ReportComponent> blocks;
    QVector<PendingImage> images;
};

// ── Small builders ──────────────────────────────────────────────────────────

namespace blocks {

inline QString number_text(double v) {
    if (!std::isfinite(v))
        return QString();
    return QString::number(v, 'g', 12);
}

inline QString as_of_text(const QDateTime& when) {
    return when.isValid() ? when.toLocalTime().toString(QStringLiteral("d MMM yyyy HH:mm")) : QString();
}

inline void stamp(ReportComponent& c, const QString& source_label, const QDateTime& as_of) {
    if (!source_label.isEmpty())
        c.config.insert(QLatin1String(keys::kSourceLabel), source_label);
    if (as_of.isValid())
        c.config.insert(QLatin1String(keys::kAsOf), format_as_of(as_of));
}

inline ReportComponent heading(const QString& text, int level = 2) {
    ReportComponent c;
    c.type = QStringLiteral("heading");
    c.content = text.trimmed();
    c.config.insert(QStringLiteral("level"), QString::number(qBound(1, level, 3)));
    return c;
}

inline ReportComponent text(const QString& markdown) {
    ReportComponent c;
    c.type = QStringLiteral("text");
    c.content = markdown.trimmed();
    c.config.insert(QStringLiteral("format"), QStringLiteral("markdown"));
    return c;
}

/// Small print in the report's meta ink (provenance, notes under results).
inline ReportComponent caption(const QString& text) {
    ReportComponent c;
    c.type = QStringLiteral("text");
    c.content = text.trimmed();
    c.config.insert(QStringLiteral("style"), QStringLiteral("caption"));
    return c;
}

inline ReportComponent provenance(const QString& source_label, const QDateTime& as_of) {
    QString line;
    if (!source_label.isEmpty())
        line = QStringLiteral("Source: ") + source_label;
    const QString when = as_of_text(as_of);
    if (!when.isEmpty())
        line += (line.isEmpty() ? QStringLiteral("As of ") : QStringLiteral(" · as of ")) + when;
    ReportComponent c = caption(line);
    c.config.insert(QStringLiteral("role"), QStringLiteral("provenance"));
    return c;
}

inline ReportComponent page_break() {
    ReportComponent c;
    c.type = QStringLiteral("page_break");
    return c;
}

inline ReportComponent table(const QString& title, const QStringList& columns, const QVector<QStringList>& rows,
                             const QString& note = {}, int max_rows = 500) {
    ReportComponent c;
    c.type = QStringLiteral("table");
    QVector<QStringList> grid;
    if (!columns.isEmpty())
        grid.append(columns);
    const int keep = max_rows > 0 ? qMin(static_cast<int>(rows.size()), max_rows) : static_cast<int>(rows.size());
    for (int i = 0; i < keep; ++i)
        grid.append(rows[i]);
    int cols = 0;
    for (const QStringList& r : grid)
        cols = qMax(cols, static_cast<int>(r.size()));
    c.config.insert(QStringLiteral("csv"), table::encode(grid));
    c.config.insert(QStringLiteral("rows"), QString::number(qMax(1, static_cast<int>(grid.size()))));
    c.config.insert(QStringLiteral("cols"), QString::number(qMax(1, cols)));
    if (!title.trimmed().isEmpty())
        c.config.insert(QStringLiteral("title"), title.trimmed());
    QString n = note.trimmed();
    if (keep < rows.size()) {
        const QString cut = QStringLiteral("Showing the first %1 of %2 rows.").arg(keep).arg(rows.size());
        n = n.isEmpty() ? cut : n + QLatin1Char(' ') + cut;
    }
    if (!n.isEmpty())
        c.config.insert(QStringLiteral("note"), n);
    return c;
}

/// KPI strip → stats_block ("Label: Value unit" lines, two pairs per printed row).
inline ReportComponent kpis(const QString& title, const QVector<PayloadKpi>& items) {
    ReportComponent c;
    c.type = QStringLiteral("stats_block");
    c.config.insert(QStringLiteral("title"), title.trimmed().isEmpty() ? QStringLiteral("Key figures") : title.trimmed());
    QStringList lines, tones;
    bool any_tone = false;
    for (const PayloadKpi& k : items) {
        QString label = k.label.trimmed();
        label.replace(QLatin1Char(':'), QChar(0x2236)); // the block splits each line at its first ':'
        label.replace(QLatin1Char('\n'), QLatin1Char(' '));
        QString value = k.value.trimmed();
        value.replace(QLatin1Char('\n'), QLatin1Char(' '));
        const QString unit = k.unit.trimmed();
        if (!unit.isEmpty() && !value.isEmpty())
            value += unit == QLatin1String("%") ? unit : QLatin1Char(' ') + unit;
        lines << label + QStringLiteral(": ") + (value.isEmpty() ? QStringLiteral("—") : value);
        tones << k.tone.trimmed();
        any_tone = any_tone || !k.tone.trimmed().isEmpty();
    }
    c.config.insert(QStringLiteral("data"), lines.join(QLatin1Char('\n')));
    if (any_tone)
        c.config.insert(QStringLiteral("tones"), tones.join(QLatin1Char(',')));
    return c;
}

inline ReportComponent inputs_echo(const QVector<QPair<QString, QString>>& inputs, const QString& title = {}) {
    QVector<PayloadKpi> items;
    items.reserve(inputs.size());
    for (const auto& p : inputs)
        items.append({p.first, p.second, {}, {}});
    ReportComponent c = kpis(title.isEmpty() ? QStringLiteral("Inputs") : title, items);
    c.config.insert(QStringLiteral("role"), QStringLiteral("inputs"));
    return c;
}

inline QString values_csv(const QVector<double>& v) {
    QStringList out;
    out.reserve(v.size());
    for (double d : v)
        out << number_text(d);
    return out.join(QLatin1Char(','));
}

/// A chart the report draws itself from series.
inline ReportComponent chart(const PayloadChart& ch) {
    ReportComponent c;
    c.type = QStringLiteral("chart");
    QString kind = ch.kind.trimmed().toLower();
    if (kind == QLatin1String("pie"))
        kind = QStringLiteral("donut");
    if (kind != QLatin1String("bar") && kind != QLatin1String("candle") && kind != QLatin1String("donut"))
        kind = QStringLiteral("line");
    c.config.insert(QStringLiteral("chart_type"), kind);
    c.config.insert(QStringLiteral("title"), ch.title.trimmed().isEmpty() ? QStringLiteral("Chart") : ch.title.trimmed());
    c.config.insert(QStringLiteral("width"), QString::number(qBound(200, ch.width, 760)));
    if (!ch.caption.trimmed().isEmpty())
        c.config.insert(QStringLiteral("caption"), ch.caption.trimmed());
    if (ch.series.isEmpty())
        return c;
    const PayloadSeries& first = ch.series.first();
    c.config.insert(QStringLiteral("labels"), table::encode_list(first.x));
    c.config.insert(QStringLiteral("data"), values_csv(first.y));
    if (kind == QLatin1String("candle")) {
        QJsonArray ohlc;
        for (int i = 0; i < first.y.size(); ++i) {
            const double cl = first.y[i];
            const double o = i < first.open.size() ? first.open[i] : cl;
            const double h = i < first.high.size() ? first.high[i] : qMax(o, cl);
            const double l = i < first.low.size() ? first.low[i] : qMin(o, cl);
            ohlc.append(QJsonArray{o, h, l, cl});
        }
        c.config.insert(QStringLiteral("ohlc"), QString::fromUtf8(QJsonDocument(ohlc).toJson(QJsonDocument::Compact)));
    } else if (ch.series.size() > 1 && kind != QLatin1String("donut")) {
        QJsonArray arr;
        for (const PayloadSeries& s : ch.series) {
            QJsonArray vals;
            for (double d : s.y)
                vals.append(std::isfinite(d) ? QJsonValue(d) : QJsonValue());
            arr.append(QJsonObject{{QStringLiteral("name"), s.name}, {QStringLiteral("values"), vals}});
        }
        c.config.insert(QStringLiteral("series"), QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)));
    }
    return c;
}

/// An image block whose file does not exist yet (see PendingImage).
inline ReportComponent pending_image(const PayloadChart& ch) {
    ReportComponent c;
    c.type = QStringLiteral("image");
    c.config.insert(QStringLiteral("width"), QString::number(qBound(120, ch.width, 760)));
    c.config.insert(QStringLiteral("align"), QStringLiteral("center"));
    QString cap = ch.caption.trimmed();
    if (cap.isEmpty())
        cap = ch.title.trimmed();
    if (!cap.isEmpty())
        c.config.insert(QStringLiteral("caption"), cap);
    c.config.insert(QStringLiteral("pending_image"), QStringLiteral("1"));
    return c;
}

/// An empty block of the right type for a recipe: the Report Builder fills it on refresh.
inline ReportComponent bound(const Binding& b, const QString& title = {}) {
    ReportComponent c;
    const QString& v = b.view;
    if (v == QLatin1String(view::kQuote))
        c.type = QStringLiteral("market_data");
    else if (v == QLatin1String(view::kKpi))
        c.type = QStringLiteral("stats_block");
    else if (v == QLatin1String(view::kChart))
        c.type = QStringLiteral("chart");
    else if (v == QLatin1String(view::kList))
        c.type = QStringLiteral("list");
    else if (v == QLatin1String(view::kSparkline))
        c.type = QStringLiteral("sparkline");
    else
        c.type = QStringLiteral("table");
    if (!title.isEmpty())
        c.config.insert(QStringLiteral("title"), title);
    if (c.type == QLatin1String("chart"))
        c.config.insert(QStringLiteral("chart_type"),
                        b.param("chart", b.source == QLatin1String(source::kPortfolio) ? QStringLiteral("donut")
                                                                                        : QStringLiteral("line")));
    set_binding(c, b);
    return c;
}

// ── Markdown → blocks ───────────────────────────────────────────────────────

inline QStringList split_pipe_row(QString line) {
    line = line.trimmed();
    if (line.startsWith(QLatin1Char('|')))
        line.remove(0, 1);
    if (line.endsWith(QLatin1Char('|')) && !line.endsWith(QLatin1String("\\|")))
        line.chop(1);
    QStringList cells;
    QString cell;
    for (int i = 0; i < line.size(); ++i) {
        const QChar ch = line[i];
        if (ch == QLatin1Char('\\') && i + 1 < line.size() && line[i + 1] == QLatin1Char('|')) {
            cell += QLatin1Char('|');
            ++i;
        } else if (ch == QLatin1Char('|')) {
            cells << cell.trimmed();
            cell.clear();
        } else {
            cell += ch;
        }
    }
    cells << cell.trimmed();
    for (QString& c : cells) { // inline emphasis has no meaning in a cell of the report table
        if (c.startsWith(QLatin1String("**")) && c.endsWith(QLatin1String("**")) && c.size() > 4)
            c = c.mid(2, c.size() - 4);
    }
    return cells;
}

inline bool is_table_separator(const QString& line) {
    static const QRegularExpression re(QStringLiteral("^\\s*\\|?\\s*:?-{2,}:?\\s*(\\|\\s*:?-{2,}:?\\s*)*\\|?\\s*$"));
    return re.match(line).hasMatch();
}

/// Markdown → heading / text / table / code blocks: "#" headings become heading blocks,
/// GitHub pipe tables become table blocks, ``` fences become code blocks, everything
/// else (paragraphs, lists, emphasis, links) stays Markdown in text blocks.
inline QVector<ReportComponent> from_markdown(const QString& markdown) {
    QVector<ReportComponent> out;
    const QStringList lines = QString(markdown).replace(QStringLiteral("\r\n"), QStringLiteral("\n")).split(QLatin1Char('\n'));
    QStringList para;
    auto flush = [&]() {
        const QString body = para.join(QLatin1Char('\n')).trimmed();
        para.clear();
        if (!body.isEmpty())
            out.append(text(body));
    };
    static const QRegularExpression heading_re(QStringLiteral("^\\s{0,3}(#{1,6})\\s+(.*?)\\s*#*\\s*$"));
    for (int i = 0; i < lines.size(); ++i) {
        const QString& line = lines[i];
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith(QLatin1String("```"))) {
            flush();
            QStringList code;
            int j = i + 1;
            while (j < lines.size() && !lines[j].trimmed().startsWith(QLatin1String("```")))
                code << lines[j++];
            ReportComponent c;
            c.type = QStringLiteral("code");
            c.content = code.join(QLatin1Char('\n'));
            if (!c.content.trimmed().isEmpty())
                out.append(c);
            i = j; // the closing fence (or the end)
            continue;
        }
        const auto hm = heading_re.match(line);
        if (hm.hasMatch() && !hm.captured(2).isEmpty()) {
            flush();
            out.append(heading(hm.captured(2), hm.captured(1).size()));
            continue;
        }
        if (trimmed.startsWith(QLatin1Char('|')) && i + 1 < lines.size() && is_table_separator(lines[i + 1])) {
            flush();
            const QStringList header = split_pipe_row(trimmed);
            QVector<QStringList> rows;
            int j = i + 2;
            while (j < lines.size() && lines[j].trimmed().startsWith(QLatin1Char('|')))
                rows.append(split_pipe_row(lines[j++]));
            out.append(table(QString(), header, rows));
            i = j - 1;
            continue;
        }
        para << line;
    }
    flush();
    return out;
}

} // namespace blocks

// ── Payload → blocks ────────────────────────────────────────────────────────

/// Maps a payload onto component blocks (ids 0) in this order: page break, heading,
/// KPIs, Markdown, charts, tables, inputs, notes, provenance. Data blocks carry
/// as_of + source_label; recipes become config["bind"]. Image charts come back as
/// image blocks listed in `images` (their bytes still have to be stored).
inline BuiltBlocks build_blocks(const ReportPayload& p, const BuildOptions& opt = {}) {
    BuiltBlocks out;
    const QDateTime as_of = p.as_of.isValid() ? p.as_of : QDateTime::currentDateTime();
    const QString src = p.source.trimmed();
    bool refresh_used = !p.refresh.is_valid();
    auto take_refresh = [&](ReportComponent& c, const Binding& own) {
        if (own.is_valid()) {
            set_binding(c, own);
        } else if (!refresh_used) {
            set_binding(c, p.refresh);
            refresh_used = true;
        }
    };

    if (opt.page_break_before)
        out.blocks.append(blocks::page_break());
    if (opt.heading && !p.title.trimmed().isEmpty())
        out.blocks.append(blocks::heading(p.title, 2));

    if (!p.kpis.isEmpty()) {
        ReportComponent c = blocks::kpis(p.kpi_title, p.kpis);
        blocks::stamp(c, src, as_of);
        take_refresh(c, p.kpi_bind);
        out.blocks.append(c);
    }
    if (!p.markdown.trimmed().isEmpty()) {
        for (ReportComponent c : blocks::from_markdown(p.markdown)) {
            if (c.type == QLatin1String("table"))
                blocks::stamp(c, src, as_of);
            out.blocks.append(c);
        }
    }
    for (const PayloadChart& ch : p.charts) {
        if (!ch.image.isEmpty()) {
            ReportComponent c = blocks::pending_image(ch);
            blocks::stamp(c, src, as_of);
            out.images.append({static_cast<int>(out.blocks.size()), ch.image,
                               ch.image_format.isEmpty() ? QStringLiteral("png") : ch.image_format.toLower()});
            out.blocks.append(c);
        } else if (!ch.series.isEmpty() || ch.bind.is_valid()) {
            ReportComponent c = blocks::chart(ch);
            blocks::stamp(c, src, as_of);
            take_refresh(c, ch.bind);
            out.blocks.append(c);
        }
    }
    for (const PayloadTable& t : p.tables) {
        ReportComponent c = blocks::table(t.title, t.columns, t.rows, t.note, opt.max_table_rows);
        blocks::stamp(c, src, as_of);
        take_refresh(c, t.bind);
        out.blocks.append(c);
    }
    if (!p.inputs.isEmpty())
        out.blocks.append(blocks::inputs_echo(p.inputs));
    if (!p.notes.trimmed().isEmpty())
        out.blocks.append(blocks::caption(p.notes));
    if (opt.provenance && (!src.isEmpty() || p.as_of.isValid()))
        out.blocks.append(blocks::provenance(src, as_of));
    return out;
}

} // namespace fincept::report
