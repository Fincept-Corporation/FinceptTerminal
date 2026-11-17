// ChatMarkdown.cpp — model output (Markdown) -> safe transcript blocks (see the header).

#include "screens/chat_mode/ChatMarkdown.h"

#include "services/agent_tasks/ArtefactLogic.h"

#include <QCoreApplication>
#include <QHash>
#include <QStringView>

#include <md4c.h>

#include <algorithm>
#include <cmath>

namespace fincept::chat_mode::md {

namespace chat_markdown_detail {

QString cmd_tr(const char* text) {
    return QCoreApplication::translate("fincept::chat_mode::ChatMarkdown", text);
}

/// The few named entities model output uses; numeric ones in full. Unknown: as written.
QString cmd_decode_entity(const QString& e) {
    if (e.size() < 3 || !e.startsWith(QLatin1Char('&')) || !e.endsWith(QLatin1Char(';')))
        return e;
    const QString name = e.mid(1, e.size() - 2);
    if (name.startsWith(QLatin1Char('#'))) {
        bool ok = false;
        const uint code = (name.size() > 1 && (name.at(1) == QLatin1Char('x') || name.at(1) == QLatin1Char('X')))
                              ? name.mid(2).toUInt(&ok, 16)
                              : name.mid(1).toUInt(&ok, 10);
        if (!ok || code == 0 || code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF))
            return QString(QChar(0xFFFD));
        const char32_t c = static_cast<char32_t>(code);
        return QString::fromUcs4(&c, 1);
    }
    static const QHash<QString, QString> kNamed = {
        {QStringLiteral("amp"), QStringLiteral("&")},    {QStringLiteral("lt"), QStringLiteral("<")},
        {QStringLiteral("gt"), QStringLiteral(">")},     {QStringLiteral("quot"), QStringLiteral("\"")},
        {QStringLiteral("apos"), QStringLiteral("'")},   {QStringLiteral("nbsp"), QString(QChar(0x00A0))},
        {QStringLiteral("mdash"), QString(QChar(0x2014))}, {QStringLiteral("ndash"), QString(QChar(0x2013))},
        {QStringLiteral("hellip"), QString(QChar(0x2026))}, {QStringLiteral("copy"), QString(QChar(0x00A9))},
        {QStringLiteral("reg"), QString(QChar(0x00AE))}, {QStringLiteral("trade"), QString(QChar(0x2122))},
        {QStringLiteral("euro"), QString(QChar(0x20AC))}, {QStringLiteral("pound"), QString(QChar(0x00A3))},
        {QStringLiteral("yen"), QString(QChar(0x00A5))}, {QStringLiteral("times"), QString(QChar(0x00D7))},
        {QStringLiteral("rarr"), QString(QChar(0x2192))}, {QStringLiteral("larr"), QString(QChar(0x2190))},
    };
    const auto it = kNamed.constFind(name);
    return it == kNamed.constEnd() ? e : it.value();
}

/// An md4c attribute (href / src / lang) as text: entities decoded, NUL replaced.
QString cmd_attribute(const MD_ATTRIBUTE& a) {
    if (!a.text || a.size == 0)
        return {};
    if (!a.substr_types || !a.substr_offsets)
        return QString::fromUtf8(a.text, static_cast<qsizetype>(a.size));
    QString out;
    for (int i = 0; a.substr_offsets[i] < a.size; ++i) {
        const MD_OFFSET from = a.substr_offsets[i];
        const MD_OFFSET to = a.substr_offsets[i + 1];
        const QString part = QString::fromUtf8(a.text + from, static_cast<qsizetype>(to - from));
        switch (a.substr_types[i]) {
            case MD_TEXT_ENTITY:
                out += cmd_decode_entity(part);
                break;
            case MD_TEXT_NULLCHAR:
                out += QChar(0xFFFD);
                break;
            default:
                out += part;
                break;
        }
    }
    return out;
}

bool cmd_word_char(QChar c) {
    return c.isLetterOrNumber() || c == QLatin1Char('_');
}

/// One top-level block being assembled, plus the inline / nesting state.
struct CmdBuilder {
    ParseOptions opts;
    Document doc;
    QSet<QString> seen_links;

