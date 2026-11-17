# ============================================================================
# Fincept Terminal - Strategy Engine lazy module proxy
# `from AlgorithmImports import *` exports np / pd like LEAN does; the proxy defers
# the (slow) import until a strategy actually touches the module.
# ============================================================================

import importlib
import sys


class LazyModule:
    """Stand-in for a module: the real module is imported on first attribute access."""

    __slots__ = ('_name', '_mod')

    def __init__(self, name: str):
        object.__setattr__(self, '_name', name)
        object.__setattr__(self, '_mod', None)

    def _load(self):
        mod = object.__getattribute__(self, '_mod')
        if mod is None:
            name = object.__getattribute__(self, '_name')
            mod = sys.modules.get(name) or importlib.import_module(name)
            object.__setattr__(self, '_mod', mod)
        return mod

    def __getattr__(self, attr):
        return getattr(self._load(), attr)

    def __setattr__(self, attr, value):
        setattr(self._load(), attr, value)

    def __dir__(self):
        return dir(self._load())

    def __repr__(self):
        mod = object.__getattribute__(self, '_mod')
        name = object.__getattribute__(self, '_name')
        return repr(mod) if mod is not None else f"<lazy module '{name}'>"

    def __bool__(self):
        return True
