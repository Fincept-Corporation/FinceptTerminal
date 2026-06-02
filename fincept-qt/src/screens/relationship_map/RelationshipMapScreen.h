// src/screens/relationship_map/RelationshipMapScreen.h
#pragma once

#include "screens/common/IStatefulScreen.h"
#include "screens/relationship_map/RelationshipMapTypes.h"
#include "services/markets/MarketSearchService.h"

#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QListWidget>
#include <QPair>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>
#include <QWidget>

namespace fincept::relmap {
class RelationshipGraphScene;
class RelationshipGraphView;
} // namespace fincept::relmap

namespace fincept::screens {

/// Corporate Intelligence Relationship Map — full screen with search, graph,
/// filter panel, detail panel, legend, and status bar.
class RelationshipMapScreen : public QWidget, public IStatefulScreen {
    Q_OBJECT
  public:
    explicit RelationshipMapScreen(QWidget* parent = nullptr);

    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return "relationship_map"; }
    int state_version() const override { return 1; }

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;
    void changeEvent(QEvent* event) override;

  private:
    void build_ui();
    /// Re-apply tr() lookups to every widget whose text we keep a handle to.
    /// Called from changeEvent() on QEvent::LanguageChange.
    void retranslateUi();
    void on_search();
    void on_search_text_changed(const QString& text);
    void fire_asset_search(const QString& query);
    void on_asset_results(const QList<fincept::services::MarketSearchService::Item>& results);
    void show_dropdown();
    void hide_dropdown();
    void on_data_ready(const fincept::relmap::RelationshipData& data);
    void on_fetch_failed(const QString& error);
    void on_progress(int percent, const QString& message);
    void on_node_selected();
    void rebuild_graph();
    void update_status_bar();
    QWidget* build_filter_panel();
    QWidget* build_detail_panel();
    QWidget* build_legend();

    // Graph
    relmap::RelationshipGraphScene* scene_ = nullptr;
    relmap::RelationshipGraphView* view_ = nullptr;

    // Header
    QLineEdit* search_input_ = nullptr;
    QListWidget* search_dropdown_ = nullptr;
    QTimer* search_debounce_ = nullptr;
    QString pending_query_;
    bool search_connected_ = false;
    QComboBox* layout_combo_ = nullptr;
    QPushButton* filter_btn_ = nullptr;
    QProgressBar* progress_bar_ = nullptr;
    QLabel* progress_label_ = nullptr;

    // Header static-text widgets (cached for retranslateUi).
    QLabel* header_title_ = nullptr;
    QPushButton* search_btn_ = nullptr;
    QPushButton* fit_btn_ = nullptr;

    // Panels
    QWidget* filter_panel_ = nullptr;
    QWidget* detail_panel_ = nullptr;
    QWidget* legend_widget_ = nullptr;

    // Filter panel + legend titles + checkbox handles (cached for retranslateUi).
    QLabel* filter_title_ = nullptr;
    QLabel* legend_title_ = nullptr;
    QList<QCheckBox*> filter_checks_;
    // Legend entry labels paired with their category so retranslateUi can
    // re-apply the localized category_label().
    QList<QPair<QLabel*, relmap::NodeCategory>> legend_entries_;

    // Detail panel labels
    QLabel* detail_title_ = nullptr;
    QLabel* detail_category_ = nullptr;
    QWidget* detail_props_container_ = nullptr;

    // Status bar
    QLabel* status_nodes_ = nullptr;
    QLabel* status_quality_ = nullptr;
    QLabel* status_brand_ = nullptr;

    // State
    relmap::FilterState filters_;
    relmap::LayoutMode layout_mode_ = relmap::LayoutMode::Layered;
    relmap::RelationshipData current_data_;
    bool has_data_ = false;
    QString loaded_ticker_;       // ticker currently shown in the graph
    bool search_input_focused_ = false; // only show dropdown when user is actively typing
};

} // namespace fincept::screens
