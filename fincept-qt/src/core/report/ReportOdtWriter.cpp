// ReportOdtWriter.cpp — see ReportOdtWriter.h.
//
// Literal colours in this file are DOCUMENT ink (the printed page), the same values as
// screens/report_builder/ReportRenderer.cpp. Lengths the renderer gives in px (96 dpi)
// are converted to mm; point sizes are the renderer's sizes x font_size / 12.
//
// content.xml is written in two parts: the body first (which registers the automatic
// styles it uses), then the head with those styles; the body bytes are appended after
// <office:text>. Every element is written by QXmlStreamWriter, so both parts are
// well-formed by construction.

#include "core/report/ReportOdtWriter.h"

#include "core/report/ReportBinding.h"
#include "core/report/ReportTableFormat.h"
#include "core/report/ReportVariables.h"
#include "core/report/ReportZipWriter.h"

#include <QHash>
#include <QMap>
#include <QPair>
#include <QRegularExpression>
#include <QSizeF>
#include <QStringList>
#include <QVector>
#include <QXmlStreamWriter>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace fincept::report::odt {

namespace {

namespace ef = ::fincept::report::exportfmt;
namespace tf = ::fincept::report::tablefmt;

const char* const kOdtRed = "#dc2626";   // printed red ink
const char* const kOdtGreen = "#16a34a"; // printed green ink
const char* const kOdtAmber = "#d97706"; // printed amber ink
const char* const kOdtBlue = "#3b82f6";  // info callout ink
const char* const kOdtMime = "application/vnd.oasis.opendocument.text";
const char* const kOdtMono = "Consolas";
constexpr double kOdtPxMm = 25.4 / 96.0;

using OdtAttrs = QVector<QPair<QString, QString>>;

// ── Units, colours, text ────────────────────────────────────────────────────

QString odt_num(double v, int decimals = 3) {
    if (!std::isfinite(v))
        v = 0;
    QString s = QString::number(v, 'f', decimals);
    if (s.contains(QLatin1Char('.'))) {
        while (s.endsWith(QLatin1Char('0')))
            s.chop(1);
        if (s.endsWith(QLatin1Char('.')))
            s.chop(1);
    }
    if (s == QLatin1String("-0"))
        s = QStringLiteral("0");
    return s;
}

QString odt_mm(double mm) {
    return odt_num(mm) + QStringLiteral("mm");
}

QString odt_pt(double pt) {
    return odt_num(pt, 2) + QStringLiteral("pt");
}

/// A renderer length (px at 96 dpi) in mm.
QString odt_px(double px) {
    return odt_mm(px * kOdtPxMm);
}

struct OdtRgb {
    int r = 255;
    int g = 255;
    int b = 255;
};

bool odt_parse_rgb(const QString& hex, OdtRgb* out) {
    QString h = hex.trimmed();
    if (!h.startsWith(QLatin1Char('#')))
        return false;
    h.remove(0, 1);
    if (h.size() == 3) {
        QString six;
        for (QChar ch : h) {
            six += ch;
            six += ch;
        }
        h = six;
    }
    if (h.size() != 6)
        return false;
    bool ok = false;
    const uint v = h.toUInt(&ok, 16);
    if (!ok)
        return false;
    out->r = static_cast<int>((v >> 16) & 0xFFu);
    out->g = static_cast<int>((v >> 8) & 0xFFu);
    out->b = static_cast<int>(v & 0xFFu);
    return true;
}

QString odt_hex(const OdtRgb& c) {
    return QStringLiteral("#%1%2%3")
        .arg(c.r, 2, 16, QLatin1Char('0'))
        .arg(c.g, 2, 16, QLatin1Char('0'))
        .arg(c.b, 2, 16, QLatin1Char('0'));
}

/// "#rrggbb" (lower case) or `fallback` when `hex` is not a colour.
QString odt_color(const QString& hex, const char* fallback) {
    OdtRgb c;
    return odt_parse_rgb(hex, &c) ? odt_hex(c) : QString::fromLatin1(fallback);
}

/// `a` with `t` (0..1) of `b` mixed in.
QString odt_blend(const QString& a, const QString& b, double t) {
    OdtRgb x, y{0, 0, 0};
    odt_parse_rgb(a, &x);
    odt_parse_rgb(b, &y);
    auto mix = [t](int p, int q) {
        return std::clamp(static_cast<int>(std::lround(p * (1.0 - t) + q * t)), 0, 255);
    };
    return odt_hex({mix(x.r, y.r), mix(x.g, y.g), mix(x.b, y.b)});
}

/// HSL lightness 0..255 (QColor::lightness()).
int odt_lightness(const QString& hex) {
    OdtRgb c;
    if (!odt_parse_rgb(hex, &c))
        return 255;
    return (std::max({c.r, c.g, c.b}) + std::min({c.r, c.g, c.b})) / 2;
}

/// Characters XML 1.0 cannot carry are dropped (controls, unpaired surrogates, U+FFFE/F).
QString odt_clean(const QString& s) {
    QString out;
    out.reserve(s.size());
    for (qsizetype i = 0; i < s.size(); ++i) {
        const QChar ch = s.at(i);
        if (ch.isHighSurrogate()) {
            if (i + 1 < s.size() && s.at(i + 1).isLowSurrogate()) {
                out += ch;
                out += s.at(i + 1);
                ++i;
            }
            continue;
        }
        const char16_t u = ch.unicode();
        if (ch.isLowSurrogate() || u == 0xFFFE || u == 0xFFFF)
            continue;
        if (u < 0x20 && u != u'\t' && u != u'\n' && u != u'\r')
            continue;
        out += ch;
    }
    return out;
}

/// Trailing line breaks off (the block's bottom margin is the spacing).
QString odt_chop_newlines(QString s) {
    while (s.endsWith(QLatin1Char('\n')) || s.endsWith(QLatin1Char('\r')))
        s.chop(1);
    return s;
}

void odt_set(OdtAttrs& a, const QString& key, const QString& value) {
    for (auto& kv : a) {
        if (kv.first == key) {
            kv.second = value;
            return;
        }
    }
    a.append({key, value});
}

void odt_merge(OdtAttrs& a, const OdtAttrs& b) {
    for (const auto& kv : b)
        odt_set(a, kv.first, kv.second);
}

/// Text properties: size (pt, 0 = inherit), colour (empty = inherit), weight, posture.
OdtAttrs odt_font(double pt, const QString& color, bool bold = false, bool italic = false) {
    OdtAttrs a;
    if (pt > 0) {
        a.append({QStringLiteral("fo:font-size"), odt_pt(pt)});
        a.append({QStringLiteral("style:font-size-asian"), odt_pt(pt)});
        a.append({QStringLiteral("style:font-size-complex"), odt_pt(pt)});
    }
    if (!color.isEmpty())
        a.append({QStringLiteral("fo:color"), color});
    if (bold) {
        a.append({QStringLiteral("fo:font-weight"), QStringLiteral("bold")});
        a.append({QStringLiteral("style:font-weight-asian"), QStringLiteral("bold")});
        a.append({QStringLiteral("style:font-weight-complex"), QStringLiteral("bold")});
    }
    if (italic) {
        a.append({QStringLiteral("fo:font-style"), QStringLiteral("italic")});
        a.append({QStringLiteral("style:font-style-asian"), QStringLiteral("italic")});
        a.append({QStringLiteral("style:font-style-complex"), QStringLiteral("italic")});
    }
    return a;
}

void odt_attrs(QXmlStreamWriter& w, const OdtAttrs& a) {
    for (const auto& kv : a)
        w.writeAttribute(kv.first, odt_clean(kv.second));
}

void odt_namespaces(QXmlStreamWriter& w) {
    static const char* const kNs[][2] = {
        {"office", "urn:oasis:names:tc:opendocument:xmlns:office:1.0"},
        {"style", "urn:oasis:names:tc:opendocument:xmlns:style:1.0"},
        {"text", "urn:oasis:names:tc:opendocument:xmlns:text:1.0"},
        {"table", "urn:oasis:names:tc:opendocument:xmlns:table:1.0"},
        {"draw", "urn:oasis:names:tc:opendocument:xmlns:drawing:1.0"},
        {"fo", "urn:oasis:names:tc:opendocument:xmlns:xsl-fo-compatible:1.0"},
        {"svg", "urn:oasis:names:tc:opendocument:xmlns:svg-compatible:1.0"},
        {"xlink", "http://www.w3.org/1999/xlink"},
        {"dc", "http://purl.org/dc/elements/1.1/"},
        {"meta", "urn:oasis:names:tc:opendocument:xmlns:meta:1.0"},
        {"number", "urn:oasis:names:tc:opendocument:xmlns:datastyle:1.0"},
    };
    for (const auto& ns : kNs)
        w.writeAttribute(QStringLiteral("xmlns:") + QLatin1String(ns[0]), QLatin1String(ns[1]));
    w.writeAttribute("office:version", "1.3");
}

void odt_font_decls(QXmlStreamWriter& w, const QString& family) {
    w.writeStartElement("office:font-face-decls");
    w.writeEmptyElement("style:font-face");
    w.writeAttribute("style:name", odt_clean(family));
    w.writeAttribute("svg:font-family", family.contains(QLatin1Char(' '))
                                            ? QLatin1Char('\'') + odt_clean(family) + QLatin1Char('\'')
                                            : odt_clean(family));
    if (family != QLatin1String(kOdtMono)) {
        w.writeEmptyElement("style:font-face");
        w.writeAttribute("style:name", kOdtMono);
        w.writeAttribute("svg:font-family", kOdtMono);
        w.writeAttribute("style:font-family-generic", "modern");
        w.writeAttribute("style:font-pitch", "fixed");
    }
    w.writeEndElement();
}

// ── Automatic styles ────────────────────────────────────────────────────────

struct OdtProp {
    QString element; ///< "style:paragraph-properties", ...
    OdtAttrs attrs;
};

struct OdtStyle {
    QString name;
    QString family;
    QString parent;
    QVector<OdtProp> props;
};

/// Automatic styles of content.xml, shared by every element with the same properties
/// (names P1, T1, Tb1, Co1, Ce1, fr1 in order of first use: deterministic).
class OdtStyleSet {
  public:
    QString add(const QString& prefix, const QString& family, const QString& parent, const QVector<OdtProp>& props) {
        QVector<OdtProp> used;
        QString key = family + QChar(0x1F) + parent;
        for (const OdtProp& p : props) {
            if (p.attrs.isEmpty())
                continue;
            used.append(p);
            key += QChar(0x1E) + p.element;
            for (const auto& kv : p.attrs)
                key += QChar(0x1F) + kv.first + QLatin1Char('=') + kv.second;
        }
        const auto it = index_.constFind(key);
        if (it != index_.cend())
            return styles_[*it].name;
        OdtStyle s;
        s.name = prefix + QString::number(++counters_[prefix]);
        s.family = family;
        s.parent = parent;
        s.props = used;
        index_.insert(key, static_cast<int>(styles_.size()));
        styles_.append(s);
        return s.name;
    }

