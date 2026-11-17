// NotebookDocument.cpp — see NotebookDocument.h. No widgets, no theme.

#include "screens/code_editor/NotebookDocument.h"

#include <QJsonDocument>
#include <QJsonValue>
#include <QRegularExpression>
#include <QSet>
#include <QUuid>

#include <algorithm>

namespace fincept::screens::notebook {

namespace {

/// MIME type under which a rendered table's rows are saved in the .ipynb.
constexpr const char* kTableMime = "application/vnd.fincept.table+json";
/// Notebook metadata key marking files written by this editor (format 2+).
constexpr const char* kFinceptMetaKey = "fincept";
constexpr int kFormat = 2;

// nbformat stores multi-line text as an array of lines, each keeping its "\n".
QJsonArray text_to_lines(const QString& text) {
    QJsonArray lines;
    const QStringList parts = text.split(QLatin1Char('\n'));
    for (int i = 0; i < parts.size(); ++i) {
        const bool last = (i + 1 == parts.size());
        if (last && parts[i].isEmpty())
            break; // text ended with a newline — no phantom empty line
        lines.append(last ? parts[i] : parts[i] + QLatin1Char('\n'));
    }
    return lines;
}

// A text field is either one string or an array of lines.
QString lines_to_text(const QJsonValue& v) {
    if (v.isArray()) {
        QString text;
        for (const QJsonValue& line : v.toArray())
            text += line.toString();
        return text;
    }
    return v.toString();
}

QString type_name(NotebookCell::Type t) {
    switch (t) {
        case NotebookCell::Type::Markdown:
            return QStringLiteral("markdown");
        case NotebookCell::Type::Raw:
            return QStringLiteral("raw");
        case NotebookCell::Type::Code:
            break;
    }
    return QStringLiteral("code");
}

bool valid_cell_id(const QString& id) {
    static const QRegularExpression re(QStringLiteral("^[a-zA-Z0-9_-]{1,64}$"));
    return re.match(id).hasMatch();
}

// ── Outputs: file -> model ───────────────────────────────────────────────────

CellOutput output_from_bundle(const QJsonObject& data, const QJsonObject& metadata, bool display) {
    CellOutput out;
    out.metadata = metadata;
    out.is_display = display;
    QJsonObject rest = data;
    const QString plain = strip_ansi(lines_to_text(rest.value(QStringLiteral("text/plain"))));
    if (rest.value(QLatin1String(kTableMime)).isObject()) {
        out.kind = CellOutput::Kind::Table;
        out.table = rest.take(QLatin1String(kTableMime)).toObject();
        out.text = plain;
        rest.remove(QStringLiteral("text/plain"));
        rest.remove(QStringLiteral("text/html")); // regenerated from the table on save
    } else if (rest.contains(QStringLiteral("image/png"))) {
        out.kind = CellOutput::Kind::Image;
        // nbformat allows the base64 split into lines.
        out.png = QByteArray::fromBase64(lines_to_text(rest.take(QStringLiteral("image/png"))).toLatin1());
    } else {
        out.kind = CellOutput::Kind::Result;
        out.text = plain;
        rest.remove(QStringLiteral("text/plain"));
    }
    out.extra_data = rest;
    return out;
}

QVector<CellOutput> outputs_from_v4(const QJsonArray& arr) {
    QVector<CellOutput> outputs;
    for (const QJsonValue& v : arr) {
        const QJsonObject o = v.toObject();
        const QString type = o.value(QStringLiteral("output_type")).toString();
        if (type == QLatin1String("stream")) {
            CellOutput out;
            out.kind = CellOutput::Kind::Stream;
            out.is_stderr = o.value(QStringLiteral("name")).toString() == QLatin1String("stderr");
            out.text = strip_ansi(lines_to_text(o.value(QStringLiteral("text"))));
            outputs.append(out);
        } else if (type == QLatin1String("error")) {
            CellOutput out;
            out.kind = CellOutput::Kind::Error;
            out.ename = o.value(QStringLiteral("ename")).toString();
            out.evalue = o.value(QStringLiteral("evalue")).toString();
            for (const QJsonValue& line : o.value(QStringLiteral("traceback")).toArray()) {
                QString l = strip_ansi(lines_to_text(line));
                if (l.endsWith(QLatin1Char('\n')))
                    l.chop(1);
                out.traceback << l;
            }
            out.text = out.traceback.join(QLatin1Char('\n'));
            out.error_line = error_line_from_traceback(out.traceback);
            outputs.append(out);
        } else if (type == QLatin1String("execute_result") || type == QLatin1String("display_data")) {
            CellOutput out = output_from_bundle(o.value(QStringLiteral("data")).toObject(),
                                                o.value(QStringLiteral("metadata")).toObject(),
                                                type == QLatin1String("display_data"));
            if (out.kind == CellOutput::Kind::Image && out.png.isEmpty())
                continue;
            outputs.append(out);
        }
    }
    return outputs;
}

// nbformat 3 outputs ("pyout", "pyerr", "stream" with "stream", data keys at top level).
QVector<CellOutput> outputs_from_v3(const QJsonArray& arr) {
    QVector<CellOutput> outputs;
    for (const QJsonValue& v : arr) {
        const QJsonObject o = v.toObject();
        const QString type = o.value(QStringLiteral("output_type")).toString();
        if (type == QLatin1String("stream")) {
            CellOutput out;
            out.is_stderr = o.value(QStringLiteral("stream")).toString() == QLatin1String("stderr");
            out.text = strip_ansi(lines_to_text(o.value(QStringLiteral("text"))));
            outputs.append(out);
        } else if (type == QLatin1String("pyerr")) {
            CellOutput out;
            out.kind = CellOutput::Kind::Error;
            out.ename = o.value(QStringLiteral("ename")).toString();
            out.evalue = o.value(QStringLiteral("evalue")).toString();
            for (const QJsonValue& line : o.value(QStringLiteral("traceback")).toArray())
                out.traceback << strip_ansi(line.toString());
            out.text = out.traceback.join(QLatin1Char('\n'));
            out.error_line = error_line_from_traceback(out.traceback);
            outputs.append(out);
        } else if (type == QLatin1String("pyout") || type == QLatin1String("display_data")) {
            QJsonObject data;
            if (o.contains(QStringLiteral("text")))
                data[QStringLiteral("text/plain")] = o.value(QStringLiteral("text"));
            if (o.contains(QStringLiteral("png")))
                data[QStringLiteral("image/png")] = o.value(QStringLiteral("png"));
            if (o.contains(QStringLiteral("html")))
                data[QStringLiteral("text/html")] = o.value(QStringLiteral("html"));
            CellOutput out = output_from_bundle(data, {}, type == QLatin1String("display_data"));
            if (out.kind == CellOutput::Kind::Image && out.png.isEmpty())
                continue;
            outputs.append(out);
        }
    }
    return outputs;
}

// ── Outputs: model -> file ───────────────────────────────────────────────────

QJsonArray outputs_to_json(const QVector<CellOutput>& outputs, int exec_count) {
    QJsonArray arr;
    const QJsonValue count = exec_count > 0 ? QJsonValue(exec_count) : QJsonValue();
    for (const CellOutput& out : outputs) {
        QJsonObject o;
        switch (out.kind) {
            case CellOutput::Kind::Stream:
                o[QStringLiteral("output_type")] = QStringLiteral("stream");
                o[QStringLiteral("name")] = out.is_stderr ? QStringLiteral("stderr") : QStringLiteral("stdout");
                o[QStringLiteral("text")] = text_to_lines(out.text);
                break;
            case CellOutput::Kind::Error: {
                o[QStringLiteral("output_type")] = QStringLiteral("error");
                o[QStringLiteral("ename")] = out.ename;
                o[QStringLiteral("evalue")] = out.evalue;
                QJsonArray tb;
                for (const QString& line : out.traceback)
                    tb.append(line);
                o[QStringLiteral("traceback")] = tb;
                break;
            }
            case CellOutput::Kind::Result:
            case CellOutput::Kind::Table:
            case CellOutput::Kind::Image: {
                QJsonObject data = out.extra_data;
                if (out.kind == CellOutput::Kind::Table) {
                    data[QStringLiteral("text/plain")] = text_to_lines(out.text);
                    data[QStringLiteral("text/html")] = text_to_lines(table_to_html(out.table));
                    data[QLatin1String(kTableMime)] = out.table;
                } else if (out.kind == CellOutput::Kind::Image) {
                    data[QStringLiteral("image/png")] = QString::fromLatin1(out.png.toBase64());
                    if (!data.contains(QStringLiteral("text/plain")))
                        data[QStringLiteral("text/plain")] = QJsonArray{QStringLiteral("<Figure>")};
                } else if (!out.text.isEmpty() || !data.contains(QStringLiteral("text/plain"))) {
                    data[QStringLiteral("text/plain")] = text_to_lines(out.text);
                }
                const bool display = out.is_display || out.kind == CellOutput::Kind::Image;
                o[QStringLiteral("output_type")] = display ? QStringLiteral("display_data")
                                                           : QStringLiteral("execute_result");
                if (!display)
                    o[QStringLiteral("execution_count")] = count;
                o[QStringLiteral("data")] = data;
                o[QStringLiteral("metadata")] = out.metadata;
                break;
            }
        }
        arr.append(o);
    }
    return arr;
}

// ── Cells ────────────────────────────────────────────────────────────────────

void read_cell_metadata(NotebookCell& cell, QJsonObject meta) {
    cell.title = meta.take(QStringLiteral("fincept_title")).toString().trimmed();
    QJsonObject jupyter = meta.value(QStringLiteral("jupyter")).toObject();
    cell.input_collapsed = jupyter.take(QStringLiteral("source_hidden")).toBool();
    cell.output_collapsed = jupyter.take(QStringLiteral("outputs_hidden")).toBool();
    if (meta.take(QStringLiteral("collapsed")).toBool())
        cell.output_collapsed = true;
    if (jupyter.isEmpty())
        meta.remove(QStringLiteral("jupyter"));
    else
        meta[QStringLiteral("jupyter")] = jupyter;
    cell.metadata = meta;
}

QJsonObject write_cell_metadata(const NotebookCell& cell) {
    QJsonObject meta = cell.metadata;
    if (!cell.title.trimmed().isEmpty())
        meta[QStringLiteral("fincept_title")] = cell.title.trimmed();
    QJsonObject jupyter = meta.value(QStringLiteral("jupyter")).toObject();
    if (cell.input_collapsed)
        jupyter[QStringLiteral("source_hidden")] = true;
    if (cell.output_collapsed && cell.is_code()) {
        jupyter[QStringLiteral("outputs_hidden")] = true;
        meta[QStringLiteral("collapsed")] = true;
    }
    if (!jupyter.isEmpty())
        meta[QStringLiteral("jupyter")] = jupyter;
    return meta;
}

void ensure_unique_ids(NotebookDoc& doc) {
    QSet<QString> seen;
    for (NotebookCell& c : doc.cells) {
        if (!valid_cell_id(c.id) || seen.contains(c.id))
            c.id = new_cell_id();
        seen.insert(c.id);
    }
}

bool parse_v3(const QJsonObject& root, NotebookDoc* out) {
    const QJsonArray sheets = root.value(QStringLiteral("worksheets")).toArray();
    out->metadata = root.value(QStringLiteral("metadata")).toObject();
    for (const QJsonValue& sv : sheets) {
        for (const QJsonValue& cv : sv.toObject().value(QStringLiteral("cells")).toArray()) {
            const QJsonObject co = cv.toObject();
            const QString type = co.value(QStringLiteral("cell_type")).toString();
            NotebookCell cell;
            if (type == QLatin1String("code")) {
                cell.type = NotebookCell::Type::Code;
                cell.source = lines_to_text(co.value(QStringLiteral("input")));
                cell.execution_count = co.value(QStringLiteral("prompt_number")).toInt(0);
                cell.outputs = outputs_from_v3(co.value(QStringLiteral("outputs")).toArray());
            } else if (type == QLatin1String("heading")) {
                cell.type = NotebookCell::Type::Markdown;
                const int level = std::clamp(co.value(QStringLiteral("level")).toInt(1), 1, 6);
                cell.source = QString(level, QLatin1Char('#')) + QLatin1Char(' ') +
                              lines_to_text(co.value(QStringLiteral("source")));
            } else {
                cell.type = type == QLatin1String("markdown") ? NotebookCell::Type::Markdown : NotebookCell::Type::Raw;
                cell.source = lines_to_text(co.value(QStringLiteral("source")));
            }
            read_cell_metadata(cell, co.value(QStringLiteral("metadata")).toObject());
            out->cells.append(cell);
        }
    }
    return true;
}

} // namespace

// ── Doc helpers ──────────────────────────────────────────────────────────────

QString NotebookDoc::environment() const {
    const QString name =
        metadata.value(QStringLiteral("kernelspec")).toObject().value(QStringLiteral("name")).toString();
    return name == QLatin1String("fincept-numpy1") ? QStringLiteral("numpy1") : QStringLiteral("numpy2");
}

void NotebookDoc::set_environment(const QString& env) {
    QJsonObject spec = metadata.value(QStringLiteral("kernelspec")).toObject();
    const bool numpy1 = env == QLatin1String("numpy1");
    spec[QStringLiteral("display_name")] = numpy1 ? QStringLiteral("Python 3 (Fincept numpy1)") : QStringLiteral("Python 3");
    spec[QStringLiteral("language")] = QStringLiteral("python");
    spec[QStringLiteral("name")] = numpy1 ? QStringLiteral("fincept-numpy1") : QStringLiteral("python3");
    metadata[QStringLiteral("kernelspec")] = spec;
}

int NotebookDoc::index_of(const QString& cell_id) const {
    for (int i = 0; i < cells.size(); ++i)
        if (cells[i].id == cell_id)
            return i;
    return -1;
}

QString new_cell_id() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

// ── Read / write ─────────────────────────────────────────────────────────────

bool parse_ipynb(const QByteArray& bytes, NotebookDoc* out, QString* error) {
    if (!out)
        return false;
    QJsonParseError perr{};
    const QJsonDocument jd = QJsonDocument::fromJson(bytes, &perr);
    if (jd.isNull() || !jd.isObject()) {
        if (error)
            *error = perr.error != QJsonParseError::NoError ? perr.errorString() : QStringLiteral("not a JSON object");
        return false;
    }
    const QJsonObject root = jd.object();
    NotebookDoc doc;
    if (root.contains(QStringLiteral("worksheets"))) {
        parse_v3(root, &doc);
    } else {
        if (!root.value(QStringLiteral("cells")).isArray()) {
            if (error)
                *error = QStringLiteral("no \"cells\" array");
            return false;
        }
        doc.metadata = root.value(QStringLiteral("metadata")).toObject();
        doc.nbformat_minor = root.value(QStringLiteral("nbformat_minor")).toInt(5);
        // Files written before format 2 (the old editor) gained a "\n" at the end of
        // every cell on each save (every line was written with one, the last
        // included): drop that trailing run.
        const bool legacy_trailing_newline =
            doc.metadata.value(QLatin1String(kFinceptMetaKey)).toObject().value(QStringLiteral("format")).toInt(0) <
                kFormat &&
            doc.metadata.value(QStringLiteral("kernelspec"))
                    .toObject()
                    .value(QStringLiteral("display_name"))
                    .toString()
                    .startsWith(QLatin1String("Python 3")) &&
            !doc.metadata.contains(QStringLiteral("language_info"));
        for (const QJsonValue& cv : root.value(QStringLiteral("cells")).toArray()) {
            const QJsonObject co = cv.toObject();
            NotebookCell cell;
            const QString type = co.value(QStringLiteral("cell_type")).toString(QStringLiteral("code"));
            cell.type = type == QLatin1String("markdown") ? NotebookCell::Type::Markdown
                        : type == QLatin1String("raw")    ? NotebookCell::Type::Raw
                                                          : NotebookCell::Type::Code;
            cell.id = co.value(QStringLiteral("id")).toString();
            cell.source = lines_to_text(co.value(QStringLiteral("source")));
            while (legacy_trailing_newline && cell.source.endsWith(QLatin1Char('\n')))
                cell.source.chop(1);
            read_cell_metadata(cell, co.value(QStringLiteral("metadata")).toObject());
            cell.attachments = co.value(QStringLiteral("attachments")).toObject();
            if (cell.is_code()) {
                cell.execution_count = co.value(QStringLiteral("execution_count")).toInt(0);
                cell.outputs = outputs_from_v4(co.value(QStringLiteral("outputs")).toArray());
            }
            doc.cells.append(cell);
        }
    }
    ensure_unique_ids(doc);
    *out = doc;
    return true;
}

QByteArray serialize_ipynb(const NotebookDoc& doc) {
    QJsonArray cells;
    for (const NotebookCell& cell : doc.cells) {
        QJsonObject co;
        co[QStringLiteral("cell_type")] = type_name(cell.type);
        co[QStringLiteral("id")] = valid_cell_id(cell.id) ? cell.id : new_cell_id();
        co[QStringLiteral("metadata")] = write_cell_metadata(cell);
        co[QStringLiteral("source")] = text_to_lines(cell.source);
        if (!cell.attachments.isEmpty() && !cell.is_code())
            co[QStringLiteral("attachments")] = cell.attachments;
        if (cell.is_code()) {
            co[QStringLiteral("execution_count")] =
                cell.execution_count > 0 ? QJsonValue(cell.execution_count) : QJsonValue();
            co[QStringLiteral("outputs")] = outputs_to_json(cell.outputs, cell.execution_count);
        }
        cells.append(co);
    }
    QJsonObject metadata = doc.metadata;
    if (!metadata.contains(QStringLiteral("kernelspec"))) {
        NotebookDoc tmp;
        tmp.set_environment(QStringLiteral("numpy2"));
        metadata[QStringLiteral("kernelspec")] = tmp.metadata.value(QStringLiteral("kernelspec"));
    }
    if (!metadata.contains(QStringLiteral("language_info")))
        metadata[QStringLiteral("language_info")] = QJsonObject{{QStringLiteral("name"), QStringLiteral("python")}};
    QJsonObject fincept = metadata.value(QLatin1String(kFinceptMetaKey)).toObject();
    fincept[QStringLiteral("format")] = kFormat;
    metadata[QLatin1String(kFinceptMetaKey)] = fincept;

    QJsonObject root;
    root[QStringLiteral("cells")] = cells;
    root[QStringLiteral("metadata")] = metadata;
    root[QStringLiteral("nbformat")] = 4;
    root[QStringLiteral("nbformat_minor")] = std::max(5, doc.nbformat_minor);
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

// ── Export ───────────────────────────────────────────────────────────────────

QString export_python(const NotebookDoc& doc, const QString& title) {
    QString out;
    out += QStringLiteral("# %1\n# Exported from Fincept Notebook (cells marked with \"# %%\")\n\n").arg(title);
    for (const NotebookCell& cell : doc.cells) {
        if (cell.is_code()) {
            out += cell.title.isEmpty() ? QStringLiteral("# %%\n") : QStringLiteral("# %% %1\n").arg(cell.title);
            out += cell.source;
            if (!cell.source.endsWith(QLatin1Char('\n')))
                out += QLatin1Char('\n');
        } else {
            out += cell.type == NotebookCell::Type::Markdown ? QStringLiteral("# %% [markdown]\n")
                                                             : QStringLiteral("# %% [raw]\n");
            for (const QString& line : cell.source.split(QLatin1Char('\n')))
                out += line.isEmpty() ? QStringLiteral("#\n") : QStringLiteral("# ") + line + QLatin1Char('\n');
        }
        out += QLatin1Char('\n');
    }
    return out;
}

QString table_to_html(const QJsonObject& table) {
    auto cell = [](const QJsonValue& v) {
        if (v.isDouble())
            return QString::number(v.toDouble(), 'g', 12);
        if (v.isBool())
            return v.toBool() ? QStringLiteral("True") : QStringLiteral("False");
        if (v.isNull() || v.isUndefined())
            return QStringLiteral("NaN");
        return v.toString().toHtmlEscaped();
    };
    QString html = QStringLiteral("<table border=\"1\" class=\"dataframe\">\n<thead><tr><th>%1</th>")
                       .arg(table.value(QStringLiteral("index_name")).toString().toHtmlEscaped());
    for (const QJsonValue& c : table.value(QStringLiteral("columns")).toArray())
        html += QStringLiteral("<th>%1</th>").arg(c.toString().toHtmlEscaped());
    html += QStringLiteral("</tr></thead>\n<tbody>\n");
    const QJsonArray index = table.value(QStringLiteral("index")).toArray();
    const QJsonArray rows = table.value(QStringLiteral("rows")).toArray();
    for (int r = 0; r < rows.size(); ++r) {
        html += QStringLiteral("<tr><th>%1</th>").arg(index.at(r).toString().toHtmlEscaped());
        for (const QJsonValue& v : rows.at(r).toArray())
            html += QStringLiteral("<td>%1</td>").arg(cell(v));
        html += QStringLiteral("</tr>\n");
    }
    html += QStringLiteral("</tbody>\n</table>\n");
    return html;
}

namespace {

QString md_inline(const QString& line) {
    // Code spans first, held out of the emphasis / link passes.
    QStringList spans;
    QString text;
    static const QRegularExpression code_re(QStringLiteral("`([^`]+)`"));
    int last = 0;
    auto it = code_re.globalMatch(line);
    while (it.hasNext()) {
        const auto m = it.next();
        text += line.mid(last, m.capturedStart() - last).toHtmlEscaped();
        text += QStringLiteral("\x01%1\x02").arg(spans.size());
        spans << m.captured(1).toHtmlEscaped();
        last = m.capturedEnd();
    }
    text += line.mid(last).toHtmlEscaped();

    static const QRegularExpression img_re(QStringLiteral("!\\[([^\\]]*)\\]\\(([^)]+)\\)"));
    text.replace(img_re, QStringLiteral("[\\1]")); // no remote images: the alt text stands in
    static const QRegularExpression link_re(QStringLiteral("\\[([^\\]]+)\\]\\(([^)\\s]+)\\)"));
    QString linked;
    last = 0;
    auto li = link_re.globalMatch(text);
    while (li.hasNext()) {
        const auto m = li.next();
        linked += text.mid(last, m.capturedStart() - last);
        const QString url = m.captured(2);
        const bool safe = url.startsWith(QLatin1String("http://")) || url.startsWith(QLatin1String("https://")) ||
                          url.startsWith(QLatin1String("mailto:"));
        linked += safe ? QStringLiteral("<a href=\"%1\">%2</a>").arg(url, m.captured(1)) : m.captured(1);
        last = m.capturedEnd();
    }
    linked += text.mid(last);
    text = linked;

    static const QRegularExpression bold_re(QStringLiteral("\\*\\*([^*]+)\\*\\*|__([^_]+)__"));
    text.replace(bold_re, QStringLiteral("<b>\\1\\2</b>"));
    static const QRegularExpression ital_re(QStringLiteral("(^|[^*\\w])\\*([^*\\s][^*]*)\\*(?!\\*)"));
    text.replace(ital_re, QStringLiteral("\\1<i>\\2</i>"));
    static const QRegularExpression strike_re(QStringLiteral("~~([^~]+)~~"));
    text.replace(strike_re, QStringLiteral("<s>\\1</s>"));

    for (int i = 0; i < spans.size(); ++i)
        text.replace(QStringLiteral("\x01%1\x02").arg(i), QStringLiteral("<code>%1</code>").arg(spans[i]));
    return text;
}

QStringList split_table_row(QString line) {
    line = line.trimmed();
    if (line.startsWith(QLatin1Char('|')))
        line.remove(0, 1);
    if (line.endsWith(QLatin1Char('|')))
        line.chop(1);
    QStringList cells = line.split(QLatin1Char('|'));
    for (QString& c : cells)
        c = c.trimmed();
    return cells;
}

bool is_table_separator(const QString& line) {
    static const QRegularExpression re(QStringLiteral("^\\s*\\|?\\s*:?-{2,}:?\\s*(\\|\\s*:?-{2,}:?\\s*)*\\|?\\s*$"));
    return re.match(line).hasMatch();
}

} // namespace

QString markdown_to_html(const QString& markdown, const HtmlPalette& pal, bool with_style_block) {
    QString html;
    if (with_style_block) {
        html += QStringLiteral(
                    "<style>"
                    "body{color:%1;font-family:%2;font-size:%3px;}"
                    "h1,h2,h3,h4,h5,h6{color:%1;font-weight:700;margin:6px 0 3px 0;}"
                    "h1{font-size:%4px;} h2{font-size:%5px;} h3{font-size:%6px;} h4,h5,h6{font-size:%3px;}"
                    "p{margin:2px 0 6px 0;} li{margin:1px 0;}"
                    "code{background:%7;color:%1;font-family:%2;}"
                    "pre{background:%7;color:%1;font-family:%2;padding:6px;margin:4px 0;}"
                    "a{color:%8;} blockquote{color:%9;margin:4px 0 4px 12px;}"
                    "table{border-collapse:collapse;margin:4px 0;} th{color:%9;font-weight:700;text-align:left;}"
                    "th,td{border:1px solid %10;padding:2px 8px;}"
                    "</style>")
                    .arg(pal.text, pal.font_family)
                    .arg(pal.font_px)
                    .arg(pal.font_px + 8)
                    .arg(pal.font_px + 5)
                    .arg(pal.font_px + 2)
                    .arg(pal.code_bg, pal.link, pal.dim, pal.border);
    }

    const QStringList lines = markdown.split(QLatin1Char('\n'));
    QString para;
    QString list_tag; // "ul" / "ol" while inside a list
    auto flush_para = [&]() {
        if (!para.isEmpty()) {
            html += QStringLiteral("<p>") + md_inline(para) + QStringLiteral("</p>");
            para.clear();
        }
    };
    auto close_list = [&]() {
        if (!list_tag.isEmpty()) {
            html += QStringLiteral("</%1>").arg(list_tag);
            list_tag.clear();
        }
    };

    static const QRegularExpression heading_re(QStringLiteral("^(#{1,6})\\s+(.*?)\\s*#*\\s*$"));
    static const QRegularExpression ul_re(QStringLiteral("^\\s*[-*+]\\s+(.*)$"));
    static const QRegularExpression ol_re(QStringLiteral("^\\s*\\d+[.)]\\s+(.*)$"));
    static const QRegularExpression hr_re(QStringLiteral("^\\s*([-*_])(\\s*\\1){2,}\\s*$"));

    for (int i = 0; i < lines.size(); ++i) {
        const QString& line = lines[i];
        const QString trimmed = line.trimmed();

        if (trimmed.startsWith(QLatin1String("```")) || trimmed.startsWith(QLatin1String("~~~"))) {
            flush_para();
            close_list();
            const QString fence = trimmed.left(3);
            QString code;
            ++i;
            for (; i < lines.size() && !lines[i].trimmed().startsWith(fence); ++i)
                code += lines[i] + QLatin1Char('\n');
            if (code.endsWith(QLatin1Char('\n')))
                code.chop(1);
            html += QStringLiteral("<pre>") + code.toHtmlEscaped() + QStringLiteral("</pre>");
            continue;
        }
        if (trimmed.isEmpty()) {
            flush_para();
            close_list();
            continue;
        }
        if (const auto m = heading_re.match(trimmed); m.hasMatch()) {
            flush_para();
            close_list();
            const int level = static_cast<int>(m.captured(1).size());
            html += QStringLiteral("<h%1>%2</h%1>").arg(level).arg(md_inline(m.captured(2)));
            continue;
        }
        if (hr_re.match(trimmed).hasMatch()) {
            flush_para();
            close_list();
            html += QStringLiteral("<hr/>");
            continue;
        }
        if (trimmed.startsWith(QLatin1Char('|')) && i + 1 < lines.size() && is_table_separator(lines[i + 1])) {
            flush_para();
            close_list();
            html += QStringLiteral("<table><tr>");
            for (const QString& c : split_table_row(trimmed))
                html += QStringLiteral("<th>") + md_inline(c) + QStringLiteral("</th>");
            html += QStringLiteral("</tr>");
            i += 2;
            for (; i < lines.size() && lines[i].trimmed().startsWith(QLatin1Char('|')); ++i) {
                html += QStringLiteral("<tr>");
                for (const QString& c : split_table_row(lines[i]))
                    html += QStringLiteral("<td>") + md_inline(c) + QStringLiteral("</td>");
                html += QStringLiteral("</tr>");
            }
            --i;
            html += QStringLiteral("</table>");
            continue;
        }
        if (trimmed.startsWith(QLatin1Char('>'))) {
            flush_para();
            close_list();
            html += QStringLiteral("<blockquote>") + md_inline(trimmed.mid(1).trimmed()) + QStringLiteral("</blockquote>");
            continue;
        }
        const auto ul = ul_re.match(line);
        const auto ol = ul.hasMatch() ? QRegularExpressionMatch() : ol_re.match(line);
        if (ul.hasMatch() || ol.hasMatch()) {
            flush_para();
            const QString tag = ul.hasMatch() ? QStringLiteral("ul") : QStringLiteral("ol");
            if (list_tag != tag) {
                close_list();
                html += QStringLiteral("<%1>").arg(tag);
                list_tag = tag;
            }
            html += QStringLiteral("<li>") + md_inline((ul.hasMatch() ? ul : ol).captured(1)) + QStringLiteral("</li>");
            continue;
        }
        close_list();
        para += para.isEmpty() ? trimmed : QLatin1Char(' ') + trimmed;
    }
    flush_para();
    close_list();
    return html;
}

QString export_html(const NotebookDoc& doc, const QString& title, const HtmlPalette& pal) {
    QString html;
    html += QStringLiteral("<!doctype html>\n<html><head><meta charset=\"utf-8\"><title>%1</title>\n").arg(title.toHtmlEscaped());
    html += QStringLiteral(
                "<style>"
                "body{background:%1;color:%3;font-family:%10;font-size:%11px;margin:24px auto;max-width:1100px;padding:0 16px;}"
                "h1.nbtitle{font-size:%12px;border-bottom:2px solid %6;padding-bottom:6px;}"
                ".cell{margin:10px 0;} .lbl{color:%4;font-size:%13px;margin-bottom:2px;}"
                "pre{margin:0;padding:8px 10px;white-space:pre-wrap;word-wrap:break-word;font-family:%10;}"
                "pre.code{background:%8;border-left:3px solid %5;}"
                "pre.out{background:%2;} pre.err{background:%2;color:%4;}"
                "pre.error{background:%2;color:%9;border-left:3px solid %9;}"
                ".md code,.md pre{background:%8;} .md a{color:%7;}"
                "table.dataframe{border-collapse:collapse;font-size:%13px;margin:4px 0;}"
                "table.dataframe th,table.dataframe td{border:1px solid %5;padding:2px 8px;text-align:right;}"
                "table.dataframe thead th{color:%4;} img{max-width:100%%;}"
                "</style></head><body>\n")
                .arg(pal.bg, pal.surface, pal.text, pal.dim, pal.border, pal.accent, pal.link, pal.code_bg, pal.error)
                .arg(pal.font_family)
                .arg(pal.font_px)
                .arg(pal.font_px + 10)
                .arg(std::max(9, pal.font_px - 2));
    html += QStringLiteral("<h1 class=\"nbtitle\">%1</h1>\n").arg(title.toHtmlEscaped());
    for (const NotebookCell& cell : doc.cells) {
        html += QStringLiteral("<div class=\"cell\">");
        if (cell.type == NotebookCell::Type::Markdown) {
            html += QStringLiteral("<div class=\"md\">") + markdown_to_html(cell.source, pal, false) + QStringLiteral("</div>");
        } else if (cell.type == NotebookCell::Type::Raw) {
            html += QStringLiteral("<pre class=\"out\">") + cell.source.toHtmlEscaped() + QStringLiteral("</pre>");
        } else {
            html += QStringLiteral("<div class=\"lbl\">In [%1]</div>")
                        .arg(cell.execution_count > 0 ? QString::number(cell.execution_count) : QStringLiteral(" "));
            html += QStringLiteral("<pre class=\"code\">") + cell.source.toHtmlEscaped() + QStringLiteral("</pre>");
            for (const CellOutput& out : cell.outputs) {
                switch (out.kind) {
                    case CellOutput::Kind::Stream:
                        html += QStringLiteral("<pre class=\"%1\">").arg(out.is_stderr ? QStringLiteral("err") : QStringLiteral("out")) +
                                out.text.toHtmlEscaped() + QStringLiteral("</pre>");
                        break;
                    case CellOutput::Kind::Result:
                        html += QStringLiteral("<pre class=\"out\">") + out.text.toHtmlEscaped() + QStringLiteral("</pre>");
                        break;
                    case CellOutput::Kind::Table:
                        html += table_to_html(out.table);
                        break;
                    case CellOutput::Kind::Image:
                        html += QStringLiteral("<img alt=\"figure\" src=\"data:image/png;base64,%1\"/>")
                                    .arg(QString::fromLatin1(out.png.toBase64()));
                        break;
                    case CellOutput::Kind::Error:
                        html += QStringLiteral("<pre class=\"error\">") +
                                (out.ename + QStringLiteral(": ") + out.evalue).toHtmlEscaped() +
                                (out.traceback.isEmpty() ? QString()
                                                         : QStringLiteral("\n\n") + out.traceback.join(QLatin1Char('\n')).toHtmlEscaped()) +
                                QStringLiteral("</pre>");
                        break;
                }
            }
        }
        html += QStringLiteral("</div>\n");
    }
    html += QStringLiteral("</body></html>\n");
    return html;
}

// ── Kernel events ────────────────────────────────────────────────────────────

QString strip_ansi(const QString& text) {
    if (!text.contains(QChar(0x1b)))
        return text;
    static const QRegularExpression ansi(QStringLiteral("\\x1b\\[[0-9;?]*[A-Za-z]|\\x1b\\][^\\x07]*\\x07"));
    QString out = text;
    out.remove(ansi);
    return out;
}

QString apply_terminal_text(const QString& existing, const QString& chunk) {
    QString add = strip_ansi(chunk);
    add.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    const bool pending_cr = existing.endsWith(QLatin1Char('\r'));
    if (!add.contains(QLatin1Char('\r')) && !pending_cr)
        return existing + add; // the common case: plain appended text
    // Re-process only the current (last) line of `existing` with the new text.
    const qsizetype line_start = existing.lastIndexOf(QLatin1Char('\n')) + 1;
    const QString tail = existing.mid(line_start) + add;
    QString out = existing.left(line_start);
    const qsizetype base = out.size();
    for (qsizetype i = 0; i < tail.size(); ++i) {
        const QChar c = tail.at(i);
        if (c == QLatin1Char('\r')) {
            if (i + 1 == tail.size()) {
                out += c; // keep a trailing CR: the next chunk decides what it overwrites
                break;
            }
            if (tail.at(i + 1) == QLatin1Char('\n'))
                continue;
            out.truncate(std::max(base, out.lastIndexOf(QLatin1Char('\n')) + 1));
            continue;
        }
        out += c;
    }
    return out;
}

int error_line_from_traceback(const QStringList& traceback) {
    static const QRegularExpression re(QStringLiteral("File \"<cell>\", line (\\d+)"));
    int line = 0;
    for (const QString& l : traceback) {
        auto it = re.globalMatch(l);
        while (it.hasNext())
            line = it.next().captured(1).toInt();
    }
    return line;
}

int append_kernel_event(QVector<CellOutput>& outputs, const QJsonObject& event) {
    const QString type = event.value(QStringLiteral("type")).toString();
    if (type == QLatin1String("stream")) {
        const bool err = event.value(QStringLiteral("name")).toString() == QLatin1String("stderr");
        const QString text = event.value(QStringLiteral("text")).toString();
        if (!outputs.isEmpty() && outputs.last().kind == CellOutput::Kind::Stream && outputs.last().is_stderr == err) {
            outputs.last().text = apply_terminal_text(outputs.last().text, text);
            return static_cast<int>(outputs.size() - 1);
        }
        CellOutput out;
        out.is_stderr = err;
        out.text = apply_terminal_text(QString(), text);
        outputs.append(out);
        return static_cast<int>(outputs.size() - 1);
    }
    if (type == QLatin1String("result")) {
        CellOutput out;
        out.kind = CellOutput::Kind::Result;
        out.text = event.value(QStringLiteral("text")).toString();
        outputs.append(out);
        return static_cast<int>(outputs.size() - 1);
    }
    if (type == QLatin1String("display")) {
        const QString mime = event.value(QStringLiteral("mime")).toString();
        if (mime == QLatin1String("application/x-table")) {
            QJsonObject table = event;
            table.remove(QStringLiteral("type"));
            table.remove(QStringLiteral("mime"));
            table.remove(QStringLiteral("id"));
            if (!outputs.isEmpty() && outputs.last().kind == CellOutput::Kind::Result) {
                outputs.last().kind = CellOutput::Kind::Table; // the table of the repr just sent
                outputs.last().table = table;
                return static_cast<int>(outputs.size() - 1);
            }
            CellOutput out;
            out.kind = CellOutput::Kind::Table;
            out.table = table;
            outputs.append(out);
            return static_cast<int>(outputs.size() - 1);
        }
        if (mime == QLatin1String("image/png")) {
            CellOutput out;
            out.kind = CellOutput::Kind::Image;
            out.is_display = true;
            out.png = QByteArray::fromBase64(event.value(QStringLiteral("data")).toString().toLatin1());
            if (out.png.isEmpty())
                return -1;
            outputs.append(out);
            return static_cast<int>(outputs.size() - 1);
        }
        return -1;
    }
    if (type == QLatin1String("error")) {
        CellOutput out;
        out.kind = CellOutput::Kind::Error;
        out.ename = event.value(QStringLiteral("ename")).toString();
        out.evalue = event.value(QStringLiteral("evalue")).toString();
        for (const QJsonValue& v : event.value(QStringLiteral("traceback")).toArray()) {
            QString l = strip_ansi(v.toString());
            if (l.endsWith(QLatin1Char('\n')))
                l.chop(1);
            out.traceback << l;
        }
        out.text = out.traceback.join(QLatin1Char('\n'));
        out.error_line = event.value(QStringLiteral("line")).toInt(0);
        if (out.error_line <= 0)
            out.error_line = error_line_from_traceback(out.traceback);
        outputs.append(out);
        return static_cast<int>(outputs.size() - 1);
    }
    return -1;
}

// ── Names / outline ──────────────────────────────────────────────────────────

QString cell_display_name(const NotebookCell& cell) {
    if (!cell.title.trimmed().isEmpty())
        return cell.title.trimmed();
    QString s;
    for (const QString& ln : cell.source.split(QLatin1Char('\n'))) {
        const QString t = ln.trimmed();
        if (t.isEmpty() || t == QLatin1String("#") || t.startsWith(QLatin1String("```")))
            continue;
        s = t;
        break;
    }
    if (s.isEmpty())
        return {};
    s.remove(QLatin1Char('`'));
    int i = 0;
    while (i < s.size() && QStringLiteral("#>*-=+ \t").contains(s.at(i)))
        ++i;
    s = s.mid(i);
    // Drop a leading emoji / symbol run so "📘 Title" becomes "Title".
    while (!s.isEmpty() && !s.at(0).isLetterOrNumber() && s.at(0) != QLatin1Char('(') && s.at(0) != QLatin1Char('"') &&
           s.at(0) != QLatin1Char('\'') && s.at(0) != QLatin1Char('_') && s.at(0) != QLatin1Char('['))
        s.remove(0, 1);
    return s.simplified();
}

QVector<OutlineEntry> outline(const NotebookDoc& doc) {
    static const QRegularExpression heading_re(QStringLiteral("^(#{1,6})\\s+(.+?)\\s*#*\\s*$"));
    QVector<OutlineEntry> out;
    for (int i = 0; i < doc.cells.size(); ++i) {
        const NotebookCell& cell = doc.cells[i];
        if (cell.type == NotebookCell::Type::Markdown) {
            bool fenced = false;
            for (const QString& line : cell.source.split(QLatin1Char('\n'))) {
                const QString t = line.trimmed();
                if (t.startsWith(QLatin1String("```")) || t.startsWith(QLatin1String("~~~"))) {
                    fenced = !fenced;
                    continue;
                }
                if (fenced)
                    continue;
                const auto m = heading_re.match(t);
                if (!m.hasMatch())
                    continue;
                NotebookCell tmp;
                tmp.source = m.captured(2);
                OutlineEntry e;
                e.cell_index = i;
                e.level = static_cast<int>(m.captured(1).size());
                e.text = cell_display_name(tmp);
                if (e.text.isEmpty())
                    e.text = m.captured(2).trimmed();
                out.append(e);
            }
        } else if (cell.is_code()) {
            OutlineEntry e;
            e.cell_index = i;
            e.level = 0;
            e.text = cell_display_name(cell);
            out.append(e);
        }
    }
    return out;
}

QString fingerprint(const NotebookDoc& doc) {
    QString fp;
    fp.reserve(4096);
    for (const NotebookCell& c : doc.cells) {
        fp += type_name(c.type);
        fp += QLatin1Char('\x1f');
        fp += c.title;
        fp += QLatin1Char('\x1f');
        fp += c.input_collapsed ? QLatin1Char('1') : QLatin1Char('0');
        fp += c.output_collapsed ? QLatin1Char('1') : QLatin1Char('0');
        fp += QLatin1Char('\x1f');
        fp += c.source;
        fp += QLatin1Char('\x1e');
    }
    fp += doc.environment();
    return fp;
}

QStringList identifiers_in(const QString& source) {
    static const QRegularExpression re(QStringLiteral("\\b[A-Za-z_][A-Za-z0-9_]{2,}\\b"));
    QSet<QString> seen;
    QStringList out;
    auto it = re.globalMatch(source);
    while (it.hasNext()) {
        const QString w = it.next().captured(0);
        if (!seen.contains(w)) {
            seen.insert(w);
            out << w;
        }
    }
    return out;
}

} // namespace fincept::screens::notebook
