// ArtefactLogic.cpp — tables, names, tool-result shapes, artefact extraction and
// retention for run artefacts. Qt Core only (linked by tests/tst_run_artefacts).

#include "services/agent_tasks/ArtefactLogic.h"

#include "services/agent_tasks/ReportHandoff.h"

#include <QCoreApplication>
#include <QHash>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocale>
#include <QRegularExpression>

#include <algorithm>

namespace fincept::agent_tasks::artefact_logic {

namespace artefact_logic_detail {

QString al_tr(const char* text) {
    return QCoreApplication::translate("fincept::agent_tasks::Artefacts", text);
}

/// A markdown table row split into trimmed cells; a pipe escaped as "\|" stays text.
QStringList al_split_row(const QString& line) {
    QString t = line.trimmed();
    if (t.startsWith(QLatin1Char('|')))
        t = t.mid(1);
    if (t.endsWith(QLatin1Char('|')) && !t.endsWith(QStringLiteral("\\|")))
        t.chop(1);
    QStringList cells;
    QString cell;
    for (int i = 0; i < t.size(); ++i) {
        const QChar c = t.at(i);
        if (c == QLatin1Char('\\') && i + 1 < t.size() && t.at(i + 1) == QLatin1Char('|')) {
            cell += QLatin1Char('|');
            ++i;
            continue;
        }
        if (c == QLatin1Char('|')) {
            cells << cell.trimmed();
            cell.clear();
            continue;
        }
        cell += c;
    }
    cells << cell.trimmed();
    return cells;
}

/// "|---|:--:|" / "--- | ---": every cell is dashes with optional colons.
bool al_is_delimiter_row(const QString& line, int* cells_out) {
    const QString t = line.trimmed();
    if (!t.contains(QLatin1Char('-')))
        return false;
    const QStringList cells = al_split_row(t);
    if (cells.isEmpty())
        return false;
    static const QRegularExpression kCell(QStringLiteral("^:?-{1,}:?$"));
    for (const QString& c : cells) {
        if (!kCell.match(c).hasMatch())
            return false;
    }
    if (cells_out)
        *cells_out = static_cast<int>(cells.size());
    return true;
}

/// Cell text without markdown emphasis / code ticks / line-break tags.
QString al_clean_cell(QString c) {
    static const QRegularExpression kBr(QStringLiteral("<br\\s*/?>"), QRegularExpression::CaseInsensitiveOption);
    c.replace(kBr, QStringLiteral(" "));
    c.remove(QStringLiteral("**"));
    c.remove(QStringLiteral("__"));
    c.remove(QLatin1Char('`'));
    return c.simplified();
}

/// The caption of a table: a heading or "lead-in:" line just above it.
QString al_caption(const QStringList& lines, int header_line) {
    for (int i = header_line - 1, seen = 0; i >= 0 && seen < 3; --i) {
        QString t = lines.at(i).trimmed();
        if (t.isEmpty())
            continue;
        ++seen;
        if (t.contains(QLatin1Char('|')))
            return {}; // another table right above
        int marks = 0;
        while (marks < t.size() && t.at(marks) == QLatin1Char('#'))
            ++marks;
        if (marks > 0 && marks <= 6)
            return al_clean_cell(t.mid(marks)).left(120);
        if (t.startsWith(QStringLiteral("**")) && t.endsWith(QStringLiteral("**")) && t.size() > 4)
            return al_clean_cell(t).left(120);
        if (t.endsWith(QLatin1Char(':')))
            return al_clean_cell(t.left(t.size() - 1)).left(120);
        return {};
    }
    return {};
}

void al_normalize(ArtefactTable& t) {
    if (t.columns.size() > kMaxTableColumns)
        t.columns = t.columns.mid(0, kMaxTableColumns);
    const int n = static_cast<int>(t.columns.size());
    for (QStringList& r : t.rows) {
        while (r.size() < n)
            r << QString();
        if (r.size() > n)
            r = r.mid(0, n);
    }
}

QString al_value_text(const QJsonValue& v) {
    switch (v.type()) {
        case QJsonValue::String:
            return v.toString();
        case QJsonValue::Double: {
            const double d = v.toDouble();
            return QString::number(d, 'g', 15);
        }
        case QJsonValue::Bool:
            return v.toBool() ? QStringLiteral("true") : QStringLiteral("false");
        case QJsonValue::Array:
            return QString::fromUtf8(QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact));
        case QJsonValue::Object:
            return QString::fromUtf8(QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact));
        case QJsonValue::Null:
        case QJsonValue::Undefined:
            break;
    }
    return {};
}

