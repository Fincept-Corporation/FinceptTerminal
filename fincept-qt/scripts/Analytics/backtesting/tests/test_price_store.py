"""base.price_store with a fake Yahoo (no network): warm = zero calls, incremental tail updates,
adjustment changes, session dates, inclusive end, errors, concurrency, corruption, LRU cap."""
import multiprocessing as mp
import os
import time
from datetime import date, timedelta

import numpy as np
import pandas as pd
import pytest

from base.price_store import (FileLock, PriceDataError, PriceStore, _encode_entry, currency_for_symbol,
                              normalize_symbol, yahoo_frame_to_raw)
from helpers import FakeYahoo, child_get_bars, raw_to_yahoo, yahoo_layout


def _store(tmp_path, frames, **kw):
    fake = FakeYahoo(frames, today=kw.pop('today', None), meta=kw.pop('meta', None))
    return PriceStore(root=tmp_path / 'prices', fetcher=fake, offline=False, clock=fake.clock, **kw), fake


def test_cold_then_warm_zero_calls(tmp_path, spy):
    st, fake = _store(tmp_path, {'SPY': yahoo_layout(spy)})
    a = st.get(['SPY'], '2016-01-01', '2020-12-31', progress=False)
    assert a.network_calls == 1 and len(fake.calls) == 1
    b = st.get([' spy '], '2017-01-01', '2019-12-31', progress=False)
    assert b.network_calls == 0 and len(fake.calls) == 1
    assert b.frames['SPY'].index[0] == pd.Timestamp('2017-01-03') and b.provenance['SPY']['fromCache']
    pd.testing.assert_frame_equal(a.frames['SPY'].loc['2017':'2019'], b.frames['SPY'], check_freq=False)


def test_end_date_is_inclusive_and_values_exact(tmp_path, spy):
    st, _ = _store(tmp_path, {'SPY': yahoo_layout(spy)})
    f = st.get(['SPY'], '2024-12-01', '2024-12-31', progress=False).frames['SPY']
    assert f.index[-1] == pd.Timestamp('2024-12-31')
    src = spy.loc['2024-12-01':'2024-12-31']
    assert np.allclose(f['Close'].values, src['Close'].values, rtol=1e-12)
    assert np.allclose(f['Open'].values, src['Open'].values, rtol=1e-12)


def test_incremental_tail_update_fetches_only_missing(tmp_path, spy):
    frames = {'SPY': yahoo_layout(spy)}
    st, fake = _store(tmp_path, frames, today='2023-06-30', ttl_s=0)
    st.get(['SPY'], '2015-01-01', '2023-06-30', progress=False)
    fake.today = pd.Timestamp('2024-12-31')
    res = st.get(['SPY'], '2015-01-01', '2024-12-31', progress=False)
    assert len(fake.calls) == 2
    tail_start = date.fromisoformat(fake.calls[1][1])
    assert date(2023, 6, 20) <= tail_start <= date(2023, 6, 30)
    assert len(res.frames['SPY']) == len(spy)
    assert np.allclose(res.frames['SPY']['Close'].values, spy['Close'].values, rtol=1e-12)


def test_new_dividend_rescales_cached_adjusted_history(tmp_path, spy):
    y = yahoo_layout(spy)
    st, fake = _store(tmp_path, {'SPY': y}, today='2023-06-30', ttl_s=0)
    st.get(['SPY'], '2015-01-01', '2023-06-30', progress=False)
    y2 = y.copy()
    y2.loc[y2.index.tz_localize(None) <= '2024-06-20', 'Adj Close'] *= 0.99   # a new dividend re-scales history
    fake.frames['SPY'] = y2
    fake.today = pd.Timestamp('2024-12-31')
    res = st.get(['SPY'], '2015-01-01', '2024-12-31', progress=False)
    assert len(fake.calls) == 2                                               # tail only, no full refetch
    want = (y2['Adj Close'].values)
    assert np.allclose(res.frames['SPY']['Close'].values, want, rtol=1e-12)
    raw = st.get(['SPY'], '2015-01-01', '2024-12-31', adjust='none', progress=False).frames['SPY']
    assert np.allclose(raw['Close'].values, spy['Close'].values, rtol=1e-12)  # raw prices untouched


def test_changed_raw_history_triggers_full_refetch(tmp_path, spy):
    y = yahoo_layout(spy)
    st, fake = _store(tmp_path, {'SPY': y}, today='2023-06-30', ttl_s=0)
    st.get(['SPY'], '2015-01-01', '2023-06-30', progress=False)
    y2 = y.copy()
    y2[['Open', 'High', 'Low', 'Close']] *= 1.01                              # Yahoo corrected the history
    fake.frames['SPY'] = y2
    fake.today = pd.Timestamp('2024-12-31')
    res = st.get(['SPY'], '2015-01-01', '2024-12-31', progress=False)
    assert len(fake.calls) == 3 and fake.calls[2][1] == '2015-01-01'
    assert np.allclose(res.frames['SPY']['Close'].values, y2['Adj Close'].values, rtol=1e-12)


