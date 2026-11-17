#pragma once
// WatchlistImportExport — parsing of watchlist import files and the JSON list
// format. Pure (Qt Core); validation against the market (symbol resolution)
// is the screen's job.
//
// Import accepts
//   * CSV / TSV / semicolon-separated text with a header naming a SYMBOL (or
//     TICKER / CODE) column, plus optional NAME, NOTES, GROUP, EXCHANGE;
//   * a plain one-symbol-per-line list (no header);
//   * JSON: {"symbols": [{"symbol": "AAPL", "name": ..., "notes": ..., "group": ...}]}
//     (what watchlist_export_json writes), or a bare array of symbols / objects.

#include "storage/repositories/WatchlistRepository.h"

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVector>

namespace fincept::screens {

struct WatchlistImportEntry {
    QString symbol; ///< as written in the file, upper-cased and trimmed
    QString name;
    QString notes;
    QString group;
    QString exchange;
};

struct WatchlistImportParse {
    QVector<WatchlistImportEntry> entries; ///< ticker-shaped, de-duplicated, file order
    QStringList invalid;                   ///< values that are not ticker-shaped
    int duplicates = 0;                    ///< repeated symbols inside the file
    QString error;                         ///< set when the file could not be read at all
    QString list_name;                     ///< JSON: the exported list's name (may be empty)
};

/// Parse an import file. `file_name` picks JSON by extension (".json"),
/// otherwise the content decides.
WatchlistImportParse watchlist_parse_import(const QByteArray& bytes, const QString& file_name);

/// A symbol as typed is plausible: letters / digits plus ^ . - = & _ (Yahoo forms), at most 25 chars.
bool watchlist_is_ticker_shaped(const QString& symbol);

/// Split what was typed in the ADD box ("AAPL, MSFT TSLA; NVDA") into upper-cased parts.
QStringList watchlist_split_symbols(const QString& text);

/// The list as JSON (format "fincept-watchlist", version 1), re-importable.
QByteArray watchlist_export_json(const fincept::Watchlist& list, const QVector<fincept::WatchlistStock>& stocks);

} // namespace fincept::screens
