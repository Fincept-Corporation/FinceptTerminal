"""Write the frozen bar fixtures under tests/fixtures/ (run once; needs the network or a warm cache).

    FINCEPT_DATA_DIR=<a data dir> python make_fixtures.py

Reads through base.price_store (so the fixtures are exactly what engines will see) and writes
small CSVs (a few hundred KB in total):
  SPY_1d.csv          SPY 2015-01-02..2024-12-31, split+dividend adjusted OHLCV (4 dp)
  ETF5_1d.csv         SPY QQQ TLT GLD EFA 2020-01-02..2024-12-31, adjusted open + close
  CAL_1d.csv          RELIANCE.NS 7203.T BTC-USD ^NSEI SPY 2024-01-01..2024-06-30 (long format)
  AAPL_raw_2020.csv   AAPL 2020-06-01..2020-12-31 RAW prices + adj_close + dividends + splits
  IRX_1d.csv          ^IRX (13-week T-bill yield, percent) 2015-01-02..2024-12-31
  meta.json           exchange time zone and currency per symbol
"""
import json
import os
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))

from base.price_store import PriceStore, get_bars  # noqa: E402

OUT = HERE / 'fixtures'


def main():
    OUT.mkdir(exist_ok=True)
    store = PriceStore()
    spy = get_bars(['SPY'], '2015-01-01', '2024-12-31', store=store, progress=False).frames['SPY']
    spy.round({'Open': 4, 'High': 4, 'Low': 4, 'Close': 4}).astype({'Volume': 'int64'}).to_csv(
        OUT / 'SPY_1d.csv', index_label='Date', float_format='%.4f')
    syms = ['SPY', 'QQQ', 'TLT', 'GLD', 'EFA']
    res = get_bars(syms, '2020-01-01', '2024-12-31', store=store, progress=False)
    import pandas as pd
    cols = {}
    for s in syms:
        cols[f'{s}_Open'] = res.frames[s]['Open']
        cols[f'{s}_Close'] = res.frames[s]['Close']
    pd.DataFrame(cols).round(4).to_csv(OUT / 'ETF5_1d.csv', index_label='Date', float_format='%.4f')
    cal = ['RELIANCE.NS', '7203.T', 'BTC-USD', '^NSEI', 'SPY']
    res = get_bars(cal, '2024-01-01', '2024-06-30', store=store, progress=False)
    parts = []
    for s in cal:
        f = res.frames[s].copy()
        f.insert(0, 'Symbol', s)
        parts.append(f)
    pd.concat(parts).round(4).to_csv(OUT / 'CAL_1d.csv', index_label='Date', float_format='%.4f')
    meta = {s: {'tz': res.provenance[s].get('exchangeTz'), 'currency': res.provenance[s].get('currency')} for s in cal}
    e = store.read_entry('AAPL', '1d')
    import numpy as np
    t = pd.DatetimeIndex(e.t)
    m = (t >= '2020-06-01') & (t <= '2020-12-31')
    raw = pd.DataFrame({k: e.cols[k][m] for k in ('open', 'high', 'low', 'close', 'adj_close', 'volume',
                                                    'dividends', 'splits')}, index=t[m])
    # product of the splits after 2020-12-31 up to the fetch (to rebuild Yahoo's split-adjusted view)
    later = float(np.prod(e.cols['splits'][t > '2020-12-31']))
    raw.to_csv(OUT / 'AAPL_raw_2020.csv', index_label='Date', float_format='%.6f')
    meta['AAPL'] = {'tz': 'America/New_York', 'currency': 'USD', 'laterSplitProduct': later}
    irx = get_bars(['^IRX'], '2015-01-01', '2024-12-31', adjust='split', store=store, progress=False).frames['^IRX']
    irx[['Close']].round(4).to_csv(OUT / 'IRX_1d.csv', index_label='Date', float_format='%.4f')
    (OUT / 'meta.json').write_text(json.dumps(meta, indent=1), encoding='utf-8')
    for p in sorted(OUT.iterdir()):
        print(f'{p.name:22s} {p.stat().st_size / 1024:7.1f} KB')


if __name__ == '__main__':
    main()
