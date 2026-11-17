#pragma once
// SymbolSearchBox — THE symbol picker of the terminal (symbol system WP4). Use it for
// every field where a user adds or selects a symbol: watchlist / portfolio / dashboard
// add boxes, trading symbol selectors, backtest and algo symbol lists, research boxes.
// It never hands out typed text as a symbol: it emits a resolved
// core::instrument::Instrument (symbol = the Yahoo-form market-data key, plus id,
// display, name, class, venue, MIC, country, currency, provider symbols).
//
// Behaviour
//   * A line edit with a drop-down result list (SymbolPickerPopup) that never takes the
//     focus. Search-as-you-type through services::InstrumentResolver::search (local index
//     at once, backend / Yahoo merged when they answer). The best match is pre-selected.
//   * Keys: Up / Down / PageUp / PageDown move, Enter picks the highlighted row, Tab
//     completes the text to it, Esc closes the list WITHOUT clearing the text (a second
//     Esc clears only with set_clear_on_escape(true)); a mouse click picks a row.
//   * Enter with no list for the current text resolves it (InstrumentResolver::resolve):
//     an unambiguous exact hit is picked, otherwise the list opens ("did you mean").
//   * Multi mode: "AAPL, RELIANCE; BTC" or a pasted column is resolved item by item on
//     Enter -> instruments_picked(resolved) + unresolved(rest, message).
//   * Recent picks (per recent key) are shown on focus with an empty box.
//   * Rows carry markers: "TATAMOTORS -> now TMPV", delisted / renamed, "no quote data",
//     or the caller's row-filter reason. Rows without quote data and inactive rows are
//     shown but not pickable unless allowed (set_allow_unquoted / set_allow_inactive).
//   * Colours and fonts only from ui/theme; sizes from font metrics; restyled on
//     ThemeManager::theme_changed.

#include "core/instrument/Instrument.h"
#include "services/instruments/InstrumentResolver.h"
#include "ui/widgets/symbol/SymbolPickerPopup.h"

#include <QPointer>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVector>
#include <QWidget>

#include <functional>
#include <utility>

class QLineEdit;
class QTimer;

namespace fincept::ui {

/// One line for tooltips / labels: "RELIANCE.NS · Reliance Industries · NSE · INR"
/// (display symbol + "(no quote data)" when the instrument has no market-data symbol).
QString describe_instrument(const fincept::core::instrument::Instrument& inst);

class SymbolSearchBox : public QWidget {
    Q_OBJECT
  public:
    using RowState = SymbolRowState; ///< Pickable, Marked (pickable with a note), Disabled (greyed, not pickable)
    struct RowCheck {
        RowState state = RowState::Pickable;
        QString reason; ///< shown on the row for Marked / Disabled ("not tradeable with Zerodha")
        RowCheck() = default;
        RowCheck(RowState s, QString r = {}) : state(s), reason(std::move(r)) {} // implicit from RowState
    };
    using RowFilter = std::function<RowCheck(const fincept::core::instrument::Instrument&)>;

    explicit SymbolSearchBox(QWidget* parent = nullptr);
    ~SymbolSearchBox() override;

    void set_placeholder(const QString& text);
    void set_options(const fincept::services::ResolveOptions& opts); ///< venue hint, home country, allowed classes …
    void set_multi(bool on);         ///< comma / newline separated entry and paste of lists
    void set_clear_on_pick(bool on); ///< default true (add-box behaviour); false = the box keeps the picked symbol (selector behaviour)
    void set_text(const QString& text);
    QString text() const;
    void focus_and_select();

