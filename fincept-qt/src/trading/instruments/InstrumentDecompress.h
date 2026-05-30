#pragma once
// InstrumentDecompress — minimal zlib-backed decompression for instrument masters.
// Upstox ships gzip JSON; Shoonya ships per-exchange .txt.zip (single entry).
// Qt's qUncompress only handles its own length-prefixed zlib stream, so we use
// zlib directly.

#include <QByteArray>

namespace fincept::trading::decompress {

/// Inflate a gzip stream (RFC 1952). Returns {} on error/empty.
QByteArray gunzip(const QByteArray& gz);

/// Extract the FIRST entry of a ZIP archive (sufficient for Noren symbol zips
/// which contain a single "<EX>_symbols.txt"). Supports stored (method 0) and
/// deflate (method 8). Returns {} on error/empty.
QByteArray unzip_first_entry(const QByteArray& zip);

} // namespace fincept::trading::decompress
