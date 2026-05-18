// src/screens/ai_quant_lab/AIQuantLabScreen.h
#pragma once
#include "screens/common/IStatefulScreen.h"
#include "services/ai_quant_lab/AIQuantLabTypes.h"

#include <QHideEvent>
#include <QLabel>
#include <QMap>
#include <QPushButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

class QuantModulePanel;

/// AI Quant Lab — 24-module quantitative research platform.
/// Left: module navigation (4 categories), Center: lazy-constructed module panel,
/// Right: module info. Panels are built on first activation (P2 compliant).
class AIQuantLabScreen : public QWidget, public IStatefulScreen {
    Q_OBJECT
  public:
    explicit AIQuantLabScreen(QWidget* parent = nullptr);

    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return "ai_quant_lab"; }
    int state_version() const override { return 1; }

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

  private slots:
    void on_module_selected(int index);

  private:
    void build_ui();
    QWidget* build_top_bar();
    QWidget* build_left_sidebar();
    QWidget* build_right_sidebar();
    QWidget* build_status_bar();
    void refresh_theme();
    void update_right_panel();
    void update_sidebar_selection();

    // Lazy panel construction — returns existing or newly built panel
    QuantModulePanel* get_or_create_panel(int index);

    QVector<services::quant::QuantModule> modules_;
    int active_index_ = 0;

    // Lazy panel map: module index → panel (nullptr = not yet built)
    QMap<int, QuantModulePanel*> panels_;

    QStackedWidget* content_stack_ = nullptr;
    QVector<QPushButton*> module_buttons_;

    // Badge bar (scrollable, top bar)
    QScrollArea* badge_scroll_ = nullptr;
    QWidget* badge_bar_ = nullptr;
    QVector<QPushButton*> badge_buttons_;

    // Sidebar widgets that need theme refresh
    QWidget* top_bar_ = nullptr;
    QWidget* left_panel_ = nullptr;
    QWidget* right_panel_ = nullptr;
    QWidget* status_bar_ = nullptr;
    QLabel* sidebar_title_ = nullptr;
    QWidget* stats_card_ = nullptr;
    QLabel* stats_title_ = nullptr;

    QVBoxLayout* left_items_layout_ = nullptr;

    // Right panel
    QLabel* right_title_ = nullptr;
    QLabel* right_category_ = nullptr;
    QLabel* right_desc_ = nullptr;
    QLabel* right_script_ = nullptr;

    // Status bar labels
    QLabel* status_engine_val_ = nullptr;
    QLabel* status_ready_lbl_ = nullptr;

    bool first_show_ = true;
};

} // namespace fincept::screens