bool al_is_scalar(const QJsonValue& v) {
    return v.isString() || v.isDouble() || v.isBool() || v.isNull();
}

/// A table from an array of objects; false when the array is not that.
bool al_table_from_objects(const QJsonArray& arr, ArtefactTable* out) {
    if (arr.isEmpty())
        return false;
    QStringList cols;
    for (const QJsonValue& v : arr) {
        if (!v.isObject())
            return false;
        const QJsonObject o = v.toObject();
        for (auto it = o.constBegin(); it != o.constEnd(); ++it)
            if (!cols.contains(it.key()) && cols.size() < kMaxTableColumns)
                cols << it.key();
    }
    if (cols.isEmpty())
        return false;
    // QJsonObject keeps its keys sorted (the source order is gone): the columns
    // that identify a row come first, the rest alphabetically.
    static const QStringList kLead = {QStringLiteral("symbol"), QStringLiteral("ticker"), QStringLiteral("name"),
                                      QStringLiteral("date"),   QStringLiteral("time"),   QStringLiteral("timestamp"),
                                      QStringLiteral("period"), QStringLiteral("label")};
    std::stable_sort(cols.begin(), cols.end(), [](const QString& a, const QString& b) {
        const auto rank = [](const QString& k) {
            const qsizetype i = kLead.indexOf(k.toLower());
            return i < 0 ? kLead.size() : i;
        };
        return rank(a) < rank(b);
    });
    ArtefactTable t;
    t.columns = cols;
    int count = 0;
    for (const QJsonValue& v : arr) {
        if (++count > kMaxTableRows)
            break;
        const QJsonObject o = v.toObject();
        QStringList row;
        row.reserve(cols.size());
        for (const QString& c : std::as_const(cols))
            row << al_value_text(o.value(c));
        t.rows.append(row);
    }
    *out = t;
    return true;
}

bool al_table_from_arrays(const QJsonArray& arr, ArtefactTable* out) {
    if (arr.isEmpty() || !arr.first().isArray())
        return false;
    ArtefactTable t;
    const QJsonArray first = arr.first().toArray();
    bool header = arr.size() > 1 && !first.isEmpty();
    for (const QJsonValue& v : first)
        header = header && v.isString();
    int width = 0;
    for (const QJsonValue& v : arr) {
        if (!v.isArray())
            return false;
        width = std::max(width, static_cast<int>(v.toArray().size()));
    }
    width = std::min(width, kMaxTableColumns);
    if (width == 0)
        return false;
    if (header) {
        for (const QJsonValue& v : first)
            t.columns << v.toString();
    }
    while (t.columns.size() < width)
        t.columns << QString::number(t.columns.size() + 1);
    for (int i = header ? 1 : 0; i < arr.size() && t.rows.size() < kMaxTableRows; ++i) {
        QStringList row;
        for (const QJsonValue& v : arr.at(i).toArray())
            row << al_value_text(v);
        t.rows.append(row);
    }
    al_normalize(t);
    *out = t;
    return true;
}

QString al_number_clip(const QString& s, int max_chars) {
    if (s.size() <= max_chars)
        return s;
    return s.left(std::max(0, max_chars - 1)) + QChar(0x2026);
}

} // namespace artefact_logic_detail

using namespace artefact_logic_detail;

// ── Tables ───────────────────────────────────────────────────────────────────

QVector<MarkdownTable> find_markdown_tables(const QString& markdown, int max_tables) {
    QVector<MarkdownTable> out;
    if (markdown.isEmpty() || max_tables <= 0)
        return out;
    const QStringList lines = markdown.split(QLatin1Char('\n'));
    bool in_fence = false;
    for (int i = 0; i + 1 < lines.size() && out.size() < max_tables; ++i) {
        const QString line = lines.at(i).trimmed();
        if (line.startsWith(QStringLiteral("```")) || line.startsWith(QStringLiteral("~~~"))) {
            in_fence = !in_fence;
            continue;
        }
        if (in_fence || !line.contains(QLatin1Char('|')))
            continue;
        int delim_cells = 0;
        if (!al_is_delimiter_row(lines.at(i + 1), &delim_cells))
            continue;
        QStringList header = al_split_row(line);
        if (header.size() < 2 && delim_cells < 2)
            continue; // "a | b" with fewer than two columns is not a table worth keeping
        MarkdownTable mt;
        mt.start_line = i;
        mt.caption = al_caption(lines, i);
        for (QString& h : header)
            h = al_clean_cell(h);
        mt.table.columns = header;
        int j = i + 2;
        for (; j < lines.size(); ++j) {
            const QString body = lines.at(j).trimmed();
            if (body.isEmpty() || !body.contains(QLatin1Char('|')) || body.startsWith(QStringLiteral("```")))
                break;
            if (mt.table.rows.size() >= kMaxTableRows)
                continue;
            QStringList cells = al_split_row(body);
            for (QString& c : cells)
                c = al_clean_cell(c);
            mt.table.rows.append(cells);
        }
        al_normalize(mt.table);
        if (!mt.table.rows.isEmpty())
            out.append(mt);
        i = j - 1;
    }
    return out;
}

