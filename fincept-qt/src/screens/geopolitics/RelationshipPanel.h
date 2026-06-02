// src/screens/geopolitics/RelationshipPanel.h
#pragma once
#include "services/geopolitics/GeopoliticsTypes.h"

#include <QEvent>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

/// Network relationship visualization of conflicts, crises, and organizations.
class RelationshipPanel : public QWidget {
    Q_OBJECT
  public:
    explicit RelationshipPanel(QWidget* parent = nullptr);

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void build_ui();
    void build_network_view();
    QWidget* build_node_card(const fincept::services::geo::RelationshipNode& node, QWidget* parent);
    void retranslateUi();

    QVBoxLayout* network_layout_ = nullptr;
    QLabel* selected_label_ = nullptr;
    QWidget* detail_panel_ = nullptr;

    // Static text widgets (cached for retranslateUi)
    QLabel* title_lbl_ = nullptr;
    QLabel* stats_lbl_ = nullptr;
    QLabel* sec_conflicts_ = nullptr;
    QLabel* sec_crisis_ = nullptr;
    QLabel* sec_orgs_ = nullptr;
    int node_count_ = 0;
    int conflict_count_ = 0;
    int org_count_ = 0;
};

} // namespace fincept::screens