    void write(QXmlStreamWriter& w) const {
        for (const OdtStyle& s : styles_) {
            w.writeStartElement("style:style");
            w.writeAttribute("style:name", s.name);
            w.writeAttribute("style:family", s.family);
            if (!s.parent.isEmpty())
                w.writeAttribute("style:parent-style-name", s.parent);
            for (const OdtProp& p : s.props) {
                w.writeEmptyElement(p.element);
                odt_attrs(w, p.attrs);
            }
            w.writeEndElement();
        }
    }

  private:
    QVector<OdtStyle> styles_;
    QHash<QString, int> index_;
    QHash<QString, int> counters_;
};

/// A paragraph: its named style plus the properties this one paragraph adds.
struct OdtPara {
    QString parent;
    OdtAttrs pp; ///< paragraph properties
    OdtAttrs tp; ///< text properties
};

struct OdtPicture {
    QString path;
    QString mime;
    QByteArray bytes;
};

struct OdtCtx {
    const ReportDocument* doc = nullptr;
    const ef::ExportAssets* assets = nullptr;
    QXmlStreamWriter* w = nullptr;
    OdtStyleSet* styles = nullptr;
    QVector<OdtPicture> pictures;
    QMap<QString, QString> vars;
    QVector<outline::Heading> headings; ///< printed headings (numbered when the report numbers them)
    QHash<int, int> heading_index;      ///< heading id -> index in `headings`
    double scale = 1.0;
    double content_mm = 180;
    QString font;
    bool pending_break = false; ///< the next paragraph / table starts a new page
    bool pending_soft = false;  ///< ...set by a cover page (a page_break right after it adds nothing)
    bool block_bold = false;    ///< the block's B / I (FONT section) over everything it prints
    bool block_italic = false;
    bool fresh = true; ///< at the start of a paragraph or line: spaces must be text:s
    QVector<QPair<QString, OdtAttrs>> prefix; ///< runs written at the start of the next paragraph