    int depth = 0;            ///< open blocks below the document
    bool have_block = false;
    Block cur;

    // Top-level table capture.
    bool table_head = false;
    QStringList table_row;
    QString cell;
    bool in_cell = false;

    // Nested lists (plain text numbering) and headings.
    QVector<int> ol_counters; ///< -1 for an unordered list
    QVector<int> h_levels;

    // Inline state.
    int link_depth = 0;           ///< inside <a>: no ticker chips
    QVector<bool> link_open;      ///< per MD_SPAN_A: whether an <a> was written
    QVector<QString> link_text;   ///< text of the links being read
    QVector<QString> link_url;
    int img_depth = 0;
    QString img_alt;
    int code_span = 0;
    int code_block = 0;           ///< inside a (nested or top-level) code block

    void begin_top(BlockKind kind) {
        cur = Block();
        cur.kind = kind;
        have_block = true;
    }

    void end_top() {
        if (!have_block)
            return;
        have_block = false;
        Block b = std::move(cur);
        cur = Block();
        if (b.kind == BlockKind::Code) {
            if (b.plain.endsWith(QLatin1Char('\n')))
                b.plain.chop(1);
        } else if (b.kind == BlockKind::Table) {
            const int cols = static_cast<int>(b.header.size());
            if (cols == 0)
                return;
            b.align.resize(cols);
            b.numeric.fill(false, cols);
            for (QStringList& r : b.rows) {
                while (r.size() < cols)
                    r << QString();
                if (r.size() > cols)
                    r = r.mid(0, cols);
            }
            for (int c = 0; c < cols; ++c) {
                int filled = 0;
                int numbers = 0;
                for (const QStringList& r : std::as_const(b.rows)) {
                    const QString v = r.at(c).trimmed();
                    if (v.isEmpty() || v == QLatin1String("-") || v == QStringLiteral("—"))
                        continue;
                    ++filled;
                    if (agent_tasks::artefact_logic::looks_numeric(v))
                        ++numbers;
                }
                b.numeric[c] = filled > 0 && numbers * 10 >= filled * 6;
            }
            QStringList lines;
            lines << b.header.join(QLatin1Char('\t'));
            for (const QStringList& r : std::as_const(b.rows))
                lines << r.join(QLatin1Char('\t'));
            b.plain = lines.join(QLatin1Char('\n'));
        } else {
            b.plain = b.plain.trimmed();
            if (b.kind != BlockKind::Rule && b.html.trimmed().isEmpty() && b.plain.isEmpty())
                return; // an empty paragraph (only whitespace)
        }
        doc.blocks.append(std::move(b));
    }

    void html(const QString& s) {
        if (img_depth > 0)
            return;
        if (in_cell && cur.kind == BlockKind::Table && depth >= 1)
            return; // top-level table cells are plain
        cur.html += s;
    }

    void plain(const QString& s) {
        if (img_depth > 0) {
            img_alt += s;
            return;
        }
        if (!link_text.isEmpty())
            link_text.last() += s;
        if (in_cell && cur.kind == BlockKind::Table) {
            cell += s;
            return;
        }
        cur.plain += s;
    }

