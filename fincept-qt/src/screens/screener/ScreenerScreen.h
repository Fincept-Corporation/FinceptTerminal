#pragma once
#include "core/symbol/IGroupLinked.h"
#include "screens/common/IStatefulScreen.h"
#include "services/markets/MarketDataService.h"
#include "services/screener/ScreenerService.h"

#include <QHash>
#include <QJsonObject>
#include <QPair>
#include <QSet>
#include <QVariantMap>
#include <QVector>
#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QMenu;
class QModelIndex;
class QPushButton;
class QScrollArea;
class QStackedWidget;
class QTableView;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;

namespace fincept::screens {

class ScreenerCriteriaBuilder;
class ScreenerResultsModel;
class ScreenerSortProxy;

/// Equity screener over the Yahoo Finance screener API.
///
/// Left: the screens list (Yahoo's predefined screens, the user's saved screens and the live
/// large-cap basket the screen used to be) and the "new screen" criteria builder. Right: a
/// sortable model/view results table with paging, status (rows, matches, quote time, source),
/// CSV export, watchlist hand-off and "Open in…" (nav.open_symbol).
///
/// Data paths: ScreenerService for screens, catalogue, saved screens and watchlists; the DataHub
/// (`market:quote:*`, subscribed only while the basket is shown and visible) for the live basket.
/// The screen itself never runs Python, HTTP or SQL.
class ScreenerScreen : public QWidget, public IStatefulScreen, public IGroupLinked {
    Q_OBJECT
    Q_INTERFACES(fincept::IGroupLinked)
  public:
    explicit ScreenerScreen(QWidget* parent = nullptr);

    // IGroupLinked: the selected row is this screen's symbol, so linked panels follow it.
    void set_group(SymbolGroup g) override { link_group_ = g; }
    SymbolGroup group() const override { return link_group_; }
    void on_group_symbol_changed(const SymbolRef& ref) override;
    SymbolRef current_symbol() const override;

    // IStatefulScreen: active screen, builder definition, page size and search survive restarts.
    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return QStringLiteral("screener"); }

  public slots:
    /// F5 / View ▸ Refresh (WindowFrame::refresh_focused_panel): re-fetch, bypassing the cache.
    void refresh();

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void changeEvent(QEvent* e) override;

  private:
    enum class Source { None, Basket, Predefined, Custom };

    void build_ui();
    QWidget* build_sidebar();
    QWidget* build_results_panel();
    void apply_styles();
    void retranslate();

    // Catalogue + screens list
    void on_catalog_ready();
    void on_catalog_failed(const QString& error);
    void rebuild_tree();
    void select_tree_view(const QString& view_id);
    void show_tree_menu(const QPoint& pos);

    // Running screens
    void activate_view(const QString& view_id);
    void run_current(bool force);
    void change_page(int direction);
    void on_results_ready(const services::ScreenerResult& r);
    void on_run_failed(const QString& key, const QString& error);
    void show_result(const services::ScreenerResult& r, const QString& failure);
    void show_message(const QString& text, bool offer_retry);
    void set_running(bool running);
    void set_status(const QString& text, const QString& tooltip = {}, bool warn = false);
    void update_actions();
    void update_count_label();
    QString view_id() const;
    QString active_title() const;
    void apply_state(const QVariantMap& state);
    void notify_state_changed();

    // Builder
    void on_builder_edited();
    void new_screen();
    void save_screen();

    // Live large-cap basket (the screen's original view; DataHub quotes)
    void hub_subscribe_all();
    void hub_unsubscribe_all();
    void schedule_basket_rebuild();
    void rebuild_basket();
    void refresh_basket();

    // Row actions
    QString symbol_at(const QModelIndex& proxy_index) const;
    QVector<QPair<QString, QString>> selected_symbols() const;
    void select_symbol(const QString& symbol);
    void open_symbol_in(const QString& screen_id, const QString& symbol);
    void show_row_menu(const QPoint& pos);
    void populate_watchlist_menu(QMenu* menu, const QVector<QPair<QString, QString>>& items);
    void add_to_watchlist(const QString& watchlist_id, const QString& list_name,
                          const QVector<QPair<QString, QString>>& items);
    void export_csv();

    // Header
    QWidget* header_bar_ = nullptr;
    QLabel* title_lbl_ = nullptr;
    QLabel* subtitle_lbl_ = nullptr;
    QLineEdit* search_ = nullptr;
    QLabel* count_lbl_ = nullptr;

    // Sidebar
    QLabel* screens_lbl_ = nullptr;
    QTreeWidget* tree_ = nullptr;
    QLabel* builder_lbl_ = nullptr;
    QPushButton* new_btn_ = nullptr;
    QScrollArea* builder_scroll_ = nullptr;
    ScreenerCriteriaBuilder* builder_ = nullptr;
    QLabel* catalog_lbl_ = nullptr;
    QPushButton* catalog_retry_btn_ = nullptr;

    // Results
    QPushButton* run_btn_ = nullptr;
    QPushButton* save_btn_ = nullptr;
    QPushButton* export_btn_ = nullptr;
    QToolButton* watch_btn_ = nullptr;
    QMenu* watch_menu_ = nullptr;
    QLabel* page_size_lbl_ = nullptr;
    QComboBox* page_size_ = nullptr;
    QToolButton* prev_btn_ = nullptr;
    QToolButton* next_btn_ = nullptr;
    QLabel* page_lbl_ = nullptr;
    QStackedWidget* stack_ = nullptr;
    QTableView* table_ = nullptr;
    QWidget* message_page_ = nullptr;
    QLabel* message_lbl_ = nullptr;
    QPushButton* retry_btn_ = nullptr;
    QLabel* status_lbl_ = nullptr;
    ScreenerResultsModel* model_ = nullptr;
    ScreenerSortProxy* proxy_ = nullptr;

    // Run state
    Source source_ = Source::None;
    QString predefined_id_;
    QString saved_name_;          ///< saved screen loaded into the builder ("" = unsaved)
    bool custom_modified_ = false; ///< builder edited since the saved screen was loaded
    QString pending_key_;         ///< query key of the run we are waiting for
    QString shown_screen_;        ///< identity of the rows on screen, ignoring paging
    services::ScreenerResult shown_; ///< metadata (+ rows) of what is on screen
    bool showing_result_ = false; ///< the table holds a screen result (not basket / nothing)
    int offset_ = 0;
    QJsonObject builder_json_;    ///< last valid builder definition (persisted)
    QVariantMap pending_state_;   ///< restore_state() before the catalogue arrived
    bool catalog_requested_ = false;
    bool selecting_ = false;      ///< programmatic selection — do not publish to the symbol group

    // Live basket
    QHash<QString, services::QuoteData> basket_quotes_;
    QHash<QString, QString> basket_names_;
    QSet<QString> basket_failed_;
    bool hub_active_ = false;
    bool basket_rebuild_pending_ = false;

    SymbolGroup link_group_ = SymbolGroup::None;
};

} // namespace fincept::screens
