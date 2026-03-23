// src/screens/geopolitics/RelationshipPanel.h
#pragma once
#include "services/geopolitics/GeopoliticsTypes.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

/// Network relationship visualization of conflicts, crises, and organizations.
class RelationshipPanel : public QWidget {
    Q_OBJECT
  public:
    explicit RelationshipPanel(QWidget* parent = nullptr);

  private:
    void build_ui();
    void build_network_view();
    QWidget* build_node_card(const fincept::services::geo::RelationshipNode& node, QWidget* parent);

    QVBoxLayout* network_layout_ = nullptr;
    QLabel* selected_label_ = nullptr;
    QWidget* detail_panel_ = nullptr;
};

} // namespace fincept::screens