    /// Escaped text with ticker chips (outside links / code).
    void text_with_chips(const QString& t) {
        plain(t);
        if (img_depth > 0 || (in_cell && cur.kind == BlockKind::Table))
            return;
        if (link_depth > 0 || code_span > 0 || code_block > 0 || (opts.symbols.isEmpty() && !opts.cashtags)) {
            html(escape_html(t, false));
            return;
        }
        QString out;
        out.reserve(t.size() + 16);
        const int n = static_cast<int>(t.size());
        int i = 0;
        int flushed = 0;
        while (i < n) {
            const QChar c = t.at(i);
            const bool boundary = i == 0 || !(cmd_word_char(t.at(i - 1)) || t.at(i - 1) == QLatin1Char('$'));
            const bool cash = opts.cashtags && c == QLatin1Char('$') && i + 1 < n && t.at(i + 1).isUpper();
            if (!boundary || !(cash || (c.isUpper() && !opts.symbols.isEmpty()))) {
                ++i;
                continue;
            }
            int j = cash ? i + 1 : i;
            while (j < n && j - i <= 16 &&
                   (t.at(j).isUpper() || t.at(j).isDigit() || t.at(j) == QLatin1Char('.') || t.at(j) == QLatin1Char('-')))
                ++j;
            int end = j;
            while (end > i + 1 && (t.at(end - 1) == QLatin1Char('.') || t.at(end - 1) == QLatin1Char('-')))
                --end;
            const bool word_end = end >= n || !cmd_word_char(t.at(end));
            const QString sym = t.mid(cash ? i + 1 : i, end - (cash ? i + 1 : i));
            const bool chip = word_end && !sym.isEmpty() && (cash ? sym.size() <= 10 : opts.symbols.contains(sym));
            if (!chip) {
                i = std::max(i + 1, end);
                continue;
            }
            out += escape_html(t.mid(flushed, i - flushed), false);
            out += QStringLiteral("<a class=\"tk\" href=\"%1:%2\">%3</a>")
                       .arg(QLatin1String(kSymbolScheme), escape_html(sym, false), escape_html(t.mid(i, end - i), false));
            i = end;
            flushed = end;
        }
        out += escape_html(t.mid(flushed), false);
        html(out);
    }

    // ── md4c callbacks ───────────────────────────────────────────────────────

