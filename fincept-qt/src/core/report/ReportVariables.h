#pragma once
// ReportVariables.h — {{variables}} and the heading outline of a Report Builder report.
//
// Variables are report-level: the built-ins {{title}}, {{author}}, {{company}}, {{date}}
// are the document info fields; every other name (by default {{symbol}} and {{period}})
// lives in ReportMetadata::variables. Block text references them as {{name}} (case and
// inner spaces ignored); they are expanded when the report is drawn or exported — in
// headings, text, quotes, callouts, lists, table cells, chart titles and captions, the
// cover page, the running header / footer, and inside data recipes (a block bound to
// {{symbol}} follows the variable). A reference with no value is UNRESOLVED: the page view
// marks it, exports warn before they print it.
//
// Outline: the headings in order with their level and (when the report numbers its
// headings) "1", "1.1", "1.1.1" — used by the page view, the TOC, the exports, the MCP
// report tool and the AI drafting context.
//
// Pure, Qt Core only, header-only.

#include "core/report/ReportBinding.h"
#include "core/report/ReportDocument.h"
#include "core/report/ReportTableCodec.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QRegularExpression>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

#include <algorithm>

namespace fincept::report::vars {

/// Names the built-in fields answer to.
inline const QStringList& builtin_names() {
    static const QStringList k = {QStringLiteral("title"), QStringLiteral("author"), QStringLiteral("company"),
                                  QStringLiteral("date")};
    return k;
}

/// Variables a new report offers (empty until the user sets them).
inline const QStringList& suggested_names() {
    static const QStringList k = {QStringLiteral("symbol"), QStringLiteral("period")};
    return k;
}

/// A valid variable name: a letter, then letters / digits / '_' (at most 32), lower case.
inline bool is_valid_name(const QString& name) {
    static const QRegularExpression re(QStringLiteral("^[a-z][a-z0-9_]{0,31}$"));
    return re.match(name).hasMatch();
}

inline QString normalise(const QString& name) {
    QString n = name.trimmed().toLower();
    n.replace(QLatin1Char(' '), QLatin1Char('_'));
    return n;
}

inline const QRegularExpression& reference_re() {
    static const QRegularExpression re(QStringLiteral("\\{\\{\\s*([A-Za-z][A-Za-z0-9_ ]{0,40}?)\\s*\\}\\}"));
    return re;
}

/// One pass of substitution over `text` with `ctx` (unresolved / empty kept as typed).
inline QString substitute_once(const QString& text, const QMap<QString, QString>& ctx) {
    if (!text.contains(QLatin1String("{{")))
        return text;
    QString out;
    int last = 0;
    auto it = reference_re().globalMatch(text);
    while (it.hasNext()) {
        const auto m = it.next();
        out += text.mid(last, m.capturedStart() - last);
        const QString v = ctx.value(normalise(m.captured(1)));
        out += v.trimmed().isEmpty() ? m.captured(0) : v;
        last = m.capturedEnd();
    }
    out += text.mid(last);
    return out;
}

/// name -> value for resolving: the custom variables, then the built-ins (which win). A
/// built-in may itself use custom variables (a title "{{symbol}} — Earnings preview").
inline QMap<QString, QString> context(const ReportMetadata& m) {
    QMap<QString, QString> ctx = m.variables;
    const QMap<QString, QString> custom = m.variables;
    ctx.insert(QStringLiteral("title"), substitute_once(m.title, custom));
    ctx.insert(QStringLiteral("author"), substitute_once(m.author, custom));
    ctx.insert(QStringLiteral("company"), substitute_once(m.company, custom));
    ctx.insert(QStringLiteral("date"), m.date);
    return ctx;
}

/// The report's title as printed and listed ("{{symbol}} — Earnings preview" -> "AAPL — …").
inline QString display_title(const ReportMetadata& m) {
    const QString t = substitute_once(m.title, m.variables).trimmed();
    return t.isEmpty() ? QStringLiteral("Untitled Report") : t;
}

/// Every {{name}} in `text` (normalised, in order, without repeats).
inline QStringList references(const QString& text) {
    QStringList out;
    if (!text.contains(QLatin1String("{{")))
        return out;
    auto it = reference_re().globalMatch(text);
    while (it.hasNext()) {
        const QString n = normalise(it.next().captured(1));
        if (!out.contains(n))
            out << n;
    }
    return out;
}

/// One run of text after resolving: plain text, a substituted value, or an unresolved
/// reference (kept as "{{name}}").
struct Segment {
    enum class Kind { Plain, Value, Unresolved };
    Kind kind = Kind::Plain;
    QString text;
    QString name; ///< Value / Unresolved: the variable
};

/// `text` cut into plain / value / unresolved runs. A variable with an empty value is
/// unresolved (printing nothing silently is the failure this guards against).
inline QVector<Segment> segments(const QString& text, const QMap<QString, QString>& ctx) {
    QVector<Segment> out;
    if (!text.contains(QLatin1String("{{"))) {
        if (!text.isEmpty())
            out.append({Segment::Kind::Plain, text, {}});
        return out;
    }
    int last = 0;
    auto it = reference_re().globalMatch(text);
    while (it.hasNext()) {
        const auto m = it.next();
        if (m.capturedStart() > last)
            out.append({Segment::Kind::Plain, text.mid(last, m.capturedStart() - last), {}});
        const QString name = normalise(m.captured(1));
        const QString value = ctx.value(name);
        if (value.trimmed().isEmpty())
            out.append({Segment::Kind::Unresolved, m.captured(0), name});
        else
            out.append({Segment::Kind::Value, value, name});
        last = m.capturedEnd();
    }
    if (last < text.size())
        out.append({Segment::Kind::Plain, text.mid(last), {}});
    return out;
}

struct Resolved {
    QString text;           ///< values substituted; unresolved references kept as typed
    QStringList unresolved; ///< names without a value
};

inline Resolved resolve(const QString& text, const QMap<QString, QString>& ctx) {
    Resolved r;
    if (!text.contains(QLatin1String("{{"))) {
        r.text = text;
        return r;
    }
    for (const Segment& s : segments(text, ctx)) {
        r.text += s.text;
        if (s.kind == Segment::Kind::Unresolved && !r.unresolved.contains(s.name))
            r.unresolved << s.name;
    }
    return r;
}

inline QString expand(const QString& text, const QMap<QString, QString>& ctx) {
    return resolve(text, ctx).text;
}

/// The texts of a component that may hold references (content and the config keys the
/// renderer prints). Keys: "@content" for the content.
inline QStringList text_keys(const QString& type) {
    if (type == QLatin1String("chart") || type == QLatin1String("image"))
        return {QStringLiteral("title"), QStringLiteral("caption")};
    if (type == QLatin1String("table"))
        return {QStringLiteral("@content"), QStringLiteral("title"), QStringLiteral("csv"), QStringLiteral("note")};
    if (type == QLatin1String("stats_block"))
        return {QStringLiteral("title"), QStringLiteral("data")};
    if (type == QLatin1String("callout"))
        return {QStringLiteral("@content"), QStringLiteral("heading")};
    if (type == QLatin1String("cover"))
        return {QStringLiteral("title"), QStringLiteral("subtitle"), QStringLiteral("byline"), QStringLiteral("date"),
                QStringLiteral("disclaimer")};
    if (type == QLatin1String("sparkline"))
        return {QStringLiteral("title")};
    return {QStringLiteral("@content")};
}

/// A copy of `c` with every reference in its printed texts expanded (unresolved ones kept).
inline ReportComponent expanded_component(const ReportComponent& c, const QMap<QString, QString>& ctx) {
    ReportComponent out = c;
    for (const QString& key : text_keys(c.type)) {
        if (key == QLatin1String("@content")) {
            out.content = expand(c.content, ctx);
        } else if (key == QLatin1String("csv")) {
            // Cell by cell: a value holding ',' or '|' must not split the cell.
            const QString raw = c.config.value(key);
            if (!raw.contains(QLatin1String("{{")))
                continue;
            QVector<QStringList> grid = table::parse(raw);
            for (QStringList& row : grid)
                for (QString& cell : row)
                    cell = expand(cell, ctx);
            out.config.insert(key, table::encode(grid));
        } else if (c.config.contains(key)) {
            out.config.insert(key, expand(c.config.value(key), ctx));
        }
    }
    return out;
}

/// Unresolved names used anywhere in the document (blocks, header / footer, watermark).
inline QStringList unresolved_in(const ReportDocument& doc) {
    const QMap<QString, QString> ctx = context(doc.metadata);
    QStringList out;
    auto add_from = [&](const QString& text) {
        for (const QString& n : references(text))
            if (ctx.value(n).trimmed().isEmpty() && !out.contains(n))
                out << n;
    };
    const ReportMetadata& m = doc.metadata;
    for (const QString& t : {m.header_left, m.header_center, m.header_right, m.footer_left, m.footer_center,
                             m.footer_right, m.watermark, m.confidentiality})
        add_from(t);
    for (const ReportComponent& c : doc.components) {
        for (const QString& key : text_keys(c.type))
            add_from(key == QLatin1String("@content") ? c.content : c.config.value(key));
        const Binding b = binding_of(c);
        if (b.is_valid())
            add_from(b.to_string());
    }
    return out;
}

/// Every name the document references (resolved or not), in first-use order.
inline QStringList used_in(const ReportDocument& doc) {
    QStringList out;
    auto add_from = [&out](const QString& text) {
        for (const QString& n : references(text))
            if (!out.contains(n))
                out << n;
    };
    for (const ReportComponent& c : doc.components) {
        for (const QString& key : text_keys(c.type))
            add_from(key == QLatin1String("@content") ? c.content : c.config.value(key));
        add_from(c.config.value(QLatin1String(keys::kBind)));
    }
    return out;
}

/// A recipe with {{references}} in its string parameters expanded (symbols follow
/// {{symbol}}). Unresolved references stay — the binder then reports "No symbol set".
inline Binding expand_binding(const Binding& b, const QMap<QString, QString>& ctx) {
    if (!b.to_string().contains(QLatin1String("{{")))
        return b;
    Binding out = b;
    for (auto it = out.params.begin(); it != out.params.end(); ++it) {
        if (it.value().isString()) {
            QString v = expand(it.value().toString(), ctx);
            if (v.contains(QLatin1String("{{")))
                v.clear();
            it.value() = v;
        } else if (it.value().isArray()) {
            QJsonArray arr;
            for (const QJsonValue& x : it.value().toArray()) {
                if (!x.isString()) {
                    arr.append(x);
                    continue;
                }
                const QString v = expand(x.toString(), ctx);
                if (!v.contains(QLatin1String("{{")) && !v.trimmed().isEmpty())
                    arr.append(v);
            }
            it.value() = arr;
        }
    }
    return out;
}

} // namespace fincept::report::vars

