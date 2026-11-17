// ReportRenderer.cpp — see ReportRenderer.h.
//
// Literal colours in this file are DOCUMENT ink (the printed page): chart palettes,
// callout fills, green / red price ink. They follow the report theme, not the terminal.

#include "screens/report_builder/ReportRenderer.h"

#include "core/config/AppPaths.h"
#include "core/report/ReportBinding.h"
#include "core/report/ReportTableCodec.h"
#include "core/report/ReportVariables.h"
#include "screens/report_builder/ReportChartObject.h"
#include "services/report_builder/ReportAssets.h"
#include "services/report_builder/ReportChartRenderer.h"

#include <QAbstractTextDocumentLayout>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QImage>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFrame>
#include <QTextImageFormat>
#include <QTextList>
#include <QTextTable>
#include <QtMath>

#include <algorithm>
#include <cmath>

namespace fincept::screens::rb {

namespace rep = ::fincept::report;

// ── Geometry ────────────────────────────────────────────────────────────────

double mm_to_px(double mm) {
    return mm * 96.0 / 25.4;
}

PageGeometry page_geometry(const ReportMetadata& meta) {
    PageGeometry g;
    const QSizeF mm = rep::page_size_mm(meta);
    g.page = QSizeF(mm_to_px(mm.width()), mm_to_px(mm.height()));
    g.margins = QMarginsF(mm_to_px(meta.margin_left_mm), mm_to_px(meta.margin_top_mm), mm_to_px(meta.margin_right_mm),
                          mm_to_px(meta.margin_bottom_mm));
    g.has_header = !meta.header_left.isEmpty() || !meta.header_center.isEmpty() || !meta.header_right.isEmpty();
    g.has_footer = !meta.footer_left.isEmpty() || !meta.footer_center.isEmpty() || !meta.footer_right.isEmpty() ||
                   meta.show_page_numbers;
    g.band = mm_to_px(8);
    const double top = g.margins.top() + (g.has_header ? g.band : 0);
    const double bottom = g.margins.bottom() + (g.has_footer ? g.band : 0);
    const double w = std::max(120.0, g.page.width() - g.margins.left() - g.margins.right());
    const double h = std::max(160.0, g.page.height() - top - bottom);
    g.content = QRectF(g.margins.left(), top, w, h);
    return g;
}

namespace {

// Every report document is laid out against this 96-dpi device, so line breaks and
// page breaks are the same on every machine, on screen and in every export.
QPaintDevice* rr_layout_device() {
    static QImage device = [] {
        QImage img(1, 1, QImage::Format_ARGB32_Premultiplied);
        img.setDotsPerMeterX(3780); // 96 dpi
        img.setDotsPerMeterY(3780);
        return img;
    }();
    return &device;
}

QColor rr_ink(const QString& hex) {
    return QColor(hex);
}

const char* const kRrRed = "#dc2626";    // printed red ink
const char* const kRrGreen = "#16a34a";  // printed green ink
const char* const kRrAmber = "#d97706";  // printed amber ink


// A rule as wide as the text column, drawn with box-drawing glyphs in `color`.
void rr_rule(QTextCursor& cursor, const RenderContext& ctx, const QString& color, double pt) {
    QTextCharFormat f;
    f.setForeground(rr_ink(color));
    f.setFontPointSize(ctx.pt(pt));
    QFont font;
    font.setFamilies({ctx.font_family});
    font.setPointSizeF(ctx.pt(pt));
    const QFontMetricsF fm(font, rr_layout_device());
    const double adv = std::max(1.0, fm.horizontalAdvance(QChar(0x2500)));
    const int n = std::clamp(static_cast<int>(std::floor(ctx.content_width * 0.98 / adv)), 4, 400);
    cursor.insertText(QString(n, QChar(0x2500)), f);
}

QTextCharFormat rr_fmt(const RenderContext& ctx, double pt, const QString& color, bool bold = false,
                       bool italic = false) {
    QTextCharFormat f;
    f.setFontPointSize(ctx.pt(pt));
    f.setForeground(rr_ink(color));
    if (bold)
        f.setFontWeight(QFont::Bold);
    if (italic)
        f.setFontItalic(true);
    return f;
}

void rr_hint(QTextCursor& cursor, const RenderContext& ctx, const QString& text) {
    cursor.insertText(text, rr_fmt(ctx, 11, ctx.theme.meta_color, false, true));
}

// ── Charts ──────────────────────────────────────────────────────────────────

QVector<double> rr_doubles(const QString& csv) {
    QVector<double> vals;
    for (const QString& tok : csv.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        bool ok = false;
        const double v = tok.trimmed().toDouble(&ok);
        if (ok)
            vals << v;
    }
    return vals;
}

// A chart as a picture file (HTML export pre-renders them; tests and any caller without a
// chart handler land here): the painter-based report renderer, cached by content.
QString rr_chart_picture(const ReportComponent& c, const RenderContext& ctx, const QSize& size) {
    const report::chart::ChartSpec spec = report::chart::from_config(c.config);
    const QString key = services::report_chart::cache_key(spec, ctx.chart_palette, ctx.font_family, size, 3.0);
    const QString path = chart_cache_dir() + QLatin1Char('/') + key + QStringLiteral(".png");
    if (QFileInfo(path).size() > 0)
        return path;
    const QByteArray png = services::report_chart::render_png(spec, ctx.chart_palette, ctx.font_family, size, 3.0);
    if (png.isEmpty())
        return {};
    QDir().mkpath(chart_cache_dir());
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(png) != png.size() || !f.commit())
        return {};
    return path;
}

QString rr_render_sparkline(const QString& data, const RenderContext& ctx, int width, int height) {
    const QVector<double> values = rr_doubles(data);
    if (values.size() < 2)
        return {};
    const QString key = QStringList{QStringLiteral("spark3"), data, QString::number(width), QString::number(height),
                                    ctx.theme.page_bg, ctx.theme.accent_color}
                            .join(QChar(0x1F));
    const QString path = chart_cache_dir() + QLatin1Char('/') +
                         QString::fromLatin1(QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha1).toHex()) +
                         QStringLiteral(".png");
    if (QFileInfo(path).size() > 0)
        return path;
    QImage img(width * 2, height * 2, QImage::Format_ARGB32_Premultiplied);
    img.fill(QColor(ctx.theme.page_bg));
    const auto [mn, mx] = std::minmax_element(values.begin(), values.end());
    const double lo = *mn, hi = *mx, span = hi > lo ? hi - lo : 1.0;
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    QPen pen(QColor(ctx.theme.accent_color));
    pen.setWidthF(3.0);
    p.setPen(pen);
    QPolygonF poly;
    for (int i = 0; i < values.size(); ++i)
        poly << QPointF(2 + i * (img.width() - 4.0) / (values.size() - 1),
                        img.height() - 3 - (values[i] - lo) / span * (img.height() - 6.0));
    p.drawPolyline(poly);
    p.end();
    QDir().mkpath(chart_cache_dir());
    return img.save(path, "PNG") ? path : QString();
}

// ── Provenance / notes under data blocks ───────────────────────────────────

