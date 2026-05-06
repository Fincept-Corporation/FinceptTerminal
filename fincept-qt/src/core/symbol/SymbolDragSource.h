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

/// Start a QDrag carrying a SymbolRef. Caller must invoke past startDragDistance.
void startSymbolDrag(QWidget* source, const SymbolRef& ref,
                     SymbolGroup source_group = SymbolGroup::None);

/// Drop handler for application/x-fincept-symbol. Caller must setAcceptDrops(true) on the target.
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

/// Installs a drop filter on `target` and enables setAcceptDrops. Filter is parented to target.
SymbolDropFilter* installDropFilter(QWidget* target, SymbolDropFilter::Handler handler);

/// Watches mouse events and starts a symbol drag past the drag threshold.
/// `provider` is invoked at drag-start (for widgets whose symbol changes); return invalid to cancel.
class SymbolDragFilter : public QObject {
    Q_OBJECT
  public:
    using RefProvider = std::function<SymbolRef()>;

    SymbolDragFilter(QWidget* source, RefProvider provider,
                     SymbolGroup source_group = SymbolGroup::None);

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

SymbolDragFilter* installDragSource(QWidget* source, SymbolDragFilter::RefProvider provider,
                                     SymbolGroup source_group = SymbolGroup::None);

} // namespace fincept::symbol_dnd
