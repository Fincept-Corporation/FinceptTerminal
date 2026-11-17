// ReportMarkdownWriter.cpp — see ReportMarkdownWriter.h.

#include "core/report/ReportMarkdownWriter.h"

#include "core/report/ReportBinding.h"
#include "core/report/ReportTableFormat.h"
#include "core/report/ReportVariables.h"

#include <QHash>
#include <QMap>
#include <QRegularExpression>
#include <QStringList>

#include <algorithm>

namespace fincept::report::md {

namespace {

namespace ef = ::fincept::report::exportfmt;
namespace tf = ::fincept::report::tablefmt;

// ── Escaping ────────────────────────────────────────────────────────────────

/// Printed text inside a line: Markdown punctuation backslash-escaped.
QString rmd_escape(const QString& s) {
    static const QString kSpecial = QStringLiteral("\\`*_[]<>|");
    QString out;
    out.reserve(s.size() + 8);
    for (QChar ch : s) {
        if (ch == QLatin1Char('\r'))
            continue;
        if (kSpecial.contains(ch))
            out += QLatin1Char('\\');
        out += ch;
    }
    return out;
}

/// A printed paragraph: escaped, and a line that would start a heading, list, quote or
/// setext underline is protected.
QString rmd_text(const QString& s) {
    static const QRegularExpression ordered_re(QStringLiteral("^(\\d{1,9})([.)])"));
    QStringList lines;
    for (const QString& raw : s.split(QLatin1Char('\n'))) {
        QString line = rmd_escape(raw.trimmed());
        if (line.startsWith(QLatin1Char('#')) || line.startsWith(QLatin1Char('-')) || line.startsWith(QLatin1Char('+')) ||
            line.startsWith(QLatin1Char('=')) || line.startsWith(QLatin1Char('~')))
            line.prepend(QLatin1Char('\\'));
        const QRegularExpressionMatch m = ordered_re.match(line);
        if (m.hasMatch())
            line.insert(m.capturedStart(2), QLatin1Char('\\'));
        lines << line;
    }
    return lines.join(QLatin1Char('\n'));
}

/// *italic* printed text (each line on its own, joined by hard breaks).
QString rmd_italic(const QString& s) {
    QStringList lines;
    for (const QString& raw : s.split(QLatin1Char('\n'))) {
        const QString t = raw.trimmed();
        if (!t.isEmpty())
            lines << QLatin1Char('*') + rmd_escape(t) + QLatin1Char('*');
    }
    return lines.join(QStringLiteral("\\\n"));
}

QString rmd_cell(const QString& s) {
    QString t = rmd_escape(s.trimmed()); // '|' -> "\|"
    t.replace(QLatin1Char('\n'), QStringLiteral("<br>"));
    return t;
}

/// A YAML double-quoted scalar.
QString rmd_yaml(const QString& s) {
    QString out(QLatin1Char('"'));
    for (QChar ch : s) {
        const char16_t u = ch.unicode();
        if (u == u'\\')
            out += QStringLiteral("\\\\");
        else if (u == u'"')
            out += QLatin1String("\\\"");
        else if (u == u'\n')
            out += QStringLiteral("\\n");
        else if (u == u'\r')
            out += QStringLiteral("\\r");
        else if (u == u'\t')
            out += QStringLiteral("\\t");
        else if (u < 0x20 || u == 0x7F)
            out += QStringLiteral("\\u%1").arg(static_cast<int>(u), 4, 16, QLatin1Char('0'));
        else
            out += ch;
    }
    out += QLatin1Char('"');
    return out;
}

/// Verbatim Markdown of a block: LF line endings, no leading / trailing blank lines.
QString rmd_verbatim(QString s) {
    s.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    s.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    while (s.endsWith(QLatin1Char('\n')) || s.endsWith(QLatin1Char(' ')) || s.endsWith(QLatin1Char('\t')))
        s.chop(1);
    while (s.startsWith(QLatin1Char('\n')))
        s.remove(0, 1);
    return s;
}

/// A link target: relative path with spaces / parentheses percent-encoded.
QString rmd_target(QString path) {
    path.replace(QLatin1Char('%'), QStringLiteral("%25"));
    path.replace(QLatin1Char(' '), QStringLiteral("%20"));
    path.replace(QLatin1Char('('), QStringLiteral("%28"));
    path.replace(QLatin1Char(')'), QStringLiteral("%29"));
    return path;
}

// ── GitHub heading anchors (github-slugger) ─────────────────────────────────

class RmdSlugger {
  public:
    QString slug(const QString& text) {
        QString base;
        for (QChar ch : text.toLower()) {
            if (ch.isLetterOrNumber() || ch.isMark() || ch == QLatin1Char('-') || ch == QLatin1Char(' ') ||
                ch.category() == QChar::Punctuation_Connector)
                base += ch;
        }
        base.replace(QLatin1Char(' '), QLatin1Char('-'));
        QString s = base;
        while (seen_.contains(s))
            s = base + QLatin1Char('-') + QString::number(++seen_[base]);
        seen_.insert(s, 0);
        return s;
    }