void rr_provenance(QTextCursor& cursor, const ReportComponent& c, const RenderContext& ctx) {
    const QDateTime as_of = rep::as_of_of(c);
    const QString source = c.config.value(QLatin1String(rep::keys::kSourceLabel));
    const QString role = c.config.value(QStringLiteral("role"));
    BlockNote note;
    if (!ctx.for_export && ctx.note_of)
        note = ctx.note_of(c.id);
    const bool stale = !ctx.for_export && rep::is_stale(c, QDateTime::currentDateTime()) && as_of.isValid();
    if ((!as_of.isValid() && source.isEmpty() && note.kind == BlockNote::Kind::None) || role == QLatin1String("provenance"))
        return;
    QString line;
    if (!source.isEmpty())
        line = QStringLiteral("Source: ") + source;
    if (as_of.isValid())
        line += (line.isEmpty() ? QStringLiteral("As of ") : QStringLiteral(" · as of ")) +
                as_of.toLocalTime().toString(QStringLiteral("d MMM yyyy HH:mm"));
    cursor.insertBlock(QTextBlockFormat(), QTextCharFormat());
    QTextBlockFormat bf;
    bf.setTopMargin(1);
    bf.setBottomMargin(6);
    if (c.type == QLatin1String("chart") || c.type == QLatin1String("image"))
        bf.setAlignment(Qt::AlignHCenter);
    cursor.setBlockFormat(bf);
    if (!line.isEmpty())
        cursor.insertText(line, rr_fmt(ctx, 8, ctx.theme.meta_color));
    if (stale)
        cursor.insertText(QStringLiteral(" · out of date — Refresh Data updates it"), rr_fmt(ctx, 8, QString::fromLatin1(kRrAmber)));
    if (note.kind == BlockNote::Kind::Loading)
        cursor.insertText(QStringLiteral("  Refreshing…"), rr_fmt(ctx, 8, ctx.theme.accent_color, true));
    else if (note.kind == BlockNote::Kind::Failed)
        cursor.insertText(QStringLiteral("  Refresh failed: %1%2")
                              .arg(note.text, as_of.isValid() ? QStringLiteral(" — showing the data above") : QString()),
                          rr_fmt(ctx, 8, QString::fromLatin1(kRrRed)));
}

[[maybe_unused]] bool rr_numeric(const QString& t) {
    if (t.isEmpty())
        return false;
    for (QChar ch : t) {
        if (!ch.isDigit() && !QStringLiteral(".,-+%$() BMKTx—−€£¥").contains(ch))
            return false;
    }
    return true;
}

// Leaves the cursor on the block right after `table`.
void rr_exit_table(QTextCursor& cursor, QTextTable* table) {
    cursor.setPosition(table->lastPosition() + 1);
}

} // namespace

QString resolve_font_family(const QString& requested) {
    const QString r = requested.trimmed();
    if (!r.isEmpty() && QFontDatabase::hasFamily(r))
        return r;
    return QGuiApplication::font().family();
}

RenderContext make_context(const QVector<ReportComponent>& comps, const ReportMetadata& meta, const ReportTheme& theme,
                           bool for_export) {
    RenderContext ctx;
    ctx.theme = theme;
    ctx.meta = meta;
    ctx.font_family = resolve_font_family(meta.font_family);
    ctx.scale = std::clamp(meta.font_size, 6, 72) / 12.0;
    ctx.for_export = for_export;
    ctx.content_width = page_geometry(meta).content.width();
    ctx.vars = rep::vars::context(meta);
    ctx.chart_palette = rep::chart::palette_for(theme);
    for (const auto& c : comps) {
        if (c.type == QLatin1String("page_break"))
            continue;
        ctx.has_cover = c.type == QLatin1String("cover");
        break;
    }
    rep::ReportDocument outline_doc;
    outline_doc.components = comps;
    outline_doc.metadata = meta;
    for (const rep::outline::Heading& h : rep::outline::headings(outline_doc)) {
        if (!h.number.isEmpty())
            ctx.heading_numbers.insert(h.id, h.number);
        const int idx = outline_doc.index_of(h.id);
        if (for_export && idx >= 0 && is_unfilled(comps[idx]))
            continue;
        ctx.toc_headings << rep::outline::heading_label(h);
        ctx.heading_ids << h.id;
        ctx.heading_levels << h.level;
    }
    return ctx;
}

QSize chart_size(const ReportComponent& c, const RenderContext& ctx) {
    const int want_w = c.config.value(QStringLiteral("width"), QStringLiteral("640")).toInt();
    const int w = std::clamp(want_w, 160, std::max(160, static_cast<int>(ctx.content_width)));
    const double aspect = rep::chart::from_config(c.config).effective_aspect();
    return QSize(w, std::max(80, static_cast<int>(std::lround(w * aspect))));
}

rep::tablefmt::FormattedTable formatted_table(const ReportComponent& in, const RenderContext& ctx) {
    namespace tf = rep::tablefmt;
    const ReportComponent c = ctx.vars.isEmpty() ? in : rep::vars::expanded_component(in, ctx.vars);
    QVector<QStringList> grid = rep::table::parse(c.config.value(QStringLiteral("csv")));
    if (grid.isEmpty()) {
        // The oldest table format ({"r_c": text} with rows / cols), or a new empty table:
        // rows x cols with "Header N" placeholders, as the page has always drawn them.
        const QJsonObject cell_data = QJsonDocument::fromJson(c.config.value(QStringLiteral("data")).toUtf8()).object();
        const int rows = std::clamp(c.config.value(QStringLiteral("rows"), QStringLiteral("3")).toInt(), 1, 200);
        const int cols = std::clamp(c.config.value(QStringLiteral("cols"), QStringLiteral("3")).toInt(), 1, 20);
        for (int r = 0; r < rows; ++r) {
            QStringList row;
            for (int k = 0; k < cols; ++k) {
                const QString key = QStringLiteral("%1_%2").arg(r).arg(k);
                row << (cell_data.contains(key) ? cell_data.value(key).toString()
                        : r == 0                ? QStringLiteral("Header %1").arg(k + 1)
                                                : QString());
            }
            grid.append(row);
        }
    }
    return tf::format_table(grid, tf::style_from_text(c.config.value(QLatin1String(tf::kStyleKey))),
                            tf::columns_from_text(c.config.value(QLatin1String(tf::kColFmtKey))), ctx.number_style,
                            ctx.theme.page_bg);
}

QTextDocument* new_section_document(const RenderContext& ctx, QObject* parent) {
    auto* doc = new QTextDocument(parent);
    doc->setUndoRedoEnabled(false);
    doc->documentLayout()->setPaintDevice(rr_layout_device());
    doc->setUseDesignMetrics(true);
    doc->setDocumentMargin(0);
    QFont f;
    f.setFamilies({ctx.font_family, QStringLiteral("Arial")});
    f.setPointSizeF(ctx.pt(12));
    doc->setDefaultFont(f);
    const PageGeometry g = page_geometry(ctx.meta);
    doc->setPageSize(g.content.size());
    return doc;
}