    int enter_block(MD_BLOCKTYPE type, void* detail) {
        if (type == MD_BLOCK_DOC)
            return 0;
        const bool top = depth == 0;
        ++depth;
        if (top) {
            switch (type) {
                case MD_BLOCK_H:
                    begin_top(BlockKind::Heading);
                    cur.level = static_cast<int>(static_cast<MD_BLOCK_H_DETAIL*>(detail)->level);
                    return 0;
                case MD_BLOCK_CODE: {
                    begin_top(BlockKind::Code);
                    const auto* d = static_cast<MD_BLOCK_CODE_DETAIL*>(detail);
                    cur.lang = cmd_attribute(d->lang).trimmed().left(32);
                    ++code_block;
                    return 0;
                }
                case MD_BLOCK_TABLE:
                    begin_top(BlockKind::Table);
                    return 0;
                case MD_BLOCK_HR:
                    begin_top(BlockKind::Rule);
                    return 0;
                case MD_BLOCK_QUOTE:
                    begin_top(BlockKind::Quote);
                    return 0;
                case MD_BLOCK_UL:
                case MD_BLOCK_OL:
                    begin_top(BlockKind::List);
                    break; // and write the list tag below
                default: // P, HTML (switched off: arrives as text)
                    begin_top(BlockKind::Paragraph);
                    return 0;
            }
        }
        if (cur.kind == BlockKind::Table && depth >= 2) {
            switch (type) {
                case MD_BLOCK_THEAD:
                    table_head = true;
                    break;
                case MD_BLOCK_TBODY:
                    table_head = false;
                    break;
                case MD_BLOCK_TR:
                    table_row.clear();
                    break;
                case MD_BLOCK_TH:
                case MD_BLOCK_TD: {
                    in_cell = true;
                    cell.clear();
                    if (type == MD_BLOCK_TH) {
                        const auto* d = static_cast<MD_BLOCK_TD_DETAIL*>(detail);
                        cur.align.append(static_cast<int>(d->align));
                    }
                    break;
                }
                default:
                    break;
            }
            return 0;
        }
        switch (type) {
            case MD_BLOCK_UL:
                ol_counters.append(-1);
                html(QStringLiteral("<ul>"));
                break;
            case MD_BLOCK_OL: {
                const auto* d = static_cast<MD_BLOCK_OL_DETAIL*>(detail);
                const int start = static_cast<int>(d->start);
                ol_counters.append(start);
                html(start == 1 ? QStringLiteral("<ol>") : QStringLiteral("<ol start=\"%1\">").arg(start));
                break;
            }
            case MD_BLOCK_LI: {
                const auto* d = static_cast<MD_BLOCK_LI_DETAIL*>(detail);
                html(QStringLiteral("<li>"));
                QString mark;
                if (!ol_counters.isEmpty() && ol_counters.last() >= 0)
                    mark = QStringLiteral("%1. ").arg(ol_counters.last()++);
                else
                    mark = QStringLiteral("• ");
                if (!cur.plain.isEmpty() && !cur.plain.endsWith(QLatin1Char('\n')))
                    cur.plain += QLatin1Char('\n');
                cur.plain += QString(std::max(0, static_cast<int>(ol_counters.size()) - 1) * 2, QLatin1Char(' ')) + mark;
                if (d && d->is_task) {
                    const bool done = d->task_mark == 'x' || d->task_mark == 'X';
                    const QString box = done ? QStringLiteral("☑ ") : QStringLiteral("☐ ");
                    html(box);
                    cur.plain += box;
                }
                break;
            }
            case MD_BLOCK_P:
                if (cur.kind != BlockKind::Paragraph)
                    html(QStringLiteral("<p>"));
                break;
            case MD_BLOCK_QUOTE:
                html(QStringLiteral("<blockquote>"));
                break;
            case MD_BLOCK_H: {
                const unsigned level = std::clamp(static_cast<MD_BLOCK_H_DETAIL*>(detail)->level, 1u, 6u);
                h_levels.append(static_cast<int>(level));
                html(QStringLiteral("<h%1>").arg(level));
                break;
            }
            case MD_BLOCK_CODE:
                ++code_block;
                html(QStringLiteral("<pre>"));
                break;
            case MD_BLOCK_HR:
                html(QStringLiteral("<hr/>"));
                break;
            case MD_BLOCK_TABLE:
                html(QStringLiteral("<table>"));
                break;
            case MD_BLOCK_TR:
                html(QStringLiteral("<tr>"));
                break;
            case MD_BLOCK_TH:
                html(QStringLiteral("<th>"));
                break;
            case MD_BLOCK_TD:
                html(QStringLiteral("<td>"));
                break;
            default:
                break;
        }
        return 0;
    }

