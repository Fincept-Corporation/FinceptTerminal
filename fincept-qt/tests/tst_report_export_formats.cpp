// tst_report_export_formats.cpp — Report Builder's worker-thread exports (Qt Core only):
// the ZIP writer (read back by a small reader here: local headers and central directory
// agree, CRC-32 matches zlib, deflated entries inflate back, deterministic bytes), the
// .odt writer (package structure, manifest, well-formed XML, page setup, running header /
// footer fields, numbered headings, formatted tables, pictures, variables, cover page,
// cancellation) and the GitHub-flavoured Markdown writer.
//
// Set FINCEPT_EXPORT_SAMPLE_DIR to also write the sample .odt / .md (+ pictures) there.

#include "core/report/ReportDocument.h"
#include "core/report/ReportExportAssets.h"
#include "core/report/ReportMarkdownWriter.h"
#include "core/report/ReportOdtWriter.h"
#include "core/report/ReportTableFormat.h"
#include "core/report/ReportZipWriter.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTimeZone>
#include <QJsonDocument>
#include <QJsonObject>
#include <QXmlStreamReader>
#include <QtTest>

#include <cstring>

// Qt Core exports its bundled zlib with a z_ prefix on Windows (as in the app).
#if defined(_WIN32) && !defined(Z_PREFIX)
#    define Z_PREFIX
#endif
#include <zlib.h>

using namespace fincept;
using report::ReportComponent;
using report::ReportDocument;
namespace ef = report::exportfmt;

namespace {

// ── A small ZIP reader ──────────────────────────────────────────────────────

quint16 rd16(const QByteArray& b, qsizetype at) {
    return static_cast<quint16>(static_cast<quint8>(b.at(at)) | (static_cast<quint8>(b.at(at + 1)) << 8));
}
quint32 rd32(const QByteArray& b, qsizetype at) {
    return static_cast<quint32>(rd16(b, at)) | (static_cast<quint32>(rd16(b, at + 2)) << 16);
}

quint32 zcrc(const QByteArray& data) {
    return static_cast<quint32>(
        crc32(crc32(0L, Z_NULL, 0), reinterpret_cast<const Bytef*>(data.constData()), static_cast<uInt>(data.size())));
}

QByteArray inflate_raw(const QByteArray& in, qsizetype expected, bool* ok) {
    *ok = false;
    z_stream s;
    std::memset(&s, 0, sizeof(s));
    if (inflateInit2(&s, -15) != Z_OK)
        return {};
    QByteArray out(expected + 1, '\0');
    s.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(in.constData()));
    s.avail_in = static_cast<uInt>(in.size());
    s.next_out = reinterpret_cast<Bytef*>(out.data());
    s.avail_out = static_cast<uInt>(out.size());
    const int rc = inflate(&s, Z_FINISH);
    const qsizetype produced = static_cast<qsizetype>(s.total_out);
    inflateEnd(&s);
    out.resize(produced);
    *ok = rc == Z_STREAM_END && produced == expected;
    return out;
}

struct ZEntry {
    QString name;
    quint16 version_needed = 0, flags = 0, method = 0, time = 0, date = 0;
    quint32 crc = 0, csize = 0, usize = 0, offset = 0;
    quint16 local_extra = 0;
    QByteArray data; ///< uncompressed
};

struct ZArchive {
    bool ok = false;
    QString error;
    QVector<ZEntry> entries;
    bool has(const QString& name) const {
        for (const ZEntry& e : entries)
            if (e.name == name)
                return true;
        return false;
    }
    QByteArray data(const QString& name) const {
        for (const ZEntry& e : entries)
            if (e.name == name)
                return e.data;
        return {};
    }
    QStringList names() const {
        QStringList out;
        for (const ZEntry& e : entries)
            out << e.name;
        return out;
    }
};

ZArchive read_zip(const QByteArray& z) {
    ZArchive a;
    auto fail = [&a](const QString& why) {
        a.ok = false;
        a.error = why;
        return a;
    };
    if (z.size() < 22)
        return fail(QStringLiteral("too short"));
    const qsizetype eocd = z.size() - 22; // the writer adds no comment
    if (rd32(z, eocd) != 0x06054b50u)
        return fail(QStringLiteral("no end record at size-22"));
    const int count = rd16(z, eocd + 10);
    if (rd16(z, eocd + 8) != count)
        return fail(QStringLiteral("entry counts differ"));
    const quint32 cd_size = rd32(z, eocd + 12);
    const quint32 cd_off = rd32(z, eocd + 16);
    if (static_cast<qsizetype>(cd_off) + cd_size != eocd)
        return fail(QStringLiteral("central directory does not end at the end record"));
    qsizetype p = cd_off;
    qsizetype expected_local = 0;
    for (int i = 0; i < count; ++i) {
        if (p + 46 > eocd || rd32(z, p) != 0x02014b50u)
            return fail(QStringLiteral("bad central header %1").arg(i));
        ZEntry e;
        e.version_needed = rd16(z, p + 6);
        e.flags = rd16(z, p + 8);
        e.method = rd16(z, p + 10);
        e.time = rd16(z, p + 12);
        e.date = rd16(z, p + 14);
        e.crc = rd32(z, p + 16);
        e.csize = rd32(z, p + 20);
        e.usize = rd32(z, p + 24);
        const int nlen = rd16(z, p + 28), elen = rd16(z, p + 30), clen = rd16(z, p + 32);
        e.offset = rd32(z, p + 42);
        e.name = QString::fromUtf8(z.mid(p + 46, nlen));
        p += 46 + nlen + elen + clen;
        if (elen != 0 || clen != 0)
            return fail(QStringLiteral("central extra/comment on %1").arg(e.name));
        // Local header: the same fields, entries back to back from offset 0.
        const qsizetype o = e.offset;
        if (o != expected_local)
            return fail(QStringLiteral("local header of %1 not where expected").arg(e.name));
        if (rd32(z, o) != 0x04034b50u)
            return fail(QStringLiteral("bad local header for %1").arg(e.name));
        if (rd16(z, o + 4) != e.version_needed || rd16(z, o + 6) != e.flags || rd16(z, o + 8) != e.method ||
            rd16(z, o + 10) != e.time || rd16(z, o + 12) != e.date || rd32(z, o + 14) != e.crc ||
            rd32(z, o + 18) != e.csize || rd32(z, o + 22) != e.usize)
            return fail(QStringLiteral("local and central headers differ for %1").arg(e.name));
        const int lnlen = rd16(z, o + 26);
        e.local_extra = rd16(z, o + 28);
        if (QString::fromUtf8(z.mid(o + 30, lnlen)) != e.name)
            return fail(QStringLiteral("local name differs for %1").arg(e.name));
        const QByteArray raw = z.mid(o + 30 + lnlen + e.local_extra, e.csize);
        expected_local = o + 30 + lnlen + e.local_extra + e.csize;
        if (e.method == 0) {
            e.data = raw;
            if (e.csize != e.usize)
                return fail(QStringLiteral("stored sizes differ for %1").arg(e.name));
        } else if (e.method == 8) {
            bool ok = false;
            e.data = inflate_raw(raw, e.usize, &ok);
            if (!ok)
                return fail(QStringLiteral("%1 does not inflate").arg(e.name));
        } else {
            return fail(QStringLiteral("unknown method on %1").arg(e.name));
        }
        if (zcrc(e.data) != e.crc)
            return fail(QStringLiteral("CRC mismatch on %1").arg(e.name));
        a.entries.append(e);
    }
    if (expected_local != cd_off)
        return fail(QStringLiteral("gap before the central directory"));
    a.ok = true;
    return a;
}

// ── XML helpers (qualified names, no namespace processing) ──────────────────

QString xml_error(const QByteArray& xml) {
    QXmlStreamReader r(xml);
    while (!r.atEnd())
        r.readNext();
    return r.hasError() ? QStringLiteral("%1 at %2:%3").arg(r.errorString()).arg(r.lineNumber()).arg(r.columnNumber())
                        : QString();
}

