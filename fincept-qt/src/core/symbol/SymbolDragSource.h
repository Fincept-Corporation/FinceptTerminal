#pragma once
#include "core/symbol/SymbolGroup.h"
#include "core/symbol/SymbolRef.h"

#include <QObject>
#include <functional>

class QWidget;
class QDropEvent;
class QDragEnterEvent;
class QMouseEvent;

namespace fincept::symbol_dnd {

/// Start a QDrag carrying the given SymbolRef. Intended to be called from
/// a mouseMoveEvent (with the left button held) once the user drags past
/// QApplication::startDragDistance. Non-blocking — spawns the drag loop
/// and returns when the drop completes.
///
/// The drag pixmap is a small amber pill showing the ticker (Bloomberg
/// convention for "security in flight").
void startSymbolDrag(QWidget* source, const SymbolRef& ref,
                     SymbolGroup source_group = SymbolGroup::None);

/// Event filter that converts drops of application/x-fincept-symbol into
/// a callback receiving the unpacked SymbolRef. Install on any QWidget
/// you want to make a drop target.
///
/// The widget must have setAcceptDrops(true) set by the caller — this
/// filter only handles the two events, not the attribute.
class SymbolDropFilter : public QObject {
    Q_OBJECT
  public:
    using Handler = std::function<void(const SymbolRef& ref, SymbolGroup source_group)>;

    SymbolDropFilter(QWidget* target, Handler handler);

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

  private:
    Handler handler_;
};

/// Convenience: install a drop filter on `target` and call `setAcceptDrops(true)`.
/// Returns the created filter (owned by `target` as parent).
SymbolDropFilter* installDropFilter(QWidget* target, SymbolDropFilter::Handler handler);

/// Event filter that watches mouse events on a widget and starts a symbol
/// drag whenever the user holds left-button and moves past the system drag
/// threshold. The `ref_provider` callback is invoked at drag-start time to
/// fetch the current SymbolRef (needed for widgets where the symbol changes
/// — watchlist rows, dynamic title labels). Returns an invalid ref to cancel.
///
/// Typical use:
///   symbol_dnd::installDragSource(my_label,
///       []() { return SymbolRef::equity("AAPL"); });
///
/// The filter is parented to `source` so it's destroyed with the widget.
class SymbolDragFilter : public QObject {
    Q_OBJECT
  public:
    using RefProvider = std::function<SymbolRef()>;

    SymbolDragFilter(QWidget* source, RefProvider provider,
                     SymbolGroup source_group = SymbolGroup::None);

    /// The group embedded in the drag is read lazily at drag-start via this
    /// setter — set to follow the screen's current link group.
    void set_source_group(SymbolGroup g) { source_group_ = g; }

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

  private:
    QWidget* source_;
    RefProvider provider_;
    SymbolGroup source_group_;
    QPoint press_pos_;
    bool pressed_ = false;
};

/// Convenience: create+install a SymbolDragFilter on `source`.
SymbolDragFilter* installDragSource(QWidget* source, SymbolDragFilter::RefProvider provider,
                                     SymbolGroup source_group = SymbolGroup::None);

} // namespace fincept::symbol_dnd