bool looks_numeric(const QString& cell) {
    static const QRegularExpression kNum(
        QStringLiteral("^[+\\-]?[$€£¥₹]?\\(?[0-9][0-9,]*(\\.[0-9]+)?\\)?\\s?(%|x|X|bp|bps|[kKmMbBtT])?$"));
    const QString t = cell.trimmed();
    return !t.isEmpty() && kNum.match(t).hasMatch();
}

QString table_to_csv(const ArtefactTable& table) {
    const auto encode = [](const QString& raw) {
        QString c = raw;
        if (!c.isEmpty()) {
            const QChar first = c.at(0);
            const bool formula_lead = first == QLatin1Char('=') || first == QLatin1Char('+') || first == QLatin1Char('-') ||
                                      first == QLatin1Char('@') || first == QLatin1Char('\t') || first == QLatin1Char('\r');
            if (formula_lead && !looks_numeric(c))
                c.prepend(QLatin1Char('\''));
        }
        const bool quote = c.contains(QLatin1Char(',')) || c.contains(QLatin1Char('"')) || c.contains(QLatin1Char('\n')) ||
                           c.contains(QLatin1Char('\r')) || c.contains(QLatin1Char('|')) ||
                           c.contains(QLatin1Char('\t')) || (!c.isEmpty() && (c.front().isSpace() || c.back().isSpace()));
        if (!quote)
            return c;
        c.replace(QLatin1String("\""), QLatin1String("\"\""));
        return QLatin1Char('"') + c + QLatin1Char('"');
    };
    QString out;
    QStringList head;
    for (const QString& c : table.columns)
        head << encode(c);
    out += head.join(QLatin1Char(',')) + QStringLiteral("\r\n");
    for (const QStringList& r : table.rows) {
        QStringList cells;
        cells.reserve(r.size());
        for (const QString& c : r)
            cells << encode(c);
        out += cells.join(QLatin1Char(',')) + QStringLiteral("\r\n");
    }
    return out;
}

ArtefactTable table_from_csv(const QString& csv) {
    QVector<QStringList> rows;
    QStringList row;
    QString cell;
    bool in_quotes = false;
    bool row_started = false;
    const auto end_cell = [&]() {
        row << cell;
        cell.clear();
    };
    const auto end_row = [&]() {
        end_cell();
        bool any = false;
        for (const QString& c : std::as_const(row))
            any = any || !c.isEmpty();
        if (any || row.size() > 1)
            rows.append(row);
        row.clear();
        row_started = false;
    };
    for (int i = 0; i < csv.size(); ++i) {
        const QChar ch = csv.at(i);
        if (in_quotes) {
            if (ch == QLatin1Char('"')) {
                if (i + 1 < csv.size() && csv.at(i + 1) == QLatin1Char('"')) {
                    cell += QLatin1Char('"');
                    ++i;
                } else {
                    in_quotes = false;
                }
            } else {
                cell += ch;
            }
            continue;
        }
        if (ch == QLatin1Char('"') && cell.isEmpty()) {
            in_quotes = true;
            row_started = true;
        } else if (ch == QLatin1Char(',') || ch == QLatin1Char('\t')) {
            end_cell();
            row_started = true;
        } else if (ch == QLatin1Char('\n') || ch == QLatin1Char('\r')) {
            if (ch == QLatin1Char('\r') && i + 1 < csv.size() && csv.at(i + 1) == QLatin1Char('\n'))
                ++i;
            end_row();
        } else {
            cell += ch;
            row_started = true;
        }
        if (rows.size() > kMaxTableRows)
            break;
    }
    if (row_started || !cell.isEmpty() || !row.isEmpty())
        end_row();
    ArtefactTable t;
    if (rows.isEmpty())
        return t;
    t.columns = rows.takeFirst();
    t.rows = rows;
    al_normalize(t);
    return t;
}

