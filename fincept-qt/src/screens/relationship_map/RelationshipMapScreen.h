// src/screens/relationship_map/RelationshipMapScreen.h
#pragma once

#include "core/symbol/IGroupLinked.h"
#include "screens/common/ISectionLinked.h"
#include "screens/common/IStatefulScreen.h"
#include "screens/relationship_map/RelationshipGraphLayout.h"
#include "screens/relationship_map/RelationshipGraphModel.h"
#include "screens/relationship_map/RelationshipMapData.h"

#include <QDateTime>
#include <QEvent>
#include <QHash>
#include <QPointer>
#include <QSet>
#include <QStringList>
#include <QWidget>

class QImage;
class QLabel;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QSpinBox;
class QSplitter;
class QStackedWidget;
class QVBoxLayout;

namespace fincept::relmap {
class RelationshipGraphScene;
class RelationshipGraphView;
class RelationshipLinkedViews;
} // namespace fincept::relmap

namespace fincept::ui {
class AutoLoadPane;
class EmptyState;
class KpiStrip;
class MatrixHeatmap;
class ScreenCommandBar;
class ScreenStatusBar;
class SymbolSearchBox;
class AddToReportButton;
} // namespace fincept::ui

namespace fincept::screens {

/// Relationship Map — who owns a company, who runs it, who it competes and moves with, what
/// it is made of (subsidiaries / an ETF's holdings), with numbers on every edge.
///
///   command strip   company picker, back / forward + breadcrumb of the companies walked,
///                   ANALYZE; layout (TIERED · HIERARCHY · RADIAL · FORCE · SANKEY · MATRIX),
///                   edge metric (% HELD · VALUE · CHANGE · ρ), depth (1 / 2 hops), FIT,
///                   REFRESH, EXPORT; type chips with counts, TOP N, find-in-graph
///   KPI ribbon      market cap, ownership split, holder count, top holder, insider buying,
///                   float, short interest, peers, beta / correlation
///   graph           RelationshipGraphScene (the structured TIERED view by default)
///   detail pane     the selected entity and its relationship to the company
///   linked views    tables and charts of the same data (selection shared both ways)
///   status bar      sources and "as of" per section, partial sections, counts, currency
///
/// Multi-instance: every panel keeps its own requests (service request ids) and state.
class RelationshipMapScreen : public QWidget, public IStatefulScreen, public IGroupLinked, public ISectionLinked {
    Q_OBJECT
    Q_INTERFACES(fincept::IGroupLinked)
  public:
    explicit RelationshipMapScreen(QWidget* parent = nullptr);
    ~RelationshipMapScreen() override;

    // IGroupLinked
    void set_group(SymbolGroup g) override { link_group_ = g; }
    SymbolGroup group() const override { return link_group_; }
    void on_group_symbol_changed(const SymbolRef& ref) override;
    SymbolRef current_symbol() const override;

    // IStatefulScreen
    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return "relationship_map"; }
    int state_version() const override { return 2; }

    // ISectionLinked: chip ids ("owners", "insiders", "management", "peers", "holdings",
    // "subsidiaries", "correlated", "supply_chain"), linked-view ids ("ownership", "holders",
    // "funds", "insiders", "management", "peers", "correlation", "subsidiaries", "holdings",
    // "supply_chain") and layout ids ("tiered", "hierarchy", "radial", "force", "sankey",
    // "matrix"); "company" with focus = a ticker re-centres the map. `focus` otherwise names an
    // entity on the map (selected and pulsed).
    bool open_section(const QString& section, const QString& focus) override;

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void changeEvent(QEvent* event) override;

  private:
    // ── Construction ──
    void build_ui();
    QWidget* build_strip();
    void build_command_bar();
    void build_ribbon();
    QWidget* build_detail_pane();
    void retranslate();

    // ── Loading ──
    /// Analyse a Yahoo-form ticker now; `push` adds it to the back / forward history.
    void analyze(const QString& ticker, bool push = true, bool force = false);
    void on_analyze_clicked();
    void start_analysis(quint64 pane_request);
    void start_with_default_ticker(quint64 pane_request);
    void on_planned(quint64 id, const QString& ticker, const QStringList& sections);
    void on_section(quint64 id, const fincept::relmap::RelationshipData& snapshot, const QString& section_id);
    void on_finished(quint64 id, const fincept::relmap::RelationshipData& data, const QDateTime& as_of,
                     bool from_cache);
    void on_failed(quint64 id, const QString& error);
    void retry_sections(const QStringList& sections);
    void cancel_requests();
    void load_user_symbols();