QVector<QXmlStreamAttributes> elements(const QByteArray& xml, const QString& qname) {
    QVector<QXmlStreamAttributes> out;
    QXmlStreamReader r(xml);
    r.setNamespaceProcessing(false);
    while (!r.atEnd())
        if (r.readNext() == QXmlStreamReader::StartElement && r.qualifiedName() == qname)
            out << r.attributes();
    return out;
}

int count_of(const QByteArray& xml, const char* qname) {
    return static_cast<int>(elements(xml, QString::fromLatin1(qname)).size());
}

QString attr(const QXmlStreamAttributes& a, const char* qname) {
    return a.value(QLatin1String(qname)).toString();
}

struct XPara {
    QString element; ///< text:p | text:h
    QString style;
    int outline = 0;
    QString text;    ///< text:s as spaces, text:tab as \t, text:line-break as \n
};

void xml_inline(QXmlStreamReader& r, QString* text) {
    const QStringView n = r.qualifiedName();
    if (n == QLatin1String("text:s"))
        *text += QString(r.attributes().value(QLatin1String("text:c")).toInt() > 0
                             ? r.attributes().value(QLatin1String("text:c")).toInt()
                             : 1,
                         QLatin1Char(' '));
    else if (n == QLatin1String("text:tab"))
        *text += QLatin1Char('\t');
    else if (n == QLatin1String("text:line-break"))
        *text += QLatin1Char('\n');
}

/// Every paragraph / heading in document order (paragraphs inside table cells included).
QVector<XPara> paragraphs(const QByteArray& xml) {
    QVector<XPara> out;
    QVector<int> open;
    int skip = 0; // inside svg:title / svg:desc
    QXmlStreamReader r(xml);
    r.setNamespaceProcessing(false);
    while (!r.atEnd()) {
        const auto tok = r.readNext();
        if (tok == QXmlStreamReader::StartElement) {
            const QStringView n = r.qualifiedName();
            if (n == QLatin1String("text:p") || n == QLatin1String("text:h")) {
                XPara p;
                p.element = n.toString();
                p.style = r.attributes().value(QLatin1String("text:style-name")).toString();
                p.outline = r.attributes().value(QLatin1String("text:outline-level")).toInt();
                open << static_cast<int>(out.size());
                out << p;
            } else if (n == QLatin1String("svg:title") || n == QLatin1String("svg:desc")) {
                ++skip;
            } else if (!open.isEmpty()) {
                xml_inline(r, &out[open.last()].text);
            }
        } else if (tok == QXmlStreamReader::EndElement) {
            const QStringView n = r.qualifiedName();
            if ((n == QLatin1String("text:p") || n == QLatin1String("text:h")) && !open.isEmpty())
                open.removeLast();
            else if (n == QLatin1String("svg:title") || n == QLatin1String("svg:desc"))
                --skip;
        } else if (tok == QXmlStreamReader::Characters && !open.isEmpty() && skip == 0) {
            out[open.last()].text += r.text();
        }
    }
    return out;
}

int find_para(const QVector<XPara>& ps, const QString& text) {
    for (int i = 0; i < ps.size(); ++i)
        if (ps[i].text == text)
            return i;
    return -1;
}

/// Style of the paragraph with exactly `text` ("<missing>" when there is none).
QString para_style(const QVector<XPara>& ps, const QString& text) {
    const int i = find_para(ps, text);
    return i < 0 ? QStringLiteral("<missing>") : ps[i].style;
}

struct XCell {
    QString style;      ///< table-cell style
    QString para_style; ///< style of its paragraph
    QString text;
};

struct XTable {
    bool found = false;
    int header_rows = 0;              ///< rows inside table:table-header-rows
    QVector<QVector<XCell>> rows;
    QStringList column_styles;
};

XTable table_named(const QByteArray& xml, const QString& name) {
    XTable t;
    QXmlStreamReader r(xml);
    r.setNamespaceProcessing(false);
    bool inside = false, in_header = false, in_p = false;
    while (!r.atEnd()) {
        const auto tok = r.readNext();
        const QStringView n = r.qualifiedName();
        if (tok == QXmlStreamReader::StartElement) {
            if (n == QLatin1String("table:table") && r.attributes().value(QLatin1String("table:name")) == name) {
                inside = true;
                t.found = true;
            } else if (!inside) {
                continue;
            } else if (n == QLatin1String("table:table-column")) {
                t.column_styles << r.attributes().value(QLatin1String("table:style-name")).toString();
            } else if (n == QLatin1String("table:table-header-rows")) {
                in_header = true;
            } else if (n == QLatin1String("table:table-row")) {
                t.rows.append(QVector<XCell>());
                if (in_header)
                    ++t.header_rows;
            } else if (n == QLatin1String("table:table-cell")) {
                XCell c;
                c.style = r.attributes().value(QLatin1String("table:style-name")).toString();
                t.rows.last().append(c);
            } else if (n == QLatin1String("text:p")) {
                in_p = true;
                t.rows.last().last().para_style = r.attributes().value(QLatin1String("text:style-name")).toString();
            } else if (in_p) {
                xml_inline(r, &t.rows.last().last().text);
            }
        } else if (tok == QXmlStreamReader::EndElement && inside) {
            if (n == QLatin1String("table:table"))
                break;
            if (n == QLatin1String("table:table-header-rows"))
                in_header = false;
            if (n == QLatin1String("text:p"))
                in_p = false;
        } else if (tok == QXmlStreamReader::Characters && inside && in_p) {
            t.rows.last().last().text += r.text();
        }
    }
    return t;
}

/// style:name -> its properties ("@family", "@parent" and every *-properties attribute).
QHash<QString, QHash<QString, QString>> styles_of(const QByteArray& xml) {
    QHash<QString, QHash<QString, QString>> out;
    QXmlStreamReader r(xml);
    r.setNamespaceProcessing(false);
    QString current;
    int depth = 0, current_depth = -1;
    while (!r.atEnd()) {
        const auto tok = r.readNext();
        if (tok == QXmlStreamReader::StartElement) {
            ++depth;
            const QXmlStreamAttributes a = r.attributes();
            if (current.isEmpty() && a.hasAttribute(QLatin1String("style:name"))) {
                current = a.value(QLatin1String("style:name")).toString();
                current_depth = depth;
                out[current][QStringLiteral("@family")] = a.value(QLatin1String("style:family")).toString();
                out[current][QStringLiteral("@parent")] = a.value(QLatin1String("style:parent-style-name")).toString();
            } else if (!current.isEmpty() && r.qualifiedName().endsWith(QLatin1String("-properties"))) {
                for (const QXmlStreamAttribute& at : a)
                    out[current][at.qualifiedName().toString()] = at.value().toString();
            }
        } else if (tok == QXmlStreamReader::EndElement) {
            if (depth == current_depth) {
                current.clear();
                current_depth = -1;
            }
            --depth;
        }
    }
    return out;
}

double mm_of(const QString& length) {
    QString s = length;
    if (s.endsWith(QLatin1String("mm")))
        s.chop(2);
    return s.toDouble();
}

// ── Test documents ──────────────────────────────────────────────────────────

ReportComponent mk(int id, const QString& type, const QString& content = {}, const QMap<QString, QString>& cfg = {}) {
    ReportComponent c;
    c.id = id;
    c.type = type;
    c.content = content;
    c.config = cfg;
    return c;
}

QByteArray be32(quint32 v) {
    QByteArray b;
    for (int s = 24; s >= 0; s -= 8)
        b.append(static_cast<char>((v >> s) & 0xFFu));
    return b;
}