    int leave_block(MD_BLOCKTYPE type, void*) {
        if (type == MD_BLOCK_DOC)
            return 0;
        --depth;
        if (cur.kind == BlockKind::Table && depth >= 1) {
            switch (type) {
                case MD_BLOCK_TH:
                case MD_BLOCK_TD:
                    in_cell = false;
                    if (type == MD_BLOCK_TH || table_head)
                        cur.header << cell.simplified();
                    else
                        table_row << cell.simplified();
                    cell.clear();
                    break;
                case MD_BLOCK_TR:
                    if (!table_head)
                        cur.rows.append(table_row);
                    table_row.clear();
                    break;
                default:
                    break;
            }
            return 0;
        }
        if (depth == 0) {
            if (type == MD_BLOCK_CODE)
                code_block = std::max(0, code_block - 1);
            if (type == MD_BLOCK_UL || type == MD_BLOCK_OL) {
                if (!ol_counters.isEmpty())
                    ol_counters.removeLast();
                html(type == MD_BLOCK_UL ? QStringLiteral("</ul>") : QStringLiteral("</ol>"));
            }
            end_top();
            return 0;
        }
        switch (type) {
            case MD_BLOCK_UL:
                if (!ol_counters.isEmpty())
                    ol_counters.removeLast();
                html(QStringLiteral("</ul>"));
                break;
            case MD_BLOCK_OL:
                if (!ol_counters.isEmpty())
                    ol_counters.removeLast();
                html(QStringLiteral("</ol>"));
                break;
            case MD_BLOCK_LI:
                html(QStringLiteral("</li>"));
                break;
            case MD_BLOCK_P:
                if (cur.kind != BlockKind::Paragraph)
                    html(QStringLiteral("</p>"));
                cur.plain += QLatin1Char('\n');
                break;
            case MD_BLOCK_QUOTE:
                html(QStringLiteral("</blockquote>"));
                break;
            case MD_BLOCK_H: {
                const int level = h_levels.isEmpty() ? 1 : h_levels.takeLast();
                html(QStringLiteral("</h%1>").arg(level));
                cur.plain += QLatin1Char('\n');
                break;
            }
            case MD_BLOCK_CODE:
                code_block = std::max(0, code_block - 1);
                html(QStringLiteral("</pre>"));
                cur.plain += QLatin1Char('\n');
                break;
            case MD_BLOCK_TABLE:
                html(QStringLiteral("</table>"));
                break;
            case MD_BLOCK_TR:
                html(QStringLiteral("</tr>"));
                cur.plain += QLatin1Char('\n');
                break;
            case MD_BLOCK_TH:
                html(QStringLiteral("</th>"));
                cur.plain += QLatin1Char('\t');
                break;
            case MD_BLOCK_TD:
                html(QStringLiteral("</td>"));
                cur.plain += QLatin1Char('\t');
                break;
            default:
                break;
        }
        return 0;
    }

    int enter_span(MD_SPANTYPE type, void* detail) {
        switch (type) {
            case MD_SPAN_EM:
                html(QStringLiteral("<i>"));
                break;
            case MD_SPAN_STRONG:
                html(QStringLiteral("<b>"));
                break;
            case MD_SPAN_DEL:
                html(QStringLiteral("<s>"));
                break;
            case MD_SPAN_U:
                html(QStringLiteral("<u>"));
                break;
            case MD_SPAN_CODE:
                ++code_span;
                html(QStringLiteral("<code>"));
                break;
            case MD_SPAN_A: {
                const auto* d = static_cast<MD_SPAN_A_DETAIL*>(detail);
                QString href = cmd_attribute(d->href).trimmed();
                if (href.startsWith(QLatin1String("www."), Qt::CaseInsensitive))
                    href.prepend(QStringLiteral("https://"));
                const QUrl url(href, QUrl::TolerantMode);
                const bool ok = link_allowed(url);
                link_open.append(ok);
                link_text.append(QString());
                link_url.append(ok ? url.toString(QUrl::FullyEncoded) : QString());
                ++link_depth;
                if (ok)
                    html(QStringLiteral("<a href=\"%1\">").arg(escape_html(link_url.last(), false)));
                break;
            }
            case MD_SPAN_IMG: {
                const auto* d = static_cast<MD_SPAN_IMG_DETAIL*>(detail);
                const QUrl url(cmd_attribute(d->src).trimmed(), QUrl::TolerantMode);
                link_url.append(link_allowed(url) ? url.toString(QUrl::FullyEncoded) : QString());
                link_open.append(false);
                link_text.append(QString());
                ++img_depth;
                img_alt.clear();
                break;
            }
            default: // LaTeX / wiki links are off
                break;
        }
        return 0;
    }