QString table_to_markdown(const ArtefactTable& table, int max_rows) {
    if (table.columns.isEmpty())
        return {};
    const auto esc = [](QString c) {
        c.replace(QLatin1Char('|'), QStringLiteral("\\|"));
        c.replace(QLatin1Char('\n'), QLatin1Char(' '));
        return c;
    };
    QStringList lines;
    QStringList head;
    QStringList rule;
    for (const QString& c : table.columns) {
        head << esc(c);
        rule << QStringLiteral("---");
    }
    lines << QStringLiteral("| ") + head.join(QStringLiteral(" | ")) + QStringLiteral(" |");
    lines << QStringLiteral("| ") + rule.join(QStringLiteral(" | ")) + QStringLiteral(" |");
    const int n = static_cast<int>(table.rows.size());
    const int shown = max_rows > 0 ? std::min(n, max_rows) : n;
    for (int i = 0; i < shown; ++i) {
        QStringList cells;
        for (const QString& c : table.rows.at(i))
            cells << esc(c);
        lines << QStringLiteral("| ") + cells.join(QStringLiteral(" | ")) + QStringLiteral(" |");
    }
    if (shown < n)
        lines << QStringLiteral("\n") + al_tr("(%1 more rows in the CSV file)").arg(n - shown);
    return lines.join(QLatin1Char('\n'));
}

QJsonObject table_to_json(const ArtefactTable& table) {
    QJsonObject o;
    o[QStringLiteral("columns")] = QJsonArray::fromStringList(table.columns);
    QJsonArray rows;
    for (const QStringList& r : table.rows)
        rows.append(QJsonArray::fromStringList(r));
    o[QStringLiteral("rows")] = rows;
    return o;
}

ArtefactTable table_from_json(const QJsonValue& value) {
    ArtefactTable t;
    if (value.isObject()) {
        const QJsonObject o = value.toObject();
        if (o.value(QStringLiteral("columns")).isArray() && o.value(QStringLiteral("rows")).isArray()) {
            for (const QJsonValue& c : o.value(QStringLiteral("columns")).toArray())
                t.columns << al_value_text(c);
            for (const QJsonValue& r : o.value(QStringLiteral("rows")).toArray()) {
                if (t.rows.size() >= kMaxTableRows)
                    break;
                QStringList row;
                if (r.isArray()) {
                    for (const QJsonValue& c : r.toArray())
                        row << al_value_text(c);
                } else if (r.isObject()) {
                    const QJsonObject ro = r.toObject();
                    for (const QString& c : std::as_const(t.columns))
                        row << al_value_text(ro.value(c));
                }
                t.rows.append(row);
            }
            al_normalize(t);
            return t;
        }
        return t;
    }
    if (value.isArray()) {
        const QJsonArray arr = value.toArray();
        if (al_table_from_objects(arr, &t) || al_table_from_arrays(arr, &t))
            return t;
        // An array of scalars: one column.
        bool scalars = !arr.isEmpty();
        for (const QJsonValue& v : arr)
            scalars = scalars && al_is_scalar(v);
        if (scalars) {
            t.columns << QStringLiteral("value");
            for (const QJsonValue& v : arr) {
                if (t.rows.size() >= kMaxTableRows)
                    break;
                t.rows.append(QStringList{al_value_text(v)});
            }
        }
    }
    return t;
}

// ── Names ────────────────────────────────────────────────────────────────────

QString safe_file_stem(const QString& title, int max_chars) {
    QString out;
    out.reserve(title.size());
    bool dash = false;
    for (const QChar c : title.trimmed()) {
        if (c.isLetterOrNumber() && c.unicode() < 0x2000) {
            out += c;
            dash = false;
        } else if (c == QLatin1Char('_') || c == QLatin1Char('-') || c.isSpace() || c.isPunct() || c.isSymbol()) {
            if (!dash && !out.isEmpty()) {
                out += QLatin1Char('-');
                dash = true;
            }
        }
    }
    while (out.endsWith(QLatin1Char('-')))
        out.chop(1);
    if (out.size() > max_chars) {
        out = out.left(max_chars);
        while (out.endsWith(QLatin1Char('-')))
            out.chop(1);
    }
    return out.isEmpty() ? QStringLiteral("artefact") : out;
}

QString stored_name_for(const QString& artefact_id, const QString& title, const QString& ext) {
    QString id = artefact_id;
    id.remove(QLatin1Char('-'));
    const QString e = ext.trimmed().isEmpty() ? QStringLiteral("bin") : ext.trimmed();
    return id.left(8) + QLatin1Char('-') + safe_file_stem(title) + QLatin1Char('.') + e;
}

