#pragma once
// EquitySymbolBox — the ONE place the Equity Trading screen gets its symbol entry
// boxes (the command-bar symbol selector and the watchlist "add symbol" box), plus
// the two instrument facts the screen needs from the symbol system.
//
// The terminal's shared picker is ui::SymbolSearchBox over services::InstrumentResolver
// (src/ui/widgets/symbol, src/services/instruments) — what make_symbol_box() returns.
// EquitySymbolBox below is the local stand-in this screen was built against while the
// shared component was being written: the same API subset and signals, resolving text
// through core::instrument and services::InstrumentIndex only. It stays as the
// fallback (FINCEPT_EQ_SHARED_SYMBOL_BOX 0) for a build without the shared sources.
//
// The one line: FINCEPT_EQ_SHARED_SYMBOL_BOX 1 = ui::SymbolSearchBox + InstrumentResolver
// (trading_meta_for, complete_instrument, resolve_stored); 0 = the local fallback.
#ifndef FINCEPT_EQ_SHARED_SYMBOL_BOX
#define FINCEPT_EQ_SHARED_SYMBOL_BOX 1
#endif

#include "core/instrument/Instrument.h"
#include "core/instrument/InstrumentRank.h"
#include "services/instruments/InstrumentResolver.h" // ResolveOptions / TradingMeta (header-only use)

#include <QPointer>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

#include <functional>
#include <utility>

#if FINCEPT_EQ_SHARED_SYMBOL_BOX
#    include "ui/widgets/symbol/SymbolSearchBox.h"
#endif

class QLineEdit;
class QCompleter;
class QStandardItemModel;
class QTimer;

namespace fincept::screens::equity {

/// Local stand-in for ui::SymbolSearchBox (same API subset, same signals). It never
/// hands out typed text as a symbol: it emits a resolved core::instrument::Instrument
/// whose `symbol` is the canonical (Yahoo-form) market-data key.
///   * Typing lists matches from the local instrument index, plus the canonical
///     candidates the parser derives from the text (RELIANCE.NS, ^NSEI, EURUSD=X, …).
///   * Enter / click picks the highlighted row. Enter with no list resolves the text:
///     a fully specified symbol (VOD.L, BTC-USD, NSE:RELIANCE) or a single candidate
///     is picked; several candidates open the list ("did you mean"); nothing →
///     unresolved(), or text_committed_raw() when set_raw_text_allowed(true).
class EquitySymbolBox : public QWidget {
    Q_OBJECT
  public:
    enum class RowState : quint8 { Pickable, Marked, Disabled };
    struct RowCheck {
        RowState state = RowState::Pickable;
        QString reason;
        RowCheck() = default;
        RowCheck(RowState s, QString r = {}) : state(s), reason(std::move(r)) {}
    };
    using RowFilter = std::function<RowCheck(const fincept::core::instrument::Instrument&)>;

    explicit EquitySymbolBox(QWidget* parent = nullptr);
    ~EquitySymbolBox() override;

    void set_placeholder(const QString& text);
    void set_options(const fincept::services::ResolveOptions& opts);
    void set_multi(bool on) { multi_ = on; } ///< accepted for API parity; one item at a time here
    void set_clear_on_pick(bool on) { clear_on_pick_ = on; }
    void set_text(const QString& text);
    QString text() const;
    void focus_and_select();
    void set_row_filter(RowFilter filter) { row_filter_ = std::move(filter); }
    void set_compact(bool on);
    void set_raw_text_allowed(bool on) { raw_allowed_ = on; }
    void set_recent_key(const QString&) {}
    /// Selector mode: show `inst` as the current pick without searching.
    void set_instrument(const fincept::core::instrument::Instrument& inst);
    fincept::core::instrument::Instrument current_instrument() const { return picked_; }
    QLineEdit* line_edit() const { return edit_; }

  signals:
    void instrument_picked(const fincept::core::instrument::Instrument& instrument);
    void instruments_picked(const QVector<fincept::core::instrument::Instrument>& list);
    void unresolved(const QStringList& texts, const QString& message);
    void text_committed_raw(const QString& text);

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void changeEvent(QEvent* event) override;

  private:
    void on_text_edited(const QString& text);
    void run_search();
    void show_rows(const QVector<fincept::core::instrument::Instrument>& rows);
    void commit();
    void pick(const fincept::core::instrument::Instrument& inst);
    void pick_row(int row);
    void apply_height();
    void refresh_popup_style();
    QString home_country() const;
    QVector<fincept::core::instrument::Instrument> parser_rows(const QString& text) const;

    QLineEdit* edit_ = nullptr;
    QCompleter* completer_ = nullptr;
    QStandardItemModel* model_ = nullptr;
    QTimer* search_timer_ = nullptr;
    fincept::services::ResolveOptions opts_;
    RowFilter row_filter_;
    QVector<fincept::core::instrument::Instrument> rows_; ///< parallel to model_ rows
    QVector<RowCheck> row_checks_;
    QString rows_text_; ///< the text rows_ answer
    fincept::core::instrument::Instrument picked_;
    quint64 search_id_ = 0;
    bool multi_ = false;
    bool clear_on_pick_ = true;
    bool compact_ = false;
    bool raw_allowed_ = false;
    bool picking_ = false;
};

// ── The factory ──────────────────────────────────────────────────────────────

#if FINCEPT_EQ_SHARED_SYMBOL_BOX
using SymbolBox = fincept::ui::SymbolSearchBox;
#else
using SymbolBox = EquitySymbolBox;
#endif

enum class SymbolBoxRole { Selector, WatchlistAdd };

/// The screen's symbol boxes: the selector keeps the picked symbol in view, the
/// watchlist box clears after each pick. Both search every asset class.
SymbolBox* make_symbol_box(SymbolBoxRole role, QWidget* parent);

/// Lot size / tick / venue time zone of an instrument (services::trading_meta_for once
/// the shared resolver is built; a local equivalent from core/market until then).
fincept::services::TradingMeta eq_trading_meta(const fincept::core::instrument::Instrument& inst);

/// The instrument a STORED symbol denotes (watchlists saved with broker-style symbols:
/// "RELIANCE" + "NSE" -> RELIANCE.NS). Never drops a row: a symbol the parser cannot
/// canonicalise comes back with `symbol` empty and `id` = the stored text, so the
/// caller keeps the row and shows why it has no data. `note` is set when the stored
/// spelling differs from the canonical one.
fincept::core::instrument::Instrument eq_instrument_for_stored(const QString& stored_symbol,
                                                                const QString& exchange_hint, QString* note = nullptr);

/// Fill the display fields (display, venue, currency, class, name) of `inst` from its
/// canonical symbol when they are missing.
fincept::core::instrument::Instrument eq_complete(const fincept::core::instrument::Instrument& inst);

} // namespace fincept::screens::equity
