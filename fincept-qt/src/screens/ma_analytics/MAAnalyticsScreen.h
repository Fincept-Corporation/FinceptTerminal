// src/screens/ma_analytics/MAAnalyticsScreen.h
#pragma once
#include "screens/IStatefulScreen.h"
#include "services/ma_analytics/MAAnalyticsTypes.h"

#include <QHideEvent>
#include <QLabel>
#include <QPushButton>
#include <QShowEvent>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

class MAModulePanel;

/// M&A Analytics main screen — 3-panel Bloomberg-style layout.
/// Left: module navigation by category (CORE / SPECIALIZED / ANALYTICS)
/// Center: active module content (stacked widget)
/// Right: module info & capabilities sidebar
class MAAnalyticsScreen : public QWidget, public IStatefulScreen {
    Q_OBJECT
  public:
    explicit MAAnalyticsScreen(QWidget* parent = nullptr);

    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return "ma_analytics"; }
    int state_version() const override { return 1; }

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

  private slots:
    void on_module_selected(int index);
    void refresh_theme();

  private:
    void build_ui();
    QWidget* build_top_bar();
    QWidget* build_left_sidebar();
    QWidget* build_right_sidebar();
    QWidget* build_status_bar();
    void update_right_panel();
    void update_sidebar_selection();

    // Module data
    QVector<services::ma::ModuleInfo> modules_;
    int active_index_ = 0;

    // Widgets
    QStackedWidget* content_stack_ = nullptr;
    QWidget* left_panel_ = nullptr;
    QWidget* right_panel_ = nullptr;
    QVBoxLayout* left_items_layout_ = nullptr;
    QVector<QPushButton*> module_buttons_;

    // Right sidebar labels
    QLabel* right_title_ = nullptr;
    QLabel* right_category_ = nullptr;
    QVBoxLayout* capabilities_layout_ = nullptr;

    // Top bar quick selector
    QVector<QPushButton*> quick_buttons_;

    // Themed widgets that need refresh
    QWidget* top_bar_ = nullptr;
    QWidget* status_bar_ = nullptr;
    QLabel* brand_label_ = nullptr;
    QLabel* ready_label_ = nullptr;
    QLabel* subtitle_label_ = nullptr;
    QLabel* modules_title_ = nullptr;
    QLabel* info_title_ = nullptr;
    QLabel* cap_header_ = nullptr;
    QLabel* stats_title_ = nullptr;
    QWidget* stats_card_ = nullptr;
    QLabel* tips_label_ = nullptr;

    bool left_open_ = true;
    bool right_open_ = true;
    bool first_show_ = true;
};

} // namespace fincept::screens