    double pt(double v) const { return v * scale; }
    double content_px() const { return content_mm / kOdtPxMm; }
    const ReportTheme& th() const { return doc->theme; }
    QString ink() const { return odt_color(doc->theme.text_color, "#333333"); }
    QString meta_ink() const { return odt_color(doc->theme.meta_color, "#666666"); }
    QString heading_ink() const { return odt_color(doc->theme.heading_color, "#1a1a1a"); }
    QString accent() const { return odt_color(doc->theme.accent_color, "#d97706"); }
};

// ── Writing text ────────────────────────────────────────────────────────────

/// Characters with ODF white space: runs of spaces as text:s, tabs as text:tab, line
/// breaks as text:line-break (white space would otherwise collapse to one space).
void odt_chars(OdtCtx& x, const QString& raw) {
    const QString s = odt_clean(raw);
    QXmlStreamWriter& w = *x.w;
    QString buf;
    auto flush = [&] {
        if (!buf.isEmpty()) {
            w.writeCharacters(buf);
            buf.clear();
        }
    };
    for (qsizetype i = 0; i < s.size(); ++i) {
        const QChar ch = s.at(i);
        if (ch == QLatin1Char(' ')) {
            qsizetype n = 1;
            while (i + n < s.size() && s.at(i + n) == QLatin1Char(' '))
                ++n;
            qsizetype extra = n;
            if (!x.fresh) {
                buf += QLatin1Char(' ');
                extra = n - 1;
            }
            if (extra > 0) {
                flush();
                w.writeEmptyElement("text:s");
                if (extra > 1)
                    w.writeAttribute("text:c", QString::number(extra));
            }
            i += n - 1;
            x.fresh = false;
        } else if (ch == QLatin1Char('\t')) {
            flush();
            w.writeEmptyElement("text:tab");
            x.fresh = false;
        } else if (ch == QLatin1Char('\n')) {
            flush();
            w.writeEmptyElement("text:line-break");
            x.fresh = true;
        } else if (ch != QLatin1Char('\r')) {
            buf += ch;
            x.fresh = false;
        }
    }
    flush();
}

void odt_span(OdtCtx& x, const QString& text, const OdtAttrs& tp) {
    if (text.isEmpty())
        return;
    if (tp.isEmpty()) {
        odt_chars(x, text);
        return;
    }
    const QString st = x.styles->add(QStringLiteral("T"), QStringLiteral("text"), {},
                                     {{QStringLiteral("style:text-properties"), tp}});
    x.w->writeStartElement("text:span");
    x.w->writeAttribute("text:style-name", st);
    odt_chars(x, text);
    x.w->writeEndElement();
}

/// The style name for paragraph `p`, with the pending page break and the block's B / I.
QString odt_pstyle(OdtCtx& x, const OdtPara& p) {
    OdtAttrs pp = p.pp;
    OdtAttrs tp = p.tp;
    if (x.pending_break) {
        odt_set(pp, QStringLiteral("fo:break-before"), QStringLiteral("page"));
        x.pending_break = false;
        x.pending_soft = false;
    }
    if (x.block_bold)
        odt_merge(tp, odt_font(0, {}, true));
    if (x.block_italic)
        odt_merge(tp, odt_font(0, {}, false, true));
    if (pp.isEmpty() && tp.isEmpty())
        return p.parent;
    return x.styles->add(QStringLiteral("P"), QStringLiteral("paragraph"), p.parent,
                         {{QStringLiteral("style:paragraph-properties"), pp}, {QStringLiteral("style:text-properties"), tp}});
}

void odt_start_p(OdtCtx& x, const OdtPara& p, int outline_level = 0) {
    const QString style = odt_pstyle(x, p);
    if (outline_level > 0) {
        x.w->writeStartElement("text:h");
        x.w->writeAttribute("text:style-name", style);
        x.w->writeAttribute("text:outline-level", QString::number(outline_level));
    } else {
        x.w->writeStartElement("text:p");
        x.w->writeAttribute("text:style-name", style);
    }
    x.fresh = true;
    if (!x.prefix.isEmpty()) {
        const auto pre = x.prefix;
        x.prefix.clear();
        for (const auto& run : pre)
            odt_span(x, run.first, run.second);
    }
}

void odt_end_p(OdtCtx& x) {
    x.w->writeEndElement();
}

void odt_para(OdtCtx& x, const OdtPara& p, const QString& text) {
    odt_start_p(x, p);
    odt_chars(x, text);
    odt_end_p(x);
}

// ── Markdown subset (text blocks) ───────────────────────────────────────────

struct OdtRun {
    QString text;
    bool bold = false;
    bool italic = false;
    bool code = false;
    QString href;
};

bool odt_ascii_punct(QChar c) {
    static const char kPunct[] = "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";
    return c.unicode() > 0 && c.unicode() < 128 && std::strchr(kPunct, static_cast<char>(c.unicode())) != nullptr;
}

qsizetype odt_indent(const QString& line) {
    qsizetype n = 0;
    while (n < line.size() && line.at(n) == QLatin1Char(' '))
        ++n;
    return n;
}

/// **bold** / __bold__, *italic* / _italic_, `code`, [text](url), backslash escapes.
QVector<OdtRun> odt_inline(const QString& s) {
    QVector<OdtRun> out;
    bool bold = false, italic = false;
    QString cur;
    auto flush = [&] {
        if (!cur.isEmpty()) {
            out.append({cur, bold, italic, false, {}});
            cur.clear();
        }
    };
    const qsizetype n = s.size();
    for (qsizetype i = 0; i < n; ++i) {
        const QChar ch = s.at(i);
        if (ch == QLatin1Char('\\') && i + 1 < n && odt_ascii_punct(s.at(i + 1))) {
            cur += s.at(i + 1);
            ++i;
            continue;
        }
        if (ch == QLatin1Char('`')) {
            const qsizetype j = s.indexOf(QLatin1Char('`'), i + 1);
            if (j > i + 1) {
                flush();
                out.append({s.mid(i + 1, j - i - 1), bold, italic, true, {}});
                i = j;
                continue;
            }
        }
        if (ch == QLatin1Char('[')) {
            const qsizetype mid = s.indexOf(QLatin1String("]("), i + 1);
            const qsizetype end = mid > i ? s.indexOf(QLatin1Char(')'), mid + 2) : -1;
            if (mid > i + 1 && end > mid + 2 && !s.mid(i + 1, mid - i - 1).contains(QLatin1Char('['))) {
                flush();
                QString url = s.mid(mid + 2, end - mid - 2).trimmed();
                if (url.startsWith(QLatin1Char('<')) && url.endsWith(QLatin1Char('>')))
                    url = url.mid(1, url.size() - 2);
                const qsizetype sp = url.indexOf(QLatin1Char(' '));
                if (sp > 0)
                    url = url.left(sp); // [text](url "title")
                for (OdtRun r : odt_inline(s.mid(i + 1, mid - i - 1))) {
                    r.bold = r.bold || bold;
                    r.italic = r.italic || italic;
                    r.href = url;
                    out.append(r);
                }
                i = end;
                continue;
            }
        }
        if (ch == QLatin1Char('*') || ch == QLatin1Char('_')) {
            const QChar prev = i > 0 ? s.at(i - 1) : QLatin1Char(' ');
            if (i + 1 < n && s.at(i + 1) == ch) {
                if (bold) {
                    flush();
                    bold = false;
                    ++i;
                    continue;
                }
                if (s.indexOf(QString(2, ch), i + 2) > i + 2) {
                    flush();
                    bold = true;
                    ++i;
                    continue;
                }
            } else {
                const QChar next = i + 1 < n ? s.at(i + 1) : QLatin1Char(' ');
                const bool intraword = ch == QLatin1Char('_') && prev.isLetterOrNumber() && next.isLetterOrNumber();
                if (!intraword) {
                    if (italic && !prev.isSpace()) {
                        flush();
                        italic = false;
                        continue;
                    }
                    if (!italic && !next.isSpace() && s.indexOf(ch, i + 1) > i + 1) {
                        flush();
                        italic = true;
                        continue;
                    }
                }
            }
        }
        cur += ch;
    }
    flush();
    return out;
}

void odt_runs(OdtCtx& x, const QVector<OdtRun>& runs) {
    for (const OdtRun& r : runs) {
        OdtAttrs tp;
        if (r.bold)
            odt_merge(tp, odt_font(0, {}, true));
        if (r.italic)
            odt_merge(tp, odt_font(0, {}, false, true));
        if (r.code)
            odt_set(tp, QStringLiteral("style:font-name"), QString::fromLatin1(kOdtMono));
        if (r.href.isEmpty()) {
            odt_span(x, r.text, tp);
            continue;
        }
        x.w->writeStartElement("text:a");
        x.w->writeAttribute("xlink:type", "simple");
        x.w->writeAttribute("xlink:href", odt_clean(r.href));
        x.w->writeAttribute("text:style-name", "Internet_20_link");
        odt_span(x, r.text, tp);
        x.w->writeEndElement();
    }
}

/// Markdown body: paragraphs (soft breaks joined), "#" headings as bold paragraphs,
/// "- " / "* " / "+ " and "1. " lists (nested by indentation) as text:list, inline runs.
void odt_markdown(OdtCtx& x, const QString& body, const OdtPara& base) {
    static const QRegularExpression heading_re(QStringLiteral("^#{1,6}\\s+(.*?)\\s*#*\\s*$"));
    static const QRegularExpression item_re(QStringLiteral("^( *)([-*+]|\\d{1,9}[.)])\\s+(.*)$"));
    QXmlStreamWriter& w = *x.w;
    struct List {
        bool ordered = false;
        bool item_open = false;
    };
    QVector<List> lists;
    QStringList para;
    bool blank = false;

    auto flush = [&] {
        if (para.isEmpty())
            return;
        QString text;
        for (int i = 0; i < para.size(); ++i) {
            QString line = para[i];
            QString sep = QStringLiteral(" ");
            if (line.endsWith(QLatin1String("  "))) {
                sep = QStringLiteral("\n");
            } else if (line.endsWith(QLatin1Char('\\'))) {
                line.chop(1);
                sep = QStringLiteral("\n");
            }
            text += line.trimmed();
            if (i + 1 < para.size())
                text += sep;
        }
        para.clear();
        odt_start_p(x, base);
        odt_runs(x, odt_inline(text));
        odt_end_p(x);
    };
    auto close_to = [&](qsizetype depth) {
        while (lists.size() > depth) {
            if (lists.last().item_open)
                w.writeEndElement(); // text:list-item
            w.writeEndElement();     // text:list
            lists.removeLast();
        }
    };

    for (QString line : body.split(QLatin1Char('\n'))) {
        line.remove(QLatin1Char('\r'));
        line.replace(QLatin1Char('\t'), QStringLiteral("    "));
        if (line.trimmed().isEmpty()) {
            flush();
            blank = true;
            continue;
        }
        const qsizetype indent = odt_indent(line);
        const QRegularExpressionMatch hm = indent < 4 ? heading_re.match(line.trimmed()) : QRegularExpressionMatch();
        if (hm.hasMatch()) {
            flush();
            close_to(0);
            OdtPara hp = base;
            odt_merge(hp.tp, odt_font(0, {}, true));
            odt_start_p(x, hp);
            odt_runs(x, odt_inline(hm.captured(1)));
            odt_end_p(x);
            blank = false;
            continue;
        }
        const QRegularExpressionMatch im = item_re.match(line);
        if (im.hasMatch()) {
            flush();
            const bool ordered = im.captured(2).at(0).isDigit();
            qsizetype level = std::min<qsizetype>(im.captured(1).size() / 2, lists.size());
            if (lists.isEmpty())
                level = 0;
            close_to(level + 1);
            if (lists.size() == level + 1) {
                if (lists.last().ordered != ordered) {
                    close_to(level);
                } else if (lists.last().item_open) {
                    w.writeEndElement(); // the previous sibling item
                    lists.last().item_open = false;
                }
            }
            if (lists.size() == level) {
                w.writeStartElement("text:list");
                w.writeAttribute("text:style-name", ordered ? "List_20_Number" : "List_20_Bullet");
                lists.append({ordered, false});
            }
            w.writeStartElement("text:list-item");
            lists.last().item_open = true;
            para << im.captured(3);
            blank = false;
            continue;
        }
        if (!lists.isEmpty() && blank && indent >= 2) {
            para << line; // a further paragraph of the open item
        } else if (!para.isEmpty() && !blank) {
            para << line; // soft line break (also a lazy item continuation)
        } else {
            close_to(0);
            para << line;
        }
        blank = false;
    }
    flush();
    close_to(0);
}

// ── Pictures ────────────────────────────────────────────────────────────────

/// Size on the page (px at 96 dpi): the asset's display size, else the block's width
/// (clamped like the renderer) with the picture's aspect.
QSizeF odt_display_px(const ReportComponent& c, const ef::ImageAsset& a, double default_w, double min_w,
                      double content_px, double default_aspect) {
    if (a.display_px.width() > 0 && a.display_px.height() > 0)
        return a.display_px;
    bool ok = false;
    double w = c.config.value(QStringLiteral("width")).toDouble(&ok);
    if (!ok || w <= 0)
        w = default_w;
    w = std::clamp(w, min_w, std::max(min_w, content_px));
    const double aspect = a.pixels.width() > 0 && a.pixels.height() > 0
                              ? static_cast<double>(a.pixels.height()) / a.pixels.width()
                              : default_aspect;
    return {w, w * aspect};
}

/// An as-character frame with the picture (registered once per path). `max_mm` caps the
/// width (the content width always does), keeping the aspect.
void odt_frame(OdtCtx& x, const QString& kind, int id, const ef::ImageAsset& a, QSizeF px, double max_mm,
               const QString& title, bool middle) {
    const bool chart = kind == QLatin1String("Chart") || kind == QLatin1String("Sparkline");
    const QString path = QStringLiteral("Pictures/%1_%2.%3")
                             .arg(chart ? QStringLiteral("chart") : QStringLiteral("image"))
                             .arg(id)
                             .arg(ef::image_ext(a));
    bool known = false;
    for (const OdtPicture& p : x.pictures)
        known = known || p.path == path;
    if (!known)
        x.pictures.append({path, ef::image_mime(a), a.bytes});

    double w_mm = std::max(1.0, px.width()) * kOdtPxMm;
    double h_mm = std::max(1.0, px.height()) * kOdtPxMm;
    const double cap = std::min(max_mm, x.content_mm);
    if (w_mm > cap) {
        h_mm *= cap / w_mm;
        w_mm = cap;
    }
    const QString gs = x.styles->add(
        QStringLiteral("fr"), QStringLiteral("graphic"), {},
        {{QStringLiteral("style:graphic-properties"),
          {{QStringLiteral("style:vertical-pos"), middle ? QStringLiteral("middle") : QStringLiteral("top")},
           {QStringLiteral("style:vertical-rel"), middle ? QStringLiteral("text") : QStringLiteral("baseline")},
           {QStringLiteral("fo:padding"), QStringLiteral("0mm")},
           {QStringLiteral("fo:border"), QStringLiteral("none")},
           {QStringLiteral("style:mirror"), QStringLiteral("none")}}}});
    QXmlStreamWriter& w = *x.w;
    w.writeStartElement("draw:frame");
    w.writeAttribute("draw:style-name", gs);
    w.writeAttribute("draw:name", QStringLiteral("%1 %2").arg(kind).arg(id));
    w.writeAttribute("text:anchor-type", "as-char");
    w.writeAttribute("svg:width", odt_mm(w_mm));
    w.writeAttribute("svg:height", odt_mm(h_mm));
    w.writeAttribute("draw:z-index", "0");
    w.writeEmptyElement("draw:image");
    w.writeAttribute("xlink:href", path);
    w.writeAttribute("xlink:type", "simple");
    w.writeAttribute("xlink:show", "embed");
    w.writeAttribute("xlink:actuate", "onLoad");
    const QString t = odt_clean(title).trimmed();
    if (!t.isEmpty())
        w.writeTextElement("svg:title", t);
    w.writeEndElement();
    x.fresh = false;
}

// ── Tables ──────────────────────────────────────────────────────────────────

QString odt_align(const QString& a) {
    if (a == QLatin1String("right"))
        return QStringLiteral("end");
    if (a == QLatin1String("center"))
        return QStringLiteral("center");
    return QStringLiteral("start");
}

/// Table style (full content width unless `width_mm` / margins say otherwise), taking the
/// pending page break.
QString odt_table_style(OdtCtx& x, double width_mm, const OdtAttrs& extra) {
    OdtAttrs tp{{QStringLiteral("style:width"), odt_mm(width_mm)}, {QStringLiteral("table:align"), QStringLiteral("margins")}};
    odt_merge(tp, extra);
    if (x.pending_break) {
        odt_set(tp, QStringLiteral("fo:break-before"), QStringLiteral("page"));
        x.pending_break = false;
        x.pending_soft = false;
    }
    return x.styles->add(QStringLiteral("Tb"), QStringLiteral("table"), {}, {{QStringLiteral("style:table-properties"), tp}});
}

void odt_columns(OdtCtx& x, const QVector<double>& percent, double width_mm) {
    double sum = 0;
    for (double v : percent)
        sum += std::max(0.0, v);
    for (double v : percent) {
        const double share = sum > 0 ? std::max(0.0, v) / sum : 1.0 / std::max<qsizetype>(1, percent.size());
        const QString cs = x.styles->add(
            QStringLiteral("Co"), QStringLiteral("table-column"), {},
            {{QStringLiteral("style:table-column-properties"),
              {{QStringLiteral("style:column-width"), odt_mm(width_mm * share)},
               {QStringLiteral("style:rel-column-width"),
                QString::number(std::max(1, static_cast<int>(std::lround(share * 10000)))) + QLatin1Char('*')}}}});
        x.w->writeEmptyElement("table:table-column");
        x.w->writeAttribute("table:style-name", cs);
    }
}

QString odt_cell_style(OdtCtx& x, const QString& background, const QString& top_border, const QString& border,
                       const QString& padding) {
    OdtAttrs cp{{QStringLiteral("fo:padding"), padding},
                {QStringLiteral("fo:border-left"), border},
                {QStringLiteral("fo:border-right"), border},
                {QStringLiteral("fo:border-top"), top_border.isEmpty() ? border : top_border},
                {QStringLiteral("fo:border-bottom"), border}};
    if (!background.isEmpty())
        cp.append({QStringLiteral("fo:background-color"), background});
    return x.styles->add(QStringLiteral("Ce"), QStringLiteral("table-cell"), {},
                         {{QStringLiteral("style:table-cell-properties"), cp}});
}

void odt_start_cell(OdtCtx& x, const QString& style) {
    x.w->writeStartElement("table:table-cell");
    x.w->writeAttribute("table:style-name", style);
    x.w->writeAttribute("office:value-type", "string");
}

/// The formatted table as printed: widths, header rows (repeated on every page when
/// header_repeat), alignment, header / zebra / colour-scale backgrounds, totals row.
void odt_grid(OdtCtx& x, int id, const tf::FormattedTable& t) {
    QXmlStreamWriter& w = *x.w;
    const ReportTheme& th = x.th();
    const int cols = std::max(1, t.columns);
    const int rows = static_cast<int>(t.rows.size());
    QVector<double> widths = t.widths.size() == cols ? t.widths : tf::default_widths(cols);
    const double cell_pt = rows > 30 || cols > 7 ? 9 : 10;
    const QString border = QStringLiteral("0.45pt solid ") + odt_color(th.divider_color, "#cccccc");
    const QString total_rule = QStringLiteral("1pt solid ") + x.ink();
    const QString zebra = odt_blend(odt_color(th.page_bg, "#ffffff"), x.ink(), 0.035);
    const QString header_bg = odt_color(th.table_header_bg, "#f0f0f0");
    const QString header_fg = odt_color(th.table_header_fg, "#1a1a1a");

    const QString ts = odt_table_style(x, x.content_mm,
                                       {{QStringLiteral("fo:margin-top"), odt_px(2)}, {QStringLiteral("fo:margin-bottom"), odt_px(2)}});
    w.writeStartElement("table:table");
    w.writeAttribute("table:name", QStringLiteral("Table%1").arg(id));
    w.writeAttribute("table:style-name", ts);
    odt_columns(x, widths, x.content_mm);
    for (int r = 0; r < rows; ++r) {
        const bool header = r < t.header_rows;
        const bool group = header && t.header_repeat;
        if (group && r == 0)
            w.writeStartElement("table:table-header-rows");
        w.writeStartElement("table:table-row");
        const QVector<tf::Cell>& row = t.rows[r];
        const bool total_row = r == t.totals_row;
        const bool zebra_row = t.zebra && !header && !total_row && (r - t.header_rows) % 2 == 1;
        for (int c = 0; c < cols; ++c) {
            const tf::Cell cell = c < row.size() ? row[c] : tf::Cell();
            const bool total = total_row || cell.total;
            QString bg;
            if (header)
                bg = header_bg;
            else if (!cell.background.isEmpty())
                bg = odt_color(cell.background, "#ffffff");
            else if (zebra_row)
                bg = zebra;
            odt_start_cell(x, odt_cell_style(x, bg, total ? total_rule : QString(), border, QStringLiteral("3pt")));
            odt_para(x,
                     {header ? QStringLiteral("Table_20_Heading") : QStringLiteral("Table_20_Contents"),
                      {{QStringLiteral("fo:text-align"), odt_align(cell.align)}},
                      odt_font(x.pt(cell_pt), header ? header_fg : x.ink(), header || total)},
                     cell.text);
            w.writeEndElement(); // cell
        }
        w.writeEndElement(); // row
        if (group && r == t.header_rows - 1)
            w.writeEndElement(); // header rows
    }
    w.writeEndElement(); // table
}

// ── Blocks ──────────────────────────────────────────────────────────────────

void odt_doc_header(OdtCtx& x) {
    const ReportMetadata& m = x.doc->metadata;
    odt_para(x, {QStringLiteral("Title"), {}, {}}, vars::expand(m.title, x.vars));
    QStringList parts;
    for (const QString& s : {m.author, m.company, m.date})
        if (!s.trimmed().isEmpty())
            parts << vars::expand(s, x.vars);
    odt_para(x, {QStringLiteral("Subtitle"), {}, {}}, parts.join(QStringLiteral("  ·  ")));
}

void odt_heading(OdtCtx& x, const ReportComponent& c) {
    outline::Heading h;
    const int idx = x.heading_index.value(c.id, -1);
    if (idx >= 0) {
        h = x.headings[idx];
    } else {
        h.level = std::clamp(c.config.value(QStringLiteral("level"), QStringLiteral("1")).toInt(), 1, 3);
        h.text = c.content;
    }
    odt_start_p(x, {QStringLiteral("Heading_20_%1").arg(h.level), {}, {}}, h.level);
    odt_chars(x, outline::heading_label(h));
    odt_end_p(x);
}

void odt_text_block(OdtCtx& x, const ReportComponent& c) {
    if (c.config.value(QStringLiteral("style")) == QLatin1String("caption")) {
        odt_para(x, {QStringLiteral("Caption"), {}, {}}, odt_chop_newlines(c.content));
        return;
    }
    if (ef::text_is_markdown(c)) {
        odt_markdown(x, c.content, {QStringLiteral("Text_20_body"), {}, {}});
        return;
    }
    // Plain text: one paragraph per line, as the page prints it.
    for (const QString& line : odt_chop_newlines(c.content).split(QLatin1Char('\n')))
        odt_para(x, {QStringLiteral("Text_20_body"), {}, {}}, line);
}

void odt_table_block(OdtCtx& x, const ReportComponent& c) {
    const QString title = c.config.value(QStringLiteral("title")).trimmed();
    if (!title.isEmpty())
        odt_para(x, {QStringLiteral("Report_20_Table_20_Title"), {}, {}}, title);
    const ef::PrintedTable pt = ef::printed_table(c, *x.assets, odt_color(x.th().page_bg, "#ffffff"));
    odt_grid(x, c.id, pt.table);
    QStringList notes;
    const QString note = c.config.value(QStringLiteral("note")).trimmed();
    if (!note.isEmpty())
        notes << note;
    if (!pt.truncated.isEmpty())
        notes << pt.truncated;
    if (!notes.isEmpty())
        odt_para(x, {QStringLiteral("Report_20_Note"), {}, {}}, notes.join(QLatin1Char(' ')));
}

void odt_chart(OdtCtx& x, const ReportComponent& c) {
    const auto it = x.assets->charts.constFind(c.id);
    if (it == x.assets->charts.cend())
        return;
    const QString title = c.config.value(QStringLiteral("title"), QStringLiteral("Chart"));
    odt_start_p(x, {QStringLiteral("Report_20_Figure"), {}, {}});
    odt_frame(x, QStringLiteral("Chart"), c.id, *it, odt_display_px(c, *it, 640, 160, x.content_px(), 0.5), x.content_mm,
              title, false);
    odt_end_p(x);
    const QString caption = c.config.value(QStringLiteral("caption")).trimmed();
    if (!caption.isEmpty())
        odt_para(x, {QStringLiteral("Caption"), {{QStringLiteral("fo:text-align"), QStringLiteral("center")}}, {}}, caption);
    if (c.config.value(QStringLiteral("sample")) == QLatin1String("1"))
        // Template placeholder numbers must never pass for real data.
        odt_para(x,
                 {QStringLiteral("Standard"),
                  {{QStringLiteral("fo:text-align"), QStringLiteral("center")}},
                  odt_font(x.pt(9), QString::fromLatin1(kOdtRed), true)},
                 QStringLiteral("SAMPLE DATA — placeholder values from the template, replace before publishing"));
}

void odt_sparkline(OdtCtx& x, const ReportComponent& c) {
    const QString title = c.config.value(QStringLiteral("title"));
    const QString current = c.config.value(QStringLiteral("current"));
    const QString change = c.config.value(QStringLiteral("change_pct"));
    odt_start_p(x, {QStringLiteral("Standard"),
                    {{QStringLiteral("fo:margin-top"), odt_px(3)}, {QStringLiteral("fo:margin-bottom"), odt_px(2)}},
                    {}});
    if (!title.isEmpty())
        odt_span(x, title + QStringLiteral("  "), odt_font(x.pt(11), x.ink(), true));
    if (!current.isEmpty())
        odt_span(x, current + QStringLiteral("  "), odt_font(x.pt(11), x.accent()));
    if (!change.isEmpty())
        odt_span(x, change + (change.endsWith(QLatin1Char('%')) ? QString() : QStringLiteral("%")) + QStringLiteral("  "),
                 odt_font(x.pt(10), QString::fromLatin1(change.startsWith(QLatin1Char('-')) ? kOdtRed : kOdtGreen)));
    const auto it = x.assets->charts.constFind(c.id);
    if (it != x.assets->charts.cend() && !it->bytes.isEmpty()) {
        const QSizeF px = it->display_px.width() > 0 && it->display_px.height() > 0 ? it->display_px : QSizeF(160, 40);
        odt_frame(x, QStringLiteral("Sparkline"), c.id, *it, px, x.content_mm, title, true);
    }
    odt_end_p(x);
}

void odt_image(OdtCtx& x, const ReportComponent& c) {
    const auto it = x.assets->images.constFind(c.id);
    if (it == x.assets->images.cend())
        return;
    const QString align = c.config.value(QStringLiteral("align"), QStringLiteral("left"));
    const QString al = align == QLatin1String("center")  ? QStringLiteral("center")
                       : align == QLatin1String("right") ? QStringLiteral("end")
                                                         : QStringLiteral("start");
    const QString caption = c.config.value(QStringLiteral("caption")).trimmed();
    odt_start_p(x, {QStringLiteral("Report_20_Figure"),
                    {{QStringLiteral("fo:text-align"), al},
                     {QStringLiteral("fo:margin-top"), odt_px(4)},
                     {QStringLiteral("fo:margin-bottom"), odt_px(2)}},
                    {}});
    odt_frame(x, QStringLiteral("Image"), c.id, *it, odt_display_px(c, *it, 400, 40, x.content_px(), 0.75), x.content_mm,
              caption, false);
    odt_end_p(x);
    if (!caption.isEmpty())
        odt_para(x,
                 {QStringLiteral("Caption"),
                  {{QStringLiteral("fo:text-align"), al}, {QStringLiteral("fo:margin-bottom"), odt_px(6)}},
                  {}},
                 caption);
}

void odt_stats(OdtCtx& x, const ReportComponent& c) {
    QXmlStreamWriter& w = *x.w;
    odt_para(x, {QStringLiteral("Report_20_Block_20_Title"), {}, {}},
             c.config.value(QStringLiteral("title"), QStringLiteral("Key Statistics")));
    const QStringList lines = c.config.value(QStringLiteral("data")).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    const QStringList tones = c.config.value(QStringLiteral("tones")).split(QLatin1Char(','));
    const QString border = QStringLiteral("0.45pt solid ") + odt_color(x.th().divider_color, "#cccccc");
    const QString ts = odt_table_style(x, x.content_mm,
                                       {{QStringLiteral("fo:margin-top"), odt_px(2)}, {QStringLiteral("fo:margin-bottom"), odt_px(4)}});
    w.writeStartElement("table:table");
    w.writeAttribute("table:name", QStringLiteral("Stats%1").arg(c.id));
    w.writeAttribute("table:style-name", ts);
    odt_columns(x, {25, 25, 25, 25}, x.content_mm);
    const QString cs = odt_cell_style(x, {}, {}, border, QStringLiteral("3pt"));
    const int rows = (static_cast<int>(lines.size()) + 1) / 2;
    for (int r = 0; r < rows; ++r) {
        w.writeStartElement("table:table-row");
        for (int k = 0; k < 2; ++k) {
            const int li = r * 2 + k;
            QString label, value;
            QString ink = x.ink();
            if (li < lines.size()) {
                const int colon = static_cast<int>(lines[li].indexOf(QLatin1Char(':')));
                if (colon >= 0) {
                    label = lines[li].left(colon).trimmed();
                    value = lines[li].mid(colon + 1).trimmed();
                } else {
                    label = lines[li].trimmed();
                }
                const QString tone = tones.value(li).trimmed();
                if (tone == QLatin1String("positive"))
                    ink = QString::fromLatin1(kOdtGreen);
                else if (tone == QLatin1String("negative"))
                    ink = QString::fromLatin1(kOdtRed);
                else if (tone == QLatin1String("warning"))
                    ink = QString::fromLatin1(kOdtAmber);
            }
            odt_start_cell(x, cs);
            odt_para(x, {QStringLiteral("Table_20_Contents"), {}, odt_font(x.pt(9.5), x.meta_ink())}, label);
            w.writeEndElement();
            odt_start_cell(x, cs);
            odt_para(x,
                     {QStringLiteral("Table_20_Contents"),
                      {{QStringLiteral("fo:text-align"), QStringLiteral("end")}},
                      odt_font(x.pt(10.5), ink, true)},
                     value);
            w.writeEndElement();
        }
        w.writeEndElement(); // row
    }
    w.writeEndElement(); // table
}

void odt_market(OdtCtx& x, const ReportComponent& c) {
    const QString symbol = c.config.value(QStringLiteral("symbol"));
    const QString price = c.config.value(QStringLiteral("price"));
    const QString chg = c.config.value(QStringLiteral("change_pct"));
    const QString name = c.config.value(QStringLiteral("name"), symbol);
    const QString high = c.config.value(QStringLiteral("high"));
    const QString low = c.config.value(QStringLiteral("low"));
    const QString volume = c.config.value(QStringLiteral("volume"));
    const QString ccy = c.config.value(QStringLiteral("currency"));
    const OdtPara p{QStringLiteral("Standard"),
                    {{QStringLiteral("fo:background-color"), odt_color(x.th().quote_bg, "#f9f9f9")},
                     {QStringLiteral("fo:margin-left"), odt_px(10)},
                     {QStringLiteral("fo:margin-top"), odt_px(4)},
                     {QStringLiteral("fo:margin-bottom"), odt_px(2)}},
                    {}};
    odt_start_p(x, p);
    odt_span(x, symbol + QStringLiteral("  "), odt_font(x.pt(13), x.accent(), true));
    if (!name.isEmpty() && name != symbol)
        odt_span(x, name + QStringLiteral("  "), odt_font(x.pt(11), x.meta_ink()));
    odt_span(x, price + (ccy.isEmpty() ? QString() : QLatin1Char(' ') + ccy) + QStringLiteral("  "),
             odt_font(x.pt(13), x.ink(), true));
    if (!chg.isEmpty()) {
        const bool negative = chg.startsWith(QLatin1Char('-'));
        const QString sign = (!negative && !chg.startsWith(QLatin1Char('+'))) ? QStringLiteral("+") : QString();
        odt_span(x, sign + chg + QLatin1Char('%'),
                 odt_font(x.pt(12), QString::fromLatin1(negative ? kOdtRed : kOdtGreen), true));
    }
    odt_end_p(x);
    if (high.isEmpty() && low.isEmpty() && volume.isEmpty())
        return;
    QString detail;
    if (!high.isEmpty() && !low.isEmpty())
        detail += QStringLiteral("H: ") + high + QStringLiteral("  L: ") + low + QStringLiteral("  ");
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
        detail += QStringLiteral("Vol: ") + vs;
    }
    odt_start_p(x, p);
    odt_span(x, detail, odt_font(x.pt(10), x.meta_ink()));
    odt_end_p(x);
}

