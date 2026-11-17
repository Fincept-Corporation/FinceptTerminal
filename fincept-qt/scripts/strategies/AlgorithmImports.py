# ============================================================================
# Fincept Terminal - AlgorithmImports Shim
# Thin wrapper that re-exports everything from fincept_engine.algorithm_imports
# All actual code lives in fincept_engine/algorithm_imports.py
# ============================================================================

import os, sys
_dir = os.path.dirname(os.path.abspath(__file__))
if _dir not in sys.path:
    sys.path.insert(0, _dir)

from fincept_engine.algorithm_imports import *

# Strategies written for LEAN import `from Alphas.RsiAlphaModel import ...` (capital A),
# but the shim package is the directory `alphas`. The import system matches module names
# case-sensitively on every OS (on Windows too: a case-insensitive file system does not
# make `import Alphas` find `alphas`), so register the package under both names. Importing
# `alphas` registers its `Alphas.<Model>` submodules in sys.modules.
import alphas as _alphas_pkg
sys.modules.setdefault('Alphas', _alphas_pkg)
del _alphas_pkg
