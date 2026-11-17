// src/screens/geopolitics/RelationshipPanel.h
#pragma once
#include "services/geopolitics/GeopoliticsTypes.h"

#include <QEvent>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

/// Network relationship visualization of conflicts, crises, and organizations.
class RelationshipPanel : public QWidget {
    Q_OBJECT
  public:
    explicit RelationshipPanel(QWidget* parent = nullptr);

    /// Live inputs (GeopoliticsService::countries_loaded / categories_loaded,
    /// forwarded by the screen): the conflict cards are the most active
    /// countries, the crisis cards the event categories.
    void set_countries(const QVector<fincept::services::geo::UniqueCountry>& countries);
    void set_categories(const QVector<fincept::services::geo::UniqueCategory>& categories);
    /// Country / category counts of the events page the Monitor shows (free):
    /// drawn, and labelled as such, while the live statistics are not loaded.
    void set_page_statistics(const QVector<fincept::services::geo::UniqueCountry>& countries,
                             const QVector<fincept::services::geo::UniqueCategory>& categories, int events);
    /// The live statistics are being fetched (skeleton, button disabled).
    void set_stats_loading(bool on);
    /// The live statistics could not be loaded (`error` shown with a retry).
    void set_stats_error(const QString& error);

  signals:
    /// LOAD LIVE STATISTICS: the countries + categories lists (3 credits each).
    void stats_requested();
    /// A conflict card asked for that country's events in the Conflict Monitor.
    void events_requested(const QString& country);
    /// A conflict card asked for HDX datasets for that country.
    void hdx_country_requested(const QString& country);
    /// A crisis card asked for HDX datasets on that topic.
    void hdx_topic_requested(const QString& topic);
    /// An organisation card asked for that organisation's HDX datasets
    /// (`org_slug` = the HDX organisation name, e.g. "unhcr").
    void hdx_organization_requested(const QString& org_slug);

  protected:
    void changeEvent(QEvent* event) override;
    /// Click on an actionable (conflict / crisis) node card opens its action menu.
    bool eventFilter(QObject* obj, QEvent* event) override;

  private:
    void build_ui();
    /// `detail` = the live figure shown on the card (e.g. "1,204 events").
    QWidget* build_node_card(const fincept::services::geo::RelationshipNode& node, QWidget* parent,
                             const QString& detail = {});
    /// (Re)build the three card sections from countries_ / categories_.
    void rebuild_sections();
    void show_node_menu(QWidget* card);
    void retranslateUi();

    QVBoxLayout* network_layout_ = nullptr;
    QVBoxLayout* sections_layout_ = nullptr; // holds the rebuilt sections
    QVector<fincept::services::geo::UniqueCountry> countries_;
    QVector<fincept::services::geo::UniqueCategory> categories_;
    // Counts derived from the Monitor's events page (used while countries_ /
    // categories_ are empty) and how many events they come from.
    QVector<fincept::services::geo::UniqueCountry> page_countries_;
    QVector<fincept::services::geo::UniqueCategory> page_categories_;
    int page_events_ = 0;
    bool stats_loading_ = false;
    QString stats_error_;
    bool showing_live_ = false;
    QPushButton* load_stats_btn_ = nullptr; // header: LOAD LIVE STATISTICS (6 credits)
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
