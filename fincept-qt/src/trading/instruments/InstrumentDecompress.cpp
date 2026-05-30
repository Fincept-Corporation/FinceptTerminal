#include "trading/instruments/InstrumentDecompress.h"

#include "core/logging/Logger.h"

#include <zlib.h>

#include <cstring>

namespace fincept::trading::decompress {

namespace {

// Inflate `in` using the given window-bits setting. 15|16 → gzip, -15 → raw deflate.
QByteArray inflate_stream(const QByteArray& in, int window_bits) {
    if (in.isEmpty())
        return {};

    z_stream s;
    std::memset(&s, 0, sizeof(s));
    if (inflateInit2(&s, window_bits) != Z_OK)
        return {};

    s.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(in.constData()));
    s.avail_in = static_cast<uInt>(in.size());

    QByteArray out;
    char chunk[32768];
    int rc = Z_OK;
    do {
        s.next_out = reinterpret_cast<Bytef*>(chunk);
        s.avail_out = sizeof(chunk);
        rc = inflate(&s, Z_NO_FLUSH);
        if (rc != Z_OK && rc != Z_STREAM_END && rc != Z_BUF_ERROR) {
            inflateEnd(&s);
            return {};
        }
        const int produced = static_cast<int>(sizeof(chunk) - s.avail_out);
        if (produced > 0)
            out.append(chunk, produced);
        // Z_BUF_ERROR with no progress and no input left → done.
        if (rc == Z_BUF_ERROR && s.avail_in == 0 && produced == 0)
            break;
    } while (rc != Z_STREAM_END);

    inflateEnd(&s);
    return out;
}

quint16 le16(const QByteArray& b, int off) {
    return static_cast<quint16>(static_cast<unsigned char>(b[off])) |
           (static_cast<quint16>(static_cast<unsigned char>(b[off + 1])) << 8);
}

quint32 le32(const QByteArray& b, int off) {
    return static_cast<quint32>(static_cast<unsigned char>(b[off])) |
           (static_cast<quint32>(static_cast<unsigned char>(b[off + 1])) << 8) |
           (static_cast<quint32>(static_cast<unsigned char>(b[off + 2])) << 16) |
           (static_cast<quint32>(static_cast<unsigned char>(b[off + 3])) << 24);
}

} // namespace

QByteArray gunzip(const QByteArray& gz) {
    QByteArray out = inflate_stream(gz, 15 + 16);
    if (out.isEmpty())
        LOG_WARN("InstrumentDecompress", "gunzip produced no output");
    return out;
}

QByteArray unzip_first_entry(const QByteArray& zip) {
    // Local file header: "PK\x03\x04", then at fixed offsets:
    //   8  compression method (u16)   18 compressed size (u32)
    //   26 file name length (u16)     28 extra field length (u16)
    //   30 + nameLen + extraLen → start of compressed data.
    if (zip.size() < 30 || !zip.startsWith("PK\x03\x04")) {
        LOG_WARN("InstrumentDecompress", "not a zip (bad local header signature)");
        return {};
    }
    const quint16 method = le16(zip, 8);
    const quint16 flags = le16(zip, 6);
    quint32 comp_size = le32(zip, 18);
    const quint16 name_len = le16(zip, 26);
    const quint16 extra_len = le16(zip, 28);
    const int data_off = 30 + name_len + extra_len;
    if (data_off > zip.size()) {
        LOG_WARN("InstrumentDecompress", "zip data offset past end");
        return {};
    }

    // When a data descriptor is used (bit 3), the local header size may be 0;
    // hand the rest of the buffer to the inflater and let Z_STREAM_END stop it.
    if (comp_size == 0 || (flags & 0x08))
        comp_size = static_cast<quint32>(zip.size() - data_off);
    if (data_off + static_cast<int>(comp_size) > zip.size())
        comp_size = static_cast<quint32>(zip.size() - data_off);

    const QByteArray payload = zip.mid(data_off, static_cast<int>(comp_size));
    if (method == 0) // stored
        return payload;
    if (method == 8) // deflate
        return inflate_stream(payload, -15);

    LOG_WARN("InstrumentDecompress", QString("unsupported zip method %1").arg(method));
    return {};
}

} // namespace fincept::trading::decompress
