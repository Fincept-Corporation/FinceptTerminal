#pragma once
#include "core/symbol/SymbolGroup.h"
#include "core/symbol/SymbolRef.h"

#include <QMimeData>
#include <QString>

namespace fincept {

/// MIME type used for cross-panel drag/drop of securities. Any panel that
/// produces a drag of a symbol tags its QMimeData with this type; any panel
/// that wants to accept a symbol registers it as an accepted type in its
/// dragEnterEvent. The payload is a UTF-8 JSON blob of SymbolRef plus an
/// optional `source_group` field so receivers can do things like
/// "came from Group A — match colour".
namespace symbol_mime {

inline constexpr const char* kMimeType = "application/x-fincept-symbol";

/// Stuff a SymbolRef into an owned QMimeData. Caller takes ownership of
/// the returned pointer (or passes it to QDrag::setMimeData, which does).
QMimeData* pack(const SymbolRef& ref, SymbolGroup source_group = SymbolGroup::None);

/// Returns the SymbolRef embedded in the mime data, or an invalid ref if
/// the blob is missing or malformed.
SymbolRef unpack(const QMimeData* mime);

/// Source group encoded alongside the symbol, or SymbolGroup::None.
SymbolGroup unpack_source_group(const QMimeData* mime);

/// Convenience: does this mime data carry a symbol drag?
bool has_symbol(const QMimeData* mime);

} // namespace symbol_mime

} // namespace fincept
