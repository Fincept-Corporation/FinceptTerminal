#pragma once
#include "core/symbol/SymbolGroup.h"

#include <QColor>
#include <QWidget>

class QLabel;

namespace fincept::ui {

/// Coloured letter pill on dock titles whose screen implements IGroupLinked.
/// Click cycles group; emits group_change_requested only — DockScreenRouter owns the wiring to SymbolContext.
class GroupBadge : public QWidget {
    Q_OBJECT
  public:
    explicit GroupBadge(QWidget* parent = nullptr);

    SymbolGroup group() const { return group_; }

    /// Update display without emitting — for workspace load and programmatic set_group.
    void set_group_silent(SymbolGroup g);

    static QColor color_for_group(SymbolGroup g);

  signals:
    void group_change_requested(fincept::SymbolGroup g);

  protected:
    void mousePressEvent(QMouseEvent* e) override;
    void contextMenuEvent(QContextMenuEvent* e) override;
    void paintEvent(QPaintEvent* e) override;
    QSize sizeHint() const override;

  private:
    SymbolGroup group_ = SymbolGroup::None;
};

} // namespace fincept::ui