/// A real (tiny, solid colour) PNG: the sample opens with pictures in Word / LibreOffice.
QByteArray make_png(int w, int h, quint8 r, quint8 g, quint8 b) {
    QByteArray raw;
    for (int y = 0; y < h; ++y) {
        raw.append('\0');
        for (int x = 0; x < w; ++x) {
            raw.append(static_cast<char>(r));
            raw.append(static_cast<char>(g));
            raw.append(static_cast<char>(b));
        }
    }
    auto chunk = [](const char* type, const QByteArray& data) {
        const QByteArray td = QByteArray(type, 4) + data;
        return be32(static_cast<quint32>(data.size())) + td + be32(zcrc(td));
    };
    QByteArray ihdr = be32(static_cast<quint32>(w)) + be32(static_cast<quint32>(h));
    ihdr.append(static_cast<char>(8)).append(static_cast<char>(2)).append('\0').append('\0').append('\0');
    return QByteArray("\x89PNG\r\n\x1a\n", 8) + chunk("IHDR", ihdr) + chunk("IDAT", qCompress(raw).mid(4)) +
           chunk("IEND", {});
}

ef::ImageAsset png_asset(int w, int h, quint8 r, quint8 g, quint8 b, QSizeF display = {}) {
    ef::ImageAsset a;
    a.bytes = make_png(w, h, r, g, b);
    a.ext = QStringLiteral("png");
    a.pixels = QSize(w, h);
    a.display_px = display;
    return a;
}

const char* const kTableCsv =
    "Segment,Revenue,Margin|Phones,1234.5,0.42|Services,-250,0.61|Wearables,\"1,000\",0.18|Mac,99,0.3";
const char* const kTableCols =
    "[{},{\"type\":\"currency\",\"ccy\":\"USD\",\"neg\":\"brackets\"},{\"type\":\"number\",\"dec\":2,\"heat\":\"green_red\"}]";

ReportDocument rich_doc() {
    ReportDocument d;
    report::ReportMetadata& m = d.metadata;
    m.title = QStringLiteral("{{period}} {{symbol}} Review");
    m.author = QStringLiteral("Jane Analyst");
    m.company = QStringLiteral("Acme Capital");
    m.date = QStringLiteral("17 Nov 2026");
    m.variables.insert(QStringLiteral("symbol"), QStringLiteral("AAPL"));
    m.variables.insert(QStringLiteral("period"), QStringLiteral("Q3"));
    m.number_headings = true;
    m.font_family = QStringLiteral("Segoe UI");
    m.header_left = QStringLiteral("{title}");
    m.header_right = QStringLiteral("{{period}} draft");
    m.footer_center = QStringLiteral("Page {page} of {total}");
    m.confidentiality = QStringLiteral("Confidential — internal use only");
    m.watermark = QStringLiteral("DRAFT");
    d.components = {
        mk(1, "heading", "Overview of {{symbol}}", {{"level", "1"}}),
        mk(2, "text",
           "Revenue **grew** strongly in *{{period}}*.\n\n- First point\n- Second with `code`\n  - nested item\n1. one\n2. "
           "two\n\nSee [the filing](https://example.com/10q) and {{missing}}.",
           {{"format", "markdown"}}),
        mk(3, "text", "   "),
        mk(4, "heading", "Financials", {{"level", "2"}}),
        mk(5, "table", {},
           {{"csv", kTableCsv},
            {"col_fmt", kTableCols},
            {"table_style", "{\"totals\":\"sum\"}"},
            {"title", "Segment results"},
            {"note", "USD millions"},
            {"source_label", "Company filings"},
            {"as_of", "2026-11-17T13:02:00Z"}}),
        mk(6, "chart", {}, {{"data", "1,2,3"}, {"title", "Revenue trend"}, {"caption", "Quarterly revenue"}}),
        mk(7, "chart", {}, {{"data", "4,5,6"}, {"title", "No picture"}}),
        mk(8, "image", {}, {{"width", "300"}, {"align", "center"}, {"caption", "Store photo"}}),
        mk(9, "stats_block", {}, {{"title", "Valuation"}, {"data", "P/E: 28.5\nDiv yield: 0.5%\nBeta: 1.2"}, {"tones", "positive,,negative"}}),
        mk(10, "market_data", {},
           {{"symbol", "AAPL"}, {"price", "189.50"}, {"change_pct", "1.25"}, {"name", "Apple Inc."}, {"high", "190"}, {"low", "185"},
            {"volume", "52100000"}, {"currency", "USD"}}),
        mk(11, "callout", "Watch the **margin**.", {{"style", "warning"}, {"heading", "Heads up"}}),
        mk(12, "quote", "To be  or not to be"),
        mk(13, "code", "int main() {\n    return 0; // ```\n}"),
        mk(14, "list", "Alpha\nBeta **bold**\n\nGamma"),
        mk(15, "divider"),
        mk(16, "page_break"),
        mk(17, "toc"),
        mk(18, "heading", "Outlook", {{"level", "1"}}),
        mk(19, "heading", "Risks", {{"level", "3"}}),
        mk(20, "sparkline", {}, {{"title", "AAPL"}, {"data", "1,2,3,2"}, {"current", "189.5"}, {"change_pct", "-0.4"}}),
        mk(21, "heading", "", {{"level", "2"}}),
        mk(22, "text", "Figures unaudited", {{"style", "caption"}}),
    };
    d.next_id = 23;
    return d;
}

ef::ExportAssets rich_assets() {
    ef::ExportAssets a;
    a.charts.insert(6, png_asset(64, 32, 0xd9, 0x77, 0x06, QSizeF(640, 320)));
    a.images.insert(8, png_asset(40, 20, 0x3b, 0x82, 0xf6));
    a.charts.insert(20, png_asset(32, 8, 0x16, 0xa3, 0x4a, QSizeF(160, 40)));
    return a;
}

QDateTime fixed_time() {
    return QDateTime(QDate(2026, 11, 17), QTime(9, 30, 0), QTimeZone::UTC);
}

report::odt::OdtOptions odt_opts() {
    report::odt::OdtOptions o;
    o.created = fixed_time();
    return o;
}

QString as_of_text() {
    return QDateTime::fromString(QStringLiteral("2026-11-17T13:02:00Z"), Qt::ISODate)
        .toLocalTime()
        .toString(QStringLiteral("d MMM yyyy HH:mm"));
}

QByteArray odt_of(const ReportDocument& d, const ef::ExportAssets& a) {
    QString err;
    const QByteArray out = report::odt::write(d, a, odt_opts(), {}, &err);
    if (!err.isEmpty())
        qWarning("odt error: %s", qPrintable(err));
    return out;
}

void save_sample(const QString& name, const QByteArray& bytes) {
    const QString dir = qEnvironmentVariable("FINCEPT_EXPORT_SAMPLE_DIR");
    if (dir.isEmpty())
        return;
    const QString path = dir + QLatin1Char('/') + name;
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (f.open(QIODevice::WriteOnly))
        f.write(bytes);
}

} // namespace

class TestReportExportFormats : public QObject {
    Q_OBJECT

