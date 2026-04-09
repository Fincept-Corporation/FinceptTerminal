// src/screens/geopolitics/GeopoliticsScreen.h
#pragma once
#include "screens/IStatefulScreen.h"
#include "services/geopolitics/GeopoliticsTypes.h"

#include <QComboBox>
#include <QHideEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QShowEvent>
#include <QStackedWidget>
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

  private slots:
    void on_apply_filters();
    void on_clear_filters();
    void on_tab_changed(int index);
    void on_events_loaded(QVector<services::geo::NewsEvent> events, int total);
    void on_error(const QString& context, const QString& message);

  private:
    void build_ui();
    QWidget* build_top_bar();
    QWidget* build_filter_panel();
    QWidget* build_status_bar();
    void connect_service();
    void refresh_theme();

    // Filter inputs
    QLineEdit* country_edit_ = nullptr;
    QLineEdit* city_edit_ = nullptr;
    QComboBox* category_combo_ = nullptr;

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

    // Auto-refresh
    QTimer* refresh_timer_ = nullptr;
    // Clock (1s tick, started/stopped in showEvent/hideEvent)
    QTimer* clock_timer_ = nullptr;
    bool first_show_ = true;
};

} // namespace fincept::screens