/// A boxed note: one shaded table cell (a box that holds several paragraphs in Word and
/// LibreOffice alike) with the renderer's fill, coloured heading line and body.
void odt_callout(OdtCtx& x, const ReportComponent& c) {
    QXmlStreamWriter& w = *x.w;
    const QString style = c.config.value(QStringLiteral("style"), QStringLiteral("info"));
    const QString heading = c.config.value(QStringLiteral("heading"));
    const bool dark = odt_lightness(x.th().page_bg) < 100;
    QString bg, ink, icon;
    if (style == QLatin1String("success")) {
        bg = dark ? QStringLiteral("#052e16") : QStringLiteral("#f0fdf4");
        ink = QString::fromLatin1(kOdtGreen);
        icon = QStringLiteral("✓");
    } else if (style == QLatin1String("warning")) {
        bg = dark ? QStringLiteral("#1c1506") : QStringLiteral("#fffbeb");
        ink = QString::fromLatin1(kOdtAmber);
        icon = QStringLiteral("⚠");
    } else if (style == QLatin1String("danger")) {
        bg = dark ? QStringLiteral("#1f0505") : QStringLiteral("#fef2f2");
        ink = QString::fromLatin1(kOdtRed);
        icon = QStringLiteral("✗");
    } else {
        bg = dark ? QStringLiteral("#0c1a33") : QStringLiteral("#eff6ff");
        ink = QString::fromLatin1(kOdtBlue);
        icon = QStringLiteral("ℹ");
    }
    const double width = std::max(20.0, x.content_mm - 22 * kOdtPxMm);
    const QString ts = odt_table_style(x, width,
                                       {{QStringLiteral("fo:margin-left"), odt_px(14)},
                                        {QStringLiteral("fo:margin-right"), odt_px(8)},
                                        {QStringLiteral("fo:margin-top"), odt_px(6)},
                                        {QStringLiteral("fo:margin-bottom"), odt_px(6)}});
    w.writeStartElement("table:table");
    w.writeAttribute("table:name", QStringLiteral("Callout%1").arg(c.id));
    w.writeAttribute("table:style-name", ts);
    odt_columns(x, {100}, width);
    w.writeStartElement("table:table-row");
    odt_start_cell(x, odt_cell_style(x, bg, QStringLiteral("none"), QStringLiteral("none"), QStringLiteral("4pt")));
    const OdtPara body_p{QStringLiteral("Standard"), {}, odt_font(x.pt(11), x.ink())};
    if (!heading.isEmpty())
        odt_para(x, {QStringLiteral("Standard"), {}, odt_font(x.pt(12), ink, true)}, icon + QStringLiteral("  ") + heading);
    else
        x.prefix.append({icon + QStringLiteral("  "), odt_font(x.pt(12), ink)});
    // An empty body prints nothing (the page view's "Enter callout text..." is an editing hint).
    const QString body = odt_chop_newlines(c.content);
    const bool markdown = body.contains(QLatin1String("**")) || body.contains(QLatin1String("__")) ||
                          body.contains(QLatin1String("\n- "));
    if (!body.trimmed().isEmpty() && markdown) {
        odt_markdown(x, body, body_p);
    } else if (!body.trimmed().isEmpty()) {
        for (const QString& line : body.split(QLatin1Char('\n')))
            odt_para(x, body_p, line);
    }
    if (!x.prefix.isEmpty()) { // nothing took the icon
        odt_start_p(x, body_p);
        odt_end_p(x);
    }
    w.writeEndElement(); // cell
    w.writeEndElement(); // row
    w.writeEndElement(); // table
}

