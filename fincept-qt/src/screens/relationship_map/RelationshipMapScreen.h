// src/screens/relationship_map/RelationshipMapScreen.h
#pragma once

#include "screens/IStatefulScreen.h"
#include "screens/relationship_map/RelationshipMapTypes.h"

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
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

  private:
    void build_ui();
    void on_search();
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
    QComboBox* layout_combo_ = nullptr;
    QPushButton* filter_btn_ = nullptr;
    QProgressBar* progress_bar_ = nullptr;
    QLabel* progress_label_ = nullptr;

    // Panels
    QWidget* filter_panel_ = nullptr;
    QWidget* detail_panel_ = nullptr;
    QWidget* legend_widget_ = nullptr;

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
};

} // namespace fincept::screens
