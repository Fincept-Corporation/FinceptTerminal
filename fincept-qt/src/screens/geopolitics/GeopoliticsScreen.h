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
    void on_tab_changed(int index);
    void on_events_loaded(services::geo::EventsPage page);
    void on_categories_loaded(QVector<services::geo::UniqueCategory> cats);
    void on_error(const QString& context, const QString& message);

  private:
    void build_ui();
    QWidget* build_top_bar();
    QWidget* build_filter_panel();
    QWidget* build_status_bar();
    void connect_service();
    void refresh_theme();
    void retranslateUi();
    void rebuild_legend(const QVector<services::geo::UniqueCategory>& cats);

    // Filter inputs
    QLineEdit* country_edit_ = nullptr;
    QLineEdit* city_edit_ = nullptr;
    QComboBox* category_combo_ = nullptr;

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

    // Legend (built dynamically from API categories)
    QWidget* legend_container_ = nullptr;
    QVBoxLayout* legend_layout_ = nullptr;

    // Auto-refresh
    QTimer* refresh_timer_ = nullptr;
    // Clock (1s tick, started/stopped in showEvent/hideEvent)
    QTimer* clock_timer_ = nullptr;
    bool first_show_ = true;
};

} // namespace fincept::screens
