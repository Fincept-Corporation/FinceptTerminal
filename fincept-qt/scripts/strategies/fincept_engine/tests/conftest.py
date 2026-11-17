# Unit tests of the Fincept strategy engine (no network: they use small saved bar fixtures
# under tests/fixtures or build bars in code).
#
# Run (repo mounted read-only, so keep pytest's cache off and put tmp dirs elsewhere):
#   python -m pytest -q -p no:cacheprovider --basetemp=<writable dir> scripts/strategies/fincept_engine/tests
import os
import sys

_STRATEGIES = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
if _STRATEGIES not in sys.path:
    sys.path.insert(0, _STRATEGIES)

FIXTURES = os.path.join(os.path.dirname(__file__), 'fixtures')