    int leave_span(MD_SPANTYPE type, void*) {
        switch (type) {
            case MD_SPAN_EM:
                html(QStringLiteral("</i>"));
                break;
            case MD_SPAN_STRONG:
                html(QStringLiteral("</b>"));
                break;
            case MD_SPAN_DEL:
                html(QStringLiteral("</s>"));
                break;
            case MD_SPAN_U:
                html(QStringLiteral("</u>"));
                break;
            case MD_SPAN_CODE:
                code_span = std::max(0, code_span - 1);
                html(QStringLiteral("</code>"));
                break;
            case MD_SPAN_A: {
                link_depth = std::max(0, link_depth - 1);
                const bool ok = link_open.isEmpty() ? false : link_open.takeLast();
                const QString text = link_text.isEmpty() ? QString() : link_text.takeLast();
                const QString url = link_url.isEmpty() ? QString() : link_url.takeLast();
                if (ok) {
                    html(QStringLiteral("</a>"));
                    if (!seen_links.contains(url)) {
                        seen_links.insert(url);
                        doc.links.append(Link{url, text.simplified()});
                    }
                }
                if (!link_text.isEmpty())
                    link_text.last() += text;
                break;
            }
            case MD_SPAN_IMG: {
                img_depth = std::max(0, img_depth - 1);
                if (!link_open.isEmpty())
                    link_open.removeLast();
                if (!link_text.isEmpty())
                    link_text.removeLast();
                const QString url = link_url.isEmpty() ? QString() : link_url.takeLast();
                const QString alt = img_alt.simplified();
                const QString label = QStringLiteral("[%1]").arg(alt.isEmpty() ? cmd_tr("image") : cmd_tr("image: %1").arg(alt));
                if (img_depth == 0) {
                    plain(label);
                    if (!url.isEmpty()) {
                        html(QStringLiteral("<a href=\"%1\">%2</a>").arg(escape_html(url, false), escape_html(label, false)));
                        if (!seen_links.contains(url)) {
                            seen_links.insert(url);
                            doc.links.append(Link{url, label});
                        }
                    } else {
                        html(escape_html(label, false));
                    }
                }
                break;
            }
            default:
                break;
        }
        return 0;
    }

    int text(MD_TEXTTYPE type, const MD_CHAR* data, MD_SIZE size) {
        const QString t = QString::fromUtf8(data, static_cast<qsizetype>(size));
        switch (type) {
            case MD_TEXT_NULLCHAR:
                text_with_chips(QString(QChar(0xFFFD)));
                break;
            case MD_TEXT_BR:
                html(QStringLiteral("<br/>"));
                plain(QStringLiteral("\n"));
                break;
            case MD_TEXT_SOFTBR:
                html(QStringLiteral(" "));
                plain(in_cell ? QStringLiteral(" ") : QStringLiteral("\n"));
                break;
            case MD_TEXT_ENTITY:
                text_with_chips(cmd_decode_entity(t));
                break;
            case MD_TEXT_CODE:
                if (have_block && cur.kind == BlockKind::Code && depth >= 1 && code_block > 0 && link_depth == 0 &&
                    code_span == 0) {
                    cur.plain += t; // top-level code: raw
                } else {
                    plain(t);
                    html(escape_html(t, false)); // <pre> keeps its line breaks
                }
                break;
            case MD_TEXT_NORMAL:
            case MD_TEXT_HTML: // raw HTML is off; anything left is shown as text
            default:
                text_with_chips(t);
                break;
        }
        return 0;
    }
};

int cmd_enter_block(MD_BLOCKTYPE t, void* d, void* u) {
    return static_cast<CmdBuilder*>(u)->enter_block(t, d);
}
int cmd_leave_block(MD_BLOCKTYPE t, void* d, void* u) {
    return static_cast<CmdBuilder*>(u)->leave_block(t, d);
}
int cmd_enter_span(MD_SPANTYPE t, void* d, void* u) {
    return static_cast<CmdBuilder*>(u)->enter_span(t, d);
}
int cmd_leave_span(MD_SPANTYPE t, void* d, void* u) {
    return static_cast<CmdBuilder*>(u)->leave_span(t, d);
}
int cmd_text(MD_TEXTTYPE t, const MD_CHAR* s, MD_SIZE n, void* u) {
    return static_cast<CmdBuilder*>(u)->text(t, s, n);
}

} // namespace chat_markdown_detail

using namespace chat_markdown_detail;

