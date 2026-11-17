"""
The real bt library (pmorissette/bt), imported past this engine's own package name.

Market data no longer comes from here: every bar is read through base.price_store (one on-disk
cache, session dates, no UTC shift - the v1 yfinance loader of this module stamped non-US bars a
day early and could not combine calendars, audit T1.1 / T1.2).

import_bt_library(): this provider lives in a directory that is itself called `bt` (with an
__init__.py) and that directory's parent is on sys.path, so a plain `import bt` imports THIS
package. The library is imported with those directories temporarily removed from sys.path.
bt imports ffn, which imports matplotlib: matplotlib's config / font cache is pointed at the
backtesting cache folder (fincept_paths, edition coexistence rule) and its backend at Agg
before the import, so nothing is written to the home directory and no GUI backend loads.
matplotlib.pyplot / sklearn.cluster / sklearn.manifold (ffn's plotting and clustering helpers, never
used by the engine) are lazy stand-ins until something touches them (~1 s less per allocation run).
"""

import os
import sys
import types
from typing import List

_LIB = {'mod': None, 'error': None}


def _prepare_matplotlib() -> None:
    os.environ.setdefault('MPLBACKEND', 'Agg')
    if 'MPLCONFIGDIR' not in os.environ:
        try:
            here = os.path.dirname(os.path.abspath(__file__))
            parent = os.path.dirname(here)
            if parent not in sys.path:
                sys.path.insert(0, parent)
            from base.paths import cache_root
            os.environ['MPLCONFIGDIR'] = str(cache_root('matplotlib'))
        except Exception:  # pragma: no cover - matplotlib then uses its own default
            pass


# ffn imports these at module level but uses them only inside its plotting / clustering helpers (plot,
# calc_clusters, ...), which this engine never calls: ~1.2 s of the ~2.3 s bt import. They are put in
# sys.modules as lazy stand-ins that import the real module on first attribute access, so any real use still works.
_LAZY = ('matplotlib.pyplot', 'sklearn.cluster', 'sklearn.manifold')


class _LazyModule(types.ModuleType):
    def __getattr__(self, attr):
        if attr.startswith('__') and attr.endswith('__'):
            raise AttributeError(attr)
        real = self.__dict__.get('_fincept_real')
        if real is None:
            import importlib
            _drop_stand_ins()
            real = importlib.import_module(self.__name__)
            self.__dict__['_fincept_real'] = real
        return getattr(real, attr)


def _install_lazy_modules() -> List[str]:
    added = []
    for name in _LAZY:
        if name not in sys.modules:
            sys.modules[name] = _LazyModule(name)
            added.append(name)
    return added


def _drop_stand_ins() -> None:
    """Take the stand-ins out of sys.modules again (right after bt is imported): every later import - also of
    the stand-ins' own submodules - is then the normal one; the names ffn / bt bound during their import
    still point at the stand-ins, which load the real module on first use."""
    for name in _LAZY:
        if isinstance(sys.modules.get(name), _LazyModule):
            del sys.modules[name]


def import_bt_library():
    """The bt library module, or None when it is not installed (the reason in library_error())."""
    import importlib
    if _LIB['mod'] is not None:
        return _LIB['mod']
    here = os.path.dirname(os.path.abspath(__file__))
    parent = os.path.dirname(here)
    mod = sys.modules.get('bt')
    if mod is not None and hasattr(mod, 'algos') and hasattr(mod, 'Backtest'):
        _LIB['mod'] = mod
        return mod
    if mod is not None:
        del sys.modules['bt']

    def _shadows(entry):
        try:
            return os.path.normcase(os.path.abspath(entry or os.getcwd())) in (
                os.path.normcase(here), os.path.normcase(parent))
        except Exception:
            return False

    _prepare_matplotlib()
    _install_lazy_modules()
    saved = list(sys.path)
    try:
        sys.path[:] = [p for p in sys.path if not _shadows(p)]
        lib = importlib.import_module('bt')
        if not hasattr(lib, 'algos'):
            _LIB['error'] = 'the module named bt is not the bt library'
            return None
        _LIB['mod'] = lib
        return lib
    except ImportError as e:
        _LIB['error'] = str(e)
        return None
    finally:
        sys.path[:] = saved
        _drop_stand_ins()


def library_error():
    return _LIB['error']


def library_version():
    try:
        import importlib.metadata as md
        return md.version('bt')
    except Exception:
        return None


def library_installed() -> bool:
    """The bt distribution is installed (checked without importing it: ~2 s of ffn / matplotlib /
    scikit-learn imports that the catalogue and the signal strategies never need)."""
    return library_version() is not None