  private slots:
    // ── ZIP ─────────────────────────────────────────────────────────────────
    void zipRoundTrip() {
        QByteArray text;
        for (int i = 0; i < 400; ++i)
            text += "<text:p>Revenue grew strongly in the third quarter.</text:p>\n";
        const QByteArray tiny("xy");
        QByteArray noise;
        quint32 s = 12345;
        for (int i = 0; i < 64; ++i) {
            s = s * 1103515245u + 12345u;
            noise.append(static_cast<char>((s >> 16) & 0xFF));
        }
        auto build = [&] {
            report::zip::ZipWriter z(QDateTime(QDate(2026, 11, 17), QTime(14, 2, 31)));
            z.add_stored(QStringLiteral("mimetype"), "application/vnd.oasis.opendocument.text");
            z.add_deflated(QStringLiteral("content.xml"), text);
            z.add_deflated(QStringLiteral("tiny.txt"), tiny);
            z.add_deflated(QStringLiteral("noise.bin"), noise);
            z.add_deflated(QStringLiteral("empty.txt"), {});
            z.add_stored(QStringLiteral("Pictures/grafik-ü.png"), noise);
            z.add_stored(QStringLiteral("content.xml"), "duplicate ignored");
            return z.finish();
        };
        const QByteArray zip = build();
        const ZArchive a = read_zip(zip);
        QVERIFY2(a.ok, qPrintable(a.error));
        QCOMPARE(a.names(), QStringList({"mimetype", "content.xml", "tiny.txt", "noise.bin", "empty.txt",
                                         QStringLiteral("Pictures/grafik-ü.png")}));
        QCOMPARE(a.data("content.xml"), text);
        QCOMPARE(a.data("tiny.txt"), tiny);
        QCOMPARE(a.data("noise.bin"), noise);
        QCOMPARE(a.data("empty.txt"), QByteArray());
        QCOMPARE(a.data(QStringLiteral("Pictures/grafik-ü.png")), noise);
        for (const ZEntry& e : a.entries) {
            QVERIFY(e.flags & 0x0800); // UTF-8 names
            QCOMPARE(e.local_extra, quint16(0));
            QCOMPARE(e.date, quint16(((2026 - 1980) << 9) | (11 << 5) | 17));
            QCOMPARE(e.time, quint16((14 << 11) | (2 << 5) | (31 / 2)));
        }
        QCOMPARE(a.entries[0].method, quint16(0));
        QCOMPARE(a.entries[1].method, quint16(8)); // compressible: deflated
        QVERIFY(a.entries[1].csize < a.entries[1].usize / 4);
        QCOMPARE(a.entries[2].method, quint16(0)); // deflate would grow it: stored
        QCOMPARE(a.entries[3].method, quint16(0));
        QCOMPARE(a.entries[4].method, quint16(0));
        QCOMPARE(build(), zip); // deterministic
    }

    void zipDefaultStampAndFinishTwice() {
        report::zip::ZipWriter z;
        z.add_deflated(QStringLiteral("a.txt"), QByteArray(1000, 'a'));
        const QByteArray first = z.finish();
        z.add_stored(QStringLiteral("late.txt"), "ignored");
        QCOMPARE(z.finish(), first);
        const ZArchive a = read_zip(first);
        QVERIFY2(a.ok, qPrintable(a.error));
        QCOMPARE(int(a.entries.size()), 1);
        QCOMPARE(a.entries[0].date, quint16((1 << 5) | 1)); // 1980-01-01
        QCOMPARE(a.entries[0].time, quint16(0));
        QCOMPARE(a.entries[0].method, quint16(8));
        QCOMPARE(a.data("a.txt"), QByteArray(1000, 'a'));

        report::zip::ZipWriter empty;
        const ZArchive e = read_zip(empty.finish());
        QVERIFY2(e.ok, qPrintable(e.error));
        QCOMPARE(int(e.entries.size()), 0);
    }

    // ── ODT package ─────────────────────────────────────────────────────────
    void odtPackage() {
        const ReportDocument d = rich_doc();
        const ef::ExportAssets assets = rich_assets();
        const QByteArray odt = odt_of(d, assets);
        QVERIFY(!odt.isEmpty());
        save_sample(QStringLiteral("sample.odt"), odt);
        QCOMPARE(odt_of(d, assets), odt); // deterministic with a fixed creation time

        // mimetype: first, stored, no extra field, exact content at the fixed offsets.
        QCOMPARE(odt.mid(30, 8), QByteArray("mimetype"));
        QCOMPARE(odt.mid(38, 39), QByteArray("application/vnd.oasis.opendocument.text"));
        const ZArchive a = read_zip(odt);
        QVERIFY2(a.ok, qPrintable(a.error));
        QCOMPARE(a.entries.first().name, QStringLiteral("mimetype"));
        QCOMPARE(a.entries.first().method, quint16(0));
        QCOMPARE(a.entries.first().local_extra, quint16(0));
        QCOMPARE(a.entries.first().data, QByteArray("application/vnd.oasis.opendocument.text"));

        // Manifest: every entry (but mimetype and itself) listed with a media type; every
        // listed file exists.
        const QByteArray manifest = a.data("META-INF/manifest.xml");
        QHash<QString, QString> listed;
        for (const QXmlStreamAttributes& e : elements(manifest, QStringLiteral("manifest:file-entry")))
            listed.insert(attr(e, "manifest:full-path"), attr(e, "manifest:media-type"));
        QCOMPARE(listed.value(QStringLiteral("/")), QStringLiteral("application/vnd.oasis.opendocument.text"));
        for (const QString& n : a.names()) {
            if (n == QLatin1String("mimetype") || n == QLatin1String("META-INF/manifest.xml"))
                continue;
            QVERIFY2(listed.contains(n), qPrintable(n));
            QVERIFY2(!listed.value(n).isEmpty(), qPrintable(n));
        }
        for (auto it = listed.cbegin(); it != listed.cend(); ++it)
            if (it.key() != QLatin1String("/"))
                QVERIFY2(a.has(it.key()), qPrintable(it.key()));
        QCOMPARE(listed.value(QStringLiteral("Pictures/chart_6.png")), QStringLiteral("image/png"));

        for (const char* xml : {"content.xml", "styles.xml", "meta.xml", "META-INF/manifest.xml"}) {
            const QByteArray bytes = a.data(QString::fromLatin1(xml));
            QVERIFY2(!bytes.isEmpty(), xml);
            const QString err = xml_error(bytes);
            QVERIFY2(err.isEmpty(), qPrintable(QString::fromLatin1(xml) + QStringLiteral(": ") + err));
        }
        QVERIFY(a.data("content.xml").contains("office:version=\"1.3\""));

        // meta.xml
        const QByteArray meta = a.data("meta.xml");
        QVERIFY(meta.contains("<dc:title>Q3 AAPL Review</dc:title>"));
        QVERIFY(meta.contains("<meta:generator>Fincept Terminal</meta:generator>"));
        QVERIFY(meta.contains("<meta:initial-creator>Jane Analyst</meta:initial-creator>"));
        QVERIFY(meta.contains("<dc:date>2026-11-17T09:30:00Z</dc:date>"));
    }

    void odtPageSetup() {
        ReportDocument d = rich_doc();
        {
            const ZArchive a = read_zip(odt_of(d, rich_assets()));
            QVERIFY2(a.ok, qPrintable(a.error));
            const auto pl = elements(a.data("styles.xml"), QStringLiteral("style:page-layout-properties"));
            QCOMPARE(int(pl.size()), 1);
            QCOMPARE(mm_of(attr(pl[0], "fo:page-width")), 210.0);
            QCOMPARE(mm_of(attr(pl[0], "fo:page-height")), 297.0);
            QCOMPARE(attr(pl[0], "style:print-orientation"), QStringLiteral("portrait"));
            QCOMPARE(mm_of(attr(pl[0], "fo:margin-left")), 15.0);
            QCOMPARE(mm_of(attr(pl[0], "fo:margin-right")), 15.0);
            QCOMPARE(mm_of(attr(pl[0], "fo:margin-top")), 18.0);
            QCOMPARE(mm_of(attr(pl[0], "fo:margin-bottom")), 18.0);
            QCOMPARE(attr(pl[0], "fo:background-color"), QStringLiteral("#ffffff"));
        }
        d.metadata.page_size = QStringLiteral("Letter");
        d.metadata.landscape = true;
        d.metadata.margin_left_mm = 20;
        d.metadata.margin_right_mm = 25;
        d.metadata.margin_top_mm = 10;
        d.metadata.margin_bottom_mm = 12;
        d.theme = report::themes::dark_corporate();
        const ZArchive a = read_zip(odt_of(d, rich_assets()));
        QVERIFY2(a.ok, qPrintable(a.error));
        const auto pl = elements(a.data("styles.xml"), QStringLiteral("style:page-layout-properties"));
        QCOMPARE(int(pl.size()), 1);
        QCOMPARE(mm_of(attr(pl[0], "fo:page-width")), 279.4);
        QCOMPARE(mm_of(attr(pl[0], "fo:page-height")), 215.9);
        QCOMPARE(attr(pl[0], "style:print-orientation"), QStringLiteral("landscape"));
        QCOMPARE(mm_of(attr(pl[0], "fo:margin-left")), 20.0);
        QCOMPARE(mm_of(attr(pl[0], "fo:margin-right")), 25.0);
        QCOMPARE(mm_of(attr(pl[0], "fo:margin-top")), 10.0);
        QCOMPARE(mm_of(attr(pl[0], "fo:margin-bottom")), 12.0);
        QCOMPARE(attr(pl[0], "fo:background-color"), QStringLiteral("#1e1e1e"));
        // Tables span the content width of the landscape page.
        const auto tables = elements(a.data("content.xml"), QStringLiteral("style:table-properties"));
        QVERIFY(!tables.isEmpty());
        QCOMPARE(mm_of(attr(tables[0], "style:width")), 279.4 - 45);
    }

