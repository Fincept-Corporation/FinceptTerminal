#pragma once
#include "core/symbol/SymbolGroup.h"
#include "core/symbol/SymbolRef.h"

#include <QtPlugin>

namespace fincept {

/// Mix-in interface implemented by any screen that participates in symbol
/// group linking (Bloomberg Launchpad "Security Groups" equivalent).
///
/// A screen can be a publisher, a subscriber, or both:
///   - Publishers: when the user changes the active symbol inside the
///     screen, call SymbolContext::set_group_symbol(group(), ref, this)
///     so other panels in the same group follow.
///   - Subscribers: override on_group_symbol_changed() to react to a new
///     symbol published by any other panel in the same group.
///
/// `set_group(None)` unlinks the screen — no traffic in either direction.
///
/// Implementations inherit this alongside QWidget; DockScreenRouter
/// qobject_casts each registered widget to IGroupLinked to decide whether
/// to attach a GroupBadge to its dock title.
class IGroupLinked {
  public:
    virtual ~IGroupLinked() = default;

    virtual void set_group(SymbolGroup g) = 0;
    virtual SymbolGroup group() const = 0;

    /// Called by DockScreenRouter when any publisher (including this one,
    /// unless it filters itself out via the `source` arg of SymbolContext)
    /// changes the group symbol. Implementations should update their UI
    /// without re-publishing to avoid feedback loops.
    virtual void on_group_symbol_changed(const SymbolRef& ref) = 0;

    /// The screen's current symbol, for one-shot "load group from me" flows.
    /// Return an invalid SymbolRef if the screen has nothing selected yet.
    virtual SymbolRef current_symbol() const = 0;
};

} // namespace fincept

Q_DECLARE_INTERFACE(fincept::IGroupLinked, "in.fincept.IGroupLinked/1.0")
