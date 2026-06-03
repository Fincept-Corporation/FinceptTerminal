# Tests the broker-candle injection guard in VectorBTProvider._load_market_data.
# Runs with pandas/numpy only — never imports yfinance or hits the network,
# because the guard must return before _load_market_data's yfinance try-block.
#
# Run (use the app's backtest venv, which has pandas/numpy):
#   python3 scripts/Analytics/backtesting/vectorbt/test_broker_data_injection.py
# or, if pytest is available:
#   python3 -m pytest scripts/Analytics/backtesting/vectorbt/test_broker_data_injection.py -v
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vectorbt_provider import VectorBTProvider  # noqa: E402

DAY = 86400
JAN_1_2024 = 1704067200  # 2024-01-01 00:00:00 UTC


def _candles(n, start_ts=JAN_1_2024, base=100.0):
    return [
        {"t": start_ts + i * DAY, "o": base, "h": base + 1, "l": base - 1,
         "c": base + i, "v": 1000 + i}
        for i in range(n)
    ]


def test_injected_single_symbol_builds_real_series():
    import pandas as pd
    p = VectorBTProvider()
    p._broker_candles = {"RELIANCE": _candles(30)}
    data, synthetic = p._load_market_data(["RELIANCE"], "2024-01-01", "2024-02-15")
    assert synthetic is False
    assert isinstance(data, pd.Series)
    assert len(data) == 30
    assert float(data.iloc[0]) == 100.0
    assert float(data.iloc[29]) == 129.0
    assert str(data.index.dtype).startswith("datetime64")


def test_canonical_matching_handles_ns_suffix_and_exchange_prefix():
    p = VectorBTProvider()
    p._broker_candles = {"RELIANCE": _candles(10)}
    data, synthetic = p._load_market_data(["NSE:RELIANCE.NS"], "2024-01-01", "2024-02-15")
    assert synthetic is False
    assert len(data) == 10


def test_no_broker_candles_returns_none_from_helper():
    p = VectorBTProvider()
    p._broker_candles = {}
    assert p._load_from_broker_candles(["RELIANCE"], False) is None


def test_partial_multi_symbol_falls_back_none():
    p = VectorBTProvider()
    p._broker_candles = {"RELIANCE": _candles(30)}  # INFY deliberately missing
    assert p._load_from_broker_candles(["RELIANCE", "INFY"], True) is None


def test_complete_multi_symbol_builds_dataframe():
    import pandas as pd
    p = VectorBTProvider()
    p._broker_candles = {"RELIANCE": _candles(30), "INFY": _candles(30, base=200.0)}
    data, synthetic = p._load_market_data(["RELIANCE", "INFY"], "2024-01-01",
                                          "2024-02-15", multi_asset=True)
    assert synthetic is False
    assert isinstance(data, pd.DataFrame)
    assert data.shape[1] == 2
    assert len(data) == 30


def test_too_few_bars_returns_none():
    p = VectorBTProvider()
    p._broker_candles = {"RELIANCE": _candles(3)}  # < 5 bar minimum
    assert p._load_from_broker_candles(["RELIANCE"], False) is None


if __name__ == "__main__":
    fns = [v for k, v in sorted(globals().items()) if k.startswith("test_")]
    failed = 0
    for fn in fns:
        try:
            fn()
            print(f"PASS {fn.__name__}")
        except Exception as e:  # noqa: BLE001
            failed += 1
            print(f"FAIL {fn.__name__}: {e}")
    print(f"\n{len(fns) - failed}/{len(fns)} passed")
    sys.exit(1 if failed else 0)
