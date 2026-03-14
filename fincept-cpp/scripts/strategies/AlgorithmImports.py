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