    void odtHeaderFooter() {
        ReportDocument d = rich_doc();
        ZArchive a = read_zip(odt_of(d, rich_assets()));
        QVERIFY2(a.ok, qPrintable(a.error));
        QByteArray styles = a.data("styles.xml");
        QCOMPARE(count_of(styles, "style:header"), 1);
        QCOMPARE(count_of(styles, "style:footer"), 1);
        QCOMPARE(count_of(styles, "text:page-number"), 1);
        QCOMPARE(count_of(styles, "text:page-count"), 1);
        const QVector<XPara> ps = paragraphs(styles);
        // header: {title} -> the expanded title, {{period}} expanded, watermark at the right
        QVERIFY(find_para(ps, QStringLiteral("Q3 AAPL Review\t\tQ3 draft  DRAFT")) >= 0);
        QVERIFY(find_para(ps, QStringLiteral("\tPage 1 of 1")) >= 0);
        const int conf = find_para(ps, QStringLiteral("Confidential — internal use only"));
        QVERIFY(conf >= 0);
        QCOMPARE(ps[conf].style, QStringLiteral("Report_20_Confidentiality"));
        QVERIFY(styles.contains("text:style-name=\"Report_20_Watermark\""));

        // "Show page numbers" with an empty footer: "N / M" in the centre.
        d.metadata.footer_center.clear();
        d.metadata.header_left.clear();
        d.metadata.header_right.clear();
        d.metadata.watermark.clear();
        d.metadata.confidentiality.clear();
        a = read_zip(odt_of(d, rich_assets()));
        styles = a.data("styles.xml");
        QCOMPARE(count_of(styles, "style:header"), 0);
        QCOMPARE(count_of(styles, "style:header-style"), 0);
        QVERIFY(find_para(paragraphs(styles), QStringLiteral("\t1 / 1")) >= 0);
        QCOMPARE(count_of(styles, "text:page-number"), 1);
        QCOMPARE(count_of(styles, "text:page-count"), 1);

        d.metadata.show_page_numbers = false;
        a = read_zip(odt_of(d, rich_assets()));
        QCOMPARE(count_of(a.data("styles.xml"), "style:footer"), 0);
    }

    void odtBody() {
        const ReportDocument d = rich_doc();
        const ZArchive a = read_zip(odt_of(d, rich_assets()));
        QVERIFY2(a.ok, qPrintable(a.error));
        const QByteArray content = a.data("content.xml");
        const QVector<XPara> ps = paragraphs(content);
        const auto st = styles_of(content);
        auto prop = [&st](const QString& style, const char* key) { return st.value(style).value(QString::fromLatin1(key)); };

        // Document header (no cover first): title, byline with the accent rule.
        QCOMPARE(ps[0].style, QStringLiteral("Title"));
        QCOMPARE(ps[0].text, QStringLiteral("Q3 AAPL Review"));
        QCOMPARE(ps[1].style, QStringLiteral("Subtitle"));
        QCOMPARE(ps[1].text, QStringLiteral("Jane Analyst  ·  Acme Capital  ·  17 Nov 2026"));

        // Headings: outline levels and numbers (the empty heading is left out).
        QVector<XPara> hs;
        for (const XPara& p : ps)
            if (p.element == QLatin1String("text:h"))
                hs << p;
        QCOMPARE(int(hs.size()), 4);
        QCOMPARE(hs[0].text, QStringLiteral("1  Overview of AAPL"));
        QCOMPARE(hs[0].outline, 1);
        QCOMPARE(hs[0].style, QStringLiteral("Heading_20_1"));
        QCOMPARE(hs[1].text, QStringLiteral("1.1  Financials"));
        QCOMPARE(hs[1].outline, 2);
        QCOMPARE(hs[2].text, QStringLiteral("2  Outlook"));
        QCOMPARE(hs[3].text, QStringLiteral("2.1  Risks"));
        QCOMPARE(hs[3].outline, 3);

        // Markdown text: inline runs, lists (nested), links; unresolved references kept.
        QVERIFY(find_para(ps, QStringLiteral("Revenue grew strongly in Q3.")) >= 0);
        QVERIFY(find_para(ps, QStringLiteral("nested item")) >= 0);
        QVERIFY(find_para(ps, QStringLiteral("See the filing and {{missing}}.")) >= 0);
        QCOMPARE(count_of(content, "text:a"), 1);
        QCOMPARE(attr(elements(content, QStringLiteral("text:a"))[0], "xlink:href"), QStringLiteral("https://example.com/10q"));
        const auto lists = elements(content, QStringLiteral("text:list"));
        QStringList list_styles;
        for (const auto& l : lists)
            list_styles << attr(l, "text:style-name");
        QCOMPARE(list_styles, QStringList({"List_20_Bullet", "List_20_Bullet", "List_20_Number", "List_20_Bullet"}));
        QVERIFY(content.contains("<text:span text:style-name=\"T")); // bold / italic / code runs

        // Plain + caption text, quote spacing, code with line breaks and space runs.
        QCOMPARE(para_style(ps, QStringLiteral("Figures unaudited")), QStringLiteral("Caption"));
        QCOMPARE(para_style(ps, QStringLiteral("To be  or not to be")), QStringLiteral("Quotations"));
        const int code = find_para(ps, QStringLiteral("int main() {\n    return 0; // ```\n}"));
        QVERIFY(code >= 0);
        QCOMPARE(ps[code].style, QStringLiteral("Preformatted_20_Text"));

        // Market data, stats (tone ink), callout box, sparkline line.
        QVERIFY(find_para(ps, QStringLiteral("AAPL  Apple Inc.  189.50 USD  +1.25%")) >= 0);
        QVERIFY(find_para(ps, QStringLiteral("H: 190  L: 185  Vol: 52.10M")) >= 0);
        const XTable stats = table_named(content, QStringLiteral("Stats9"));
        QVERIFY(stats.found);
        QCOMPARE(int(stats.rows.size()), 2);
        QCOMPARE(stats.rows[0][0].text, QStringLiteral("P/E"));
        QCOMPARE(stats.rows[0][1].text, QStringLiteral("28.5"));
        QCOMPARE(prop(stats.rows[0][1].para_style, "fo:color"), QStringLiteral("#16a34a"));
        QCOMPARE(prop(stats.rows[0][1].para_style, "fo:text-align"), QStringLiteral("end"));
        QCOMPARE(prop(stats.rows[0][1].para_style, "fo:font-weight"), QStringLiteral("bold"));
        QCOMPARE(prop(stats.rows[1][1].para_style, "fo:color"), QStringLiteral("#dc2626"));
        QCOMPARE(stats.rows[1][2].text, QString()); // odd count: empty pair
        const XTable callout = table_named(content, QStringLiteral("Callout11"));
        QVERIFY(callout.found);
        QCOMPARE(callout.rows[0][0].text, QStringLiteral("⚠  Heads upWatch the margin.")); // heading + body paragraphs
        QCOMPARE(prop(callout.rows[0][0].style, "fo:background-color"), QStringLiteral("#fffbeb"));
        QVERIFY(find_para(ps, QStringLiteral("AAPL  189.5  -0.4%  ")) >= 0);

        // Provenance under the table.
        const int src = find_para(ps, QStringLiteral("Source: Company filings · as of ") + as_of_text());
        QVERIFY(src >= 0);
        QCOMPARE(ps[src].style, QStringLiteral("Report_20_Source"));

        // Page break: the TOC after it starts a new page; TOC entries numbered and indented.
        const int toc = find_para(ps, QStringLiteral("Table of Contents"));
        QVERIFY(toc >= 0);
        QCOMPARE(prop(ps[toc].style, "fo:break-before"), QStringLiteral("page"));
        QCOMPARE(prop(ps[toc].style, "@parent"), QStringLiteral("Contents_20_Heading"));
        QCOMPARE(ps[toc + 1].text, QStringLiteral("1  Overview of AAPL"));
        QCOMPARE(ps[toc + 1].style, QStringLiteral("Contents_20_1"));
        QCOMPARE(ps[toc + 2].text, QStringLiteral("1.1  Financials"));
        QCOMPARE(ps[toc + 2].style, QStringLiteral("Contents_20_2"));
        QCOMPARE(ps[toc + 4].text, QStringLiteral("2.1  Risks"));
        QCOMPARE(ps[toc + 4].style, QStringLiteral("Contents_20_3"));

        // Unfilled blocks are left out: the blank text, the chart without a picture.
        QVERIFY(!content.contains("Chart 7"));
        QVERIFY(!content.contains("No picture"));
        QVERIFY(!a.has(QStringLiteral("Pictures/chart_7.png")));
    }