void odt_quote(OdtCtx& x, const ReportComponent& c) {
    const QString body = odt_chop_newlines(c.content);
    odt_start_p(x, {QStringLiteral("Quotations"), {}, {}});
    if (body.contains(QLatin1String("**")) || body.contains(QLatin1String("__")))
        odt_runs(x, odt_inline(body));
    else
        odt_chars(x, body);
    odt_end_p(x);
}

void odt_list(OdtCtx& x, const ReportComponent& c) {
    QXmlStreamWriter& w = *x.w;
    w.writeStartElement("text:list");
    w.writeAttribute("text:style-name", "List_20_Bullet");
    for (const QString& item : c.content.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        w.writeStartElement("text:list-item");
        odt_start_p(x, {QStringLiteral("Report_20_List"), {}, {}});
        const QString text = QString(item).remove(QLatin1Char('\r'));
        if (text.contains(QLatin1String("**")) || text.contains(QLatin1String("__")))
            odt_runs(x, odt_inline(text));
        else
            odt_chars(x, text);
        odt_end_p(x);
        w.writeEndElement();
    }
    w.writeEndElement();
}

void odt_toc(OdtCtx& x) {
    odt_para(x, {QStringLiteral("Contents_20_Heading"), {}, {}}, QStringLiteral("Table of Contents"));
    if (x.headings.isEmpty()) {
        odt_para(x, {QStringLiteral("Standard"), {}, odt_font(x.pt(11), x.meta_ink(), false, true)},
                 QStringLiteral("  (No headings found in document)"));
        return;
    }
    for (int i = 0; i < x.headings.size(); ++i) {
        const outline::Heading& h = x.headings[i];
        odt_start_p(x, {QStringLiteral("Contents_20_%1").arg(h.level), {}, {}});
        const QString number = h.number.isEmpty() ? QString::number(i + 1) + QLatin1Char('.') : h.number;
        odt_span(x, number + QStringLiteral("  "), odt_font(x.pt(11), x.accent(), true));
        odt_chars(x, h.text);
        odt_end_p(x);
    }
}

void odt_cover(OdtCtx& x, const ReportComponent& c) {
    const ReportMetadata& m = x.doc->metadata;
    const auto cfg = [&c](const char* key) { return c.config.value(QString::fromLatin1(key)).trimmed(); };
    const auto logo = x.assets->images.constFind(c.id);
    const bool has_logo = logo != x.assets->images.cend() && !logo->bytes.isEmpty();
    if (has_logo) {
        odt_start_p(x, {QStringLiteral("Report_20_Cover_20_Logo"), {}, {}});
        QSizeF px = logo->display_px;
        if (px.width() <= 0 || px.height() <= 0)
            px = logo->pixels.width() > 0 && logo->pixels.height() > 0 ? QSizeF(logo->pixels) : QSizeF(227, 76);
        odt_frame(x, QStringLiteral("Logo"), c.id, *logo, px, 60.0, QStringLiteral("Logo"), false);
        odt_end_p(x);
    }
    const QString title = cfg("title").isEmpty() ? vars::expand(m.title, x.vars) : cfg("title");
    OdtPara title_p{QStringLiteral("Report_20_Cover_20_Title"), {}, {}};
    if (has_logo)
        title_p.pp.append({QStringLiteral("fo:margin-top"), QStringLiteral("10mm")});
    odt_para(x, title_p, title);
    if (!cfg("subtitle").isEmpty())
        odt_para(x, {QStringLiteral("Report_20_Cover_20_Subtitle"), {}, {}}, cfg("subtitle"));
    QString byline = cfg("byline");
    if (byline.isEmpty()) {
        QStringList parts;
        for (const QString& s : {m.author, m.company})
            if (!s.trimmed().isEmpty())
                parts << vars::expand(s.trimmed(), x.vars);
        byline = parts.join(QStringLiteral(" · "));
    }
    if (!byline.isEmpty())
        odt_para(x, {QStringLiteral("Report_20_Cover_20_Byline"), {}, {}}, byline);
    const QString date = cfg("date").isEmpty() ? m.date.trimmed() : cfg("date");
    if (!date.isEmpty())
        odt_para(x, {QStringLiteral("Report_20_Cover_20_Byline"), {}, {}}, date);
    if (!cfg("disclaimer").isEmpty())
        odt_para(x, {QStringLiteral("Report_20_Cover_20_Disclaimer"), {}, {}}, cfg("disclaimer"));
    x.pending_break = true; // the report starts on the next page
    x.pending_soft = true;
}

void odt_provenance(OdtCtx& x, const ReportComponent& c) {
    const QString line = ef::provenance_text(c);
    if (line.isEmpty())
        return;
    OdtPara p{QStringLiteral("Report_20_Source"), {}, {}};
    if (c.type == QLatin1String("chart") || c.type == QLatin1String("image"))
        p.pp.append({QStringLiteral("fo:text-align"), QStringLiteral("center")});
    odt_para(x, p, line);
}