    // ── additions ─────────────────────────────────────────────────────────
    fincept::services::ResolveOptions options() const { return opts_; }
    /// Mark / disable rows the caller cannot use (called for every row shown).
    void set_row_filter(RowFilter filter);
    /// Rows without a market-data symbol become pickable (Marked "no quote data"). Default false.
    void set_allow_unquoted(bool on);
    /// Delisted / renamed rows become pickable (Marked). Default false.
    void set_allow_inactive(bool on);
    /// Dense single line (~24 px at the default font) for panel header rows. Default false (~30 px).
    void set_compact(bool on);
    /// Esc with the list closed clears the text. Default false.
    void set_clear_on_escape(bool on);
    /// Key of the recent-picks list (default: derived from the allowed classes and venue hint).
    void set_recent_key(const QString& key);
    /// Also emit text_committed_raw(text) when Enter resolves nothing. Default false.
    void set_raw_text_allowed(bool on);
    /// Selector mode: show `inst` as the current pick (text = its symbol) without searching.
    void set_instrument(const fincept::core::instrument::Instrument& inst);
    /// The last pick while the text still shows it; an invalid record otherwise.
    fincept::core::instrument::Instrument current_instrument() const;
    /// Input memory (ui::InputMemory::track(name, box)): the current pick as a storable
    /// value — canonical symbol, the text shown and the Instrument JSON
    /// (input_memory_detail::encode_symbol) — or just the text typed without a pick.
    QVariant stored_value() const;
    /// Puts a stored_value() back without a network call: the stored Instrument through
    /// set_instrument(); a value with only a symbol is shown as text and looked up in the
    /// local index (InstrumentResolver::resolve_stored; an exact hit becomes the pick);
    /// otherwise the text. Emits nothing.
    void restore_stored_value(const QVariant& value);
    /// Use another resolver (tests, tools). nullptr = InstrumentResolver::instance().
    void set_resolver(fincept::services::InstrumentResolver* resolver);
    bool is_popup_visible() const;
    /// The status line of the list ("Searching…", a note, an error); empty when none.
    QString status_text() const;
    /// Rows currently listed (for tests and accessibility glue).
    QVector<fincept::core::instrument::Instrument> listed_instruments() const;
    int current_row() const;
    QLineEdit* line_edit() const { return edit_; }

  signals:
    void instrument_picked(const fincept::core::instrument::Instrument& instrument);     ///< one confirmed instrument
    void instruments_picked(const QVector<fincept::core::instrument::Instrument>& list); ///< multi entry / pasted list (resolved ones)
    void unresolved(const QStringList& texts, const QString& message); ///< what could not be resolved, with a plain message
    void text_committed_raw(const QString& text); ///< only if nothing resolved and the caller wants raw text

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void changeEvent(QEvent* event) override;
    void hideEvent(QHideEvent* event) override;

  private:
    fincept::services::InstrumentResolver* resolver();
    QString recent_key() const;
    QString current_item() const; ///< the item being typed (multi: after the last separator)
    void on_text_edited(const QString& text);
    void run_search();
    void on_search_result(const fincept::services::ResolveResult& res);
    void show_recent();
    void commit();
    void commit_single(const QString& text);
    void commit_list(const QStringList& items);
    void complete_current();
    void pick_row(int row);
    void pick(const fincept::core::instrument::Instrument& inst);
    bool handle_key(QKeyEvent* event);
    bool handle_paste();
    QVector<SymbolRow> rows_from(const fincept::services::ResolveResult& res) const;
    void evaluate(SymbolRow& row, const QVector<fincept::core::instrument::Instrument>& all) const;
    void show_rows(QVector<SymbolRow> rows, const QString& for_text, const QString& status, SymbolTone tone);
    void open_popup();
    void close_popup();
    void cancel_pending();
    void refresh_theme();
    void apply_height();
    void watch_window();
    QString status_for(const fincept::services::ResolveResult& res, int rows, SymbolTone* tone) const;

    QLineEdit* edit_ = nullptr;
    SymbolPickerPopup* popup_ = nullptr;
    QTimer* search_timer_ = nullptr;
    QPointer<fincept::services::InstrumentResolver> resolver_;
    fincept::services::ResolveOptions opts_;
    RowFilter row_filter_;
    QString recent_key_;
    bool multi_ = false;
    bool clear_on_pick_ = true;
    bool compact_ = false;
    bool clear_on_escape_ = false;
    bool allow_unquoted_ = false;
    bool allow_inactive_ = false;
    bool raw_allowed_ = false;
    bool showing_recent_ = false;
    bool resolving_ = false;
    quint64 search_id_ = 0;
    quint64 resolve_id_ = 0;
    quint64 batch_id_ = 0;
    QString rows_text_;   ///< the text the listed rows answer
    fincept::core::instrument::Instrument picked_;
    QString picked_text_;
    QPointer<QWidget> watched_window_;
};

} // namespace fincept::ui