    void odtFormattedTable() {
        const ReportDocument d = rich_doc();
        const ZArchive a = read_zip(odt_of(d, rich_assets()));
        QVERIFY2(a.ok, qPrintable(a.error));
        const QByteArray content = a.data("content.xml");
        const auto st = styles_of(content);
        auto prop = [&st](const QString& style, const char* key) { return st.value(style).value(QString::fromLatin1(key)); };

        const report::tablefmt::FormattedTable expect = report::tablefmt::format_table(
            report::table::parse(QString::fromLatin1(kTableCsv)), report::tablefmt::style_from_text(QStringLiteral("{\"totals\":\"sum\"}")),
            report::tablefmt::columns_from_text(QString::fromLatin1(kTableCols)), report::tablefmt::default_style(), QStringLiteral("#ffffff"));
        const XTable t = table_named(content, QStringLiteral("Table5"));
        QVERIFY(t.found);
        QCOMPARE(t.header_rows, 1);
        QCOMPARE(int(t.column_styles.size()), 3);
        QCOMPARE(t.rows.size(), expect.rows.size());
        for (int r = 0; r < t.rows.size(); ++r) {
            QCOMPARE(int(t.rows[r].size()), 3);
            for (int c = 0; c < 3; ++c)
                QCOMPARE(t.rows[r][c].text, expect.rows[r][c].text);
        }
        QCOMPARE(t.rows[2][1].text, QStringLiteral("($250.00)")); // currency, brackets
        QCOMPARE(t.rows[5][0].text, QStringLiteral("Total"));
        QCOMPARE(t.rows[5][1].text, QStringLiteral("$2,083.50"));
        // Widths: the default (36% first column) of the content width.
        QCOMPARE(mm_of(prop(t.column_styles[0], "style:column-width")), 180 * 0.36);
        // Header: theme header colours, bold; numbers right-aligned.
        QCOMPARE(prop(t.rows[0][0].style, "fo:background-color"), QStringLiteral("#f0f0f0"));
        QCOMPARE(prop(t.rows[0][1].para_style, "fo:font-weight"), QStringLiteral("bold"));
        QCOMPARE(prop(t.rows[0][1].para_style, "@parent"), QStringLiteral("Table_20_Heading"));
        QCOMPARE(prop(t.rows[1][1].para_style, "fo:text-align"), QStringLiteral("end"));
        QCOMPARE(prop(t.rows[1][0].para_style, "fo:text-align"), QStringLiteral("start"));
        // Colour scale on the margin column, zebra on every second body row elsewhere.
        for (int r = 1; r <= 4; ++r) {
            QVERIFY(!expect.rows[r][2].background.isEmpty());
            QCOMPARE(prop(t.rows[r][2].style, "fo:background-color"), expect.rows[r][2].background);
        }
        QVERIFY(expect.zebra);
        QCOMPARE(prop(t.rows[2][0].style, "fo:background-color"), QStringLiteral("#f8f8f8"));
        QCOMPARE(prop(t.rows[1][0].style, "fo:background-color"), QString());
        // Totals row: bold with a rule above.
        QCOMPARE(prop(t.rows[5][1].para_style, "fo:font-weight"), QStringLiteral("bold"));
        QVERIFY(prop(t.rows[5][1].style, "fo:border-top").startsWith(QLatin1String("1pt solid")));
        QVERIFY(prop(t.rows[4][1].style, "fo:border-top").startsWith(QLatin1String("0.45pt solid")));
        // Title above, note below.
        const QVector<XPara> ps = paragraphs(content);
        const int title = find_para(ps, QStringLiteral("Segment results"));
        QVERIFY(title >= 0);
        QCOMPARE(ps[title].style, QStringLiteral("Report_20_Table_20_Title"));
        QCOMPARE(para_style(ps, QStringLiteral("USD millions")), QStringLiteral("Report_20_Note"));

        // A table formatted on the GUI thread is printed as given.
        ReportDocument d2 = rich_doc();
        ef::ExportAssets assets = rich_assets();
        report::tablefmt::FormattedTable mine = expect;
        mine.rows[1][1].text = QStringLiteral("USD 1,234.50");
        assets.tables.insert(5, mine);
        const XTable t2 = table_named(read_zip(odt_of(d2, assets)).data("content.xml"), QStringLiteral("Table5"));
        QCOMPARE(t2.rows[1][1].text, QStringLiteral("USD 1,234.50"));

        // Legacy {"r_c": text} tables.
        ReportDocument legacy;
        legacy.components = {mk(3, "table", {},
                                {{"data", "{\"0_0\":\"Name\",\"0_1\":\"Value\",\"1_0\":\"Alpha\",\"1_1\":\"12\"}"},
                                 {"rows", "2"},
                                 {"cols", "2"}})};
        const XTable t3 = table_named(read_zip(odt_of(legacy, {})).data("content.xml"), QStringLiteral("Table3"));
        QVERIFY(t3.found);
        QCOMPARE(int(t3.rows.size()), 2);
        QCOMPARE(t3.rows[0][1].text, QStringLiteral("Value"));
        QCOMPARE(t3.rows[1][0].text, QStringLiteral("Alpha"));
        QCOMPARE(t3.rows[1][1].text, QStringLiteral("12"));
    }

