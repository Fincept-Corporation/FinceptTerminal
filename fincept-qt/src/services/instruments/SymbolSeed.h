#pragma once
// SymbolSeed — the bundled seed of the local instrument index (symbol system WP2),
// so search works before the backend symbol master (WP8) exists and offline.
//
// Built at run time from:
//   * WP1's reference tables (core/instrument/VenueSymbols): every index with a Yahoo
//     symbol, every continuous future, every crypto asset (BASE-USD), plus FX majors,
//     USD crosses and common crosses generated through to_yahoo();
//   * a curated company list: the built-in companies of the news symbol directory
//     (services/news/NewsTickerTagger.cpp kNtBuiltins, ~220 names) with full names,
//     primary-listing flags, home listings of the ADRs in that list, NSE + BSE listings
//     of the Indian names, a few ISINs (Luhn-checked by the unit test) and the known
//     renames (TATAMOTORS -> TMPV.NS, FB -> META, RDS-A / RDS-B -> SHEL);
//   * a handful of major ETFs.
// Pure and deterministic (Qt Core + WP1 only, no I/O); safe on any thread.
//
// InstrumentIndex applies the seed once per kSymbolSeedVersion with
// SymbolMasterRepository::Source::Seed, which never overrides backend-master data.

#include "core/instrument/Instrument.h"

#include <QHash>
#include <QString>
#include <QVector>

namespace fincept::services {

/// Bump whenever the seed content changes: the index re-applies it once per version.
inline constexpr char kSymbolSeedVersion[] = "2026-11-16.1";

struct SymbolSeedStats {
    int indices = 0;
    int futures = 0;
    int crypto = 0;
    int fx = 0;
    int companies = 0; ///< equities, ADRs, ETFs and renamed records
    int skipped = 0;   ///< table rows without a valid Yahoo form (not seeded)
    int total() const { return indices + futures + crypto + fx + companies; }
};

/// Every seed record, ids filled (instrument_id_for). Built once, then copied.
QVector<core::instrument::Instrument> builtin_symbol_seed(SymbolSeedStats* stats = nullptr);

/// Last day a seeded former symbol was used, keyed for
/// SymbolMasterRepository::UpsertOptions::former_valid_to ("XNSE:TMPV|TATAMOTORS" -> "2025-10-24").
QHash<QString, QString> builtin_symbol_seed_former_dates();

} // namespace fincept::services
