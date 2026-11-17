// ReportZipWriter.cpp — see ReportZipWriter.h.

#include "core/report/ReportZipWriter.h"

#include <QDate>
#include <QTime>

#include <algorithm>
#include <cstring>

// Qt Core bundles zlib and exports it with a `z_` prefix; the app links Qt6::ZlibPrivate on
// Windows (see trading/instruments/InstrumentDecompress.cpp), the system zlib elsewhere.
#if defined(_WIN32) && !defined(Z_PREFIX)
#    define Z_PREFIX
#endif
#include <zlib.h>

namespace fincept::report::zip {

namespace {

constexpr quint32 kRzwLocalSig = 0x04034b50u;
constexpr quint32 kRzwCentralSig = 0x02014b50u;
constexpr quint32 kRzwEndSig = 0x06054b50u;
constexpr quint16 kRzwUtf8Flag = 0x0800u;  // general purpose bit 11: names are UTF-8
constexpr quint64 kRzwMaxBytes = 0xFFFFFFFEull; // no ZIP64
constexpr int kRzwMaxEntries = 0xFFFF;

void rzw_u16(QByteArray& b, quint32 v) {
    b.append(static_cast<char>(v & 0xFFu));
    b.append(static_cast<char>((v >> 8) & 0xFFu));
}

void rzw_u32(QByteArray& b, quint32 v) {
    rzw_u16(b, v & 0xFFFFu);
    rzw_u16(b, (v >> 16) & 0xFFFFu);
}

quint32 rzw_crc(const QByteArray& data) {
    uLong crc = crc32(0L, Z_NULL, 0);
    const auto* p = reinterpret_cast<const Bytef*>(data.constData());
    qsizetype left = data.size();
    while (left > 0) {
        const uInt n = static_cast<uInt>(std::min<qsizetype>(left, qsizetype(1) << 30));
        crc = crc32(crc, p, n);
        p += n;
        left -= n;
    }
    return static_cast<quint32>(crc);
}

// Raw deflate (no zlib header / trailer), level 6. Empty on failure.
QByteArray rzw_deflate(const QByteArray& data) {
    z_stream s;
    std::memset(&s, 0, sizeof(s));
    if (deflateInit2(&s, 6, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY) != Z_OK)
        return {};
    QByteArray out;
    out.resize(static_cast<qsizetype>(deflateBound(&s, static_cast<uLong>(data.size()))) + 16);
    s.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.constData()));
    s.avail_in = static_cast<uInt>(data.size());
    s.next_out = reinterpret_cast<Bytef*>(out.data());
    s.avail_out = static_cast<uInt>(out.size());
    const int rc = deflate(&s, Z_FINISH);
    const qsizetype produced = static_cast<qsizetype>(s.total_out);
    deflateEnd(&s);
    if (rc != Z_STREAM_END)
        return {};
    out.resize(produced);
    return out;
}

} // namespace

ZipWriter::ZipWriter(const QDateTime& stamp) {
    QDate d(1980, 1, 1);
    QTime t(0, 0);
    if (stamp.isValid()) {
        d = stamp.date();
        t = stamp.time();
    }
    if (d.year() < 1980) {
        d = QDate(1980, 1, 1);
        t = QTime(0, 0);
    } else if (d.year() > 2107) {
        d = QDate(2107, 12, 31);
        t = QTime(23, 59, 58);
    }
    dos_date_ = static_cast<quint16>(((d.year() - 1980) << 9) | (d.month() << 5) | d.day());
    dos_time_ = static_cast<quint16>((t.hour() << 11) | (t.minute() << 5) | (t.second() / 2));
}

void ZipWriter::add_stored(const QString& name, const QByteArray& data) {
    add(name, data, false);
}

void ZipWriter::add_deflated(const QString& name, const QByteArray& data) {
    add(name, data, true);
}

