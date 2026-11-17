#pragma once
// ReportRenderer — writes Report Builder components into QTextDocuments, and paints
// pages. Shared by the on-screen page view (DocumentCanvas) and the exports
// (ReportExportJob), so the screen shows exactly where the PDF breaks pages.
//
// Everything written here is DOCUMENT content: paper, ink, point sizes and the report
// typeface come from the report (ReportTheme / ReportMetadata), never from the
// terminal's appearance — paper stays paper in the dark UI theme.
//
// Geometry: documents are laid out at 96 dpi with design metrics; one QTextDocument per
// section (the components between page_break blocks), its page size = the content area
// of the chosen paper. Painting a page = drawing that slice of the document; exports
// scale the same layout to the printer, so pagination is identical everywhere.

#include "core/report/ReportChartSpec.h"
#include "core/report/ReportDocument.h"
#include "core/report/ReportTableFormat.h"

#include <QHash>
#include <QMap>
#include <QMarginsF>
#include <QRectF>
#include <QSize>
#include <QSizeF>
#include <QString>
#include <QStringList>

#include <functional>

class QPainter;
class QTextCursor;
class QTextDocument;
class QObject;

namespace fincept::screens {
class ReportChartObject;
}

namespace fincept::screens::rb {

using ReportComponent = ::fincept::report::ReportComponent;
using ReportMetadata = ::fincept::report::ReportMetadata;
using ReportTheme = ::fincept::report::ReportTheme;

/// Screen-only note under a data block (never exported).
struct BlockNote {
    enum class Kind { None, Loading, Failed };
    Kind kind = Kind::None;
    QString text;
};

/// Page geometry in px at 96 dpi.
struct PageGeometry {
    QSizeF page;          ///< whole sheet
    QMarginsF margins;    ///< paper margins
    double band = 0;      ///< header / footer strip height (0 when unused)
    bool has_header = false;
    bool has_footer = false;
    QRectF content;       ///< the area the document flows into (page coordinates)
};

PageGeometry page_geometry(const ReportMetadata& meta);
double mm_to_px(double mm);

/// The report typeface, or the application's default family when it is not installed.
QString resolve_font_family(const QString& requested);

struct RenderContext {
    ReportTheme theme;
    ReportMetadata meta;
    QString font_family; ///< resolved
    double scale = 1.0;  ///< body size / 12: every point size scales with it
    bool for_export = false;
    double content_width = 600; ///< px; charts and images are capped to it
    QStringList toc_headings;   ///< every heading of the report (for TOC blocks), numbered when asked
    QHash<int, int> heading_pages; ///< heading id -> page number (TOC page numbers; may be empty)
    QVector<int> heading_ids;      ///< ids of the headings, in toc_headings order
    QVector<int> heading_levels;   ///< their levels (TOC indent)
    std::function<BlockNote(int component_id)> note_of; ///< screen only

    QMap<QString, QString> vars;          ///< {{variables}} (core/report/ReportVariables.h)
    QHash<int, QString> heading_numbers;  ///< "2.1" per heading id when the report numbers headings
    bool has_cover = false;               ///< the first block is a cover page (no title header, no bands on page 1)
    report::chart::ChartPalette chart_palette;
    /// Screen / PDF: charts become fixed-size text objects drawn by this handler (cached
    /// image on screen, vector in the PDF). Null: charts are pictures (HTML, tests).
    ReportChartObject* chart_object = nullptr;
    /// HTML export: chart pictures rendered beforehand, by block id.
    QHash<int, QString> chart_images;
    /// Numbers / money in formatted table columns (the page view passes the terminal's).
    report::tablefmt::NumberStyle number_style = report::tablefmt::default_style();

    double pt(double v) const { return v * scale; }
};

RenderContext make_context(const QVector<ReportComponent>& comps, const ReportMetadata& meta,
                           const ReportTheme& theme, bool for_export);

/// The terminal's number formatter (ui/formatting) for formatted tables — GUI thread.
/// Lives in its own translation unit (ReportNumberStyle.cpp) so the renderer's tests do not
/// link the formatting module.
report::tablefmt::NumberStyle terminal_number_style();

/// Logical size (px at 96 dpi) of chart block `c` on the page.
QSize chart_size(const ReportComponent& c, const RenderContext& ctx);

/// The printed table of a table block (formats applied, variables expanded): what the page,
/// the PDF, the .odt and the Markdown export all show. GUI thread when `ctx` carries the
/// terminal's number style.
report::tablefmt::FormattedTable formatted_table(const ReportComponent& c, const RenderContext& ctx);

/// A fresh section document: default font, page size, design metrics, no margin.
QTextDocument* new_section_document(const RenderContext& ctx, QObject* parent);

/// The cover lines (title, author · company · date, rule) at the start of section 0.
/// Leaves the cursor at the end of the header.
void write_header(QTextCursor& cursor, const RenderContext& ctx);

/// The invisible first block of sections after a page break (keeps every component
/// starting with its own block).
void write_section_spacer(QTextCursor& cursor);

/// Writes component `c` after the cursor (the cursor is at the end of the previous
/// block); starts with a new block of fresh formats and leaves the cursor at the end of
/// the component's last block. Everything it writes is [start, cursor) for in-place
/// replacement.
void write_component(QTextCursor& cursor, const ReportComponent& c, const RenderContext& ctx);

/// True when `c` has no content yet (exports leave it out).
bool is_unfilled(const ReportComponent& c);

/// Header / footer bands of one page, in page coordinates (px at 96 dpi).
void paint_page_furniture(QPainter* painter, const RenderContext& ctx, const PageGeometry& g, int page_no,
                          int total_pages);

/// Draws page `page_index` of `doc` (content only) with the painter in page coordinates.
void paint_page_content(QPainter* painter, QTextDocument* doc, const PageGeometry& g, int page_index,
                        const ReportTheme& theme);

/// Where report charts are cached (profile cache, size-capped).
QString chart_cache_dir();

} // namespace fincept::screens::rb