  private:
    QHash<QString, int> seen_;
};

/// The text of every ATX heading in verbatim Markdown (outside code fences).
QStringList rmd_atx_headings(const QString& body) {
    static const QRegularExpression re(QStringLiteral("^ {0,3}#{1,6}(?:[ \\t]+(.*?))?[ \\t]*#*[ \\t]*$"));
    QStringList out;
    QString fence;
    for (const QString& line : body.split(QLatin1Char('\n'))) {
        const QString t = line.trimmed();
        if (!fence.isEmpty()) {
            if (t.startsWith(fence))
                fence.clear();
            continue;
        }
        if (t.startsWith(QLatin1String("```")) || t.startsWith(QLatin1String("~~~"))) {
            fence = t.left(3);
            continue;
        }
        const QRegularExpressionMatch m = re.match(line);
        if (m.hasMatch())
            out << m.captured(1);
    }
    return out;
}

// ── Writer ──────────────────────────────────────────────────────────────────

struct RmdCtx {
    const ReportDocument* doc = nullptr;
    const ef::ExportAssets* assets = nullptr;
    const MarkdownOptions* opt = nullptr;
    QMap<QString, QString> vars;
    QVector<outline::Heading> headings; ///< printed headings
    QHash<int, int> heading_index;      ///< heading id -> index in `headings`
    QHash<int, QString> slugs;          ///< heading id -> anchor
    QString assets_dir;
    QVector<QPair<QString, QByteArray>> files;
};

QString rmd_label(const outline::Heading& h) {
    QString label = h.number.isEmpty() ? h.text : h.number + QLatin1Char(' ') + h.text;
    label.replace(QLatin1Char('\n'), QLatin1Char(' '));
    return label.trimmed();
}

outline::Heading rmd_heading_of(const RmdCtx& x, const ReportComponent& c) {
    const int idx = x.heading_index.value(c.id, -1);
    if (idx >= 0)
        return x.headings[idx];
    outline::Heading h;
    h.id = c.id;
    h.level = std::clamp(c.config.value(QStringLiteral("level"), QStringLiteral("1")).toInt(), 1, 3);
    h.text = c.content;
    return h;
}

QString rmd_image(RmdCtx& x, const QString& kind, int id, const ef::ImageAsset& a, const QString& alt) {
    const QString name = QStringLiteral("%1_%2.%3").arg(kind).arg(id).arg(ef::image_ext(a));
    QString target;
    if (x.opt->images == MarkdownOptions::Images::Inline) {
        target = QStringLiteral("data:") + ef::image_mime(a) + QStringLiteral(";base64,") +
                 QString::fromLatin1(a.bytes.toBase64());
    } else {
        const QString path = x.assets_dir.isEmpty() ? name : x.assets_dir + QLatin1Char('/') + name;
        bool known = false;
        for (const auto& f : x.files)
            known = known || f.first == path;
        if (!known)
            x.files.append({path, a.bytes});
        target = rmd_target(path);
    }
    return QStringLiteral("![") + rmd_escape(alt.simplified()) + QStringLiteral("](") + target + QLatin1Char(')');
}

QString rmd_row(const QStringList& cells) {
    return QStringLiteral("| ") + cells.join(QStringLiteral(" | ")) + QStringLiteral(" |");
}

QString rmd_table(const tf::FormattedTable& t) {
    const int cols = std::max(1, t.columns);
    const int rows = static_cast<int>(t.rows.size());
    if (rows == 0)
        return {};
    auto cells_of = [&](int r) {
        QStringList out;
        const bool total = r == t.totals_row;
        for (int c = 0; c < cols; ++c) {
            const tf::Cell cell = c < t.rows[r].size() ? t.rows[r][c] : tf::Cell();
            QString s = rmd_cell(cell.text);
            if ((total || cell.total) && !s.isEmpty())
                s = QStringLiteral("**") + s + QStringLiteral("**");
            out << s;
        }
        return out;
    };
    QStringList lines;
    lines << rmd_row(cells_of(0));
    // Alignment of a column: the most common one of its body cells (header's when none).
    const int first_body = rows > 1 ? 1 : 0;
    QStringList align_row;
    for (int c = 0; c < cols; ++c) {
        QHash<QString, int> votes;
        QString first;
        for (int r = first_body; r < rows; ++r) {
            if (r == t.totals_row || c >= t.rows[r].size())
                continue;
            const QString a = t.rows[r][c].align;
            if (first.isEmpty())
                first = a;
            ++votes[a];
        }
        QString best = first.isEmpty() && c < t.rows[0].size() ? t.rows[0][c].align : first;
        for (const QString& a : {QStringLiteral("left"), QStringLiteral("right"), QStringLiteral("center")})
            if (votes.value(a) > votes.value(best))
                best = a;
        align_row << (best == QLatin1String("right")    ? QStringLiteral("---:")
                      : best == QLatin1String("center") ? QStringLiteral(":---:")
                                                        : QStringLiteral(":---"));
    }
    lines << rmd_row(align_row);
    for (int r = 1; r < rows; ++r)
        lines << rmd_row(cells_of(r));
    return lines.join(QLatin1Char('\n'));
}

QString rmd_table_block(RmdCtx& x, const ReportComponent& c) {
    QStringList parts;
    const QString title = c.config.value(QStringLiteral("title")).trimmed();
    if (!title.isEmpty())
        parts << QStringLiteral("**") + rmd_escape(title) + QStringLiteral("**");
    const ef::PrintedTable pt = ef::printed_table(c, *x.assets, x.doc->theme.page_bg);
    const QString grid = rmd_table(pt.table);
    if (!grid.isEmpty())
        parts << grid;
    QStringList notes;
    const QString note = c.config.value(QStringLiteral("note")).trimmed();
    if (!note.isEmpty())
        notes << note;
    if (!pt.truncated.isEmpty())
        notes << pt.truncated;
    if (!notes.isEmpty())
        parts << rmd_italic(notes.join(QLatin1Char(' ')));
    return parts.join(QStringLiteral("\n\n"));
}

QString rmd_stats(const ReportComponent& c) {
    QStringList lines;
    lines << QStringLiteral("**") + rmd_escape(c.config.value(QStringLiteral("title"), QStringLiteral("Key Statistics"))) +
                 QStringLiteral("**");
    lines << QString();
    lines << QStringLiteral("| Metric | Value |") << QStringLiteral("| :--- | ---: |");
    for (const QString& line : c.config.value(QStringLiteral("data")).split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        const qsizetype colon = line.indexOf(QLatin1Char(':'));
        const QString label = colon >= 0 ? line.left(colon) : line;
        const QString value = colon >= 0 ? line.mid(colon + 1) : QString();
        lines << rmd_row({rmd_cell(label), rmd_cell(value)});
    }
    return lines.join(QLatin1Char('\n'));
}

QString rmd_market(const ReportComponent& c) {
    const auto cfg = [&c](const char* key) { return c.config.value(QString::fromLatin1(key)); };
    const QString symbol = cfg("symbol");
    const QString name = c.config.value(QStringLiteral("name"), symbol);
    const QString price = cfg("price");
    const QString ccy = cfg("currency");
    const QString chg = cfg("change_pct");
    QString line = QStringLiteral("**") + rmd_escape(symbol) + QStringLiteral("**");
    if (!name.isEmpty() && name != symbol)
        line += QLatin1Char(' ') + rmd_escape(name);
    line += QStringLiteral(" **") + rmd_escape(price + (ccy.isEmpty() ? QString() : QLatin1Char(' ') + ccy)) +
            QStringLiteral("**");
    if (!chg.isEmpty()) {
        const bool negative = chg.startsWith(QLatin1Char('-'));
        const QString sign = (!negative && !chg.startsWith(QLatin1Char('+'))) ? QStringLiteral("+") : QString();
        line += QStringLiteral(" **") + rmd_escape(sign + chg + QLatin1Char('%')) + QStringLiteral("**");
    }
    const QString high = cfg("high"), low = cfg("low"), volume = cfg("volume");
    if (!high.isEmpty() && !low.isEmpty())
        line += QStringLiteral(" · H: ") + rmd_escape(high) + QStringLiteral(" · L: ") + rmd_escape(low);
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
        line += QStringLiteral(" · Vol: ") + rmd_escape(vs);
    }
    return line;
}

QString rmd_quoted(const QString& body) {
    QStringList lines;
    for (const QString& line : rmd_verbatim(body).split(QLatin1Char('\n')))
        lines << (line.trimmed().isEmpty() ? QStringLiteral(">") : QStringLiteral("> ") + line);
    return lines.join(QLatin1Char('\n'));
}

QString rmd_callout(const ReportComponent& c) {
    const QString heading = c.config.value(QStringLiteral("heading")).trimmed();
    const QString body = rmd_verbatim(c.content);
    QStringList parts;
    if (!heading.isEmpty())
        parts << QStringLiteral("> **") + rmd_escape(heading) + QStringLiteral("**");
    if (!body.trimmed().isEmpty())
        parts << rmd_quoted(body);
    return parts.join(QStringLiteral("\n>\n"));
}

QString rmd_code(const ReportComponent& c) {
    const QString body = rmd_verbatim(c.content);
    int longest = 0, run = 0;
    for (QChar ch : body) {
        run = ch == QLatin1Char('`') ? run + 1 : 0;
        longest = std::max(longest, run);
    }
    const QString fence(std::max(3, longest + 1), QLatin1Char('`'));
    QString lang;
    for (QChar ch : c.config.value(QStringLiteral("language")).trimmed())
        if (ch.isLetterOrNumber() || ch == QLatin1Char('+') || ch == QLatin1Char('-') || ch == QLatin1Char('_') ||
            ch == QLatin1Char('#'))
            lang += ch;
    return fence + lang + QLatin1Char('\n') + body + QLatin1Char('\n') + fence;
}

QString rmd_list(const ReportComponent& c) {
    QStringList lines;
    for (QString item : c.content.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        item.remove(QLatin1Char('\r'));
        if (!item.trimmed().isEmpty())
            lines << QStringLiteral("- ") + item.trimmed();
    }
    return lines.join(QLatin1Char('\n'));
}

QString rmd_toc(const RmdCtx& x) {
    QStringList lines;
    lines << QStringLiteral("**Table of Contents**") << QString();
    if (x.headings.isEmpty()) {
        lines << QStringLiteral("*(No headings found in document)*");
        return lines.join(QLatin1Char('\n'));
    }
    for (const outline::Heading& h : x.headings)
        lines << QString(2 * (h.level - 1), QLatin1Char(' ')) + QStringLiteral("- [") + rmd_escape(rmd_label(h)) +
                     QStringLiteral("](#") + x.slugs.value(h.id) + QLatin1Char(')');
    return lines.join(QLatin1Char('\n'));
}

QString rmd_cover_title(const RmdCtx& x, const ReportComponent& c) {
    const QString t = c.config.value(QStringLiteral("title")).trimmed();
    return t.isEmpty() ? vars::expand(x.doc->metadata.title, x.vars).trimmed() : t;
}

QString rmd_cover(RmdCtx& x, const ReportComponent& c) {
    const ReportMetadata& m = x.doc->metadata;
    const auto cfg = [&c](const char* key) { return c.config.value(QString::fromLatin1(key)).trimmed(); };
    QStringList parts;
    const auto logo = x.assets->images.constFind(c.id);
    if (logo != x.assets->images.cend() && !logo->bytes.isEmpty())
        parts << rmd_image(x, QStringLiteral("image"), c.id, *logo, QStringLiteral("Logo"));
    parts << QStringLiteral("# ") + rmd_escape(rmd_cover_title(x, c).simplified());
    if (!cfg("subtitle").isEmpty())
        parts << rmd_italic(cfg("subtitle"));
    QString byline = cfg("byline");
    if (byline.isEmpty()) {
        QStringList who;
        for (const QString& s : {m.author, m.company})
            if (!s.trimmed().isEmpty())
                who << vars::expand(s.trimmed(), x.vars);
        byline = who.join(QStringLiteral(" · "));
    }
    if (!byline.isEmpty())
        parts << rmd_text(byline);
    const QString date = cfg("date").isEmpty() ? m.date.trimmed() : cfg("date");
    if (!date.isEmpty())
        parts << rmd_text(date);
    if (!cfg("disclaimer").isEmpty())
        parts << rmd_italic(cfg("disclaimer"));
    parts << QStringLiteral("---");
    return parts.join(QStringLiteral("\n\n"));
}

QString rmd_block(RmdCtx& x, const ReportComponent& c) {
    const QString& t = c.type;
    QStringList parts;
    if (t == QLatin1String("heading")) {
        const outline::Heading h = rmd_heading_of(x, c);
        parts << QString(std::min(6, h.level + 1), QLatin1Char('#')) + QLatin1Char(' ') + rmd_escape(rmd_label(h));
    } else if (t == QLatin1String("text")) {
        if (c.config.value(QStringLiteral("style")) == QLatin1String("caption"))
            parts << rmd_italic(c.content);
        else
            parts << rmd_verbatim(c.content); // Markdown already
    } else if (t == QLatin1String("table")) {
        parts << rmd_table_block(x, c);
    } else if (t == QLatin1String("chart")) {
        const auto it = x.assets->charts.constFind(c.id);
        if (it == x.assets->charts.cend())
            return {};
        const QString title = c.config.value(QStringLiteral("title"), QStringLiteral("Chart")).trimmed();
        const QString caption = c.config.value(QStringLiteral("caption")).trimmed();
        parts << rmd_image(x, QStringLiteral("chart"), c.id, *it, caption.isEmpty() ? title : caption);
        if (!caption.isEmpty())
            parts << rmd_italic(caption);
        if (c.config.value(QStringLiteral("sample")) == QLatin1String("1"))
            parts << QStringLiteral("**SAMPLE DATA — placeholder values from the template, replace before publishing**");
    } else if (t == QLatin1String("sparkline")) {
        QStringList bits;
        const QString title = c.config.value(QStringLiteral("title")).trimmed();
        const QString current = c.config.value(QStringLiteral("current")).trimmed();
        const QString change = c.config.value(QStringLiteral("change_pct")).trimmed();
        if (!title.isEmpty())
            bits << QStringLiteral("**") + rmd_escape(title) + QStringLiteral("**");
        if (!current.isEmpty())
            bits << rmd_escape(current);
        if (!change.isEmpty())
            bits << rmd_escape(change + (change.endsWith(QLatin1Char('%')) ? QString() : QStringLiteral("%")));
        const auto it = x.assets->charts.constFind(c.id);
        if (it != x.assets->charts.cend() && !it->bytes.isEmpty())
            bits << rmd_image(x, QStringLiteral("chart"), c.id, *it, title.isEmpty() ? QStringLiteral("Sparkline") : title);
        parts << bits.join(QLatin1Char(' '));
    } else if (t == QLatin1String("image")) {
        const auto it = x.assets->images.constFind(c.id);
        if (it == x.assets->images.cend())
            return {};
        const QString caption = c.config.value(QStringLiteral("caption")).trimmed();
        parts << rmd_image(x, QStringLiteral("image"), c.id, *it, caption.isEmpty() ? QStringLiteral("Image") : caption);
        if (!caption.isEmpty())
            parts << rmd_italic(caption);
    } else if (t == QLatin1String("stats_block")) {
        parts << rmd_stats(c);
    } else if (t == QLatin1String("market_data")) {
        parts << rmd_market(c);
    } else if (t == QLatin1String("callout")) {
        parts << rmd_callout(c);
    } else if (t == QLatin1String("quote")) {
        parts << rmd_quoted(c.content);
    } else if (t == QLatin1String("code")) {
        parts << rmd_code(c);
    } else if (t == QLatin1String("list")) {
        parts << rmd_list(c);
    } else if (t == QLatin1String("divider")) {
        parts << QStringLiteral("---");
    } else if (t == QLatin1String("page_break")) {
        parts << QString::fromLatin1("<div style=\"page-break-after: always;\"></div>");
    } else if (t == QLatin1String("toc")) {
        parts << rmd_toc(x);
    } else if (t == QLatin1String("cover")) {
        parts << rmd_cover(x, c);
    } else {
        return {};
    }
    if (ef::has_provenance(t)) {
        const QString line = ef::provenance_text(c);
        if (!line.isEmpty())
            parts << rmd_italic(line);
    }
    parts.removeAll(QString());
    return parts.join(QStringLiteral("\n\n"));
}

} // namespace