void write_header(QTextCursor& cursor, const RenderContext& ctx) {
    if (ctx.has_cover) {
        // The cover page carries the title: the section starts with an empty spacer instead.
        write_section_spacer(cursor);
        return;
    }
    QTextBlockFormat title_bf;
    title_bf.setBottomMargin(2);
    cursor.setBlockFormat(title_bf);
    cursor.insertText(ctx.vars.value(QStringLiteral("title"), ctx.meta.title),
                      rr_fmt(ctx, 24, ctx.theme.heading_color, true));

    cursor.insertBlock(QTextBlockFormat(), QTextCharFormat());
    QTextBlockFormat meta_bf;
    meta_bf.setTopMargin(2);
    meta_bf.setBottomMargin(4);
    cursor.setBlockFormat(meta_bf);
    QStringList parts;
    const QString author = ctx.vars.value(QStringLiteral("author"), ctx.meta.author);
    const QString company = ctx.vars.value(QStringLiteral("company"), ctx.meta.company);
    if (!author.trimmed().isEmpty())
        parts << author;
    if (!company.trimmed().isEmpty())
        parts << company;
    if (!ctx.meta.date.trimmed().isEmpty())
        parts << ctx.meta.date;
    cursor.insertText(parts.join(QStringLiteral("  ·  ")), rr_fmt(ctx, 10, ctx.theme.meta_color));

    cursor.insertBlock(QTextBlockFormat(), QTextCharFormat());
    QTextBlockFormat line_bf;
    line_bf.setTopMargin(2);
    line_bf.setBottomMargin(12);
    cursor.setBlockFormat(line_bf);
    rr_rule(cursor, ctx, ctx.theme.accent_color, 8);
}

void write_section_spacer(QTextCursor& cursor) {
    QTextBlockFormat bf;
    bf.setLineHeight(1, QTextBlockFormat::FixedHeight);
    bf.setTopMargin(0);
    bf.setBottomMargin(0);
    cursor.setBlockFormat(bf);
    QTextCharFormat cf;
    cf.setFontPointSize(1);
    cursor.setCharFormat(cf);
}

bool is_unfilled(const ReportComponent& c) {
    const auto cfg = [&c](const char* key) { return c.config.value(QString::fromLatin1(key)).trimmed(); };
    if (c.type == "heading" || c.type == "text" || c.type == "code" || c.type == "quote")
        return c.content.trimmed().isEmpty();
    if (c.type == "callout")
        return c.content.trimmed().isEmpty() && cfg("heading").isEmpty();
    if (c.type == "list")
        return c.content.split(QLatin1Char('\n'), Qt::SkipEmptyParts).isEmpty();
    if (c.type == "table") {
        if (!rep::table::parse(c.config.value("csv")).isEmpty())
            return false;
        if (rep::is_bound(c))
            return true; // a recipe that was never fetched
        const QJsonDocument doc = QJsonDocument::fromJson(c.config.value("data").toUtf8());
        return !doc.isObject() || doc.object().isEmpty(); // only "Header N" placeholders
    }
    if (c.type == "chart")
        return cfg("data").isEmpty() && cfg("series").isEmpty() && cfg("ohlc").isEmpty();
    if (c.type == "stats_block" || c.type == "sparkline")
        return cfg("data").isEmpty();
    if (c.type == "image") // a picture on this computer, or one carried with the report
        return services::report_assets::picture_path(c).isEmpty() &&
               c.config.value(QLatin1String(services::report_assets::kB64Key)).isEmpty();
    if (c.type == "market_data")
        return cfg("symbol").isEmpty() || cfg("price").isEmpty();
    return false; // divider, page_break, toc
}