QString mime_for_kind(const QString& kind) {
    if (kind == QLatin1String(kArtefactReport))
        return QStringLiteral("text/markdown");
    if (kind == QLatin1String(kArtefactTable) || kind == QLatin1String(kArtefactSeries))
        return QStringLiteral("text/csv");
    if (kind == QLatin1String(kArtefactJson))
        return QStringLiteral("application/json");
    if (kind == QLatin1String(kArtefactText))
        return QStringLiteral("text/plain");
    if (kind == QLatin1String(kArtefactImage))
        return QStringLiteral("image/png");
    return QStringLiteral("application/octet-stream");
}

QString extension_for_kind(const QString& kind) {
    if (kind == QLatin1String(kArtefactReport))
        return QStringLiteral("md");
    if (kind == QLatin1String(kArtefactTable) || kind == QLatin1String(kArtefactSeries))
        return QStringLiteral("csv");
    if (kind == QLatin1String(kArtefactJson))
        return QStringLiteral("json");
    if (kind == QLatin1String(kArtefactText))
        return QStringLiteral("txt");
    if (kind == QLatin1String(kArtefactImage))
        return QStringLiteral("png");
    return QStringLiteral("bin");
}

// ── Tool results ─────────────────────────────────────────────────────────────

ToolResultView classify_tool_result(const QJsonValue& value_in, const QString& fallback_text, bool success) {
    ToolResultView out;
    QJsonValue value = value_in;
    // Terminal tools answer {success, data, error, message}: look inside.
    if (value.isObject()) {
        const QJsonObject o = value.toObject();
        if (o.contains(QStringLiteral("success")) && (o.contains(QStringLiteral("data")) || o.contains(QStringLiteral("error")))) {
            if (!o.value(QStringLiteral("success")).toBool(true)) {
                success = false;
                const QString err = o.value(QStringLiteral("error")).toString(o.value(QStringLiteral("message")).toString());
                if (!err.trimmed().isEmpty()) {
                    out.shape = ResultShape::Error;
                    out.text = err.trimmed();
                    out.value = value;
                    return out;
                }
            }
            value = o.value(QStringLiteral("data"));
        }
    }
    if ((value.isUndefined() || value.isNull()) && !fallback_text.trimmed().isEmpty()) {
        QJsonParseError pe{};
        const QByteArray raw = fallback_text.trimmed().toUtf8();
        const QJsonDocument doc = QJsonDocument::fromJson(raw, &pe);
        if (pe.error == QJsonParseError::NoError && !doc.isNull())
            value = doc.isArray() ? QJsonValue(doc.array()) : QJsonValue(doc.object());
    }
    if (!success) {
        out.shape = ResultShape::Error;
        QString err;
        if (value.isObject())
            err = value.toObject().value(QStringLiteral("error")).toString(
                value.toObject().value(QStringLiteral("message")).toString());
        if (err.trimmed().isEmpty())
            err = value.isString() ? value.toString() : fallback_text;
        out.text = err.trimmed().isEmpty() ? al_tr("The tool reported a failure.") : err.trimmed();
        out.value = value;
        return out;
    }
    out.value = value;
    if (value.isUndefined() || value.isNull()) {
        out.shape = fallback_text.trimmed().isEmpty() ? ResultShape::Empty : ResultShape::Text;
        out.text = fallback_text.trimmed();
        return out;
    }
    if (value.isString()) {
        out.shape = value.toString().trimmed().isEmpty() ? ResultShape::Empty : ResultShape::Text;
        out.text = value.toString();
        return out;
    }
    if (value.isDouble() || value.isBool()) {
        out.shape = ResultShape::Text;
        out.text = al_value_text(value);
        return out;
    }
    if (value.isArray()) {
        ArtefactTable t = table_from_json(value);
        if (!t.columns.isEmpty() && !t.rows.isEmpty()) {
            out.shape = ResultShape::Table;
            out.table = t;
            return out;
        }
        out.shape = value.toArray().isEmpty() ? ResultShape::Empty : ResultShape::Text;
        out.text = QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Indented));
        return out;
    }
    // An object: {columns, rows}, one array of records under a key, or fields.
    const QJsonObject o = value.toObject();
    ArtefactTable direct = table_from_json(value);
    if (!direct.isEmpty()) {
        out.shape = ResultShape::Table;
        out.table = direct;
        return out;
    }
    QString array_key;
    int arrays = 0;
    int scalars = 0;
    for (auto it = o.constBegin(); it != o.constEnd(); ++it) {
        if (it.value().isArray() && !it.value().toArray().isEmpty() && it.value().toArray().first().isObject()) {
            ++arrays;
            array_key = it.key();
        } else if (al_is_scalar(it.value())) {
            ++scalars;
        }
    }
    if (arrays == 1 && scalars <= 3) {
        ArtefactTable t;
        if (al_table_from_objects(o.value(array_key).toArray(), &t)) {
            out.shape = ResultShape::Table;
            out.table = t;
            out.text = array_key;
            return out;
        }
    }
    if (o.isEmpty()) {
        out.shape = ResultShape::Empty;
        return out;
    }
    out.shape = ResultShape::KeyValue;
    for (auto it = o.constBegin(); it != o.constEnd(); ++it)
        out.fields.append({it.key(), al_number_clip(al_value_text(it.value()), 400)});
    return out;
}

