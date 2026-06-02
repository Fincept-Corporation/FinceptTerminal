#pragma once
#include "services/notifications/NotificationService.h"

#include <QEvent>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::ui {

/// Dropdown notification history panel.
/// Shown/hidden by DashboardStatusBar when the bell is clicked.
/// Anchored just above the status bar, right-aligned.
class NotifPanel : public QFrame {
    Q_OBJECT
  public:
    explicit NotifPanel(QWidget* parent = nullptr);

    /// Repopulate the list from NotificationService::history().
    void refresh();

    /// Position relative to a reference widget (the bell button).
    void anchor_to(QWidget* ref_widget);

  protected:
    void showEvent(QShowEvent* e) override;
    void changeEvent(QEvent* event) override;

  private:
    /// Re-apply tr() lookups to header + empty-state text after a language switch.
    void retranslateUi();

    QLabel* title_lbl_ = nullptr;
    QPushButton* mark_all_btn_ = nullptr;
    QWidget* list_container_ = nullptr;
    QVBoxLayout* list_layout_ = nullptr;
    QScrollArea* scroll_ = nullptr;

    static constexpr int PANEL_W = 380;
    static constexpr int PANEL_H = 400;
};

} // namespace fincept::ui