    // ── Rendering ──
    void render_result();
    /// Rebuild the graph model from current_data_ and hand it to the scene (+ tables, ribbon).
    void rebuild(bool animate);
    void update_ribbon();
    void update_chips();
    void update_status();
    void update_partial_notice();
    void update_history_ui();
    void show_mode_page();
    void set_layout_mode(fincept::relmap::RelLayoutMode m, bool notify = true);

    // ── Selection / detail ──
    void select_entity(const QString& node_id, bool from_table);
    void activate_entity(const QString& node_id);
    void show_detail(const QString& node_id);
    void show_context_menu(const QString& node_id, const QPoint& global_pos);

    // ── Export / report ──
    void export_png();
    void export_csv(bool edges);
    void export_table_csv();
    QImage render_graph_image(double scale) const;

    // ── Widgets ──
    QWidget* strip_ = nullptr;
    QLabel* title_ = nullptr;
    fincept::ui::SymbolSearchBox* symbol_box_ = nullptr;
    QPushButton* analyze_btn_ = nullptr;
    QPushButton* back_btn_ = nullptr;
    QPushButton* fwd_btn_ = nullptr;
    QLabel* crumbs_ = nullptr;
    QLabel* load_label_ = nullptr;
    fincept::ui::AddToReportButton* report_btn_ = nullptr;
    fincept::ui::ScreenCommandBar* bar_ = nullptr;
    QSpinBox* top_n_spin_ = nullptr;
    QLineEdit* find_edit_ = nullptr;
    fincept::ui::KpiStrip* ribbon_ = nullptr;
    QSplitter* vsplit_ = nullptr;
    QSplitter* hsplit_ = nullptr;
    fincept::ui::AutoLoadPane* graph_pane_ = nullptr;
    QStackedWidget* graph_stack_ = nullptr;
    relmap::RelationshipGraphScene* scene_ = nullptr;
    relmap::RelationshipGraphView* view_ = nullptr;
    QWidget* legend_ = nullptr;
    fincept::ui::MatrixHeatmap* matrix_ = nullptr;
    fincept::ui::EmptyState* no_rel_page_ = nullptr;
    QScrollArea* detail_scroll_ = nullptr;
    QWidget* detail_host_ = nullptr;
    QVBoxLayout* detail_layout_ = nullptr;
    relmap::RelationshipLinkedViews* linked_ = nullptr;
    fincept::ui::ScreenStatusBar* status_ = nullptr;

    // ── State ──
    relmap::RelationshipData current_data_;
    relmap::RelGraphModel model_;
    relmap::RelGraphOptions options_;
    relmap::RelLayoutMode layout_mode_ = relmap::RelLayoutMode::Tiered;
    relmap::RelEdgeMetric metric_ = relmap::RelEdgeMetric::Percent;
    QDateTime as_of_;
    bool has_data_ = false;
    bool streaming_shown_ = false; ///< the pane already shows the map being streamed
    QString loaded_ticker_;
    QString requested_ticker_;
    QString selected_id_;
    quint64 pane_request_ = 0;    ///< AutoLoadPane run the current service request answers
    quint64 svc_request_ = 0;     ///< service request of the map being loaded
    quint64 retry_request_ = 0;   ///< service request of a section retry
    quint64 resolve_request_ = 0; ///< InstrumentResolver request of ANALYZE on typed text
    bool force_next_ = false;     ///< the next pane run bypasses the remembered map (REFRESH)
    bool refit_pending_ = false;  ///< a new company: fit the next picture to the view
    QStringList history_;
    int history_pos_ = -1;
    QSet<QString> user_holdings_;
    QSet<QString> user_watchlist_;
    QVector<QPair<QString, QString>> watchlists_; ///< id, name (for "Add to watchlist")

    // Symbol-group linking
    SymbolGroup link_group_ = SymbolGroup::None;
    QString pending_group_symbol_;
    QString group_driven_ticker_;
    bool restore_pending_ = false;
};

} // namespace fincept::screens
