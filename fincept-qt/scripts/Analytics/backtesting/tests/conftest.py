"""pytest setup for the backtesting suite: no network, caches in pytest's temp dir.

Run (Windows, venv-numpy1):
    cd scripts/Analytics/backtesting/tests
    python -m pytest -q -p no:cacheprovider --basetemp <scratch dir>
Environment: FINCEPT_PRICE_CACHE_OFFLINE=1 (a price-store miss is an error), FINCEPT_DATA_DIR =
a fresh temp dir, BLAS threads 1. Set NUMBA_CACHE_DIR to reuse numba's compiled kernel across runs.
FINCEPT_BT_ENGINE_OUTPUTS=<dir of saved provider outputs> makes test_contract validate them too.
"""
import json
import os
import sys
from pathlib import Path

import pytest

HERE = Path(__file__).resolve().parent
BT_DIR = HERE.parent
for p in (str(BT_DIR), str(HERE)):
    if p not in sys.path:
        sys.path.insert(0, p)
for k in ('OMP_NUM_THREADS', 'OPENBLAS_NUM_THREADS', 'MKL_NUM_THREADS', 'NUMEXPR_NUM_THREADS'):
    os.environ.setdefault(k, '1')
os.environ['FINCEPT_PRICE_CACHE_OFFLINE'] = '1'
FIX = HERE / 'fixtures'


@pytest.fixture(scope='session', autouse=True)
def _isolated_data_dir(tmp_path_factory):
    d = tmp_path_factory.mktemp('fincept_data')
    old = os.environ.get('FINCEPT_DATA_DIR')
    os.environ['FINCEPT_DATA_DIR'] = str(d)
    if 'NUMBA_CACHE_DIR' not in os.environ:
        os.environ['NUMBA_CACHE_DIR'] = str(tmp_path_factory.mktemp('numba'))
    yield d
    if old is None:
        os.environ.pop('FINCEPT_DATA_DIR', None)
    else:
        os.environ['FINCEPT_DATA_DIR'] = old


def _read(name):
    import pandas as pd
    return pd.read_csv(FIX / name, index_col=0, parse_dates=[0])


@pytest.fixture(scope='session')
def spy():
    return _read('SPY_1d.csv')


@pytest.fixture(scope='session')
def etf5():
    df = _read('ETF5_1d.csv')
    syms = ['SPY', 'QQQ', 'TLT', 'GLD', 'EFA']
    opens = df[[f'{s}_Open' for s in syms]].set_axis(syms, axis=1)
    closes = df[[f'{s}_Close' for s in syms]].set_axis(syms, axis=1)
    return opens, closes


@pytest.fixture(scope='session')
def cal():
    df = _read('CAL_1d.csv')
    return {s: g.drop(columns=['Symbol']) for s, g in df.groupby('Symbol')}


@pytest.fixture(scope='session')
def aapl_raw():
    return _read('AAPL_raw_2020.csv')


@pytest.fixture(scope='session')
def irx():
    return _read('IRX_1d.csv')['Close']


@pytest.fixture(scope='session')
def meta():
    return json.loads((FIX / 'meta.json').read_text(encoding='utf-8'))