    void odtPictures() {
        ReportDocument d = rich_doc();
        ef::ExportAssets assets = rich_assets();
        const ZArchive a = read_zip(odt_of(d, assets));
        QVERIFY2(a.ok, qPrintable(a.error));
        const QByteArray content = a.data("content.xml");
        QStringList hrefs;
        for (const auto& e : elements(content, QStringLiteral("draw:image")))
            hrefs << attr(e, "xlink:href");
        QCOMPARE(hrefs, QStringList({"Pictures/chart_6.png", "Pictures/image_8.png", "Pictures/chart_20.png"}));
        QCOMPARE(a.data("Pictures/chart_6.png"), assets.charts.value(6).bytes);
        QCOMPARE(a.data("Pictures/image_8.png"), assets.images.value(8).bytes);
        QCOMPARE(a.data("Pictures/chart_20.png"), assets.charts.value(20).bytes);
        const auto frames = elements(content, QStringLiteral("draw:frame"));
        QCOMPARE(int(frames.size()), 3);
        QCOMPARE(attr(frames[0], "text:anchor-type"), QStringLiteral("as-char"));
        QCOMPARE(mm_of(attr(frames[0], "svg:width")), 169.333); // 640 px at 96 dpi
        QCOMPARE(mm_of(attr(frames[0], "svg:height")), 84.667);
        QCOMPARE(mm_of(attr(frames[1], "svg:width")), 79.375); // config width 300 px, aspect of the picture
        QCOMPARE(mm_of(attr(frames[1], "svg:height")), 39.688);
        const QVector<XPara> ps = paragraphs(content);
        QVERIFY(para_style(ps, QStringLiteral("Quarterly revenue")).startsWith(QLatin1Char('P'))); // centred Caption

        // A picture wider than the page is capped to the content width, aspect kept; JPEG.
        ReportDocument small;
        small.components = {mk(3, "image", {}, {{"caption", "Wide"}})};
        ef::ExportAssets wide;
        ef::ImageAsset jpg;
        jpg.bytes = QByteArray("\xFF\xD8\xFF\xE0 not really a jpeg");
        jpg.ext = QStringLiteral("jpeg");
        jpg.display_px = QSizeF(2000, 1000);
        wide.images.insert(3, jpg);
        const ZArchive b = read_zip(odt_of(small, wide));
        QVERIFY2(b.ok, qPrintable(b.error));
        QVERIFY(b.has(QStringLiteral("Pictures/image_3.jpg")));
        const auto f = elements(b.data("content.xml"), QStringLiteral("draw:frame"));
        QCOMPARE(int(f.size()), 1);
        QCOMPARE(mm_of(attr(f[0], "svg:width")), 180.0);
        QCOMPARE(mm_of(attr(f[0], "svg:height")), 90.0);
        bool jpeg_listed = false;
        for (const auto& e : elements(b.data("META-INF/manifest.xml"), QStringLiteral("manifest:file-entry")))
            jpeg_listed = jpeg_listed || (attr(e, "manifest:full-path") == QLatin1String("Pictures/image_3.jpg") &&
                                          attr(e, "manifest:media-type") == QLatin1String("image/jpeg"));
        QVERIFY(jpeg_listed);
    }

    void odtCoverFirst() {
        ReportDocument d = rich_doc();
        d.components.prepend(mk(30, "cover", {},
                                {{"title", "{{symbol}} Initiation"}, {"subtitle", "Equity research"}, {"disclaimer", "Not advice."}}));
        ef::ExportAssets assets = rich_assets();
        assets.images.insert(30, png_asset(600, 200, 0, 0, 0));
        const ZArchive a = read_zip(odt_of(d, assets));
        QVERIFY2(a.ok, qPrintable(a.error));
        const QByteArray content = a.data("content.xml");
        const QVector<XPara> ps = paragraphs(content);
        const auto st = styles_of(content);
        for (const XPara& p : ps)
            QVERIFY2(p.style != QLatin1String("Title") && p.style != QLatin1String("Subtitle"), qPrintable(p.text));
        const int title = find_para(ps, QStringLiteral("AAPL Initiation"));
        QVERIFY(title >= 1);
        QCOMPARE(st.value(ps[title].style).value(QStringLiteral("@parent")), QStringLiteral("Report_20_Cover_20_Title"));
        QCOMPARE(ps[title + 1].text, QStringLiteral("Equity research"));
        QCOMPARE(ps[title + 2].text, QStringLiteral("Jane Analyst · Acme Capital"));
        QCOMPARE(ps[title + 3].text, QStringLiteral("17 Nov 2026"));
        QCOMPARE(ps[title + 4].text, QStringLiteral("Not advice."));
        // The first block after the cover starts a new page.
        QCOMPARE(ps[title + 5].text, QStringLiteral("1  Overview of AAPL"));
        QCOMPARE(st.value(ps[title + 5].style).value(QStringLiteral("fo:break-before")), QStringLiteral("page"));
        // Logo: centred, at most 60 mm wide.
        QVERIFY(a.has(QStringLiteral("Pictures/image_30.png")));
        const auto frames = elements(content, QStringLiteral("draw:frame"));
        QCOMPARE(attr(frames[0], "draw:name"), QStringLiteral("Logo 30"));
        QCOMPARE(mm_of(attr(frames[0], "svg:width")), 60.0);
        QCOMPARE(mm_of(attr(frames[0], "svg:height")), 20.0);
    }

    void odtCancel() {
        int calls = 0;
        QString err;
        const QByteArray out = report::odt::write(rich_doc(), rich_assets(), odt_opts(), [&calls] { return ++calls >= 3; }, &err);
        QVERIFY(out.isEmpty());
        QCOMPARE(err, QStringLiteral("cancelled"));
        QCOMPARE(calls, 3);
        err = QStringLiteral("stale");
        QVERIFY(!report::odt::write(rich_doc(), rich_assets(), odt_opts(), [] { return false; }, &err).isEmpty());
        QVERIFY(err.isEmpty());
    }

    // ── Markdown ────────────────────────────────────────────────────────────
    void markdownDocument() {
        const ReportDocument d = rich_doc();
        const ef::ExportAssets assets = rich_assets();
        const report::md::MarkdownOutput out = report::md::write(d, assets, {}, {});
        QVERIFY(!out.cancelled);
        const QString& md = out.markdown;
        if (!qEnvironmentVariable("FINCEPT_EXPORT_SAMPLE_DIR").isEmpty()) {
            save_sample(QStringLiteral("sample.md"), md.toUtf8());
            for (const auto& f : out.files)
                save_sample(f.first, f.second);
        }
        QCOMPARE(report::md::write(d, assets, {}, {}).markdown, md); // deterministic
        QVERIFY(!md.contains(QLatin1Char('\r')));
        QVERIFY(md.endsWith(QString::fromUtf8("\n")) && !md.endsWith(QString::fromUtf8("\n\n")));
        QVERIFY(!md.contains(QString::fromUtf8("\n\n\n")));

        QVERIFY(md.startsWith(QString::fromUtf8("---\ntitle: \"Q3 AAPL Review\"\nauthor: \"Jane Analyst\"\ncompany: \"Acme "
                                            "Capital\"\ndate: \"17 Nov 2026\"\n---\n\n# Q3 AAPL Review\n\nJane Analyst · "
                                            "Acme Capital · 17 Nov 2026\n\n")));
        // Headings: report title is H1, a level-N heading is N+1 '#', with numbers.
        QVERIFY(md.contains(QString::fromUtf8("\n## 1 Overview of AAPL\n")));
        QVERIFY(md.contains(QString::fromUtf8("\n### 1.1 Financials\n")));
        QVERIFY(md.contains(QString::fromUtf8("\n## 2 Outlook\n")));
        QVERIFY(md.contains(QString::fromUtf8("\n#### 2.1 Risks\n")));
        // Text verbatim (variables expanded, unresolved kept); caption in italics.
        QVERIFY(md.contains(QString::fromUtf8("Revenue **grew** strongly in *Q3*.\n\n- First point")));
        QVERIFY(md.contains(QString::fromUtf8("{{missing}}")));
        QVERIFY(md.contains(QString::fromUtf8("\n*Figures unaudited*\n")));
        // Table: title, header, alignment row, formatted cells, bold totals, note, provenance.
        QVERIFY(md.contains(QString::fromUtf8("**Segment results**\n\n| Segment | Revenue | Margin |\n| :--- | ---: | ---: |\n"
                                          "| Phones | $1,234.50 | 0.42 |\n| Services | ($250.00) | 0.61 |")));
        QVERIFY(md.contains(QString::fromUtf8("| **Total** | **$2,083.50** | **1.51** |\n\n*USD millions*\n\n*Source: Company "
                                          "filings · as of ") + as_of_text() + QString::fromUtf8("*\n")));
        // Pictures in the assets folder (the chart without a picture is left out).
        QVERIFY(md.contains(QString::fromUtf8("![Quarterly revenue](report_files/chart_6.png)\n\n*Quarterly revenue*")));
        QVERIFY(md.contains(QString::fromUtf8("![Store photo](report_files/image_8.png)")));
        QVERIFY(md.contains(QString::fromUtf8("**AAPL** 189.5 -0.4% ![AAPL](report_files/chart_20.png)")));
        QVERIFY(!md.contains(QString::fromUtf8("chart_7")));
        QCOMPARE(int(out.files.size()), 3);
        QCOMPARE(out.files[0].first, QStringLiteral("report_files/chart_6.png"));
        QCOMPARE(out.files[0].second, assets.charts.value(6).bytes);
        QCOMPARE(out.files[1].first, QStringLiteral("report_files/image_8.png"));
        QCOMPARE(out.files[2].first, QStringLiteral("report_files/chart_20.png"));
        // Stats, quote line, callout, quote, code fence longer than the code's backticks.
        QVERIFY(md.contains(QString::fromUtf8("**Valuation**\n\n| Metric | Value |\n| :--- | ---: |\n| P/E | 28.5 |")));
        QVERIFY(md.contains(QString::fromUtf8("**AAPL** Apple Inc. **189.50 USD** **+1.25%** · H: 190 · L: 185 · Vol: 52.10M")));
        QVERIFY(md.contains(QString::fromUtf8("> **Heads up**\n>\n> Watch the **margin**.")));
        QVERIFY(md.contains(QString::fromUtf8("\n> To be  or not to be\n")));
        QVERIFY(md.contains(QString::fromUtf8("\n````\nint main() {\n    return 0; // ```\n}\n````\n")));
        QVERIFY(md.contains(QString::fromUtf8("- Alpha\n- Beta **bold**\n- Gamma")));
        QVERIFY(md.contains(QString::fromUtf8("\n---\n\n<div style=\"page-break-after: always;\"></div>\n")));
        // TOC: nested links to GitHub's anchors.
        QVERIFY(md.contains(QString::fromUtf8("**Table of Contents**\n\n- [1 Overview of AAPL](#1-overview-of-aapl)\n"
                                          "  - [1.1 Financials](#11-financials)\n- [2 Outlook](#2-outlook)\n"
                                          "    - [2.1 Risks](#21-risks)\n")));
    }

