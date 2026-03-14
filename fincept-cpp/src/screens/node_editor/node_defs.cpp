// Node Editor — Builtin node definitions
// All node definitions are now in NodeRegistry.
// This file just ensures the registry is initialized on first access.

#include "node_defs.h"
#include "node_registry.h"

namespace fincept::node_editor {

// The old free functions are now inline wrappers in node_registry.h.
// This compilation unit ensures node_registry.cpp is linked.
// Registry::init() is called by the NodeEditorScreen on startup.

} // namespace fincept::node_editor
