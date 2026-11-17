#pragma once
// MarketsSymbolEntry — the ONE place the Markets screen touches the shared symbol
// search (ui::SymbolSearchBox) and the instrument resolver (spec 4.7). Every other
// Markets file goes through these functions, so adopting the final widget is a change
// in MarketsSymbolEntry.cpp only (MARKETS_USE_SYMBOL_SEARCH_BOX there).
//
// This header deliberately includes neither SymbolSearchBox.h nor InstrumentResolver.h.
//
// Rules: the stored symbol is the canonical market-data key (Instrument::symbol, the
// key of market:quote:<SYM>); the UI shows core::instrument::display_symbol. No suffix
// / prefix tables live in screens/markets.

#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>

class QObject;
class QWidget;

namespace fincept::screens {

struct MarketsSymbolPick {
    QString symbol;      ///< canonical market-data key ("RELIANCE.NS", "JPY=X", "^NSEI")
    QString display;     ///< core::instrument::display_symbol(symbol)
    QString name;        ///< instrument name when known
    QString exchange;    ///< venue id / exchange when known
    QString asset_class; ///< slug ("equity", "index", "fx", ...)
};

struct MarketsSymbolEntryOptions {
    bool multi = false;         ///< comma / newline separated lists
    bool compact = true;        ///< dense single line (~24 px: panel add rows, command bar)
    bool clear_on_pick = true;  ///< add-box behaviour
    QString placeholder;
    QString recent_key;         ///< recent-picks list shown on focus with an empty box; empty = the box's default
};

/// A symbol entry widget. `on_picked` gets the resolved symbols (one for a single pick);
/// `on_unresolved` the texts that could not be resolved, with a plain message.
QWidget* markets_make_symbol_entry(QWidget* parent, const MarketsSymbolEntryOptions& options,
                                   std::function<void(const QVector<MarketsSymbolPick>&)> on_picked,
                                   std::function<void(const QStringList& texts, const QString& message)> on_unresolved);
/// Focus the entry and select its text.
void markets_symbol_entry_focus(QWidget* entry);
void markets_symbol_entry_set_text(QWidget* entry, const QString& text);
void markets_symbol_entry_set_placeholder(QWidget* entry, const QString& text);
/// True when the entry (or its line edit) has the keyboard focus.
bool markets_symbol_entry_has_focus(QWidget* entry);
/// True while the entry's result list is open (Esc then belongs to the list).
bool markets_symbol_entry_popup_visible(QWidget* entry);

/// Saved symbols: canonicalise and follow renames through the resolver (local index
/// only, no network); `per_symbol` runs once per symbol on `context`'s thread.
/// Without the resolver in the build this does nothing.
void markets_resolve_stored(const QStringList& symbols, QObject* context,
                            std::function<void(const QString& stored, const MarketsSymbolPick& now, const QString& note,
                                               bool renamed, bool inactive)>
                                per_symbol);
/// Teach the symbol index what the user picked.
void markets_learn(const MarketsSymbolPick& pick);

} // namespace fincept::screens