void write_component(QTextCursor& cursor, const ReportComponent& comp_in, const RenderContext& ctx) {
    cursor.insertBlock(QTextBlockFormat(), QTextCharFormat());
    const int start = cursor.position();
    // {{variables}} expanded in every printed text; unresolved ones stay as typed (marked
    // below on screen; exports warn before they print them).
    const ReportComponent comp = ctx.vars.isEmpty() ? comp_in : rep::vars::expanded_component(comp_in, ctx.vars);
    const ReportTheme& th = ctx.theme;
    const QString& type = comp.type;
    const bool bound = rep::is_bound(comp);

    if (type == QLatin1String("heading")) {
        const int level = std::clamp(comp.config.value(QStringLiteral("level"), QStringLiteral("1")).toInt(), 1, 3);
        QTextBlockFormat hbf;
        hbf.setTopMargin(level == 1 ? 18 : level == 2 ? 12 : 8);
        hbf.setBottomMargin(level == 1 ? 2 : 6);
        hbf.setPageBreakPolicy(QTextFormat::PageBreak_Auto);
        cursor.setBlockFormat(hbf);
        const QString number = ctx.heading_numbers.value(comp.id);
        const QString text = comp.content.isEmpty() ? QStringLiteral("Heading") : comp.content;
        cursor.insertText(number.isEmpty() ? text : number + QStringLiteral("  ") + text,
                          rr_fmt(ctx, level == 1 ? 18 : level == 2 ? 15 : 13, th.heading_color, true));
        if (level == 1) {
            cursor.insertBlock(QTextBlockFormat(), QTextCharFormat());
            QTextBlockFormat rule_bf;
            rule_bf.setTopMargin(0);
            rule_bf.setBottomMargin(8);
            cursor.setBlockFormat(rule_bf);
            rr_rule(cursor, ctx, th.accent_color, 6);
        }

    } else if (type == QLatin1String("text")) {
        QTextBlockFormat bf;
        bf.setLineHeight(140, QTextBlockFormat::ProportionalHeight);
        bf.setTopMargin(2);
        bf.setBottomMargin(8);
        const QString style = comp.config.value(QStringLiteral("style"));
        if (style == QLatin1String("caption")) {
            bf.setLineHeight(120, QTextBlockFormat::ProportionalHeight);
            bf.setBottomMargin(6);
        }
        cursor.setBlockFormat(bf);
        if (comp.content.trimmed().isEmpty()) {
            const QString ph = comp.config.value(QStringLiteral("placeholder"));
            rr_hint(cursor, ctx, ph.isEmpty() ? QStringLiteral("Enter text here...") : QStringLiteral("✎ ") + ph);
        } else if (style == QLatin1String("caption")) {
            cursor.insertText(comp.content, rr_fmt(ctx, 9, th.meta_color, false, true));
        } else {
            QTextCharFormat fmt = rr_fmt(ctx, 12, th.text_color);
            cursor.setCharFormat(fmt);
            const QString& body = comp.content;
            const bool markdown = comp.config.value(QStringLiteral("format")) == QLatin1String("markdown") ||
                                  body.contains(QLatin1String("**")) || body.contains(QLatin1String("__")) ||
                                  body.contains(QLatin1String("\n- ")) || body.contains(QLatin1String("\n* ")) ||
                                  body.contains(QLatin1String("\n#")) || body.startsWith(QLatin1String("- "));
            if (markdown)
                cursor.insertMarkdown(body);
            else
                cursor.insertText(body, fmt);
        }

    } else if (type == QLatin1String("table")) {
        const QVector<QStringList> rows_data = rep::table::parse(comp.config.value(QStringLiteral("csv")));
        bool legacy_cells = false;
        if (rows_data.isEmpty() && comp.config.contains(QStringLiteral("data")))
            legacy_cells = QJsonDocument::fromJson(comp.config.value(QStringLiteral("data")).toUtf8()).isObject();
        const QString title = comp.config.value(QStringLiteral("title")).trimmed();
        if (!title.isEmpty()) {
            QTextBlockFormat tb;
            tb.setTopMargin(4);
            tb.setBottomMargin(4);
            cursor.setBlockFormat(tb);
            cursor.insertText(title, rr_fmt(ctx, 11, th.heading_color, true));
            cursor.insertBlock(QTextBlockFormat(), QTextCharFormat());
        }
        if (rows_data.isEmpty() && !legacy_cells && bound) {
            rr_hint(cursor, ctx, QStringLiteral("[%1 — no data yet: press Refresh Data]").arg(rep::describe(rep::binding_of(comp))));
        } else {
            int wanted_rows, wanted_cols;
            if (!rows_data.isEmpty()) {
                wanted_rows = static_cast<int>(rows_data.size());
                wanted_cols = 0;
                for (const auto& r : rows_data)
                    wanted_cols = std::max(wanted_cols, static_cast<int>(r.size()));
            } else {
                wanted_rows = comp.config.value(QStringLiteral("rows"), QStringLiteral("3")).toInt();
                wanted_cols = comp.config.value(QStringLiteral("cols"), QStringLiteral("3")).toInt();
            }
            // Column formats, alignment, zebra, totals, colour scales: the shared table format
            // (core/report/ReportTableFormat.h), so the PDF, .odt and Markdown agree with the page.
            const rep::tablefmt::FormattedTable ft = formatted_table(comp, ctx);
            const int rows = std::max(1, static_cast<int>(ft.rows.size()));
            const int cols = std::max(1, ft.columns);

            QTextTableFormat tf;
            tf.setBorderBrush(QColor(th.divider_color));
            tf.setBorder(0.6);
            tf.setBorderStyle(QTextFrameFormat::BorderStyle_Solid);
            tf.setBorderCollapse(true);
            tf.setCellPadding(4);
            tf.setCellSpacing(0);
            tf.setWidth(QTextLength(QTextLength::PercentageLength, 100));
            tf.setTopMargin(2);
            tf.setBottomMargin(2);
            if (ft.header_rows > 0 && ft.header_repeat)
                tf.setHeaderRowCount(1); // repeated on every page the table runs onto
            const QVector<double> pct = ft.widths.isEmpty() ? rep::tablefmt::default_widths(cols) : ft.widths;
            QVector<QTextLength> widths;
            for (double w : pct)
                widths.append(QTextLength(QTextLength::PercentageLength, w));
            tf.setColumnWidthConstraints(widths);
            QTextTable* table = cursor.insertTable(rows, cols, tf);
            const QColor zebra = QColor::fromRgbF(
                QColor(th.page_bg).redF() * 0.965 + QColor(th.text_color).redF() * 0.035,
                QColor(th.page_bg).greenF() * 0.965 + QColor(th.text_color).greenF() * 0.035,
                QColor(th.page_bg).blueF() * 0.965 + QColor(th.text_color).blueF() * 0.035);
            const double cell_pt = rows > 30 || cols > 7 ? 9 : 10;
            auto align_of = [](const QString& a) {
                return a == QLatin1String("right")    ? Qt::AlignRight
                       : a == QLatin1String("center") ? Qt::AlignHCenter
                                                      : Qt::AlignLeft;
            };
            for (int r = 0; r < rows && r < ft.rows.size(); ++r) {
                for (int c = 0; c < cols && c < ft.rows[r].size(); ++c) {
                    const rep::tablefmt::Cell& cell = ft.rows[r][c];
                    QTextCharFormat cf;
                    QTextBlockFormat cb;
                    cb.setAlignment(align_of(cell.align));
                    if (r < ft.header_rows) {
                        cf = rr_fmt(ctx, cell_pt, th.table_header_fg, true);
                        cb.setBackground(QColor(th.table_header_bg));
                    } else {
                        cf = rr_fmt(ctx, cell_pt, th.text_color, cell.total);
                        if (!cell.background.isEmpty())
                            cb.setBackground(QColor(cell.background));
                        else if (ft.zebra && r % 2 == 0 && !cell.total)
                            cb.setBackground(zebra);
                    }
                    QTextTableCell tc = table->cellAt(r, c);
                    if (cell.total) {
                        QTextTableCellFormat tcf;
                        tcf.setTopBorder(1.4);
                        tcf.setTopBorderStyle(QTextFrameFormat::BorderStyle_Solid);
                        tcf.setTopBorderBrush(QColor(th.text_color));
                        tc.setFormat(tcf);
                    }
                    QTextCursor cc = tc.firstCursorPosition();
                    cc.setBlockFormat(cb);
                    cc.insertText(cell.text, cf);
                }
            }
            rr_exit_table(cursor, table);
            QStringList notes;
            const QString note = comp.config.value(QStringLiteral("note")).trimmed();
            if (!note.isEmpty())
                notes << note;
            if (wanted_rows > 200 || wanted_cols > 20)
                notes << QStringLiteral("Table truncated: showing %1 of %2 rows and %3 of %4 columns.")
                             .arg(std::min(wanted_rows, 200))
                             .arg(wanted_rows)
                             .arg(std::min(wanted_cols, 20))
                             .arg(wanted_cols);
            QTextBlockFormat after;
            after.setTopMargin(2);
            after.setBottomMargin(6);
            cursor.setBlockFormat(after);
            if (!notes.isEmpty())
                cursor.insertText(notes.join(QLatin1Char(' ')), rr_fmt(ctx, 8, th.meta_color, false, true));
        }

    } else if (type == QLatin1String("code")) {
        QTextBlockFormat bf;
        bf.setBackground(QColor(th.code_bg));
        bf.setLeftMargin(12);
        bf.setRightMargin(12);
        bf.setTopMargin(4);
        bf.setBottomMargin(8);
        cursor.setBlockFormat(bf);
        QTextCharFormat fmt = rr_fmt(ctx, 10, th.text_color);
        fmt.setFontFamilies({QStringLiteral("Consolas"), QStringLiteral("DejaVu Sans Mono"), QStringLiteral("Courier New")});
        cursor.insertText(comp.content.isEmpty() ? QStringLiteral("// code block") : comp.content, fmt);

    } else if (type == QLatin1String("divider")) {
        QTextBlockFormat bf;
        bf.setTopMargin(4);
        bf.setBottomMargin(4);
        cursor.setBlockFormat(bf);
        rr_rule(cursor, ctx, th.divider_color, 8);

    } else if (type == QLatin1String("quote")) {
        QTextBlockFormat bf;
        bf.setLeftMargin(24);
        bf.setRightMargin(8);
        bf.setTopMargin(6);
        bf.setBottomMargin(8);
        bf.setBackground(QColor(th.quote_bg));
        cursor.setBlockFormat(bf);
        const QTextCharFormat fmt = rr_fmt(ctx, 12, th.meta_color, false, true);
        cursor.setCharFormat(fmt);
        const QString body = comp.content.isEmpty() ? QStringLiteral("Block quote...") : comp.content;
        if (body.contains(QLatin1String("**")) || body.contains(QLatin1String("__")))
            cursor.insertMarkdown(body);
        else
            cursor.insertText(body, fmt);

    } else if (type == QLatin1String("list")) {
        QStringList items = comp.content.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        if (items.isEmpty()) {
            rr_hint(cursor, ctx, bound ? QStringLiteral("[%1 — no data yet: press Refresh Data]").arg(rep::describe(rep::binding_of(comp)))
                                       : QStringLiteral("Item 1 / Item 2 — one item per line in the Properties panel"));
        } else {
            QTextBlockFormat outer;
            outer.setTopMargin(2);
            outer.setBottomMargin(2);
            cursor.setBlockFormat(outer);
            QTextListFormat lf;
            lf.setStyle(QTextListFormat::ListDisc);
            lf.setIndent(1);
            const QTextCharFormat item_fmt = rr_fmt(ctx, 11, th.text_color);
            QTextList* list = cursor.createList(lf);
            for (int i = 0; i < items.size(); ++i) {
                if (i > 0) {
                    cursor.insertBlock();
                    list->add(cursor.block());
                }
                cursor.setCharFormat(item_fmt);
                const QString& item = items[i];
                if (item.contains(QLatin1String("**")) || item.contains(QLatin1String("__")))
                    cursor.insertMarkdown(item);
                else
                    cursor.insertText(item, item_fmt);
            }
            // A plain block after the list so the next component is not a list item.
            cursor.insertBlock(QTextBlockFormat(), QTextCharFormat());
            if (QTextList* l = cursor.currentList())
                l->remove(cursor.block());
            QTextBlockFormat reset;
            reset.setIndent(0);
            reset.setBottomMargin(4);
            cursor.setBlockFormat(reset);
        }

    } else if (type == QLatin1String("chart")) {
        const QString title = comp.config.value(QStringLiteral("title"), QStringLiteral("Chart"));
        QTextBlockFormat bf;
        bf.setTopMargin(6);
        bf.setBottomMargin(2);
        bf.setAlignment(Qt::AlignHCenter);
        cursor.setBlockFormat(bf);
        if (is_unfilled(comp)) {
            rr_hint(cursor, ctx, bound ? QStringLiteral("[%1 — no data yet: press Refresh Data]").arg(rep::describe(rep::binding_of(comp)))
                                       : QStringLiteral("[%1 — enter data in the Properties panel]").arg(title));
        } else {
            const QSize size = chart_size(comp, ctx);
            const QString pre = ctx.chart_images.value(comp.id);
            if (ctx.chart_object && pre.isEmpty()) {
                // A fixed-size object: drawn off the GUI thread on screen (placeholder until
                // then, no relayout when it arrives), as vectors in the PDF.
                const QString key = ctx.chart_object->add(rep::chart::from_config(comp.config), ctx.chart_palette,
                                                          ctx.font_family, size);
                cursor.insertText(QString(QChar::ObjectReplacementCharacter),
                                  ReportChartObject::object_format(key, QSizeF(size)));
            } else {
                const QString img = pre.isEmpty() ? rr_chart_picture(comp, ctx, size) : pre;
                if (img.isEmpty()) {
                    rr_hint(cursor, ctx, QStringLiteral("[Chart could not be drawn]"));
                } else {
                    QTextImageFormat f;
                    f.setName(img);
                    f.setWidth(size.width());
                    f.setHeight(size.height());
                    cursor.insertImage(f);
                }
            }
            const QString caption = comp.config.value(QStringLiteral("caption")).trimmed();
            if (!caption.isEmpty()) {
                cursor.insertBlock(QTextBlockFormat(), QTextCharFormat());
                QTextBlockFormat cb;
                cb.setAlignment(Qt::AlignHCenter);
                cursor.setBlockFormat(cb);
                cursor.insertText(caption, rr_fmt(ctx, 9, th.meta_color, false, true));
            }
            if (comp.config.value(QStringLiteral("sample")) == QLatin1String("1")) {
                // Template placeholder numbers must never pass for real data.
                cursor.insertBlock(QTextBlockFormat(), QTextCharFormat());
                QTextBlockFormat sb;
                sb.setAlignment(Qt::AlignHCenter);
                cursor.setBlockFormat(sb);
                cursor.insertText(QStringLiteral("SAMPLE DATA — placeholder values from the template, replace before publishing"),
                                  rr_fmt(ctx, 9, QString::fromLatin1(kRrRed), true));
            }
        }

    } else if (type == QLatin1String("image")) {
        QString path = services::report_assets::picture_path(comp);
        if (path.isEmpty())
            path = services::report_assets::materialize_one(comp); // carried with the report, not yet on disk
        const QString align = comp.config.value(QStringLiteral("align"), QStringLiteral("left"));
        const Qt::Alignment al = align == QLatin1String("center") ? Qt::AlignHCenter
                                 : align == QLatin1String("right") ? Qt::AlignRight
                                                                   : Qt::AlignLeft;
        QTextBlockFormat bf;
        bf.setAlignment(al);
        bf.setTopMargin(4);
        bf.setBottomMargin(2);
        cursor.setBlockFormat(bf);
        if (!path.isEmpty()) {
            const QSize px = QImageReader(path).size(); // the header only: no decode here
            const int w = std::clamp(comp.config.value(QStringLiteral("width"), QStringLiteral("400")).toInt(), 40,
                                     static_cast<int>(ctx.content_width));
            QTextImageFormat f;
            f.setName(path);
            f.setWidth(w);
            if (px.isValid() && px.width() > 0)
                f.setHeight(w * px.height() / double(px.width()));
            cursor.insertImage(f);
        } else if (services::report_assets::picture_not_synced(comp)) {
            QString name = QFileInfo(comp.config.value(QStringLiteral("path"))).fileName();
            if (name.isEmpty())
                name = QStringLiteral("the picture");
            rr_hint(cursor, ctx, QStringLiteral("[Image not synced: %1 is stored on the computer it was added on (too "
                                                "large to travel with the report) — add it again here]")
                                     .arg(name));
        } else {
            rr_hint(cursor, ctx, QStringLiteral("[Image — browse or paste in the Properties panel]"));
        }
        const QString caption = comp.config.value(QStringLiteral("caption")).trimmed();
        if (!caption.isEmpty()) {
            cursor.insertBlock(QTextBlockFormat(), QTextCharFormat());
            QTextBlockFormat cb;
            cb.setAlignment(al);
            cb.setBottomMargin(6);
            cursor.setBlockFormat(cb);
            cursor.insertText(caption, rr_fmt(ctx, 9, th.meta_color, false, true));
        }

    } else if (type == QLatin1String("cover")) {
        // A title page of its own: logo, title, subtitle, byline, date, disclaimer at the foot.
        const double h = page_geometry(ctx.meta).content.height();
        QString logo = services::report_assets::picture_path(comp);
        if (logo.isEmpty())
            logo = services::report_assets::materialize_one(comp);
        QTextBlockFormat top;
        top.setAlignment(Qt::AlignLeft);
        top.setTopMargin(h * 0.06);
        cursor.setBlockFormat(top);
        double used = h * 0.06;
        if (!logo.isEmpty()) {
            const QSize px = QImageReader(logo).size();
            const double w = std::min(ctx.content_width * 0.32, 200.0);
            QTextImageFormat f;
            f.setName(logo);
            f.setWidth(w);
            double lh = w * 0.5;
            if (px.isValid() && px.width() > 0)
                lh = w * px.height() / double(px.width());
            f.setHeight(lh);
            cursor.insertImage(f);
            used += lh;
        }
        const QString title = comp.config.value(QStringLiteral("title")).trimmed().isEmpty()
                                  ? ctx.vars.value(QStringLiteral("title"), ctx.meta.title)
                                  : comp.config.value(QStringLiteral("title")).trimmed();
        cursor.insertBlock(QTextBlockFormat(), QTextCharFormat());
        QTextBlockFormat tb;
        tb.setTopMargin(std::max(12.0, h * 0.26 - used));
        tb.setBottomMargin(6);
        cursor.setBlockFormat(tb);
        cursor.insertText(title, rr_fmt(ctx, 28, th.heading_color, true));
        const QString subtitle = comp.config.value(QStringLiteral("subtitle")).trimmed();
        if (!subtitle.isEmpty()) {
            cursor.insertBlock(QTextBlockFormat(), QTextCharFormat());
            QTextBlockFormat sb;
            sb.setBottomMargin(4);
            cursor.setBlockFormat(sb);
            cursor.insertText(subtitle, rr_fmt(ctx, 15, th.text_color));
        }
        cursor.insertBlock(QTextBlockFormat(), QTextCharFormat());
        QTextBlockFormat rb;
        rb.setTopMargin(8);
        rb.setBottomMargin(8);
        cursor.setBlockFormat(rb);
        rr_rule(cursor, ctx, th.accent_color, 8);
        QString byline = comp.config.value(QStringLiteral("byline")).trimmed();
        if (byline.isEmpty()) {
            QStringList parts;
            for (const char* k : {"author", "company"}) {
                const QString v = ctx.vars.value(QLatin1String(k)).trimmed();
                if (!v.isEmpty())
                    parts << v;
            }
            byline = parts.join(QStringLiteral("  ·  "));
        }
        if (!byline.isEmpty()) {
            cursor.insertBlock(QTextBlockFormat(), QTextCharFormat());
            cursor.insertText(byline, rr_fmt(ctx, 12, th.text_color));
        }
        QString date = comp.config.value(QStringLiteral("date")).trimmed();
        if (date.isEmpty())
            date = ctx.meta.date;
        if (!date.isEmpty()) {
            cursor.insertBlock(QTextBlockFormat(), QTextCharFormat());
            cursor.insertText(date, rr_fmt(ctx, 11, th.meta_color));
        }
        const QString disclaimer = comp.config.value(QStringLiteral("disclaimer")).trimmed();
        if (!disclaimer.isEmpty()) {
            cursor.insertBlock(QTextBlockFormat(), QTextCharFormat());
            QTextBlockFormat db;
            db.setTopMargin(h * 0.30);
            db.setLineHeight(120, QTextBlockFormat::ProportionalHeight);
            cursor.setBlockFormat(db);
            cursor.insertText(disclaimer, rr_fmt(ctx, 8, th.meta_color, false, true));
        }
        // The report starts on the next page.
        QTextBlockFormat last = cursor.blockFormat();
        last.setPageBreakPolicy(QTextFormat::PageBreak_AlwaysAfter);
        cursor.setBlockFormat(last);

    } else if (type == QLatin1String("market_data")) {
        const QString symbol = comp.config.value(QStringLiteral("symbol"));
        const QString price = comp.config.value(QStringLiteral("price"));
        const QString chg_pct = comp.config.value(QStringLiteral("change_pct"));
        const QString name = comp.config.value(QStringLiteral("name"), symbol);
        const QString high = comp.config.value(QStringLiteral("high"));
        const QString low = comp.config.value(QStringLiteral("low"));
        const QString volume = comp.config.value(QStringLiteral("volume"));
        const QString ccy = comp.config.value(QStringLiteral("currency"));
        QTextBlockFormat bf;
        bf.setBackground(QColor(th.quote_bg));
        bf.setLeftMargin(10);
        bf.setTopMargin(4);
        bf.setBottomMargin(2);
        cursor.setBlockFormat(bf);
        if (symbol.isEmpty()) {
            rr_hint(cursor, ctx, QStringLiteral("[Market data — pick a symbol in the Properties panel]"));
        } else if (price.isEmpty()) {
            rr_hint(cursor, ctx, QStringLiteral("[%1 — no quote yet: press Refresh Data]").arg(symbol));
        } else {
            cursor.insertText(symbol + QStringLiteral("  "), rr_fmt(ctx, 13, th.accent_color, true));
            if (!name.isEmpty() && name != symbol)
                cursor.insertText(name + QStringLiteral("  "), rr_fmt(ctx, 11, th.meta_color));
            cursor.insertText(price + (ccy.isEmpty() ? QString() : QLatin1Char(' ') + ccy) + QStringLiteral("  "),
                              rr_fmt(ctx, 13, th.text_color, true));
            if (!chg_pct.isEmpty()) {
                const bool negative = chg_pct.startsWith(QLatin1Char('-'));
                const QString sign = (!negative && !chg_pct.startsWith(QLatin1Char('+'))) ? QStringLiteral("+") : QString();
                cursor.insertText(sign + chg_pct + QLatin1Char('%'),
                                  rr_fmt(ctx, 12, QString::fromLatin1(negative ? kRrRed : kRrGreen), true));
            }
            if (!high.isEmpty() || !low.isEmpty() || !volume.isEmpty()) {
                cursor.insertBlock(QTextBlockFormat(), QTextCharFormat());
                cursor.setBlockFormat(bf);
                QString detail;
                if (!high.isEmpty() && !low.isEmpty())
                    detail += QStringLiteral("H: ") + high + QStringLiteral("  L: ") + low + QStringLiteral("  ");
                if (!volume.isEmpty()) {
                    bool ok = false;
                    const double vol = QString(volume).remove(QLatin1Char(',')).toDouble(&ok);
                    QString vs = volume;
                    if (ok) {
                        if (vol >= 1e9)
                            vs = QString::number(vol / 1e9, 'f', 2) + QLatin1Char('B');
                        else if (vol >= 1e6)
                            vs = QString::number(vol / 1e6, 'f', 2) + QLatin1Char('M');
                        else if (vol >= 1e3)
                            vs = QString::number(vol / 1e3, 'f', 1) + QLatin1Char('K');
                    }
                    detail += QStringLiteral("Vol: ") + vs;
                }
                cursor.insertText(detail, rr_fmt(ctx, 10, th.meta_color));
            }
        }

    } else if (type == QLatin1String("page_break")) {
        // Sections are split at page breaks by the caller; nothing to draw here.

    } else if (type == QLatin1String("toc")) {
        QTextBlockFormat hb;
        hb.setTopMargin(6);
        hb.setBottomMargin(4);
        cursor.setBlockFormat(hb);
        cursor.insertText(QStringLiteral("Table of Contents"), rr_fmt(ctx, 14, th.heading_color, true));
        if (ctx.toc_headings.isEmpty()) {
            cursor.insertBlock(QTextBlockFormat(), QTextCharFormat());
            rr_hint(cursor, ctx, QStringLiteral("  (No headings found in document)"));
        } else {
            for (int i = 0; i < ctx.toc_headings.size(); ++i) {
                cursor.insertBlock(QTextBlockFormat(), QTextCharFormat());
                QTextBlockFormat eb;
                const int level = ctx.heading_levels.value(i, 1);
                eb.setLeftMargin(12 + (level - 1) * 14);
                eb.setTopMargin(1);
                eb.setBottomMargin(1);
                eb.setTabPositions({QTextOption::Tab(ctx.content_width - 16, QTextOption::RightTab)});
                cursor.setBlockFormat(eb);
                // Numbered reports show the heading numbers; otherwise a running count.
                if (ctx.heading_numbers.isEmpty())
                    cursor.insertText(QStringLiteral("%1.  ").arg(i + 1), rr_fmt(ctx, 11, th.accent_color, true));
                cursor.insertText(ctx.toc_headings[i], rr_fmt(ctx, level == 1 ? 11 : 10.5, th.text_color, level == 1));
                const int page = ctx.heading_pages.value(ctx.heading_ids.value(i), 0);
                if (page > 0)
                    cursor.insertText(QStringLiteral("\t%1").arg(page), rr_fmt(ctx, 11, th.meta_color));
            }
            cursor.insertBlock(QTextBlockFormat(), QTextCharFormat());
            QTextBlockFormat end;
            end.setBottomMargin(4);
            cursor.setBlockFormat(end);
        }

    } else if (type == QLatin1String("stats_block")) {
        const QString title = comp.config.value(QStringLiteral("title"), QStringLiteral("Key Statistics"));
        QTextBlockFormat tb;
        tb.setTopMargin(4);
        tb.setBottomMargin(3);
        cursor.setBlockFormat(tb);
        cursor.insertText(title, rr_fmt(ctx, 12, th.heading_color, true));
        const QStringList lines = comp.config.value(QStringLiteral("data")).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        const QStringList tones = comp.config.value(QStringLiteral("tones")).split(QLatin1Char(','));
        cursor.insertBlock(QTextBlockFormat(), QTextCharFormat());
        if (lines.isEmpty()) {
            rr_hint(cursor, ctx, bound ? QStringLiteral("[%1 — no data yet: press Refresh Data]").arg(rep::describe(rep::binding_of(comp)))
                                       : QStringLiteral("(Enter Label: Value pairs in the Properties panel)"));
        } else {
            QTextTableFormat tf;
            tf.setBorderBrush(QColor(th.divider_color));
            tf.setBorder(0.6);
            tf.setBorderCollapse(true);
            tf.setCellPadding(4);
            tf.setCellSpacing(0);
            tf.setWidth(QTextLength(QTextLength::PercentageLength, 100));
            tf.setColumnWidthConstraints({QTextLength(QTextLength::PercentageLength, 25), QTextLength(QTextLength::PercentageLength, 25),
                                          QTextLength(QTextLength::PercentageLength, 25), QTextLength(QTextLength::PercentageLength, 25)});
            const int rows = (static_cast<int>(lines.size()) + 1) / 2;
            QTextTable* tbl = cursor.insertTable(rows, 4, tf);
            for (int li = 0; li < lines.size(); ++li) {
                const int row = li / 2;
                const int col = (li % 2) * 2;
                QString label, value;
                const int colon = lines[li].indexOf(QLatin1Char(':'));
                if (colon >= 0) {
                    label = lines[li].left(colon).trimmed();
                    value = lines[li].mid(colon + 1).trimmed();
                } else {
                    label = lines[li].trimmed();
                }
                const QString tone = tones.value(li).trimmed();
                const QString ink = tone == QLatin1String("positive")  ? QString::fromLatin1(kRrGreen)
                                    : tone == QLatin1String("negative") ? QString::fromLatin1(kRrRed)
                                    : tone == QLatin1String("warning")  ? QString::fromLatin1(kRrAmber)
                                                                         : th.text_color;
                tbl->cellAt(row, col).firstCursorPosition().insertText(label, rr_fmt(ctx, 9.5, th.meta_color));
                QTextCursor vc = tbl->cellAt(row, col + 1).firstCursorPosition();
                QTextBlockFormat vb;
                vb.setAlignment(Qt::AlignRight);
                vc.setBlockFormat(vb);
                vc.insertText(value, rr_fmt(ctx, 10.5, ink, true));
            }
            rr_exit_table(cursor, tbl);
            QTextBlockFormat after;
            after.setBottomMargin(4);
            cursor.setBlockFormat(after);
        }

    } else if (type == QLatin1String("callout")) {
        const QString style = comp.config.value(QStringLiteral("style"), QStringLiteral("info"));
        const QString heading = comp.config.value(QStringLiteral("heading"));
        const bool dark = QColor(th.page_bg).lightness() < 100;
        QString bg, border, icon;
        if (style == QLatin1String("success")) {
            bg = dark ? "#052e16" : "#f0fdf4";
            border = kRrGreen;
            icon = QStringLiteral("✓");
        } else if (style == QLatin1String("warning")) {
            bg = dark ? "#1c1506" : "#fffbeb";
            border = kRrAmber;
            icon = QStringLiteral("⚠");
        } else if (style == QLatin1String("danger")) {
            bg = dark ? "#1f0505" : "#fef2f2";
            border = kRrRed;
            icon = QStringLiteral("✗");
        } else {
            bg = dark ? "#0c1a33" : "#eff6ff";
            border = "#3b82f6";
            icon = QStringLiteral("ℹ");
        }
        QTextBlockFormat bf;
        bf.setBackground(QColor(bg));
        bf.setLeftMargin(14);
        bf.setRightMargin(8);
        bf.setTopMargin(6);
        bf.setBottomMargin(6);
        cursor.setBlockFormat(bf);
        if (!heading.isEmpty()) {
            cursor.insertText(icon + QStringLiteral("  ") + heading, rr_fmt(ctx, 12, border, true));
            cursor.insertBlock(bf, QTextCharFormat());
        } else {
            cursor.insertText(icon + QStringLiteral("  "), rr_fmt(ctx, 12, border));
        }
        const QTextCharFormat body_fmt = rr_fmt(ctx, 11, th.text_color);
        cursor.setCharFormat(body_fmt);
        const QString body = comp.content.isEmpty() ? QStringLiteral("Enter callout text...") : comp.content;
        if (body.contains(QLatin1String("**")) || body.contains(QLatin1String("__")) || body.contains(QLatin1String("\n- ")))
            cursor.insertMarkdown(body);
        else
            cursor.insertText(body, body_fmt);

    } else if (type == QLatin1String("sparkline")) {
        const QString title = comp.config.value(QStringLiteral("title"));
        const QString data = comp.config.value(QStringLiteral("data"));
        const QString current = comp.config.value(QStringLiteral("current"));
        const QString change = comp.config.value(QStringLiteral("change_pct"));
        QTextBlockFormat bf;
        bf.setTopMargin(3);
        bf.setBottomMargin(2);
        cursor.setBlockFormat(bf);
        if (!title.isEmpty())
            cursor.insertText(title + QStringLiteral("  "), rr_fmt(ctx, 11, th.text_color, true));
        if (!current.isEmpty())
            cursor.insertText(current + QStringLiteral("  "), rr_fmt(ctx, 11, th.accent_color));
        if (!change.isEmpty())
            cursor.insertText(change + (change.endsWith(QLatin1Char('%')) ? QString() : QStringLiteral("%")) + QStringLiteral("  "),
                              rr_fmt(ctx, 10, QString::fromLatin1(change.startsWith(QLatin1Char('-')) ? kRrRed : kRrGreen)));
        if (!data.isEmpty()) {
            const QString img = rr_render_sparkline(data, ctx, 160, 40);
            if (!img.isEmpty()) {
                QTextImageFormat f;
                f.setName(img);
                f.setWidth(160);
                f.setHeight(40);
                f.setVerticalAlignment(QTextCharFormat::AlignMiddle);
                cursor.insertImage(f);
            }
        } else {
            rr_hint(cursor, ctx, QStringLiteral("[Sparkline — enter values in the Properties panel]"));
        }
    }

    // Provenance / as-of (and, on screen, refresh state) under data blocks.
    if (type == QLatin1String("table") || type == QLatin1String("chart") || type == QLatin1String("stats_block") ||
        type == QLatin1String("market_data") || type == QLatin1String("sparkline") || type == QLatin1String("list") ||
        type == QLatin1String("image")) {
        rr_provenance(cursor, comp, ctx);
    }

    // B / I of the FONT section.
    const bool bold = comp.config.value(QStringLiteral("bold")) == QLatin1String("1");
    const bool italic = comp.config.value(QStringLiteral("italic")) == QLatin1String("1");
    if ((bold || italic) && cursor.position() > start) {
        QTextCursor c(cursor.document());
        c.setPosition(start);
        c.setPosition(cursor.position(), QTextCursor::KeepAnchor);
        QTextCharFormat f;
        if (bold)
            f.setFontWeight(QFont::Bold);
        if (italic)
            f.setFontItalic(true);
        c.mergeCharFormat(f);
    }

    // On screen, a {{variable}} with no value is marked (amber tint + a tooltip saying
    // where to set it); exports never get here with marks — they warn before printing.
    if (!ctx.for_export && cursor.position() > start) {
        static const QRegularExpression unresolved(QStringLiteral("\\{\\{\\s*[A-Za-z][A-Za-z0-9_ ]{0,40}?\\s*\\}\\}"));
        QTextDocument* doc = cursor.document();
        int from = start;
        const int end = cursor.position();
        while (from < end) {
            const QTextCursor hit = doc->find(unresolved, from);
            if (hit.isNull() || hit.selectionEnd() > end)
                break;
            QTextCursor mark(doc);
            mark.setPosition(hit.selectionStart());
            mark.setPosition(hit.selectionEnd(), QTextCursor::KeepAnchor);
            QColor tint(QString::fromLatin1(kRrAmber));
            tint.setAlphaF(0.28f);
            QTextCharFormat f;
            f.setBackground(tint);
            f.setToolTip(QStringLiteral("%1 has no value — set it in Variables (left panel). It will print as typed.")
                             .arg(hit.selectedText()));
            mark.mergeCharFormat(f);
            from = hit.selectionEnd();
        }
    }
}

