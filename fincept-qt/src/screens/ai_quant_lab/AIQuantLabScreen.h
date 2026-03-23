// src/screens/ai_quant_lab/AIQuantLabScreen.h
#pragma once
#include "services/ai_quant_lab/AIQuantLabTypes.h"

#include <QHideEvent>
#include <QLabel>
#include <QPushButton>
#include <QShowEvent>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

class QuantModulePanel;

/// AI Quant Lab — 18-module quantitative research platform.
/// Left: module navigation (4 categories), Center: active module, Right: module info.
class AIQuantLabScreen : public QWidget {
    Q_OBJECT
  public:
    explicit AIQuantLabScreen(QWidget* parent = nullptr);

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
    void update_right_panel();
    void update_sidebar_selection();

    QVector<services::quant::QuantModule> modules_;
    int active_index_ = 0;

    QStackedWidget* content_stack_ = nullptr;
    QVector<QPushButton*> module_buttons_;
    QVector<QPushButton*> quick_buttons_;
    QVBoxLayout* left_items_layout_ = nullptr;

    // Right panel
    QLabel* right_title_ = nullptr;
    QLabel* right_category_ = nullptr;
    QLabel* right_desc_ = nullptr;
    QLabel* right_script_ = nullptr;

    bool first_show_ = true;
};

} // namespace fincept::screens
