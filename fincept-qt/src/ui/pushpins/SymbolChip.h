#pragma once
#include "core/symbol/SymbolGroup.h"
#include "core/symbol/SymbolRef.h"

#include <QMouseEvent>
#include <QWidget>

namespace fincept::ui {

/// Single-symbol chip rendered in the PushpinBar. Interactions:
///   - Left-click: publish to the current "default broadcast group"
///     (Group A if no group is selected; configurable via user setting
///     "pushpins/default_group" in future). Also sets `active_group`
///     on the SymbolContext implicitly by reusing that group.
///   - Drag out: starts a symbol drag so the user can drop onto any panel.
///   - Right-click: menu with Remove, Broadcast to Group A/B/…, Copy ticker.
///
/// The chip is intentionally a plain QWidget (not QPushButton) so we can
/// freely paint the amber pill style and handle mouseMove for drag
/// initiation without QPushButton's internal state machine interfering.
class SymbolChip : public QWidget {
    Q_OBJECT
  public:
    explicit SymbolChip(SymbolRef ref, QWidget* parent = nullptr);

    const SymbolRef& ref() const { return ref_; }

  signals:
    void remove_requested(fincept::SymbolRef ref);

  protected:
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void contextMenuEvent(QContextMenuEvent* e) override;
    void paintEvent(QPaintEvent* e) override;
    QSize sizeHint() const override;

  private:
    void broadcast_to_group(SymbolGroup g);

    SymbolRef ref_;
    QPoint press_pos_;
    bool pressed_ = false;
};

} // namespace fincept::ui