void paint_page_furniture(QPainter* painter, const RenderContext& ctx, const PageGeometry& g, int page_no,
                          int total_pages) {
    const ReportMetadata& m = ctx.meta;
    const QColor paper(ctx.theme.page_bg);
    const bool cover_page = ctx.has_cover && page_no == 1;
    auto band = [&](bool header) {
        QString left = header ? m.header_left : m.footer_left;
        QString center = header ? m.header_center : m.footer_center;
        QString right = header ? m.header_right : m.footer_right;
        auto tokens = [&](QString s) {
            s.replace(QStringLiteral("{page}"), QString::number(page_no));
            s.replace(QStringLiteral("{total}"), QString::number(total_pages));
            s.replace(QStringLiteral("{title}"), ctx.vars.value(QStringLiteral("title"), m.title));
            s.replace(QStringLiteral("{date}"), m.date);
            s.replace(QStringLiteral("{author}"), m.author);
            s.replace(QStringLiteral("{company}"), m.company);
            return rep::vars::expand(s, ctx.vars);
        };
        left = tokens(left);
        center = tokens(center);
        right = tokens(right);
        // "Show page numbers" with no footer text: "3 / 12" in the centre.
        if (!header && m.show_page_numbers && left.isEmpty() && center.isEmpty() && right.isEmpty())
            center = QStringLiteral("%1 / %2").arg(page_no).arg(total_pages);
        const double y = header ? g.margins.top() : g.page.height() - g.margins.bottom() - g.band;
        const QRectF r(g.margins.left(), y, g.page.width() - g.margins.left() - g.margins.right(), g.band);
        // Pixel size (not points): the painter may be scaled onto a 1200-dpi printer.
        QFont f;
        f.setFamilies({ctx.font_family, QStringLiteral("Arial")});
        f.setPixelSize(std::max(8, static_cast<int>(std::lround(8.0 * 96.0 / 72.0 * std::min(ctx.scale, 1.5)))));
        painter->setFont(f);
        painter->setPen(paper.lightness() < 128 ? QColor("#888888") : QColor("#666666"));
        const QRectF text_r = header ? r.adjusted(0, 0, 0, -4) : r.adjusted(0, 4, 0, 0);
        if (!left.isEmpty())
            painter->drawText(text_r, Qt::AlignLeft | Qt::AlignVCenter, left);
        if (!center.isEmpty())
            painter->drawText(text_r, Qt::AlignHCenter | Qt::AlignVCenter, center);
        if (!right.isEmpty())
            painter->drawText(text_r, Qt::AlignRight | Qt::AlignVCenter, right);
        painter->setPen(QPen(paper.lightness() < 128 ? QColor("#444444") : QColor("#cccccc"), 0.75));
        const double rule_y = header ? r.bottom() - 2 : r.top() + 2;
        painter->drawLine(QPointF(r.left(), rule_y), QPointF(r.right(), rule_y));
    };
    painter->save();
    // The cover page carries no running header / footer.
    if (g.has_header && !cover_page)
        band(true);
    if (g.has_footer && !cover_page)
        band(false);

    // Confidentiality line: small, centred in the bottom margin (inside the footer band
    // when the margin is too small for it).
    const QString confidential = rep::vars::expand(m.confidentiality, ctx.vars).trimmed();
    if (!confidential.isEmpty()) {
        QFont f;
        f.setFamilies({ctx.font_family, QStringLiteral("Arial")});
        f.setPixelSize(std::max(7, static_cast<int>(std::lround(7.0 * 96.0 / 72.0 * std::min(ctx.scale, 1.4)))));
        f.setItalic(true);
        painter->setFont(f);
        painter->setPen(paper.lightness() < 128 ? QColor("#888888") : QColor("#666666"));
        const double room = g.margins.bottom();
        const QRectF r = room >= mm_to_px(6)
                             ? QRectF(g.margins.left(), g.page.height() - room + mm_to_px(1),
                                      g.page.width() - g.margins.left() - g.margins.right(), room - mm_to_px(2))
                             : QRectF(g.margins.left(), g.content.bottom() - mm_to_px(5),
                                      g.page.width() - g.margins.left() - g.margins.right(), mm_to_px(5));
        painter->drawText(r, Qt::AlignHCenter | Qt::AlignVCenter | Qt::TextSingleLine, confidential);
    }

    // Watermark: large, light, diagonal across the page ("DRAFT"), document ink.
    const QString watermark = rep::vars::expand(m.watermark, ctx.vars).trimmed();
    if (!watermark.isEmpty()) {
        QFont f;
        f.setFamilies({ctx.font_family, QStringLiteral("Arial")});
        f.setBold(true);
        const double diag = std::hypot(g.page.width(), g.page.height());
        f.setPixelSize(std::clamp(static_cast<int>(diag * 0.62 / std::max(4, static_cast<int>(watermark.size()))), 24, 160));
        painter->setFont(f);
        QColor ink(ctx.theme.text_color);
        ink.setAlphaF(paper.lightness() < 128 ? 0.10f : 0.08f);
        painter->setPen(ink);
        painter->translate(g.page.width() / 2.0, g.page.height() / 2.0);
        painter->rotate(-qRadiansToDegrees(std::atan2(g.page.height(), g.page.width())));
        const QRectF wr(-diag / 2.0, -g.page.height() / 4.0, diag, g.page.height() / 2.0);
        painter->drawText(wr, Qt::AlignCenter | Qt::TextSingleLine, watermark);
    }
    painter->restore();
}

void paint_page_content(QPainter* painter, QTextDocument* doc, const PageGeometry& g, int page_index,
                        const ReportTheme& theme) {
    const double h = g.content.height();
    painter->save();
    painter->translate(g.content.topLeft());
    painter->translate(0, -page_index * h);
    QAbstractTextDocumentLayout::PaintContext pc;
    pc.clip = QRectF(0, page_index * h, g.content.width() + 2, h);
    painter->setClipRect(pc.clip, Qt::IntersectClip);
    pc.palette.setColor(QPalette::Text, QColor(theme.text_color));
    doc->documentLayout()->draw(painter, pc);
    painter->restore();
}

QString chart_cache_dir() {
    return AppPaths::cache() + QStringLiteral("/report_charts");
}

} // namespace fincept::screens::rb
