// src/screens/relationship_map/RelationshipLinkedViews.h
// Relationship Map — the linked views under the graph: a compact tab strip of dense tables and
// small charts over the same data the graph draws (OWNERSHIP, HOLDERS, FUNDS / HOLDINGS,
// INSIDERS, MANAGEMENT, PEERS, CORRELATION, SUBSIDIARIES, SUPPLY CHAIN). Every row carries the
// graph's node id (node_id() / RelGraphModel), so a row picked here is the node picked on the
// graph and back. Each tab honours its section's state (loading skeleton, failure page with
// RETRY, designed no-data page; a not-applicable section has no tab).
//
// Styling: one token-built stylesheet on the root, set before any child exists (the theme
// manager's re-style pass keeps it in step); charts and delegates paint from the live theme;
// row heights and column widths follow the font (re-fitted on appearance_settled); numbers are
// formatted in the models' data() and refreshed on a number-format change without a reset.
#pragma once

#include "screens/relationship_map/RelationshipGraphModel.h"

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QAbstractTableModel;
class QLabel;
class QModelIndex;
class QTabBar;
class QTableView;
class QVBoxLayout;

namespace fincept::ui {
class DonutChart;
class EmptyState;
class KeySortProxy;
class LoadingSkeleton;
class MatrixHeatmap;
} // namespace fincept::ui

namespace fincept::relmap {

class RelInsiderActivityChart;
class RelPeerBulletChart;

class RelationshipLinkedViews : public QWidget {
    Q_OBJECT
  public:
    explicit RelationshipLinkedViews(QWidget* parent = nullptr);
    /// Full refresh from the data and the graph model on screen (also called each time a streamed
    /// section arrives — keep the current tab, sort order, scroll position and selection).
    void set_data(const RelationshipData& data, const RelGraphModel& model);
    /// Highlight and scroll to the row of `node_id` in the tab that lists it (switching to that tab
    /// only when `switch_tab`); no signal emitted. "" clears.
    void set_selected(const QString& node_id, bool switch_tab = false);
    /// Deep links: "ownership", "holders", "funds", "insiders", "management", "peers", "correlation",
    /// "subsidiaries", "holdings", "supply_chain". False for an unknown key or a tab not shown for
    /// this instrument.
    bool open_view(const QString& key);
    QString current_view() const;
    QStringList view_keys() const; ///< tabs shown for the current instrument, in order
    /// The current tab as CSV text (header row + rows, numbers unformatted via fmt::export_number).
    QString current_csv() const;

  signals:
    void node_selected(const QString& node_id);  ///< a row clicked (single click)
    void node_activated(const QString& node_id); ///< a row double-clicked
    void retry_requested(const QStringList& section_ids); ///< RETRY on a tab whose section failed
    void view_changed(const QString& key);

  private:
    enum PageIndex : int {
        kPageOwnership,
        kPageHolders,
        kPageFunds,
        kPageInsiders,
        kPageManagement,
        kPagePeers,
        kPageCorrelation,
        kPageSubsidiaries,
        kPageHoldings,
        kPageSupplyChain,
        kPageCount
    };
    enum class PageMode { Content, Loading, Failed, NoData };

    struct Table {
        QTableView* view = nullptr;
        QAbstractTableModel* model = nullptr; ///< the row model of the .cpp
        ui::KeySortProxy* proxy = nullptr;
        int page = -1;
    };
    struct Page {
        QString key;     ///< deep-link key
        QString section; ///< relmap::section id its data comes from
        QString title;   ///< tab caption without the count ("HOLDERS")
        QString what;    ///< lower-case noun for the loading / empty text ("institutional holders")
        QWidget* root = nullptr;
        QVBoxLayout* root_layout = nullptr;
        QWidget* content = nullptr;
        ui::LoadingSkeleton* skeleton = nullptr; ///< created on first use
        ui::EmptyState* state = nullptr;         ///< created on first use
        QVector<int> tables;                     ///< indexes into tables_; the first one is the CSV table
        int count = -1;                          ///< count on the tab (-1 = none)
        bool has_rows = false;
        bool shown = false;
        PageMode mode = PageMode::Loading;
    };

