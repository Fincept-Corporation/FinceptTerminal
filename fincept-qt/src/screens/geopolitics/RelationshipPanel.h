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

  signals:
    /// A conflict card asked for that country's events in the Conflict Monitor.
    void events_requested(const QString& country);
    /// A conflict card asked for HDX datasets for that country.
    void hdx_country_requested(const QString& country);
    /// A crisis card asked for HDX datasets on that topic.
    void hdx_topic_requested(const QString& topic);

  protected:
    void changeEvent(QEvent* event) override;
    /// Click on an actionable (conflict / crisis) node card opens its action menu.
    bool eventFilter(QObject* obj, QEvent* event) override;

  private:
    void build_ui();
    void build_network_view();
    QWidget* build_node_card(const fincept::services::geo::RelationshipNode& node, QWidget* parent);
    void show_node_menu(QWidget* card);
    void retranslateUi();

    QVBoxLayout* network_layout_ = nullptr;
    QLabel* selected_label_ = nullptr;
    QWidget* detail_panel_ = nullptr;

    // Static text widgets (cached for retranslateUi)
    QLabel* title_lbl_ = nullptr;
    QLabel* sample_badge_ = nullptr; // "SAMPLE DATA" pill — data is hardcoded, not live
    QLabel* sample_note_ = nullptr;  // honesty banner above the network
    QLabel* stats_lbl_ = nullptr;
    QLabel* sec_conflicts_ = nullptr;
    QLabel* sec_crisis_ = nullptr;
    QLabel* sec_orgs_ = nullptr;
    int node_count_ = 0;
    int conflict_count_ = 0;
    int org_count_ = 0;
};

} // namespace fincept::screens