void odt_block(OdtCtx& x, const ReportComponent& c) {
    const QString& t = c.type;
    if (t == QLatin1String("page_break")) {
        if (x.pending_break && !x.pending_soft) { // two breaks in a row: an empty page between
            odt_start_p(x, {QStringLiteral("Standard"), {}, {}});
            odt_end_p(x);
        }
        x.pending_break = true;
        x.pending_soft = false;
        return;
    }
    x.block_bold = c.config.value(QStringLiteral("bold")) == QLatin1String("1");
    x.block_italic = c.config.value(QStringLiteral("italic")) == QLatin1String("1");
    if (t == QLatin1String("heading"))
        odt_heading(x, c);
    else if (t == QLatin1String("text"))
        odt_text_block(x, c);
    else if (t == QLatin1String("table"))
        odt_table_block(x, c);
    else if (t == QLatin1String("chart"))
        odt_chart(x, c);
    else if (t == QLatin1String("sparkline"))
        odt_sparkline(x, c);
    else if (t == QLatin1String("image"))
        odt_image(x, c);
    else if (t == QLatin1String("stats_block"))
        odt_stats(x, c);
    else if (t == QLatin1String("market_data"))
        odt_market(x, c);
    else if (t == QLatin1String("callout"))
        odt_callout(x, c);
    else if (t == QLatin1String("quote"))
        odt_quote(x, c);
    else if (t == QLatin1String("code"))
        odt_para(x, {QStringLiteral("Preformatted_20_Text"), {}, {}}, odt_chop_newlines(c.content));
    else if (t == QLatin1String("list"))
        odt_list(x, c);
    else if (t == QLatin1String("divider"))
        odt_para(x, {QStringLiteral("Report_20_Divider"), {}, {}}, {});
    else if (t == QLatin1String("toc"))
        odt_toc(x);
    else if (t == QLatin1String("cover"))
        odt_cover(x, c);
    if (ef::has_provenance(t))
        odt_provenance(x, c);
    x.block_bold = false;
    x.block_italic = false;
}

// ── styles.xml ──────────────────────────────────────────────────────────────

void odt_named(QXmlStreamWriter& w, const QString& name, const QString& display, const QString& family,
               const QString& parent, const OdtAttrs& pp, const OdtAttrs& tp, const OdtAttrs& extra = {}) {
    w.writeStartElement("style:style");
    w.writeAttribute("style:name", name);
    if (!display.isEmpty())
        w.writeAttribute("style:display-name", display);
    w.writeAttribute("style:family", family);
    if (!parent.isEmpty())
        w.writeAttribute("style:parent-style-name", parent);
    odt_attrs(w, extra);
    if (!pp.isEmpty()) {
        w.writeEmptyElement("style:paragraph-properties");
        odt_attrs(w, pp);
    }
    if (!tp.isEmpty()) {
        w.writeEmptyElement("style:text-properties");
        odt_attrs(w, tp);
    }
    w.writeEndElement();
}

void odt_list_style(QXmlStreamWriter& w, const QString& name, const QString& display, bool numbered) {
    static const char* const kBullets[] = {"•", "◦", "▪", "•", "◦"};
    w.writeStartElement("text:list-style");
    w.writeAttribute("style:name", name);
    w.writeAttribute("style:display-name", display);
    for (int level = 1; level <= 5; ++level) {
        if (numbered) {
            w.writeStartElement("text:list-level-style-number");
            w.writeAttribute("text:level", QString::number(level));
            w.writeAttribute("style:num-suffix", ".");
            w.writeAttribute("style:num-format", "1");
        } else {
            w.writeStartElement("text:list-level-style-bullet");
            w.writeAttribute("text:level", QString::number(level));
            w.writeAttribute("text:bullet-char", QString::fromUtf8(kBullets[level - 1]));
        }
        w.writeStartElement("style:list-level-properties");
        w.writeAttribute("text:list-level-position-and-space-mode", "label-alignment");
        w.writeEmptyElement("style:list-level-label-alignment");
        w.writeAttribute("text:label-followed-by", "listtab");
        w.writeAttribute("text:list-tab-stop-position", odt_mm(6.0 * level));
        w.writeAttribute("fo:text-indent", odt_mm(-4.5));
        w.writeAttribute("fo:margin-left", odt_mm(6.0 * level));
        w.writeEndElement(); // list-level-properties
        w.writeEndElement(); // list-level-style
    }
    w.writeEndElement();
}

/// One running header / footer slot: {{variables}}, {title} {date} {author} {company}
/// replaced; {page} / {total} as live page-number / page-count fields.
void odt_band_text(OdtCtx& hx, QString text, const ReportMetadata& m) {
    text = vars::expand(text, hx.vars);
    text.replace(QStringLiteral("{title}"), vars::expand(m.title, hx.vars));
    text.replace(QStringLiteral("{date}"), m.date);
    text.replace(QStringLiteral("{author}"), m.author);
    text.replace(QStringLiteral("{company}"), m.company);
    static const QRegularExpression field_re(QStringLiteral("\\{(page|total)\\}"));
    QXmlStreamWriter& w = *hx.w;
    qsizetype last = 0;
    auto it = field_re.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch fm = it.next();
        odt_chars(hx, text.mid(last, fm.capturedStart() - last));
        if (fm.captured(1) == QLatin1String("page")) {
            w.writeStartElement("text:page-number");
            w.writeAttribute("text:select-page", "current");
        } else {
            w.writeStartElement("text:page-count");
        }
        w.writeCharacters(QStringLiteral("1"));
        w.writeEndElement();
        hx.fresh = false;
        last = fm.capturedEnd();
    }
    odt_chars(hx, text.mid(last));
}

/// left <tab> centre <tab> right (+ the watermark at the right of the header).
void odt_band(OdtCtx& hx, const QString& style, const QString& left, const QString& center, const QString& right,
              const QString& watermark, const ReportMetadata& m) {
    QXmlStreamWriter& w = *hx.w;
    w.writeStartElement("text:p");
    w.writeAttribute("text:style-name", style);
    hx.fresh = true;
    odt_band_text(hx, left, m);
    if (!center.isEmpty() || !right.isEmpty() || !watermark.isEmpty()) {
        w.writeEmptyElement("text:tab");
        hx.fresh = false;
        odt_band_text(hx, center, m);
    }
    if (!right.isEmpty() || !watermark.isEmpty()) {
        w.writeEmptyElement("text:tab");
        hx.fresh = false;
        odt_band_text(hx, right, m);
        if (!watermark.isEmpty()) {
            if (!right.isEmpty())
                odt_chars(hx, QStringLiteral("  "));
            w.writeStartElement("text:span");
            w.writeAttribute("text:style-name", "Report_20_Watermark");
            odt_chars(hx, watermark);
            w.writeEndElement();
        }
    }
    w.writeEndElement();
}

void odt_tab_stops(QXmlStreamWriter& w, double width_mm) {
    w.writeStartElement("style:tab-stops");
    w.writeEmptyElement("style:tab-stop");
    w.writeAttribute("style:position", odt_mm(width_mm / 2));
    w.writeAttribute("style:type", "center");
    w.writeEmptyElement("style:tab-stop");
    w.writeAttribute("style:position", odt_mm(width_mm));
    w.writeAttribute("style:type", "right");
    w.writeEndElement();
}