void ZipWriter::add(const QString& name, const QByteArray& data, bool deflate) {
    if (finished_ || overflow_)
        return;
    const QByteArray n = name.toUtf8();
    if (n.isEmpty() || n.size() > 0xFFFF)
        return;
    for (const Entry& e : entries_)
        if (e.name == n)
            return; // first wins
    if (static_cast<quint64>(data.size()) > kRzwMaxBytes || entries_.size() >= kRzwMaxEntries) {
        overflow_ = true;
        return;
    }
    Entry e;
    e.name = n;
    e.size = static_cast<quint32>(data.size());
    e.crc = rzw_crc(data);
    QByteArray packed;
    if (deflate && !data.isEmpty() && data.size() < (qsizetype(1) << 30)) { // one-shot deflate: keep the bound in range
        packed = rzw_deflate(data);
        if (!packed.isEmpty() && packed.size() < data.size())
            e.method = 8;
    }
    const QByteArray& body = e.method == 8 ? packed : data;
    e.compressed = static_cast<quint32>(body.size());
    if (static_cast<quint64>(out_.size()) + 30 + static_cast<quint64>(n.size()) + static_cast<quint64>(body.size()) >
        kRzwMaxBytes) {
        overflow_ = true;
        return;
    }
    e.offset = static_cast<quint32>(out_.size());

    rzw_u32(out_, kRzwLocalSig);
    rzw_u16(out_, e.method == 8 ? 20 : 10); // version needed to extract
    rzw_u16(out_, kRzwUtf8Flag);
    rzw_u16(out_, e.method);
    rzw_u16(out_, dos_time_);
    rzw_u16(out_, dos_date_);
    rzw_u32(out_, e.crc);
    rzw_u32(out_, e.compressed);
    rzw_u32(out_, e.size);
    rzw_u16(out_, static_cast<quint32>(n.size()));
    rzw_u16(out_, 0); // no extra field
    out_.append(n);
    out_.append(body);
    entries_.append(e);
}

QByteArray ZipWriter::finish() {
    if (overflow_)
        return {};
    if (finished_)
        return out_;
    const quint64 cd_start = static_cast<quint64>(out_.size());
    for (const Entry& e : entries_) {
        rzw_u32(out_, kRzwCentralSig);
        rzw_u16(out_, 20);                      // version made by: MS-DOS host, spec 2.0
        rzw_u16(out_, e.method == 8 ? 20 : 10); // version needed to extract
        rzw_u16(out_, kRzwUtf8Flag);
        rzw_u16(out_, e.method);
        rzw_u16(out_, dos_time_);
        rzw_u16(out_, dos_date_);
        rzw_u32(out_, e.crc);
        rzw_u32(out_, e.compressed);
        rzw_u32(out_, e.size);
        rzw_u16(out_, static_cast<quint32>(e.name.size()));
        rzw_u16(out_, 0); // extra field length
        rzw_u16(out_, 0); // comment length
        rzw_u16(out_, 0); // disk number start
        rzw_u16(out_, 0); // internal attributes
        rzw_u32(out_, 0); // external attributes
        rzw_u32(out_, e.offset);
        out_.append(e.name);
    }
    const quint64 cd_size = static_cast<quint64>(out_.size()) - cd_start;
    if (static_cast<quint64>(out_.size()) + 22 > kRzwMaxBytes) {
        overflow_ = true;
        out_.clear();
        return {};
    }
    rzw_u32(out_, kRzwEndSig);
    rzw_u16(out_, 0); // this disk
    rzw_u16(out_, 0); // disk of the central directory
    rzw_u16(out_, static_cast<quint32>(entries_.size()));
    rzw_u16(out_, static_cast<quint32>(entries_.size()));
    rzw_u32(out_, static_cast<quint32>(cd_size));
    rzw_u32(out_, static_cast<quint32>(cd_start));
    rzw_u16(out_, 0); // comment length
    finished_ = true;
    return out_;
}

} // namespace fincept::report::zip
