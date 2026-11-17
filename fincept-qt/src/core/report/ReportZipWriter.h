#pragma once
// ReportZipWriter.h — a minimal, deterministic ZIP writer for Report Builder exports (.odt).
//
// Raw deflate (zlib, level 6) or stored entries, CRC-32 from zlib, UTF-8 names (general
// purpose flag bit 11), one fixed DOS timestamp for every entry (1980-01-01 00:00 unless a
// time is passed in), no data descriptors, no extra fields, no ZIP64: the same input gives
// the same bytes. Entries keep the order they were added in (an .odt adds its `mimetype`
// first, stored). Qt's QZipWriter is deliberately not used: it is only reachable through
// optional private headers.
//
// Limits: at most 65535 entries and 4 GiB in all; past that finish() returns an empty
// array. A name added twice keeps its first entry. Qt Core + zlib only.

#include <QByteArray>
#include <QDateTime>
#include <QString>
#include <QVector>

namespace fincept::report::zip {

class ZipWriter {
  public:
    /// `stamp`: the modification time written for every entry (its own date and time
    /// fields, read as DOS local time); invalid = 1980-01-01 00:00.
    explicit ZipWriter(const QDateTime& stamp = {});

    /// Adds `data` uncompressed (method 0).
    void add_stored(const QString& name, const QByteArray& data);
    /// Adds `data` raw-deflated (method 8), or stored when deflate does not make it smaller.
    void add_deflated(const QString& name, const QByteArray& data);

    /// The archive (local entries, central directory, end record). Further calls return
    /// the same bytes; nothing can be added afterwards. Empty when a limit was exceeded.
    QByteArray finish();

  private:
    struct Entry {
        QByteArray name; ///< UTF-8
        quint32 crc = 0;
        quint32 compressed = 0;
        quint32 size = 0;
        quint16 method = 0;
        quint32 offset = 0;
    };
    void add(const QString& name, const QByteArray& data, bool deflate);

    QByteArray out_;
    QVector<Entry> entries_;
    quint16 dos_time_ = 0;
    quint16 dos_date_ = 0;
    bool overflow_ = false;
    bool finished_ = false;
};

} // namespace fincept::report::zip