QByteArray odt_styles_xml(const OdtCtx& x) {
    const ReportMetadata& m = x.doc->metadata;
    const ReportTheme& th = x.th();
    const QString ink = x.ink(), meta = x.meta_ink(), head = x.heading_ink(), accent = x.accent();
    const QString paper = odt_color(th.page_bg, "#ffffff");
    const QString divider = odt_color(th.divider_color, "#cccccc");
    const bool dark_paper = odt_lightness(paper) < 128;
    // Header / footer: 8 pt (scaled, at most 1.5x) in grey, as paint_page_furniture draws them.
    const double band_pt =
        std::max(8, static_cast<int>(std::lround(8.0 * 96.0 / 72.0 * std::min(x.scale, 1.5)))) * 72.0 / 96.0;
    const QString band_ink = dark_paper ? QStringLiteral("#888888") : QStringLiteral("#666666");
    const QString band_rule = QStringLiteral("0.75pt solid ") + (dark_paper ? QStringLiteral("#444444") : QStringLiteral("#cccccc"));
    const QString accent_rule = QStringLiteral("0.75pt solid ") + accent;
    auto pt = [&x](double v) { return x.pt(v); };
    auto kv = [](const char* k, const QString& v) { return QPair<QString, QString>(QString::fromLatin1(k), v); };

    const QString watermark = vars::expand(m.watermark, x.vars).trimmed();
    const QString confidentiality = vars::expand(m.confidentiality, x.vars).trimmed();
    const bool has_header = !m.header_left.isEmpty() || !m.header_center.isEmpty() || !m.header_right.isEmpty() ||
                            !watermark.isEmpty();
    const bool has_footer = !m.footer_left.isEmpty() || !m.footer_center.isEmpty() || !m.footer_right.isEmpty() ||
                            m.show_page_numbers || !confidentiality.isEmpty();

    QByteArray out;
    QXmlStreamWriter w(&out);
    w.setAutoFormatting(false);
    w.writeStartDocument();
    w.writeStartElement("office:document-styles");
    odt_namespaces(w);
    odt_font_decls(w, x.font);

    w.writeStartElement("office:styles");
    w.writeStartElement("style:default-style");
    w.writeAttribute("style:family", "paragraph");
    w.writeEmptyElement("style:paragraph-properties");
    w.writeAttribute("style:writing-mode", "page");
    w.writeEmptyElement("style:text-properties");
    w.writeAttribute("style:font-name", odt_clean(x.font));
    w.writeAttribute("style:font-name-asian", odt_clean(x.font));
    w.writeAttribute("style:font-name-complex", odt_clean(x.font));
    odt_attrs(w, odt_font(pt(12), ink));
    w.writeEndElement();
    w.writeStartElement("style:default-style");
    w.writeAttribute("style:family", "table");
    w.writeEmptyElement("style:table-properties");
    w.writeAttribute("table:border-model", "collapsing");
    w.writeEndElement();

    const OdtAttrs cls_text{kv("style:class", QStringLiteral("text"))};
    odt_named(w, QStringLiteral("Standard"), {}, QStringLiteral("paragraph"), {}, {}, {}, cls_text);
    odt_named(w, QStringLiteral("Text_20_body"), QStringLiteral("Text body"), QStringLiteral("paragraph"), QStringLiteral("Standard"),
              {kv("fo:margin-top", odt_px(2)), kv("fo:margin-bottom", odt_px(8)), kv("fo:line-height", QStringLiteral("140%"))}, {},
              cls_text);
    odt_named(w, QStringLiteral("Heading"), {}, QStringLiteral("paragraph"), QStringLiteral("Standard"),
              {kv("fo:keep-with-next", QStringLiteral("always"))}, odt_font(0, head, true),
              {kv("style:next-style-name", QStringLiteral("Text_20_body")), kv("style:class", QStringLiteral("text"))});
    const double heading_pt[] = {18, 15, 13};
    const double heading_top[] = {18, 12, 8};
    const double heading_bottom[] = {2, 6, 6};
    for (int level = 1; level <= 3; ++level) {
        OdtAttrs pp{kv("fo:margin-top", odt_px(heading_top[level - 1])), kv("fo:margin-bottom", odt_px(heading_bottom[level - 1]))};
        if (level == 1) { // the accent rule the page draws under a level-1 heading
            odt_set(pp, QStringLiteral("fo:margin-bottom"), odt_px(10));
            pp.append(kv("fo:padding-bottom", odt_px(2)));
            pp.append(kv("fo:border-bottom", accent_rule));
        }
        odt_named(w, QStringLiteral("Heading_20_%1").arg(level), QStringLiteral("Heading %1").arg(level), QStringLiteral("paragraph"),
                  QStringLiteral("Heading"), pp, odt_font(pt(heading_pt[level - 1]), head, true),
                  {kv("style:default-outline-level", QString::number(level)), kv("style:next-style-name", QStringLiteral("Text_20_body")),
                   kv("style:class", QStringLiteral("text"))});
    }
    odt_named(w, QStringLiteral("Title"), {}, QStringLiteral("paragraph"), QStringLiteral("Standard"),
              {kv("fo:margin-top", QStringLiteral("0mm")), kv("fo:margin-bottom", odt_px(2))}, odt_font(pt(24), head, true),
              {kv("style:class", QStringLiteral("chapter"))});
    odt_named(w, QStringLiteral("Subtitle"), {}, QStringLiteral("paragraph"), QStringLiteral("Standard"),
              {kv("fo:margin-top", odt_px(2)), kv("fo:margin-bottom", odt_px(12)), kv("fo:padding-bottom", odt_px(6)),
               kv("fo:border-bottom", accent_rule)},
              odt_font(pt(10), meta), {kv("style:class", QStringLiteral("chapter"))});
    odt_named(w, QStringLiteral("Caption"), {}, QStringLiteral("paragraph"), QStringLiteral("Standard"),
              {kv("fo:margin-top", odt_px(2)), kv("fo:margin-bottom", odt_px(6)), kv("fo:line-height", QStringLiteral("120%"))},
              odt_font(pt(9), meta, false, true), {kv("style:class", QStringLiteral("extra"))});
    odt_named(w, QStringLiteral("Report_20_Source"), QStringLiteral("Report Source"), QStringLiteral("paragraph"), QStringLiteral("Standard"),
              {kv("fo:margin-top", odt_px(1)), kv("fo:margin-bottom", odt_px(6))}, odt_font(pt(8), meta));
    odt_named(w, QStringLiteral("Report_20_Note"), QStringLiteral("Report Note"), QStringLiteral("paragraph"), QStringLiteral("Standard"),
              {kv("fo:margin-top", odt_px(2)), kv("fo:margin-bottom", odt_px(6))}, odt_font(pt(8), meta, false, true));
    odt_named(w, QStringLiteral("Report_20_Table_20_Title"), QStringLiteral("Report Table Title"), QStringLiteral("paragraph"),
              QStringLiteral("Standard"),
              {kv("fo:margin-top", odt_px(4)), kv("fo:margin-bottom", odt_px(4)), kv("fo:keep-with-next", QStringLiteral("always"))},
              odt_font(pt(11), head, true));
    odt_named(w, QStringLiteral("Report_20_Block_20_Title"), QStringLiteral("Report Block Title"), QStringLiteral("paragraph"),
              QStringLiteral("Standard"),
              {kv("fo:margin-top", odt_px(4)), kv("fo:margin-bottom", odt_px(3)), kv("fo:keep-with-next", QStringLiteral("always"))},
              odt_font(pt(12), head, true));
    odt_named(w, QStringLiteral("Table_20_Contents"), QStringLiteral("Table Contents"), QStringLiteral("paragraph"),
              QStringLiteral("Standard"), {kv("fo:margin-top", QStringLiteral("0mm")), kv("fo:margin-bottom", QStringLiteral("0mm"))},
              odt_font(pt(10), ink), {kv("style:class", QStringLiteral("extra"))});
    odt_named(w, QStringLiteral("Table_20_Heading"), QStringLiteral("Table Heading"), QStringLiteral("paragraph"),
              QStringLiteral("Table_20_Contents"), {}, odt_font(0, odt_color(th.table_header_fg, "#1a1a1a"), true),
              {kv("style:class", QStringLiteral("extra"))});
    odt_named(w, QStringLiteral("Quotations"), {}, QStringLiteral("paragraph"), QStringLiteral("Standard"),
              {kv("fo:margin-left", odt_px(24)), kv("fo:margin-right", odt_px(8)), kv("fo:margin-top", odt_px(6)),
               kv("fo:margin-bottom", odt_px(8)), kv("fo:padding", QStringLiteral("2pt")),
               kv("fo:background-color", odt_color(th.quote_bg, "#f9f9f9"))},
              odt_font(pt(12), meta, false, true), {kv("style:class", QStringLiteral("html"))});
    OdtAttrs mono = odt_font(pt(10), ink);
    mono.prepend(kv("style:font-name", QString::fromLatin1(kOdtMono)));
    odt_named(w, QStringLiteral("Preformatted_20_Text"), QStringLiteral("Preformatted Text"), QStringLiteral("paragraph"),
              QStringLiteral("Standard"),
              {kv("fo:margin-left", odt_px(12)), kv("fo:margin-right", odt_px(12)), kv("fo:margin-top", odt_px(4)),
               kv("fo:margin-bottom", odt_px(8)), kv("fo:padding", QStringLiteral("3pt")),
               kv("fo:background-color", odt_color(th.code_bg, "#f5f5f5"))},
              mono, {kv("style:class", QStringLiteral("html"))});
    odt_named(w, QStringLiteral("Report_20_List"), QStringLiteral("Report List"), QStringLiteral("paragraph"), QStringLiteral("Standard"),
              {kv("fo:margin-top", odt_px(1)), kv("fo:margin-bottom", odt_px(1))}, odt_font(pt(11), ink),
              {kv("style:class", QStringLiteral("list"))});
    odt_named(w, QStringLiteral("Report_20_Figure"), QStringLiteral("Report Figure"), QStringLiteral("paragraph"), QStringLiteral("Standard"),
              {kv("fo:text-align", QStringLiteral("center")), kv("fo:margin-top", odt_px(6)), kv("fo:margin-bottom", odt_px(2))}, {});
    odt_named(w, QStringLiteral("Report_20_Divider"), QStringLiteral("Report Divider"), QStringLiteral("paragraph"), QStringLiteral("Standard"),
              {kv("fo:margin-top", odt_px(4)), kv("fo:margin-bottom", odt_px(6)), kv("fo:padding", QStringLiteral("0mm")),
               kv("fo:border-bottom", QStringLiteral("0.75pt solid ") + divider)},
              odt_font(4, {}));
    odt_named(w, QStringLiteral("Contents_20_Heading"), QStringLiteral("Contents Heading"), QStringLiteral("paragraph"),
              QStringLiteral("Standard"), {kv("fo:margin-top", odt_px(6)), kv("fo:margin-bottom", odt_px(4))},
              odt_font(pt(14), head, true), {kv("style:class", QStringLiteral("index"))});
    for (int level = 1; level <= 3; ++level)
        odt_named(w, QStringLiteral("Contents_20_%1").arg(level), QStringLiteral("Contents %1").arg(level), QStringLiteral("paragraph"),
                  QStringLiteral("Standard"),
                  {kv("fo:margin-left", odt_px(12 + 16 * (level - 1))), kv("fo:margin-top", odt_px(1)), kv("fo:margin-bottom", odt_px(1))},
                  odt_font(pt(11), ink), {kv("style:class", QStringLiteral("index"))});
    odt_named(w, QStringLiteral("Report_20_Cover_20_Logo"), QStringLiteral("Report Cover Logo"), QStringLiteral("paragraph"),
              QStringLiteral("Standard"), {kv("fo:text-align", QStringLiteral("center")), kv("fo:margin-top", QStringLiteral("20mm"))}, {});
    odt_named(w, QStringLiteral("Report_20_Cover_20_Title"), QStringLiteral("Report Cover Title"), QStringLiteral("paragraph"),
              QStringLiteral("Standard"),
              {kv("fo:text-align", QStringLiteral("center")), kv("fo:margin-top", QStringLiteral("50mm")), kv("fo:margin-bottom", QStringLiteral("4mm"))},
              odt_font(pt(26), head, true), {kv("style:class", QStringLiteral("chapter"))});
    odt_named(w, QStringLiteral("Report_20_Cover_20_Subtitle"), QStringLiteral("Report Cover Subtitle"), QStringLiteral("paragraph"),
              QStringLiteral("Standard"), {kv("fo:text-align", QStringLiteral("center")), kv("fo:margin-bottom", QStringLiteral("10mm"))},
              odt_font(pt(14), ink), {kv("style:class", QStringLiteral("chapter"))});
    odt_named(w, QStringLiteral("Report_20_Cover_20_Byline"), QStringLiteral("Report Cover Byline"), QStringLiteral("paragraph"),
              QStringLiteral("Standard"), {kv("fo:text-align", QStringLiteral("center")), kv("fo:margin-bottom", QStringLiteral("2mm"))},
              odt_font(pt(11), meta));
    odt_named(w, QStringLiteral("Report_20_Cover_20_Disclaimer"), QStringLiteral("Report Cover Disclaimer"), QStringLiteral("paragraph"),
              QStringLiteral("Standard"), {kv("fo:text-align", QStringLiteral("center")), kv("fo:margin-top", QStringLiteral("30mm"))},
              odt_font(pt(8), meta, false, true));

    for (const char* band : {"Header", "Footer"}) {
        w.writeStartElement("style:style");
        w.writeAttribute("style:name", QString::fromLatin1(band));
        w.writeAttribute("style:family", "paragraph");
        w.writeAttribute("style:parent-style-name", "Standard");
        w.writeAttribute("style:class", "extra");
        w.writeStartElement("style:paragraph-properties");
        odt_tab_stops(w, x.content_mm);
        w.writeEndElement();
        w.writeEmptyElement("style:text-properties");
        odt_attrs(w, odt_font(band_pt, band_ink));
        w.writeEndElement();
    }
    odt_named(w, QStringLiteral("Report_20_Confidentiality"), QStringLiteral("Report Confidentiality"), QStringLiteral("paragraph"),
              QStringLiteral("Footer"), {kv("fo:text-align", QStringLiteral("center")), kv("fo:margin-top", QStringLiteral("0.5mm"))},
              odt_font(band_pt * 0.9, {}, false, true), {kv("style:class", QStringLiteral("extra"))});

    odt_named(w, QStringLiteral("Internet_20_link"), QStringLiteral("Internet link"), QStringLiteral("text"), {}, {},
              {kv("fo:color", accent), kv("style:text-underline-style", QStringLiteral("solid")),
               kv("style:text-underline-width", QStringLiteral("auto")), kv("style:text-underline-color", QStringLiteral("font-color"))});
    odt_named(w, QStringLiteral("Report_20_Watermark"), QStringLiteral("Report Watermark"), QStringLiteral("text"), {}, {},
              odt_font(0, odt_blend(paper, ink, 0.35), true));
    odt_list_style(w, QStringLiteral("List_20_Bullet"), QStringLiteral("List Bullet"), false);
    odt_list_style(w, QStringLiteral("List_20_Number"), QStringLiteral("List Number"), true);
    w.writeEndElement(); // office:styles

    // Page: size, orientation, margins and paper colour of the report; header / footer
    // bands of 8 mm (6 mm text + 2 mm spacing) with the grey rule the page view draws.
    const QSizeF page = page_size_mm(m);
    w.writeStartElement("office:automatic-styles");
    w.writeStartElement("style:page-layout");
    w.writeAttribute("style:name", "pm1");
    w.writeEmptyElement("style:page-layout-properties");
    w.writeAttribute("fo:page-width", odt_mm(page.width()));
    w.writeAttribute("fo:page-height", odt_mm(page.height()));
    w.writeAttribute("style:num-format", "1");
    w.writeAttribute("style:print-orientation", m.landscape ? "landscape" : "portrait");
    w.writeAttribute("fo:margin-top", odt_mm(m.margin_top_mm));
    w.writeAttribute("fo:margin-bottom", odt_mm(m.margin_bottom_mm));
    w.writeAttribute("fo:margin-left", odt_mm(m.margin_left_mm));
    w.writeAttribute("fo:margin-right", odt_mm(m.margin_right_mm));
    w.writeAttribute("fo:background-color", paper);
    w.writeAttribute("style:writing-mode", "lr-tb");
    if (has_header) {
        w.writeStartElement("style:header-style");
        w.writeEmptyElement("style:header-footer-properties");
        w.writeAttribute("fo:min-height", odt_mm(6));
        w.writeAttribute("fo:margin-bottom", odt_mm(2));
        w.writeAttribute("fo:padding-bottom", odt_mm(0.5));
        w.writeAttribute("fo:border-bottom", band_rule);
        w.writeAttribute("style:dynamic-spacing", "false");
        w.writeEndElement();
    }
    if (has_footer) {
        w.writeStartElement("style:footer-style");
        w.writeEmptyElement("style:header-footer-properties");
        w.writeAttribute("fo:min-height", odt_mm(6));
        w.writeAttribute("fo:margin-top", odt_mm(2));
        w.writeAttribute("fo:padding-top", odt_mm(0.5));
        w.writeAttribute("fo:border-top", band_rule);
        w.writeAttribute("style:dynamic-spacing", "false");
        w.writeEndElement();
    }
    w.writeEndElement(); // page-layout
    w.writeEndElement(); // automatic-styles

    OdtCtx hx;
    hx.doc = x.doc;
    hx.w = &w;
    hx.vars = x.vars;
    w.writeStartElement("office:master-styles");
    w.writeStartElement("style:master-page");
    w.writeAttribute("style:name", "Standard");
    w.writeAttribute("style:page-layout-name", "pm1");
    if (has_header) {
        // ODF has no portable diagonal watermark: it is printed at the right of the header.
        w.writeStartElement("style:header");
        odt_band(hx, QStringLiteral("Header"), m.header_left, m.header_center, m.header_right, watermark, m);
        w.writeEndElement();
    }
    if (has_footer) {
        QString center = m.footer_center;
        if (m.show_page_numbers && m.footer_left.isEmpty() && m.footer_center.isEmpty() && m.footer_right.isEmpty())
            center = QStringLiteral("{page} / {total}");
        w.writeStartElement("style:footer");
        if (!m.footer_left.isEmpty() || !center.isEmpty() || !m.footer_right.isEmpty())
            odt_band(hx, QStringLiteral("Footer"), m.footer_left, center, m.footer_right, {}, m);
        if (!confidentiality.isEmpty()) {
            w.writeStartElement("text:p");
            w.writeAttribute("text:style-name", "Report_20_Confidentiality");
            hx.fresh = true;
            odt_chars(hx, confidentiality);
            w.writeEndElement();
        }
        w.writeEndElement();
    }
    w.writeEndElement(); // master-page
    w.writeEndElement(); // master-styles
    w.writeEndElement(); // document-styles
    w.writeEndDocument();
    return w.hasError() ? QByteArray() : out;
}