    void markdownInlineAndOptions() {
        report::md::MarkdownOptions opt;
        opt.images = report::md::MarkdownOptions::Images::Inline;
        opt.front_matter = false;
        const ef::ExportAssets assets = rich_assets();
        const report::md::MarkdownOutput out = report::md::write(rich_doc(), assets, opt, {});
        QVERIFY(out.files.isEmpty());
        QVERIFY(out.markdown.startsWith(QString::fromUtf8("# Q3 AAPL Review\n")));
        QVERIFY(out.markdown.contains(QString::fromUtf8("![Quarterly revenue](data:image/png;base64,") +
                                      QString::fromLatin1(assets.charts.value(6).bytes.toBase64()) + QLatin1Char(')')));

        // Folder with a trailing slash and a space: one separator, encoded in the link.
        opt.images = report::md::MarkdownOptions::Images::Folder;
        opt.assets_dir = QStringLiteral("my files/");
        const report::md::MarkdownOutput f = report::md::write(rich_doc(), assets, opt, {});
        QVERIFY(f.markdown.contains(QString::fromUtf8("](my%20files/chart_6.png)")));
        QCOMPARE(f.files[0].first, QStringLiteral("my files/chart_6.png"));
    }

    void markdownEscapingAndAnchors() {
        ReportDocument d;
        d.metadata.title = QStringLiteral("He said \"hi\" \\ ok");
        d.metadata.author = QStringLiteral("A*B");
        d.metadata.company.clear();
        d.metadata.date.clear();
        d.components = {
            mk(1, "heading", "Intro", {{"level", "1"}}),
            mk(2, "heading", "Intro", {{"level", "1"}}),
            mk(3, "text", "## Notes\n\nplain"),
            mk(4, "heading", "Notes", {{"level", "2"}}),
            mk(5, "toc"),
            mk(6, "table", {}, {{"csv", "Key,Value|\"a|b\",\"x\ny\"|*c*,2"}}),
            mk(7, "chart", {}, {{"data", "1,2"}}),
            mk(8, "code", "plain code"),
            mk(9, "page_break"),
        };
        const report::md::MarkdownOutput out = report::md::write(d, {}, {}, {});
        const QString& md = out.markdown;
        QVERIFY(md.startsWith(QString::fromUtf8("---\ntitle: \"He said \\\"hi\\\" \\\\ ok\"\nauthor: \"A*B\"\n---\n\n")));
        QVERIFY(md.contains(QString::fromUtf8("\nA\\*B\n")));
        // Duplicate headings get -1; headings inside text blocks count too.
        QVERIFY(md.contains(QString::fromUtf8("- [Intro](#intro)\n- [Intro](#intro-1)\n  - [Notes](#notes-1)")));
        // Pipe table: '|' escaped, line break as <br>, emphasis characters escaped.
        QVERIFY(md.contains(QString::fromUtf8("| a\\|b | x<br>y |")));
        QVERIFY(md.contains(QString::fromUtf8("| \\*c\\* | 2 |")));
        QVERIFY(md.contains(QString::fromUtf8("```\nplain code\n```")));
        QVERIFY(!md.contains(QString::fromUtf8("page-break"))); // a trailing page break prints nothing
        QVERIFY(!md.contains(QString::fromUtf8("](report_files"))); // chart without a picture left out
    }

    void markdownCoverAndCancel() {
        ReportDocument d = rich_doc();
        d.components.prepend(mk(30, "cover", {}, {{"subtitle", "Equity research"}, {"disclaimer", "Not advice."}}));
        const report::md::MarkdownOutput out = report::md::write(d, rich_assets(), {}, {});
        QVERIFY(out.markdown.contains(QString::fromUtf8("---\n\n# Q3 AAPL Review\n\n*Equity research*\n\nJane Analyst · Acme "
                                                    "Capital\n\n17 Nov 2026\n\n*Not advice.*\n\n---\n\n## 1 Overview of AAPL")));
        QCOMPARE(int(out.markdown.count(QString::fromUtf8("\n# "))), 1); // the cover's title replaces the header

        int calls = 0;
        const report::md::MarkdownOutput c = report::md::write(rich_doc(), rich_assets(), {}, [&calls] { return ++calls >= 2; });
        QVERIFY(c.cancelled);
        QVERIFY(c.markdown.isEmpty());
        QVERIFY(c.files.isEmpty());
    }

    void unfilledRules() {
        const ef::ExportAssets none;
        ef::ExportAssets with;
        with.charts.insert(1, png_asset(2, 2, 0, 0, 0));
        with.images.insert(1, png_asset(2, 2, 0, 0, 0));
        QVERIFY(ef::is_unfilled(mk(1, "text", " \n "), none));
        QVERIFY(!ef::is_unfilled(mk(1, "text", "x"), none));
        QVERIFY(ef::is_unfilled(mk(1, "chart", {}, {{"data", "1,2"}}), none));
        QVERIFY(!ef::is_unfilled(mk(1, "chart", {}, {{"data", "1,2"}}), with));
        QVERIFY(ef::is_unfilled(mk(1, "chart", {}, {}), with));
        QVERIFY(ef::is_unfilled(mk(1, "image"), none));
        QVERIFY(!ef::is_unfilled(mk(1, "image"), with));
        QVERIFY(ef::is_unfilled(mk(1, "market_data", {}, {{"symbol", "AAPL"}}), none));
        QVERIFY(ef::is_unfilled(mk(1, "callout"), none));
        QVERIFY(!ef::is_unfilled(mk(1, "callout", {}, {{"heading", "Note"}}), none));
        QVERIFY(ef::is_unfilled(mk(1, "table", {}, {{"data", "{}"}}), none));
        QVERIFY(!ef::is_unfilled(mk(1, "toc"), none));
        QVERIFY(!ef::is_unfilled(mk(1, "cover"), none));
        QCOMPARE(ef::image_ext(png_asset(1, 1, 0, 0, 0)), QStringLiteral("png"));
        ef::ImageAsset sniff;
        sniff.bytes = QByteArray("\xFF\xD8\xFF", 3);
        QCOMPARE(ef::image_ext(sniff), QStringLiteral("jpg"));
        QCOMPARE(ef::image_mime(sniff), QStringLiteral("image/jpeg"));
    }
};

QTEST_GUILESS_MAIN(TestReportExportFormats)
#include "tst_report_export_formats.moc"