QString summarize_arguments(const QString& args_preview, int max_chars) {
    QString t = args_preview.simplified();
    if (t.startsWith(QLatin1Char('{')) && t.endsWith(QLatin1Char('}')) && QJsonDocument::fromJson(t.toUtf8()).isObject()) {
        // {"symbol": "AAPL", "period": "1y"} → symbol=AAPL, period=1y — rewritten on the
        // text, not the parsed object, so the arguments keep the order the agent gave.
        static const QRegularExpression kKey(QStringLiteral("\"([^\"\\\\]+)\"\\s*:\\s*"));
        t = t.mid(1, t.size() - 2).trimmed();
        t.replace(kKey, QStringLiteral("\\1="));
        t.remove(QLatin1Char('"'));
        t = t.simplified();
    }
    return al_number_clip(t, std::max(8, max_chars));
}

// ── Artefacts a run produces ─────────────────────────────────────────────────

bool run_produces_artefacts(const TaskRun& run) {
    if (run.plan_first || run.engine == QLatin1String(kEngineLocal))
        return true;
    return run.origin != QLatin1String(kOriginChat);
}

QString artefact_key(const RunArtefact& a) {
    return a.source.value(QStringLiteral("key")).toString();
}

QVector<ArtefactDraft> plan_run_artefacts(const TaskRun& run, const QVector<RunStep>& steps,
                                          const QSet<QString>& existing_keys, int only_step) {
    QVector<ArtefactDraft> out;
    if (!run_produces_artefacts(run))
        return out;
    const bool multi_step = steps.size() > 1;

    const auto add_tables = [&](const QString& text, const QString& key_prefix, int step_idx, const QString& owner) {
        const QVector<MarkdownTable> tables = find_markdown_tables(text);
        for (int i = 0; i < tables.size(); ++i) {
            const QString key = QStringLiteral("table:%1:%2").arg(key_prefix).arg(i + 1);
            if (existing_keys.contains(key))
                continue;
            ArtefactDraft d;
            d.key = key;
            d.kind = QString::fromLatin1(kArtefactTable);
            d.step_idx = step_idx;
            const QString caption = tables.at(i).caption;
            d.title = caption.isEmpty() ? al_tr("%1 — table %2").arg(owner).arg(i + 1)
                                        : al_tr("%1 — %2").arg(owner, caption);
            d.table = tables.at(i).table;
            d.source[QStringLiteral("origin")] = QStringLiteral("table");
            d.source[QStringLiteral("key")] = key;
            d.source[QStringLiteral("step")] = step_idx;
            d.source[QStringLiteral("source_title")] = owner;
            out.append(d);
        }
    };

    if (multi_step) {
        for (const RunStep& s : steps) {
            if (only_step >= 0 && s.idx != only_step)
                continue;
            if (s.status != StepStatus::Done || s.summary.trimmed().isEmpty())
                continue;
            const QString step_name = s.title.trimmed().isEmpty() ? al_tr("Step %1").arg(s.idx + 1)
                                                                  : al_tr("Step %1: %2").arg(s.idx + 1).arg(s.title.trimmed());
            const QString key = QStringLiteral("step:%1").arg(s.idx);
            if (!existing_keys.contains(key)) {
                ArtefactDraft d;
                d.key = key;
                d.kind = QString::fromLatin1(kArtefactReport);
                d.step_idx = s.idx;
                d.title = step_name.left(160);
                QString body = s.summary.trimmed();
                if (!body.startsWith(QLatin1Char('#')))
                    body = QStringLiteral("## ") + step_name + QStringLiteral("\n\n") + body;
                if (!s.deliverable.trimmed().isEmpty())
                    body += QStringLiteral("\n\n*") + al_tr("Deliverable: %1").arg(s.deliverable.trimmed()) +
                            QLatin1Char('*');
                d.text = body;
                d.source[QStringLiteral("origin")] = QStringLiteral("step");
                d.source[QStringLiteral("key")] = key;
                d.source[QStringLiteral("step")] = s.idx;
                if (!s.deliverable.isEmpty())
                    d.source[QStringLiteral("deliverable")] = s.deliverable;
                out.append(d);
            }
            add_tables(s.summary, QStringLiteral("step%1").arg(s.idx), s.idx, al_tr("Step %1").arg(s.idx + 1));
        }
    }
    if (only_step >= 0 || !is_terminal(run.status) || run.final_text.trimmed().isEmpty())
        return out;

    const bool completed = run.status == RunStatus::Completed;
    // A planned run that stopped early: its partial result is the steps' results
    // joined — each step already is an artefact of its own.
    if (!completed && multi_step)
        return out;
    if (!existing_keys.contains(QStringLiteral("final"))) {
        ArtefactDraft d;
        d.key = QStringLiteral("final");
        d.kind = QString::fromLatin1(kArtefactReport);
        d.step_idx = -1;
        d.title = (completed ? al_tr("Final report") : al_tr("Partial results")) +
                  (run.title.trimmed().isEmpty() ? QString() : QStringLiteral(" — ") + run.title.trimmed().left(100));
        QString body = run.final_text.trimmed();
        if (!body.startsWith(QLatin1Char('#')) && !run.title.trimmed().isEmpty())
            body = QStringLiteral("# ") + run.title.trimmed() + QStringLiteral("\n\n") + body;
        const QString when = QDateTime::fromMSecsSinceEpoch(run.finished_at_ms > 0 ? run.finished_at_ms
                                                                                   : QDateTime::currentMSecsSinceEpoch())
                                 .toString(QStringLiteral("yyyy-MM-dd HH:mm"));
        body += QStringLiteral("\n\n---\n*") +
                al_tr("Fincept Terminal agent run · %1 · %2").arg(run_status_label(run.status), when) + QLatin1Char('*');
        d.text = body;
        d.source[QStringLiteral("origin")] = QStringLiteral("final");
        d.source[QStringLiteral("key")] = d.key;
        d.source[QStringLiteral("status")] = run_status_id(run.status);
        out.append(d);
    }
    add_tables(run.final_text, QStringLiteral("final"), -1, completed ? al_tr("Final report") : al_tr("Partial results"));
    return out;
}

