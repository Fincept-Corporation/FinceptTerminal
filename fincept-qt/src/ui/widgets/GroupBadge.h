#pragma once
#include "core/symbol/SymbolGroup.h"

#include <QColor>
#include <QWidget>

class QLabel;

namespace fincept::ui {

/// Small coloured letter pill shown in the top-left of any dock widget
/// whose screen implements IGroupLinked. Mirrors Bloomberg Launchpad's
/// group indicator:
///   - `[A]` amber, `[B]` cyan, `[C]` magenta, `[D]` green, `[E]` purple, `[F]` red
///   - Empty badge ("·") when the panel is unlinked (SymbolGroup::None)
///
/// Left-click: cycle None→A→B→…→F→None.
/// Right-click: context menu with explicit pick + "Unlink" + "Show current".
///
/// The badge does not talk to SymbolContext directly — it only emits
/// group_change_requested(). DockScreenRouter owns the wiring so that a
/// single code path updates both the screen's IGroupLinked::set_group()
/// and the SymbolContext subscription.
class GroupBadge : public QWidget {
    Q_OBJECT
  public:
    explicit GroupBadge(QWidget* parent = nullptr);

    SymbolGroup group() const { return group_; }

    /// Update the displayed group without emitting a change request. Used
    /// by DockScreenRouter to reflect external state changes (workspace
    /// load, programmatic set_group).
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
