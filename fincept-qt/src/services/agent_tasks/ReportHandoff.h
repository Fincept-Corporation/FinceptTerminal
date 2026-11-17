#pragma once
// ReportHandoff.h — what a run artefact becomes in a report ("Add to report").
//
// Chat Mode never touches the Report Builder itself: it fills the neutral
// report::ReportPayload (core/report/ReportBlocks.h) and hands it to
// services::ReportInbox through the shared ui::AddToReportButton, which also
// offers the target (the report open in the Report Builder, a recent library
// report, or a new one) and shows "Added to “…” · Open". ReportInbox turns
// the Markdown into heading / text / table / code blocks, stores image bytes
// off the GUI thread and adds the provenance caption "Source: Chat Mode · as
// of …".
//
// The mapping below is pure (Qt Core; unit-tested in tests/tst_run_artefacts).

#include "core/report/ReportBlocks.h"
#include "services/agent_tasks/ArtefactLogic.h"
#include "services/agent_tasks/TaskRunTypes.h"

#include <QByteArray>
#include <QString>

namespace fincept::agent_tasks {

/// Source label of every block Chat Mode adds (undo text, provenance caption).
inline constexpr const char* kReportSourceLabel = "Chat Mode";
/// Rows of one table that go into a report; the rest stay in the artefact's file.
inline constexpr int kReportMaxTableRows = 500;
/// Series drawn in one chart (the first numeric columns after the label column).
inline constexpr int kReportMaxChartSeries = 6;

/// The loaded content of an artefact, as far as a report needs it.
struct ArtefactReportContent {
    QString text;                        ///< Markdown (report), plain text, or pretty JSON
    artefact_logic::ArtefactTable table; ///< table / series / tabular JSON
    QByteArray image;                    ///< encoded image bytes (image artefacts)
    QString image_format;                ///< "png" | "jpg"
};

/// "From agent run “Deep-dive: AAPL” · step 2 · tool get_quote".
QString artefact_provenance(const RunArtefact& artefact);

/// The report payload of one artefact: its title as the heading, then
///   * a series: a line chart of its numeric columns (label column as x) and the table;
///   * a table (or JSON that is tabular): the table;
///   * an image: the image;
///   * Markdown / text: the text (JSON fenced as code);
/// tables cut to kReportMaxTableRows with a visible note; the provenance line as
/// the trailing note; as_of = when the artefact was made. is_empty() when there
/// is nothing to add.
report::ReportPayload artefact_report_payload(const RunArtefact& artefact, const ArtefactReportContent& content);

} // namespace fincept::agent_tasks
