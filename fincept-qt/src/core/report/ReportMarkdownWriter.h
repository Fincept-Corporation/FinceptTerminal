#pragma once
// ReportMarkdownWriter.h — a Report Builder report as GitHub-flavoured Markdown. Runs on a
// WORKER thread: Qt Core only; pictures and formatted tables come in `assets`.
//
// Layout: optional YAML front matter (title / author / company / date, quoted strings);
// the report title is the only level-1 heading ("# Title" + a byline line, or the cover
// block's title when the report opens with a cover), so a report heading of level N is
// written with N + 1 '#' (capped at 6) and carries its outline number when the report
// numbers headings. Text blocks are Markdown already and are written verbatim; every other
// printed text is escaped. Tables become pipe tables of the formatted cells (alignment
// row from the cells, totals row in bold, title above in bold, note below in italics).
// Charts / images link to assets_dir/chart_<id>.png / image_<id>.<ext> (their bytes are
// returned in `files`) or are inlined as data: URIs. The TOC links to GitHub's heading
// anchors. Blocks without content are left out, as in every export. LF line endings, one
// blank line between blocks, deterministic output.

#include "core/report/ReportDocument.h"
#include "core/report/ReportExportAssets.h"

#include <QByteArray>
#include <QPair>
#include <QString>
#include <QVector>

#include <functional>

namespace fincept::report::md {

struct MarkdownOptions {
    bool front_matter = true;
    enum class Images { Folder, Inline } images = Images::Folder;
    QString assets_dir = QStringLiteral("report_files"); ///< relative folder next to the .md (Folder mode)
};

struct MarkdownOutput {
    QString markdown;
    QVector<QPair<QString, QByteArray>> files; ///< relative path (assets_dir/...) -> bytes, in document order
    bool cancelled = false;                    ///< `cancelled` returned true: markdown and files are empty
};

/// `cancelled` (may be empty) is asked between blocks.
MarkdownOutput write(const ReportDocument& doc, const exportfmt::ExportAssets& assets, const MarkdownOptions& opt,
                     const std::function<bool()>& cancelled);

} // namespace fincept::report::md
