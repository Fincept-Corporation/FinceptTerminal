#pragma once
// NotebookDocument — the Code tab's notebook model, free of widgets and of the
// theme: cells and their outputs, .ipynb (nbformat 4, and the old 3) reading
// and writing without losing what the file carried, export to a Python script
// and to HTML, the kernel-event -> output accumulation, markdown rendering to
// HTML, the outline, and small text helpers. Everything here is unit-tested
// (tests/tst_notebook_document.cpp).

#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

namespace fincept::screens::notebook {

/// One output of a code cell.
struct CellOutput {
    enum class Kind { Stream, Result, Table, Image, Error };
    Kind kind = Kind::Stream;
    bool is_stderr = false; ///< Stream: stderr instead of stdout
    QString text;           ///< Stream / Result text; Table: the repr (kept for saving); Error: traceback joined
    QJsonObject table;      ///< Table: {columns, dtypes, index, index_name, rows, total_rows, total_columns}
    QByteArray png;         ///< Image: PNG bytes
    QString ename;          ///< Error
    QString evalue;         ///< Error
    QStringList traceback;  ///< Error
    int error_line = 0;     ///< Error: 1-based line of the cell that failed, 0 = unknown
    /// Other MIME renditions the file carried (text/html, application/json, ...):
    /// not shown, written back unchanged so a round trip loses nothing.
    QJsonObject extra_data;
    QJsonObject metadata;    ///< the output's own metadata, kept
    bool is_display = false; ///< Result / Image read from a "display_data" output (else execute_result)
};

/// One cell.
struct NotebookCell {
    enum class Type { Code, Markdown, Raw };
    QString id; ///< nbformat 4.5 cell id (kept from the file; generated when missing)
    Type type = Type::Code;
    QString source;
    QString title; ///< metadata.fincept_title — the user's name for the cell (outline)
    QVector<CellOutput> outputs;
    int execution_count = 0;
    bool input_collapsed = false;  ///< metadata.jupyter.source_hidden
    bool output_collapsed = false; ///< metadata.jupyter.outputs_hidden (also the classic "collapsed")
    QJsonObject metadata;          ///< everything else in the cell metadata, kept as read
    QJsonObject attachments;       ///< markdown / raw attachments, kept as read

    bool is_code() const { return type == Type::Code; }
    bool runnable() const { return type == Type::Code && !source.trimmed().isEmpty(); }
};

/// A notebook: cells plus the notebook-level metadata the file carried.
struct NotebookDoc {
    QVector<NotebookCell> cells;
    QJsonObject metadata; ///< kernelspec, language_info, anything else — kept
    int nbformat_minor = 5;

    /// Python environment the notebook runs in: "numpy2" (default) or "numpy1"
    /// (metadata.kernelspec.name "fincept-numpy1").
    QString environment() const;
    void set_environment(const QString& env);
    int index_of(const QString& cell_id) const;
};

/// A fresh, unique cell id (nbformat 4.5: 1-64 chars of [a-zA-Z0-9-_]).
QString new_cell_id();

/// Parse an .ipynb. nbformat 4.x, and 3 ("worksheets", "input", heading
/// cells) — the format of old files. Cells without an id get one. False with
/// `error` set when the bytes are not a notebook.
bool parse_ipynb(const QByteArray& bytes, NotebookDoc* out, QString* error = nullptr);

/// nbformat 4.5 JSON (indented, one trailing newline). Unknown metadata and
/// output renditions read from the file are written back.
QByteArray serialize_ipynb(const NotebookDoc& doc);

/// Jupytext "percent" script: "# %%" before each code cell, "# %% [markdown]"
/// before each markdown cell (its lines commented). Runs as a plain .py.
QString export_python(const NotebookDoc& doc, const QString& title);

/// Colours for rendered HTML (markdown cells, the HTML export) — filled by the
/// caller from the active theme, so this file stays theme-free.
struct HtmlPalette {
    QString bg, surface, text, dim, border, accent, link, code_bg, error, font_family;
    int font_px = 14;
};

/// Markdown -> HTML (headings, paragraphs, emphasis, inline and fenced code,
/// bullet / numbered lists, block quotes, rules, links, pipe tables). Text is
/// escaped; links keep only http(s) / mailto targets.
QString markdown_to_html(const QString& markdown, const HtmlPalette& pal, bool with_style_block = true);

/// A standalone HTML page of the notebook with its saved outputs (figures
/// embedded as data URLs, tables as HTML tables).
QString export_html(const NotebookDoc& doc, const QString& title, const HtmlPalette& pal);

/// Plain HTML table of a "table" output (also written into .ipynb as text/html
/// so Jupyter shows it).
QString table_to_html(const QJsonObject& table);

// ── Kernel events -> outputs ──────────────────────────────────────────────────

/// Append one kernel event ("stream", "result", "display", "error") to `outputs`
/// in arrival order: consecutive text of the same stream is merged (carriage
/// returns overwrite the current line, terminal colour codes are dropped); a
/// table that follows its repr becomes one Table output. Returns the index of
/// the output that changed or was added, -1 when the event was ignored.
int append_kernel_event(QVector<CellOutput>& outputs, const QJsonObject& event);

/// `existing` + `chunk` the way a terminal shows it: "\r" without "\n" moves to
/// the start of the line (progress bars overwrite themselves), "\r\n" is a newline.
QString apply_terminal_text(const QString& existing, const QString& chunk);
QString strip_ansi(const QString& text);

/// 1-based cell line from a traceback ('File "<cell>", line N' — the last one),
/// 0 when there is none.
int error_line_from_traceback(const QStringList& traceback);

// ── Outline / names ───────────────────────────────────────────────────────────

/// A readable one-line name for a cell: the title, else its first meaningful
/// line without markdown markers / leading symbols / comment marks.
QString cell_display_name(const NotebookCell& cell);

struct OutlineEntry {
    int cell_index = -1;
    int level = 0;     ///< 1-6 for a markdown heading, 0 for a code cell
    QString text;
};
/// Headings of markdown cells (every heading line) and one entry per code cell.
QVector<OutlineEntry> outline(const NotebookDoc& doc);

/// Canonical text of what is saved (type, title, source, collapse flags per
/// cell) — compared with the last saved one to tell unsaved edits.
QString fingerprint(const NotebookDoc& doc);

/// Identifiers in `source` (for completion when no kernel is running).
QStringList identifiers_in(const QString& source);

} // namespace fincept::screens::notebook
