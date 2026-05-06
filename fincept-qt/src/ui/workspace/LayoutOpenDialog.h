#pragma once
// LayoutOpenDialog — modal picker over LayoutCatalog::list_layouts().
// Replaces the legacy WorkspaceOpenDialog; reads from the new Phase 6
// per-profile layouts catalogue instead of the legacy .fwsp directory.

#include "core/identity/Uuid.h"

#include <QDialog>

class QListWidget;

namespace fincept::ui {

class LayoutOpenDialog : public QDialog {
    Q_OBJECT
  public:
    explicit LayoutOpenDialog(QWidget* parent = nullptr);

    /// Selected layout id. Null until exec() returns Accepted.
    LayoutId selected_id() const { return selected_id_; }

  private:
    void populate();

    QListWidget* list_ = nullptr;
    LayoutId selected_id_;
};

} // namespace fincept::ui
