// src/screens/geopolitics/GeopoliticsScreen.h
#pragma once
#include "screens/common/IStatefulScreen.h"
#include "services/geopolitics/GeopoliticsTypes.h"

#include <QComboBox>
#include <QEvent>
#include <QHideEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QShowEvent>
#include <QStackedWidget>
#include <QStringList>
#include <QStringListModel>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

class ConflictMonitorPanel;
class HDXDataPanel;
class RelationshipPanel;
class TradeAnalysisPanel;

/// Geopolitical Conflict Monitor — multi-panel intelligence screen.
/// Top: header with branding + status
/// Left: filter controls (country, city, category)
/// Center: tab-switched content (Conflict Monitor, HDX, Relationships, Trade)
/// Right: event detail / legend panel
class GeopoliticsScreen : public QWidget, public IStatefulScreen {
    Q_OBJECT
  public:
    explicit GeopoliticsScreen(QWidget* parent = nullptr);

    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return "geopolitics"; }
    int state_version() const override { return 1; }

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void changeEvent(QEvent* event) override;

  private slots:
    void on_apply_filters();
    void on_clear_filters();
    void on_prev_page();
    void on_next_page();
    void on_tab_changed(int index);
    void on_events_loaded(services::geo::EventsPage page);
    void on_categories_loaded(QVector<services::geo::UniqueCategory> cats);
    void on_countries_loaded(QVector<services::geo::UniqueCountry> countries);
    void on_cities_loaded(QStringList cities);
    void on_error(const QString& context, const QString& message);
    /// 5-minute timer tick: re-run the current filters but keep the user's map camera.
    void on_auto_refresh();

  private:
    void build_ui();
    QWidget* build_top_bar();
    QWidget* build_filter_panel();
    QWidget* build_status_bar();
    void connect_service();
    void refresh_theme();
    void retranslateUi();
    void rebuild_legend(const QVector<services::geo::UniqueCategory>& cats);
    /// Colour the status-bar label (one stylesheet for every state).
    void set_status_color(const QString& color);
    /// Fetch one page of events. A fresh apply (`reuse_filters` false) snapshots the
    /// boxes; paging and the timer reuse that snapshot so a half-typed filter can't
    /// leak into page 2 or a background refresh.
    void request_events(int page, bool reuse_filters);
    /// Sync the PAGE x / y label and the PREV / NEXT buttons with the loaded page.
    void update_pager();

    // Filter inputs
    QLineEdit* country_edit_ = nullptr;
    QLineEdit* city_edit_ = nullptr;
    QComboBox* category_combo_ = nullptr;
    // Autocomplete sources for the country / city boxes (filled from the API's
    // reference lists once countries_loaded / cities_loaded fire).
    QStringListModel* country_model_ = nullptr;
    QStringListModel* city_model_ = nullptr;

    // Static text widgets (cached for retranslateUi)
    QLabel* filters_title_ = nullptr;
    QLabel* country_lbl_ = nullptr;
    QLabel* city_lbl_ = nullptr;
    QLabel* category_lbl_ = nullptr;
    QPushButton* apply_btn_ = nullptr;
    QPushButton* clear_btn_ = nullptr;
    QLabel* legend_title_ = nullptr;
    QLabel* clock_label_ = nullptr;
    QLabel* status_source_lbl_ = nullptr;
    QLabel* status_source_val_ = nullptr;
    QLabel* status_engine_lbl_ = nullptr;
    QLabel* status_engine_val_ = nullptr;
    // Fixed English tab labels, re-applied in retranslateUi.
    QStringList tab_labels_;

    // Content
    QStackedWidget* content_stack_ = nullptr;
    ConflictMonitorPanel* monitor_panel_ = nullptr;
    HDXDataPanel* hdx_panel_ = nullptr;
    RelationshipPanel* relationship_panel_ = nullptr;
    TradeAnalysisPanel* trade_panel_ = nullptr;

    // Tab buttons
    QVector<QPushButton*> tab_buttons_;
    int active_tab_ = 0;

    // Status
    QLabel* event_count_label_ = nullptr;
    QLabel* status_label_ = nullptr;
    QLabel* credits_label_ = nullptr;

    // Events paging (the API returns pages; the screen used to show only page 1)
    QPushButton* prev_page_btn_ = nullptr;
    QPushButton* next_page_btn_ = nullptr;
    QLabel* page_lbl_ = nullptr;
    int current_page_ = 1;
    int total_pages_ = 0; // 0 = the API did not report pagination
    bool events_inflight_ = false;
    QString applied_country_, applied_city_, applied_category_;

    // Legend (built dynamically from API categories)
    QWidget* legend_container_ = nullptr;
    QVBoxLayout* legend_layout_ = nullptr;

    // Auto-refresh
    QTimer* refresh_timer_ = nullptr;
    /// events_request_key() of the fetch the screen last issued. events_loaded is a
    /// shared signal (hub refresh, MCP tools), so replies that don't match are
    /// someone else's and must not replace the filtered table.
    QString last_request_key_;
    /// True when the in-flight fetch came from the timer: keep the map camera.
    bool auto_refresh_pending_ = false;
    // Clock (1s tick, started/stopped in showEvent/hideEvent)
    QTimer* clock_timer_ = nullptr;
    bool first_show_ = true;
};

} // namespace fincept::screens