    // Building.
    void build_ui();
    QWidget* make_page(int page, const QString& key, const char* section_id, const QString& title, const QString& what);
    int add_table(int page, QTableView* view, QAbstractTableModel* model);
    void build_ownership();
    void build_holder_page(int page, QLabel** caption, int* table);
    void build_insiders();
    void build_management();
    void build_peers();
    void build_correlation();
    void build_subsidiaries();
    void build_holdings();
    void build_supply_chain();
    void apply_metrics();

    // Filling.
    void fill_ownership();
    void fill_holders(int page, int table, bool funds);
    void fill_insiders();
    void fill_management();
    void fill_peers();
    void fill_correlation();
    void fill_subsidiaries();
    void fill_holdings();
    void fill_supply_chain();
    void refresh_decor();
    void refresh_pages();
    void apply_page_mode(int page);
    bool page_visible(int page) const;
    QVector<int> page_order() const;
    void sync_tabs();
    void show_page(int page);
    void update_source_label();

    // Selection.
    void on_view_clicked(int table, const QModelIndex& index);
    void on_view_double_clicked(int table, const QModelIndex& index);
    void on_view_current_changed(int table, const QModelIndex& index);
    void highlight(const QString& id, int except_table, bool scroll);
    int page_for_node(const QString& id) const;
    bool page_lists(int page, const QString& id) const;
    QString id_for_symbol(const QString& symbol) const;
    QString insider_id(const QString& roster_name) const;
    QString table_csv(int table) const;
    QString correlation_csv() const;

    RelationshipData data_;
    QString center_id_;
    QHash<QString, int> graph_index_;
    QString selected_id_;
    int wanted_ = -1;  ///< the tab the user (or a deep link) chose last
    int current_ = -1; ///< the page shown
    bool has_data_ = false;
    bool syncing_ = false; ///< selection changed by this class (no signal)
    bool quiet_ = false;   ///< tab switched by set_selected(): no view_changed

    QTabBar* tabs_ = nullptr;
    QLabel* source_ = nullptr;
    QWidget* body_ = nullptr;
    QVBoxLayout* body_layout_ = nullptr;
    ui::EmptyState* none_ = nullptr; ///< no tab applies to the instrument (created on first use)
    QVector<Page> pages_;
    QVector<Table> tables_;

    ui::DonutChart* donut_ = nullptr;
    QLabel* own_headline_ = nullptr;
    QLabel* own_caption_ = nullptr;
    QLabel* holders_caption_ = nullptr;
    QLabel* funds_caption_ = nullptr;
    QLabel* insider_summary_ = nullptr;
    QLabel* activity_caption_ = nullptr;
    RelInsiderActivityChart* activity_ = nullptr;
    QLabel* mgmt_caption_ = nullptr;
    QLabel* peers_caption_ = nullptr;
    RelPeerBulletChart* bullets_ = nullptr;
    ui::MatrixHeatmap* heatmap_ = nullptr;
    QLabel* corr_caption_ = nullptr;
    QLabel* subs_caption_ = nullptr;
    QLabel* holdings_caption_ = nullptr;
    QLabel* supply_caption_ = nullptr;
    QStringList heatmap_ids_; ///< node id per correlation symbol (matrix order)

    int t_owners_ = -1;
    int t_holders_ = -1;
    int t_funds_ = -1;
    int t_roster_ = -1;
    int t_trades_ = -1;
    int t_mgmt_ = -1;
    int t_peers_ = -1;
    int t_corr_ = -1;
    int t_subs_ = -1;
    int t_juris_ = -1;
    int t_holdings_ = -1;
    int t_sectors_ = -1;
    int t_supply_ = -1;
};

} // namespace fincept::relmap
