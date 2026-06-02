#pragma once
// LayoutOpenDialog — modal picker over LayoutCatalog::list_layouts().
// Replaces the legacy WorkspaceOpenDialog; reads from the new Phase 6
// per-profile layouts catalogue instead of the legacy .fwsp directory.

#include "core/identity/Uuid.h"

#include <QDialog>
#include <QEvent>

class QListWidget;

namespace fincept::ui {

class LayoutOpenDialog : public QDialog {
    Q_OBJECT
  public:
    explicit LayoutOpenDialog(QWidget* parent = nullptr);

    /// Selected layout id. Null until exec() returns Accepted.
    LayoutId selected_id() const { return selected_id_; }

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void populate();

    /// Re-apply tr() lookups to every widget whose text we keep a handle to.
    /// Called from changeEvent() on QEvent::LanguageChange.
    void retranslateUi();

    QListWidget* list_ = nullptr;
    LayoutId selected_id_;
};

} // namespace fincept::ui