def test_splits_and_dividends_raw_and_adjusted(tmp_path, aapl_raw):
    st, _ = _store(tmp_path, {'AAPL': raw_to_yahoo(aapl_raw)})
    raw = st.get(['AAPL'], '2020-06-01', '2020-12-31', adjust='none', progress=False).frames['AAPL']
    assert np.allclose(raw['Close'].values, aapl_raw['close'].values, rtol=1e-12)
    assert raw.loc['2020-08-07', 'Dividends'] == pytest.approx(0.82)
    assert raw.loc['2020-08-31', 'Splits'] == 4.0
    spl = st.get(['AAPL'], '2020-06-01', '2020-12-31', adjust='split', progress=False).frames['AAPL']
    assert spl.loc['2020-08-28', 'Close'] == pytest.approx(aapl_raw.loc['2020-08-28', 'close'] / 4)
    adj = st.get(['AAPL'], '2020-06-01', '2020-12-31', progress=False).frames['AAPL']
    assert np.allclose(adj['Close'].values, aapl_raw['adj_close'].values, rtol=1e-12)
    f = aapl_raw['adj_close'] / aapl_raw['close']
    assert np.allclose(adj['Open'].values, (aapl_raw['open'] * f).values, rtol=1e-12)


def test_session_dates_no_utc_shift(tmp_path, cal, meta):
    frames = {s: yahoo_layout(df, tz=meta[s]['tz'] or 'UTC') for s, df in cal.items()}
    st, _ = _store(tmp_path, frames)
    res = st.get(list(cal), '2024-01-01', '2024-06-30', progress=False)
    for s, df in cal.items():
        assert list(res.frames[s].index) == list(df.index), s
    assert res.frames['RELIANCE.NS'].index[0] == pd.Timestamp('2024-01-01')
    assert int((res.frames['RELIANCE.NS'].index.dayofweek >= 5).sum()) <= 2


def test_unknown_symbol_error_and_negative_cache(tmp_path, spy):
    st, fake = _store(tmp_path, {'SPY': yahoo_layout(spy)})
    res = st.get(['SPY', 'ZZZZNOTREAL'], '2020-01-01', '2020-12-31', progress=False)
    assert 'SPY' in res.frames and 'ZZZZNOTREAL' in res.errors
    e = res.errors['ZZZZNOTREAL']
    assert e.code == 'UNKNOWN_SYMBOL' and 'ZZZZNOTREAL' in str(e) and 'Traceback' not in str(e)
    n = len(fake.calls)
    res = st.get(['ZZZZNOTREAL'], '2020-01-01', '2020-12-31', progress=False)
    assert len(fake.calls) == n and res.errors['ZZZZNOTREAL'].code == 'UNKNOWN_SYMBOL'


def test_start_before_listing_reports_real_first_bar(tmp_path, spy):
    late = spy.loc['2020-12-10':]
    st, fake = _store(tmp_path, {'NEWCO': yahoo_layout(late)})
    res = st.get(['NEWCO'], '2015-01-01', '2024-12-31', progress=False)
    p = res.provenance['NEWCO']
    assert p['firstBar'] == '2020-12-10' and p['listingKnown'] is True
    st.get(['NEWCO'], '2010-01-01', '2024-12-31', progress=False)
    assert len(fake.calls) == 1                         # history start known: no refetch for an earlier start
    res = st.get(['NEWCO'], '2015-01-01', '2016-12-31', progress=False)
    assert res.errors['NEWCO'].code == 'NO_DATA_IN_RANGE' and '2020-12-10' in str(res.errors['NEWCO'])


def test_offline_miss_and_intraday_limit(tmp_path, spy):
    st = PriceStore(root=tmp_path / 'p', offline=True)
    res = st.get(['SPY'], '2020-01-01', '2020-12-31', progress=False)
    assert res.errors['SPY'].code == 'OFFLINE_MISS'
    res = st.get(['SPY'], '2015-01-01', '2015-03-01', interval='1h', progress=False)
    assert res.errors['SPY'].code == 'RANGE_LIMIT' and '730 days' in str(res.errors['SPY'])


