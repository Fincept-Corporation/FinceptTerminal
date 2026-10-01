#pragma once
#include "core/symbol/IGroupLinked.h"
#include "services/markets/MarketDataService.h"

#include <QHash>
#include <QSet>
#include <QVector>
#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

namespace fincept::screens {

/// Full-screen Stock Screener.
///
/// Reuses the dashboard `ScreenerWidget` data path — subscribes to
/// `market:quote:<sym>` on the DataHub for a broad large-cap basket, caches
/// each delivery, and sorts/filters client-side. Presents the result as a
/// full-width table with a symbol/name search box and a sort selector.
///
/// Hub lifecycle follows P3/D3: subscribe in `showEvent`, unsubscribe in
/// `hideEvent`, so the producer pauses when the screen isn't visible.
class ScreenerScreen : public QWidget, public IGroupLinked {
    Q_OBJECT
    Q_INTERFACES(fincept::IGroupLinked)
  public:
    explicit ScreenerScreen(QWidget* parent = nullptr);

    // IGroupLinked: the selected row is this screen's "current symbol", so a linked panel
    // (chart, equity research, news) follows the screener, and the screener follows it back.
    void set_group(SymbolGroup g) override { link_group_ = g; }
    SymbolGroup group() const override { return link_group_; }
    void on_group_symbol_changed(const SymbolRef& ref) override;
    SymbolRef current_symbol() const override;

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void changeEvent(QEvent* e) override;

  private:
    void build_ui();
    void apply_styles();
    void retranslate();

    void hub_subscribe_all();
    void hub_unsubscribe_all();
    /// Coalesce the ~62 per-symbol hub deliveries of a refresh sweep into one
    /// table rebuild on the next event-loop turn.
    void schedule_rebuild();
    /// Recompute `all_quotes_` from `row_cache_` (in basket order) then filter.
    void rebuild_from_cache();
    void apply_filter();
    void render_rows(const QVector<services::QuoteData>& rows);
    void refresh_now();
    /// Symbol of the table row at `row` (column 0 — the table is rebuilt in sorted order), or empty.
    QString symbol_at_row(int row) const;
    /// Hand `symbol` to another screen via the app-wide `nav.open_symbol` event.
    void open_symbol_in(const QString& screen_id, const QString& symbol);
    void show_row_menu(const QPoint& pos);

    QLineEdit* search_ = nullptr;
    QLabel* empty_lbl_ = nullptr; ///< loading / unavailable / no-match message shown in place of the table
    QComboBox* sort_combo_ = nullptr;
    QPushButton* refresh_btn_ = nullptr;
    QLabel* title_lbl_ = nullptr;
    QLabel* subtitle_lbl_ = nullptr;
    QLabel* count_lbl_ = nullptr;
    QWidget* header_bar_ = nullptr;
    QTableWidget* table_ = nullptr;

    QHash<QString, services::QuoteData> row_cache_;
    /// symbol → company name. The quote feed only carries the ticker as its "name", so the
    /// NAME column (and the search box's "or name" half) had nothing real to show.
    QHash<QString, QString> names_;
    /// Symbols whose last quote refresh failed (cleared when a quote arrives) — lets the
    /// screen say "unavailable" instead of showing an empty grid forever.
    QSet<QString> failed_;
    QVector<services::QuoteData> all_quotes_;
    bool hub_active_ = false;
    bool rebuild_pending_ = false;
    SymbolGroup link_group_ = SymbolGroup::None; // symbol-group link; None = unlinked
    /// Guards changeEvent(StyleChange) -> apply_styles() -> setStyleSheet() ->
    /// StyleChange -> ... which recursed until the stack was exhausted.
    bool restyling_ = false;
};

} // namespace fincept::screens