namespace fincept::report::outline {

struct Heading {
    int id = 0;
    int level = 1;   ///< 1..3
    QString text;    ///< variables expanded
    QString number;  ///< "2.1" when the report numbers its headings, else empty
};

/// Headings in order. Numbering restarts the lower levels at every higher heading; a
/// level-3 heading under a level-1 one is "1.0.1"-free: it counts as "1.1".
inline QVector<Heading> headings(const ReportDocument& doc) {
    QVector<Heading> out;
    const QMap<QString, QString> ctx = vars::context(doc.metadata);
    int n[3] = {0, 0, 0};
    for (const ReportComponent& c : doc.components) {
        // An empty heading prints nowhere (exports leave it out): it takes no number and
        // no contents line, so the page, the PDF, the .odt and Markdown number alike.
        if (c.type != QLatin1String("heading") || c.content.trimmed().isEmpty())
            continue;
        Heading h;
        h.id = c.id;
        h.level = std::clamp(c.config.value(QStringLiteral("level"), QStringLiteral("1")).toInt(), 1, 3);
        h.text = vars::expand(c.content.isEmpty() ? QStringLiteral("Heading") : c.content, ctx);
        if (doc.metadata.number_headings) {
            // Effective depth: never deeper than one below the previous numbered level.
            int depth = h.level - 1;
            if (depth >= 1 && n[0] == 0)
                depth = 0;
            if (depth >= 2 && n[1] == 0)
                depth = 1;
            ++n[depth];
            for (int k = depth + 1; k < 3; ++k)
                n[k] = 0;
            QStringList parts;
            for (int k = 0; k <= depth; ++k)
                parts << QString::number(n[k]);
            h.number = parts.join(QLatin1Char('.'));
        }
        out.append(h);
    }
    return out;
}

/// heading id -> number ("" when not numbered).
inline QHash<int, QString> heading_numbers(const ReportDocument& doc) {
    QHash<int, QString> out;
    if (!doc.metadata.number_headings)
        return out;
    for (const Heading& h : headings(doc))
        out.insert(h.id, h.number);
    return out;
}

/// "2.1  Revenue" — the text a heading prints with.
inline QString heading_label(const Heading& h) {
    return h.number.isEmpty() ? h.text : h.number + QStringLiteral("  ") + h.text;
}

} // namespace fincept::report::outline
