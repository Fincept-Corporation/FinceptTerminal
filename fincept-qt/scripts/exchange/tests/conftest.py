"""Unit tests for the exchange scripts: no network, saved public market lists.

fixtures/markets_fixture.json.gz holds a trimmed copy of the 14 exchanges'
public ccxt market lists (every spot / perpetual market of a few bases plus a
handful of dated futures and options), so the resolver and the order sizing
are tested against the real market structures.
"""

import gzip
import json
import os
import sys

import pytest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))  # scripts/exchange

EXCHANGES = ["kraken", "hyperliquid", "binance", "coinbase", "okx", "bybit", "kucoin", "bitget", "gate",
             "mexc", "htx", "cryptocom", "bingx", "bitfinex"]


@pytest.fixture(scope="session")
def markets():
    with gzip.open(os.path.join(HERE, "fixtures", "markets_fixture.json.gz"), "rt", encoding="utf-8") as f:
        return json.load(f)


@pytest.fixture(autouse=True)
def no_network(monkeypatch):
    """Any HTTP request from ccxt or requests fails the test."""
    import ccxt
    import requests

    def refuse(*a, **k):
        raise AssertionError("network access in a unit test")

    monkeypatch.setattr(ccxt.Exchange, "fetch", refuse)
    monkeypatch.setattr(requests.Session, "request", refuse)
    monkeypatch.setattr(requests.Session, "send", refuse)