def test_corrupt_entry_is_refetched(tmp_path, spy):
    st, fake = _store(tmp_path, {'SPY': yahoo_layout(spy)})
    st.get(['SPY'], '2020-01-01', '2020-12-31', progress=False)
    p = st.path('SPY', '1d')
    data = p.read_bytes()
    p.write_bytes(data[: len(data) // 2])                # torn / truncated file
    res = st.get(['SPY'], '2020-01-01', '2020-12-31', progress=False)
    assert len(fake.calls) == 2 and len(res.frames['SPY']) == 253
    p.write_bytes(b'not a zip at all')
    off = PriceStore(root=tmp_path / 'prices', offline=True)
    res = off.get(['SPY'], '2020-01-01', '2020-12-31', progress=False)
    assert 'SPY' in res.errors


def test_interrupted_write_leaves_old_entry_readable(tmp_path, spy):
    st, _ = _store(tmp_path, {'SPY': yahoo_layout(spy)})
    st.get(['SPY'], '2020-01-01', '2020-12-31', progress=False)
    p = st.path('SPY', '1d')
    # a writer that died after writing its temp file but before os.replace
    (p.parent / f'{p.name}.99999.1.tmp').write_bytes(b'partial')
    e = st.read_entry('SPY', '1d')
    assert e is not None and e.rows > 200


def test_concurrent_processes_one_download(tmp_path):
    root = tmp_path / 'shared'
    log = tmp_path / 'calls.log'
    ctx = mp.get_context('spawn')
    with ctx.Pool(3) as pool:
        out = pool.starmap(child_get_bars, [(str(root), ['AAA', 'BBB'], '2016-01-01', '2020-12-31', str(log), 0.5)] * 3)
    digests = [o[0] for o in out]
    assert all(not o[1] for o in out)
    assert digests[0] == digests[1] == digests[2]
    calls = [ln for ln in log.read_text().splitlines() if ln.strip()]
    assert len(calls) == 2                               # one fetch per symbol in total, not per process


def test_file_lock_excludes_other_holders(tmp_path):
    p = tmp_path / 'x.lock'
    a = FileLock(p)
    assert a.acquire()
    import subprocess
    import sys
    code = ('import sys; sys.path.insert(0, %r); from base.price_store import FileLock; '
            'print(FileLock(%r, timeout=0.3).acquire())' % (str(os.path.dirname(os.path.dirname(__file__))), str(p)))
    out = subprocess.run([sys.executable, '-c', code], capture_output=True, text=True, timeout=60)
    assert out.stdout.strip() == 'False'
    a.release()
    out = subprocess.run([sys.executable, '-c', code], capture_output=True, text=True, timeout=60)
    assert out.stdout.strip() == 'True'


def test_lru_size_cap(tmp_path, spy):
    frames = {f'S{i}': yahoo_layout(spy) for i in range(6)}
    st, _ = _store(tmp_path, frames, max_mb=1000)
    for i in range(6):
        st.get([f'S{i}'], '2015-01-01', '2024-12-31', progress=False)
        os.utime(st.path(f'S{i}', '1d'), (time.time() - 1000 + i * 10, time.time() - 1000 + i * 10))
    one = st.path('S0', '1d').stat().st_size + st.path('S0', '1d').with_suffix('.json').stat().st_size
    st.max_bytes = one * 3.5
    removed = st.enforce_size_cap()
    assert removed >= 3
    left = sorted(p.stem for p in st.entries())
    assert 'S5' in left and 'S0' not in left             # least recently used went first


def test_symbol_helpers():
    assert normalize_symbol(' nse:reliance ') == 'RELIANCE.NS'
    assert normalize_symbol('BSE:TCS') == 'TCS.BO'
    assert currency_for_symbol('RELIANCE.NS') == 'INR' and currency_for_symbol('7203.T') == 'JPY'
    assert currency_for_symbol('BTC-USD') == 'USD' and currency_for_symbol('SPY') == 'USD'
    assert currency_for_symbol('^NSEI') == 'INR' and currency_for_symbol('VOD.L') == 'GBp'


def test_yahoo_raw_rebuild_handles_tz_and_duplicates(spy):
    y = yahoo_layout(spy.iloc[:5], tz='Asia/Kolkata')
    y = pd.concat([y, y.iloc[[-1]]])
    t, cols = yahoo_frame_to_raw(y, '1d')
    assert len(t) == 5
    assert pd.Timestamp(t[0]) == spy.index[0]


def test_offline_network_fails_fast_after_two_errors(tmp_path):
    calls = []

    def down(symbol, interval, start, end):
        calls.append(symbol)
        raise PriceDataError(symbol, 'NETWORK', f'Could not reach Yahoo Finance for {symbol}.')
    st = PriceStore(root=tmp_path / 'p', fetcher=down, offline=False)
    res = st.get(['A1', 'A2', 'A3', 'A4'], '2020-01-01', '2020-12-31', progress=False)
    assert len(calls) == 2 and set(res.errors) == {'A1', 'A2', 'A3', 'A4'}
    assert all(e.code == 'NETWORK' for e in res.errors.values())