// ── Retention ────────────────────────────────────────────────────────────────

RetentionPlan plan_retention(const RetentionInput& in) {
    RetentionPlan plan;
    QSet<QString> doomed;
    qint64 kept_unpinned = 0;
    for (const RunArtefact& a : in.artefacts) {
        plan.bytes_before += std::max<qint64>(0, a.bytes);
        if (a.pinned)
            continue;
        if (!in.live_run_ids.contains(a.run_id)) {
            doomed.insert(a.id);
            continue;
        }
        kept_unpinned += std::max<qint64>(0, a.bytes);
    }
    if (in.quota_bytes > 0 && kept_unpinned > in.quota_bytes) {
        QVector<const RunArtefact*> candidates;
        for (const RunArtefact& a : in.artefacts) {
            if (a.pinned || doomed.contains(a.id) || in.active_run_ids.contains(a.run_id))
                continue;
            candidates.append(&a);
        }
        std::sort(candidates.begin(), candidates.end(), [](const RunArtefact* x, const RunArtefact* y) {
            if (x->created_at_ms != y->created_at_ms)
                return x->created_at_ms < y->created_at_ms;
            return x->id < y->id;
        });
        const qint64 target = static_cast<qint64>(static_cast<double>(in.quota_bytes) * kQuotaTargetFraction);
        for (const RunArtefact* a : std::as_const(candidates)) {
            if (kept_unpinned <= target)
                break;
            doomed.insert(a->id);
            kept_unpinned -= std::max<qint64>(0, a->bytes);
        }
    }
    for (const RunArtefact& a : in.artefacts) {
        if (doomed.contains(a.id))
            plan.delete_ids << a.id;
        else
            plan.bytes_after += std::max<qint64>(0, a.bytes);
    }
    return plan;
}

QStringList unreferenced_files(const QStringList& files, const QSet<QString>& referenced_names) {
    QStringList out;
    for (const QString& f : files)
        if (!referenced_names.contains(f))
            out << f;
    return out;
}

} // namespace fincept::agent_tasks::artefact_logic

// ── Report hand-off: an artefact as a report payload (ReportHandoff.h) ───────