bool link_allowed(const QUrl& url) {
    if (!url.isValid())
        return false;
    const QString scheme = url.scheme().toLower();
    if (scheme == QLatin1String("http") || scheme == QLatin1String("https"))
        return !url.host().isEmpty();
    if (scheme == QLatin1String("mailto"))
        return !url.path().isEmpty();
    return false;
}

QString escape_html(const QString& plain, bool keep_line_breaks) {
    QString out;
    out.reserve(plain.size() + plain.size() / 8);
    for (const QChar c : plain) {
        switch (c.unicode()) {
            case '&':
                out += QStringLiteral("&amp;");
                break;
            case '<':
                out += QStringLiteral("&lt;");
                break;
            case '>':
                out += QStringLiteral("&gt;");
                break;
            case '"':
                out += QStringLiteral("&quot;");
                break;
            case '\n':
                out += keep_line_breaks ? QStringLiteral("<br/>") : QStringLiteral("\n");
                break;
            default:
                out += c;
                break;
        }
    }
    return out;
}

Document parse(const QString& markdown, const ParseOptions& options) {
    CmdBuilder b;
    b.opts = options;
    if (markdown.trimmed().isEmpty())
        return b.doc;
    const QByteArray utf8 = markdown.toUtf8();
    MD_PARSER parser{};
    parser.abi_version = 0;
    parser.flags = MD_FLAG_NOHTML | MD_FLAG_TABLES | MD_FLAG_STRIKETHROUGH | MD_FLAG_TASKLISTS |
                   MD_FLAG_PERMISSIVEAUTOLINKS | MD_FLAG_NOINDENTEDCODEBLOCKS;
    parser.enter_block = &cmd_enter_block;
    parser.leave_block = &cmd_leave_block;
    parser.enter_span = &cmd_enter_span;
    parser.leave_span = &cmd_leave_span;
    parser.text = &cmd_text;
    parser.debug_log = nullptr;
    parser.syntax = nullptr;
    const int rc = md_parse(utf8.constData(), static_cast<MD_SIZE>(utf8.size()), &parser, &b);
    b.end_top();
    if (rc != 0 && b.doc.blocks.isEmpty()) {
        // The parser gave up (out of memory): the text as one plain paragraph.
        Block p;
        p.kind = BlockKind::Paragraph;
        p.plain = markdown;
        p.html = escape_html(markdown, true);
        b.doc.blocks.append(p);
    }
    return b.doc;
}

int StreamCursor::advance(const QString& text) {
    const int n = static_cast<int>(text.size());
    if (scanned_ > n) { // the text was replaced, not extended: start over
        reset();
    }
    while (scanned_ < n) {
        const int nl = static_cast<int>(text.indexOf(QLatin1Char('\n'), scanned_));
        if (nl < 0)
            break; // an incomplete line: wait for the rest
        const QStringView line = QStringView(text).mid(scanned_, nl - scanned_);
        int i = 0;
        while (i < line.size() && (line.at(i) == QLatin1Char(' ') || line.at(i) == QLatin1Char('\t')))
            ++i;
        bool handled = false;
        if (i < line.size() && (line.at(i) == QLatin1Char('`') || line.at(i) == QLatin1Char('~'))) {
            const QChar c = line.at(i);
            int run = 0;
            while (i + run < line.size() && line.at(i + run) == c)
                ++run;
            if (run >= 3) {
                if (!fence_) {
                    fence_ = true;
                    fence_char_ = c;
                    fence_len_ = run;
                    handled = true;
                } else if (c == fence_char_ && run >= fence_len_ && line.mid(i + run).trimmed().isEmpty()) {
                    fence_ = false;
                    stable_ = nl + 1; // the code block is complete
                    handled = true;
                }
            }
        }
        if (!handled && !fence_ && line.trimmed().isEmpty())
            stable_ = nl + 1;
        scanned_ = nl + 1;
    }
    return stable_;
}

} // namespace fincept::chat_mode::md
