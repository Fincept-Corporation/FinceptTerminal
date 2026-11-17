#pragma once
// ReportExportAssets.h — what a worker-thread export (.odt, Markdown) receives besides the
// report snapshot: the pictures and formatted tables the GUI thread prepared when the
// export started (QtGui / the terminal's number formatter are not usable on a worker).
//
//   charts  chart / sparkline component id -> PNG rendered at print resolution
//   images  image / cover-logo component id -> the picture file's bytes
//   tables  table id -> tablefmt::format_table() of vars::expanded_component(c) with the
//           terminal's NumberStyle; a table missing here is formatted by the writer with
//           tablefmt::default_style() on the theme's paper.
//
// Also the rules both writers share: which blocks are left out (mirror of
// rb::is_unfilled, with "the asset is missing" instead of "the file does not exist"), the
// printed cells of a table (legacy {"r_c": text} tables included) and the provenance line.
// Qt Core only, header-only.

#include "core/report/ReportBinding.h"
#include "core/report/ReportDocument.h"
#include "core/report/ReportTableCodec.h"
#include "core/report/ReportTableFormat.h"

#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSize>
#include <QSizeF>
#include <QString>
#include <QStringList>
#include <QVector>

#include <algorithm>
#include <cmath>

namespace fincept::report::exportfmt {

struct ImageAsset {
    QByteArray bytes;
    QString ext;        ///< "png" | "jpg" (empty: sniffed from the bytes)
    QSize pixels;       ///< intrinsic size
    QSizeF display_px;  ///< logical size at 96 dpi as on the page; 0 = derive (block width, aspect of `pixels`)
};

struct ExportAssets {
    QHash<int, ImageAsset> charts;               ///< chart / sparkline component id -> PNG at print resolution
    QHash<int, ImageAsset> images;               ///< image and cover-logo component id -> the file's bytes
    QHash<int, tablefmt::FormattedTable> tables; ///< table id -> formatted on the GUI thread
};

/// File extension of a picture: the asset's own ("jpeg" -> "jpg"), else sniffed from the
/// bytes (PNG / JPEG / GIF / BMP / WebP), else "png".
inline QString image_ext(const ImageAsset& a) {
    QString e = a.ext.trimmed().toLower();
    if (e.startsWith(QLatin1Char('.')))
        e.remove(0, 1);
    if (e == QLatin1String("jpeg"))
        e = QStringLiteral("jpg");
    if (!e.isEmpty())
        return e;
    const QByteArray& b = a.bytes;
    if (b.startsWith("\x89PNG"))
        return QStringLiteral("png");
    if (b.startsWith("\xFF\xD8"))
        return QStringLiteral("jpg");
    if (b.startsWith("GIF8"))
        return QStringLiteral("gif");
    if (b.startsWith("BM"))
        return QStringLiteral("bmp");
    if (b.startsWith("RIFF") && b.mid(8, 4) == "WEBP")
        return QStringLiteral("webp");
    return QStringLiteral("png");
}

/// Media type for image_ext().
inline QString image_mime(const ImageAsset& a) {
    const QString e = image_ext(a);
    if (e == QLatin1String("jpg"))
        return QStringLiteral("image/jpeg");
    if (e == QLatin1String("svg"))
        return QStringLiteral("image/svg+xml");
    return QStringLiteral("image/") + e;
}

/// True when the export leaves `c` out (rb::is_unfilled with assets instead of files):
/// empty text-like blocks, tables without cells, charts and images whose picture was not
/// prepared, stats / sparklines without data, quotes without a price.
inline bool is_unfilled(const ReportComponent& c, const ExportAssets& assets) {
    const auto cfg = [&c](const char* key) { return c.config.value(QString::fromLatin1(key)).trimmed(); };
    const QString& t = c.type;
    if (t == QLatin1String("heading") || t == QLatin1String("text") || t == QLatin1String("code") ||
        t == QLatin1String("quote"))
        return c.content.trimmed().isEmpty();
    if (t == QLatin1String("callout"))
        return c.content.trimmed().isEmpty() && cfg("heading").isEmpty();
    if (t == QLatin1String("list"))
        return c.content.split(QLatin1Char('\n'), Qt::SkipEmptyParts).isEmpty();
    if (t == QLatin1String("table")) {
        if (!table::parse(c.config.value(QStringLiteral("csv"))).isEmpty())
            return false;
        if (is_bound(c))
            return true; // a recipe that was never fetched
        const QJsonDocument d = QJsonDocument::fromJson(c.config.value(QStringLiteral("data")).toUtf8());
        return !d.isObject() || d.object().isEmpty(); // only "Header N" placeholders
    }
    if (t == QLatin1String("chart")) {
        if (cfg("data").isEmpty() && cfg("series").isEmpty() && cfg("ohlc").isEmpty())
            return true;
        const auto it = assets.charts.constFind(c.id);
        return it == assets.charts.cend() || it->bytes.isEmpty();
    }
    if (t == QLatin1String("stats_block") || t == QLatin1String("sparkline"))
        return cfg("data").isEmpty();
    if (t == QLatin1String("image")) {
        const auto it = assets.images.constFind(c.id);
        return it == assets.images.cend() || it->bytes.isEmpty();
    }
    if (t == QLatin1String("market_data"))
        return cfg("symbol").isEmpty() || cfg("price").isEmpty();
    return false; // divider, page_break, toc, cover
}

/// The cells a table block prints, and the renderer's truncation notice.
struct PrintedTable {
    tablefmt::FormattedTable table;
    QString truncated; ///< "Table truncated: showing ..." when the grid exceeds 200 x 20, else empty
};

/// `c` = the expanded component. CSV tables: assets.tables[c.id] when the GUI thread
/// formatted it, else format_table(default_style(), paper_hex). Legacy tables (no csv,
/// config "data" = {"r_c": text} with "rows" / "cols") as the renderer draws them.
inline PrintedTable printed_table(const ReportComponent& c, const ExportAssets& assets, const QString& paper_hex) {
    constexpr int kMaxRows = 200;
    constexpr int kMaxCols = 20;
    PrintedTable out;
    int wanted_rows = 0, wanted_cols = 0;
    const QVector<QStringList> grid = table::parse(c.config.value(QStringLiteral("csv")));
    if (!grid.isEmpty()) {
        wanted_rows = static_cast<int>(grid.size());
        for (const QStringList& r : grid)
            wanted_cols = std::max(wanted_cols, static_cast<int>(r.size()));
        const auto it = assets.tables.constFind(c.id);
        if (it != assets.tables.cend() && !it->rows.isEmpty()) {
            out.table = *it;
        } else {
            out.table = tablefmt::format_table(
                grid, tablefmt::style_from_text(c.config.value(QString::fromLatin1(tablefmt::kStyleKey))),
                tablefmt::columns_from_text(c.config.value(QString::fromLatin1(tablefmt::kColFmtKey))),
                tablefmt::default_style(), paper_hex, kMaxRows, kMaxCols);
        }
    } else {
        const QJsonDocument d = QJsonDocument::fromJson(c.config.value(QStringLiteral("data")).toUtf8());
        const QJsonObject cells = d.isObject() ? d.object() : QJsonObject();
        wanted_rows = c.config.value(QStringLiteral("rows"), QStringLiteral("3")).toInt();
        wanted_cols = c.config.value(QStringLiteral("cols"), QStringLiteral("3")).toInt();
        const int rows = std::clamp(wanted_rows, 1, kMaxRows);
        const int cols = std::clamp(wanted_cols, 1, kMaxCols);
        tablefmt::FormattedTable& t = out.table;
        t.columns = cols;
        t.header_rows = rows > 1 ? 1 : 0;
        t.header_repeat = t.header_rows > 0;
        t.zebra = rows > 4;
        for (int r = 0; r < rows; ++r) {
            QVector<tablefmt::Cell> row;
            row.reserve(cols);
            for (int col = 0; col < cols; ++col) {
                const QString key = QStringLiteral("%1_%2").arg(r).arg(col);
                tablefmt::Cell cell;
                if (cells.contains(key))
                    cell.text = cells.value(key).toString();
                else if (r == 0)
                    cell.text = QStringLiteral("Header %1").arg(col + 1);
                if (r < t.header_rows) {
                    cell.align = col > 0 ? QStringLiteral("right") : QStringLiteral("left");
                } else {
                    cell.value = tablefmt::parse_number(cell.text);
                    cell.numeric = !std::isnan(cell.value);
                    cell.align = col > 0 && tablefmt::looks_numeric(cell.text) ? QStringLiteral("right")
                                                                               : QStringLiteral("left");
                }
                row.append(cell);
            }
            t.rows.append(row);
        }
    }
    if (wanted_rows > kMaxRows || wanted_cols > kMaxCols)
        out.truncated = QStringLiteral("Table truncated: showing %1 of %2 rows and %3 of %4 columns.")
                            .arg(std::clamp(wanted_rows, 1, kMaxRows))
                            .arg(wanted_rows)
                            .arg(std::clamp(wanted_cols, 1, kMaxCols))
                            .arg(wanted_cols);
    return out;
}

/// Blocks that carry a provenance line under them (as on the page).
inline bool has_provenance(const QString& type) {
    return type == QLatin1String("table") || type == QLatin1String("chart") || type == QLatin1String("stats_block") ||
           type == QLatin1String("market_data") || type == QLatin1String("sparkline") ||
           type == QLatin1String("list") || type == QLatin1String("image");
}

/// "Source: <label> · as of 17 Nov 2026 14:02" (local time), "As of ..." without a label;
/// empty when neither is set or the block is itself a provenance caption (role=provenance).
inline QString provenance_text(const ReportComponent& c) {
    if (c.config.value(QStringLiteral("role")) == QLatin1String("provenance"))
        return {};
    const QDateTime as_of = as_of_of(c);
    const QString source = c.config.value(QLatin1String(keys::kSourceLabel)).trimmed();
    QString line;
    if (!source.isEmpty())
        line = QStringLiteral("Source: ") + source;
    if (as_of.isValid())
        line += (line.isEmpty() ? QStringLiteral("As of ") : QStringLiteral(" · as of ")) +
                as_of.toLocalTime().toString(QStringLiteral("d MMM yyyy HH:mm"));
    return line;
}

/// The renderer's rule for a text block printed as Markdown (else: plain lines).
inline bool text_is_markdown(const ReportComponent& c) {
    const QString& body = c.content;
    return c.config.value(QStringLiteral("format")) == QLatin1String("markdown") ||
           body.contains(QLatin1String("**")) || body.contains(QLatin1String("__")) ||
           body.contains(QLatin1String("\n- ")) || body.contains(QLatin1String("\n* ")) ||
           body.contains(QLatin1String("\n#")) || body.startsWith(QLatin1String("- "));
}

} // namespace fincept::report::exportfmt