MarkdownOutput write(const ReportDocument& doc, const exportfmt::ExportAssets& assets, const MarkdownOptions& opt,
                     const std::function<bool()>& cancelled) {
    MarkdownOutput result;
    const ReportMetadata& m = doc.metadata;

    // What prints: blocks with content (numbers, the TOC and anchors count only those).
    ReportDocument printed = doc;
    printed.components.clear();
    for (const ReportComponent& c : doc.components)
        if (!ef::is_unfilled(c, assets))
            printed.components.append(c);
    while (!printed.components.isEmpty() && printed.components.last().type == QLatin1String("page_break"))
        printed.components.removeLast(); // no empty last page

    RmdCtx x;
    x.doc = &doc;
    x.assets = &assets;
    x.opt = &opt;
    x.vars = vars::context(m);
    x.headings = outline::headings(printed);
    for (int i = 0; i < x.headings.size(); ++i)
        x.heading_index.insert(x.headings[i].id, i);
    x.assets_dir = QString(opt.assets_dir).replace(QLatin1Char('\\'), QLatin1Char('/')).trimmed();
    while (x.assets_dir.endsWith(QLatin1Char('/')))
        x.assets_dir.chop(1);

    QVector<ReportComponent> blocks;
    blocks.reserve(printed.components.size());
    for (const ReportComponent& raw : printed.components)
        blocks.append(vars::expanded_component(raw, x.vars));
    const bool cover_first = !blocks.isEmpty() && blocks.first().type == QLatin1String("cover");
    const QString title = vars::expand(m.title, x.vars).simplified();

    // Anchors of every heading GitHub will see, in document order (duplicates get -1, -2).
    RmdSlugger slugger;
    if (!cover_first)
        slugger.slug(title);
    for (const ReportComponent& c : blocks) {
        if (c.type == QLatin1String("heading"))
            x.slugs.insert(c.id, slugger.slug(rmd_label(rmd_heading_of(x, c))));
        else if (c.type == QLatin1String("text") && c.config.value(QStringLiteral("style")) != QLatin1String("caption"))
            for (const QString& h : rmd_atx_headings(rmd_verbatim(c.content)))
                slugger.slug(h);
        else if (c.type == QLatin1String("cover"))
            slugger.slug(rmd_cover_title(x, c).simplified());
    }

    QStringList chunks;
    if (!cover_first) {
        chunks << QStringLiteral("# ") + rmd_escape(title);
        QStringList who;
        for (const QString& s : {m.author, m.company, m.date})
            if (!s.trimmed().isEmpty())
                who << vars::expand(s.trimmed(), x.vars);
        if (!who.isEmpty())
            chunks << rmd_text(who.join(QStringLiteral(" · ")));
    }
    for (const ReportComponent& c : blocks) {
        if (cancelled && cancelled()) {
            result.cancelled = true;
            return result;
        }
        const QString chunk = rmd_block(x, c);
        if (!chunk.isEmpty())
            chunks << chunk;
    }
    if (cancelled && cancelled()) {
        result.cancelled = true;
        return result;
    }

    QString out;
    if (opt.front_matter) {
        out += QStringLiteral("---\ntitle: ") + rmd_yaml(title) + QLatin1Char('\n');
        if (!m.author.trimmed().isEmpty())
            out += QStringLiteral("author: ") + rmd_yaml(m.author.trimmed()) + QLatin1Char('\n');
        if (!m.company.trimmed().isEmpty())
            out += QStringLiteral("company: ") + rmd_yaml(m.company.trimmed()) + QLatin1Char('\n');
        if (!m.date.trimmed().isEmpty())
            out += QStringLiteral("date: ") + rmd_yaml(m.date.trimmed()) + QLatin1Char('\n');
        out += QStringLiteral("---\n\n");
    }
    out += chunks.join(QStringLiteral("\n\n"));
    out += QLatin1Char('\n');
    result.markdown = out;
    result.files = x.files;
    return result;
}

} // namespace fincept::report::md