QByteArray odt_meta_xml(const ReportDocument& doc, const OdtOptions& opt, const QDateTime& created,
                        const QMap<QString, QString>& vars_ctx) {
    QByteArray out;
    QXmlStreamWriter w(&out);
    w.setAutoFormatting(false);
    w.writeStartDocument();
    w.writeStartElement("office:document-meta");
    odt_namespaces(w);
    w.writeStartElement("office:meta");
    const QString when = created.toUTC().toString(Qt::ISODate);
    w.writeTextElement("meta:generator", odt_clean(opt.generator));
    w.writeTextElement("dc:title", odt_clean(vars::expand(doc.metadata.title, vars_ctx)));
    if (!doc.metadata.author.trimmed().isEmpty()) {
        w.writeTextElement("meta:initial-creator", odt_clean(doc.metadata.author.trimmed()));
        w.writeTextElement("dc:creator", odt_clean(doc.metadata.author.trimmed()));
    }
    w.writeTextElement("meta:creation-date", when);
    w.writeTextElement("dc:date", when);
    w.writeEndElement();
    w.writeEndElement();
    w.writeEndDocument();
    return w.hasError() ? QByteArray() : out;
}

QByteArray odt_manifest_xml(const QVector<OdtPicture>& pictures) {
    QByteArray out;
    QXmlStreamWriter w(&out);
    w.setAutoFormatting(true);
    w.writeStartDocument();
    w.writeStartElement("manifest:manifest");
    w.writeAttribute("xmlns:manifest", "urn:oasis:names:tc:opendocument:xmlns:manifest:1.0");
    w.writeAttribute("manifest:version", "1.3");
    auto entry = [&w](const QString& path, const QString& type, bool root = false) {
        w.writeEmptyElement("manifest:file-entry");
        w.writeAttribute("manifest:full-path", path);
        if (root)
            w.writeAttribute("manifest:version", "1.3");
        w.writeAttribute("manifest:media-type", type);
    };
    entry(QStringLiteral("/"), QString::fromLatin1(kOdtMime), true);
    entry(QStringLiteral("content.xml"), QStringLiteral("text/xml"));
    entry(QStringLiteral("styles.xml"), QStringLiteral("text/xml"));
    entry(QStringLiteral("meta.xml"), QStringLiteral("text/xml"));
    for (const OdtPicture& p : pictures)
        entry(p.path, p.mime);
    w.writeEndElement();
    w.writeEndDocument();
    return w.hasError() ? QByteArray() : out;
}

} // namespace

QByteArray write(const ReportDocument& doc, const exportfmt::ExportAssets& assets, const OdtOptions& opt,
                 const std::function<bool()>& cancelled, QString* error) {
    auto fail = [error](const char* why) {
        if (error)
            *error = QString::fromLatin1(why);
        return QByteArray();
    };
    if (error)
        error->clear();
    const ReportMetadata& m = doc.metadata;
    const QDateTime created = opt.created.isValid() ? opt.created : QDateTime::currentDateTime();

    // What prints: blocks with content (numbers and the TOC count only those).
    ReportDocument printed = doc;
    printed.components.clear();
    for (const ReportComponent& c : doc.components)
        if (!ef::is_unfilled(c, assets))
            printed.components.append(c);

    OdtStyleSet auto_styles;
    OdtCtx x;
    x.doc = &doc;
    x.assets = &assets;
    x.styles = &auto_styles;
    x.vars = vars::context(m);
    x.headings = outline::headings(printed);
    for (int i = 0; i < x.headings.size(); ++i)
        x.heading_index.insert(x.headings[i].id, i);
    x.scale = std::clamp(m.font_size, 6, 72) / 12.0;
    const QSizeF page = page_size_mm(m);
    x.content_mm = std::max(20.0, page.width() - m.margin_left_mm - m.margin_right_mm);
    x.font = m.font_family.trimmed().isEmpty() ? QStringLiteral("Arial") : m.font_family.trimmed();

    QByteArray body;
    {
        QXmlStreamWriter w(&body);
        w.setAutoFormatting(false);
        x.w = &w;
        // The document header the page view writes, unless the report opens with a cover.
        if (printed.components.isEmpty() || printed.components.first().type != QLatin1String("cover"))
            odt_doc_header(x);
        for (const ReportComponent& raw : printed.components) {
            if (cancelled && cancelled())
                return fail("cancelled");
            odt_block(x, vars::expanded_component(raw, x.vars));
        }
        // A page break as the last block prints nothing (no empty last page).
        if (w.hasError())
            return fail("could not write the document text");
        x.w = nullptr;
    }
    if (cancelled && cancelled())
        return fail("cancelled");

    QByteArray content;
    {
        QXmlStreamWriter w(&content);
        w.setAutoFormatting(false);
        w.writeStartDocument();
        w.writeStartElement("office:document-content");
        odt_namespaces(w);
        odt_font_decls(w, x.font);
        w.writeStartElement("office:automatic-styles");
        auto_styles.write(w);
        w.writeEndElement();
        w.writeStartElement("office:body");
        w.writeStartElement("office:text");
        w.writeCharacters(QString()); // closes the start tag: the body bytes follow
        if (w.hasError())
            return fail("could not write content.xml");
    }
    content += body;
    content += "</office:text></office:body></office:document-content>\n";

    const QByteArray styles = odt_styles_xml(x);
    const QByteArray meta = odt_meta_xml(doc, opt, created, x.vars);
    const QByteArray manifest = odt_manifest_xml(x.pictures);
    if (styles.isEmpty() || meta.isEmpty() || manifest.isEmpty())
        return fail("could not write the document styles");

    zip::ZipWriter zw(created);
    zw.add_stored(QStringLiteral("mimetype"), QByteArray(kOdtMime)); // first, stored, no extra field
    zw.add_deflated(QStringLiteral("content.xml"), content);
    zw.add_deflated(QStringLiteral("styles.xml"), styles);
    zw.add_deflated(QStringLiteral("meta.xml"), meta);
    for (const OdtPicture& p : x.pictures) {
        if (p.mime == QLatin1String("image/png") || p.mime == QLatin1String("image/jpeg"))
            zw.add_stored(p.path, p.bytes); // already compressed
        else
            zw.add_deflated(p.path, p.bytes);
    }
    zw.add_deflated(QStringLiteral("META-INF/manifest.xml"), manifest);
    const QByteArray out = zw.finish();
    if (out.isEmpty())
        return fail("the document is too large");
    return out;
}

} // namespace fincept::report::odt
