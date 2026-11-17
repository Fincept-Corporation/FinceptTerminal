"""Edition coexistence: every file the backtesting scripts write resolves through scripts/fincept_paths.py
(FINCEPT_DATA_DIR, else the edition's data root) - never the home directory, never a fixed name in the temp
folder, never the scripts folder. Mirrors tests/tst_edition_source_guard.cpp for this package."""
import os
import re
from pathlib import Path

from base import paths
from base.price_store import PriceStore, default_root, default_store

fincept_paths = paths._fincept_paths()     # scripts/fincept_paths.py (put on sys.path by base.paths)

BT_DIR = Path(__file__).resolve().parents[1]
SCRIPTS = BT_DIR.parents[1]


def test_caches_resolve_through_fincept_paths(tmp_path, monkeypatch):
    monkeypatch.setenv('FINCEPT_DATA_DIR', str(tmp_path))
    assert paths.data_root() == fincept_paths.data_root() == tmp_path
    expect = tmp_path / 'cache' / 'scripts' / 'backtesting'
    assert paths.cache_root('prices') == fincept_paths.cache_dir('backtesting', 'prices') == expect / 'prices'
    assert paths.cache_root('numba') == expect / 'numba' and (expect / 'numba').is_dir()
    assert default_root() == expect / 'prices' / 'v1'
    assert PriceStore().root == default_root() and default_store().root == default_root()
    # lock files sit next to the entries, inside the store
    st = PriceStore()
    assert st.path('SPY', '1d').with_suffix('.lock').parent == expect / 'prices' / 'v1' / '1d'


def test_no_home_or_fixed_temp_locations_in_the_package():
    """No source of the backtesting package (engines, base, tools) names a home-folder or fixed temp location."""
    bad = re.compile(r"expanduser|Path\.home\(\)|['\"](?:~/)?\.fincept['\"/]|\.oecd_cache|gettempdir\(\)|"
                     r"['\"]/tmp/|%TEMP%|os\.environ\[['\"]TEMP['\"]\]")
    hits = []
    for p in BT_DIR.rglob('*.py'):
        if '__pycache__' in p.parts or p.name == 'test_paths.py':
            continue
        for i, line in enumerate(p.read_text(encoding='utf-8', errors='replace').splitlines(), 1):
            if bad.search(line):
                hits.append(f'{p.relative_to(SCRIPTS)}:{i}: {line.strip()[:120]}')
    assert hits == [], hits