namespace fincept::agent_tasks {

namespace artefact_report_detail {

/// Longest series drawn as a chart; a longer one goes into the report as its table only.
constexpr int kArdMaxChartPoints = 2000;

QString ard_tr(const char* text) {
    return QCoreApplication::translate("fincept::agent_tasks::Artefacts", text);
}

/// A cell as a number for a chart ("1,234.5", "12%", "$3.20"); false when it is not one.
bool ard_number(const QString& text, double* out) {
    QString t = text.trimmed();
    if (t.isEmpty() || !artefact_logic::looks_numeric(t))
        return false;
    t.remove(QLatin1Char(','));
    for (const QChar c : {QChar(u'$'), QChar(u'€'), QChar(u'£'), QChar(u'¥'), QChar(u'₹'), QChar(u'%')})
        t.remove(c);
    bool ok = false;
    const double v = QLocale::c().toDouble(t.trimmed(), &ok);
    if (ok)
        *out = v;
    return ok;
}

/// The table part of a payload, cut to kReportMaxTableRows with a visible note.
report::PayloadTable ard_table(const artefact_logic::ArtefactTable& t) {
    report::PayloadTable pt;
    pt.columns = t.columns;
    const int total = static_cast<int>(t.rows.size());
    const int shown = std::min(total, kReportMaxTableRows);
    pt.rows = t.rows.mid(0, shown);
    if (shown < total)
        pt.note = ard_tr("Showing the first %1 of %2 rows; all of them are in the artefact's file.").arg(shown).arg(total);
    return pt;
}

/// A line chart of the numeric columns of a series table (column 0 = the x labels).
bool ard_chart(const artefact_logic::ArtefactTable& t, const QString& title, report::PayloadChart* out) {
    if (t.columns.size() < 2 || t.rows.size() < 2 || t.rows.size() > kArdMaxChartPoints)
        return false;
    report::PayloadChart chart;
    chart.title = title;
    chart.kind = QStringLiteral("line");
    QStringList x;
    x.reserve(t.rows.size());
    for (const QStringList& row : t.rows)
        x << row.value(0);
    for (int col = 1; col < t.columns.size() && chart.series.size() < kReportMaxChartSeries; ++col) {
        report::PayloadSeries s;
        s.name = t.columns.at(col);
        s.x = x;
        s.y.reserve(t.rows.size());
        bool numeric = true;
        for (const QStringList& row : t.rows) {
            double v = 0;
            if (!ard_number(row.value(col), &v)) {
                numeric = false;
                break;
            }
            s.y.append(v);
        }
        if (numeric)
            chart.series.append(s);
    }
    if (chart.series.isEmpty())
        return false;
    *out = chart;
    return true;
}

} // namespace artefact_report_detail

QString artefact_provenance(const RunArtefact& a) {
    using namespace artefact_report_detail;
    QStringList parts;
    const QString run = a.run_title.trimmed().isEmpty() ? a.run_id.left(8) : a.run_title.trimmed();
    parts << ard_tr("From agent run “%1”").arg(run);
    if (a.step_idx >= 0)
        parts << ard_tr("step %1").arg(a.step_idx + 1);
    const QString tool = a.source.value(QStringLiteral("tool")).toString();
    if (!tool.isEmpty())
        parts << ard_tr("tool %1").arg(tool);
    return parts.join(QStringLiteral(" · "));
}

report::ReportPayload artefact_report_payload(const RunArtefact& a, const ArtefactReportContent& c) {
    using namespace artefact_report_detail;
    report::ReportPayload p;
    const bool tabular_kind = a.kind == QLatin1String(kArtefactTable) || a.kind == QLatin1String(kArtefactSeries);
    const bool use_table = !c.table.isEmpty() && (tabular_kind || a.kind == QLatin1String(kArtefactJson) ||
                                                  c.text.trimmed().isEmpty());
    if (a.kind == QLatin1String(kArtefactImage)) {
        if (c.image.isEmpty())
            return p;
        report::PayloadChart img;
        img.title = a.title;
        img.image = c.image;
        img.image_format = c.image_format.isEmpty() ? QStringLiteral("png") : c.image_format;
        p.charts.append(img);
    } else if (use_table) {
        report::PayloadChart chart;
        if (a.kind == QLatin1String(kArtefactSeries) && ard_chart(c.table, a.title, &chart))
            p.charts.append(chart);
        p.tables.append(ard_table(c.table));
    } else if (!c.text.trimmed().isEmpty()) {
        p.markdown = a.kind == QLatin1String(kArtefactJson)
                         ? QStringLiteral("```json\n") + c.text.trimmed() + QStringLiteral("\n```")
                         : c.text;
    } else {
        return p; // nothing to add
    }
    p.title = a.title;
    p.source = QString::fromLatin1(kReportSourceLabel);
    if (a.created_at_ms > 0)
        p.as_of = QDateTime::fromMSecsSinceEpoch(a.created_at_ms);
    p.notes = artefact_provenance(a);
    return p;
}

} // namespace fincept::agent_tasks
