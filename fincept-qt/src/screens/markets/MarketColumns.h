#pragma once
// MarketColumns — the Markets panel column registry (spec 3.3): ids, headers,
// kinds, default visibility, widths, auto-hide order, legacy-code mapping and
// which asset classes a column applies to. Data only (Qt Core + value types):
// MarketPanelModel builds cells from it, MarketPanelView sizes and hides
// sections from it, MarketPanelStore maps saved legacy column codes through it.

#include "core/instrument/Instrument.h"
#include "services/markets/MarketDataTypes.h"

#include <QString>
#include <QStringList>
#include <QStringView>
#include <QVector>

namespace fincept::screens::mcols {

enum class Kind { Name, Text, Price, Change, HeatPct, Spark, RangeBar, Volume, RelVolume, State };

struct ColumnDef {
    const char* id;
    const char* header; ///< translated with QCoreApplication::translate("MarketColumns", header)
    const char* tip;    ///< translated the same way
    Kind kind;
    bool default_visible;
    int width_chars;    ///< default width in average characters (0 = fixed_px / stretch)
    int fixed_px;       ///< default width in px at the default font (spark 96, range bars 150); 0 = from chars
    int auto_hide_rank; ///< lower hides first when the panel is narrow; -1 = never auto-hidden
    bool internal = false; ///< not offered in the column chooser (the MOVERS view's PANEL column)
};

/// Logical column indices (the model has every column; the view hides).
enum Column : int {
    Name = 0,
    Ticker,
    Last,
    Change,
    ChangePct,
    Spark,
    DayRange,
    Volume,
    RelVolume,
    High,
    Low,
    Open,
    PrevClose,
    Range52w,
    State,
    Panel, ///< internal: the panel a symbol comes from (MOVERS)
    Count
};

const QVector<ColumnDef>& all();
inline int count() {
    return static_cast<int>(Column::Count);
}
const ColumnDef& def(int column);
/// Logical index of `id`, -1 when unknown.
int index_of(QStringView id);
QString id_at(int column);
QString header(int column);
QString tooltip(int column);
/// NAME and LAST cannot be hidden.
bool locked(int column);
QStringList default_ids();

/// Legacy column code (SYMBOL, LAST, CHG, CHG%, HIGH, LOW, VOL, OPEN, TICKER) -> id;
/// empty for BID / ASK / NAME and unknown codes.
QString from_legacy_code(const QString& code);
/// id -> legacy code, empty when the id has none.
QString to_legacy_code(const QString& id);
/// A saved legacy list -> ids: the old default becomes the new default; any other list is
/// kept as mapped, in order, with NAME and LAST guaranteed (first and second).
QStringList ids_from_legacy(const QStringList& codes);
/// The legacy codes of `ids` that have one (written as "column_order").
QStringList legacy_from_ids(const QStringList& ids);
/// `ids` cleaned: unknown ids dropped, duplicates dropped, NAME first and LAST present.
QStringList sanitize_ids(const QStringList& ids);

/// Does `column` apply to an instrument of `cls` with quote `q` (spec 3.7)? A cell that
/// does not apply shows "n/a". volume / rel_volume: never for FX; for an index only when
/// the feed reports a volume; rel_volume also needs an average volume.
bool applies(int column, core::instrument::AssetClass cls, const services::QuoteData& q);
/// "currency pairs", "indices", "futures", "crypto", ... (translated) for "Not reported for %1."
QString class_word(core::instrument::AssetClass cls);

} // namespace fincept::screens::mcols
