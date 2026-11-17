"""exchange_client.resolve_market — the one resolver of the stream, the REST
handlers and the order path — against real market lists."""

import pytest

import exchange_client as C
from conftest import EXCHANGES

FORMS = ("{b}/USDT", "{b}/USDC", "{b}/USD", "{b}/USDT:USDT", "{b}/USDC:USDC", "{b}/USD:USD", "{b}/USD:{b}")


class _Venue:
    def __init__(self, markets):
        self.markets = markets


def _inputs(mk):
    bases = sorted({(m.get("base") or "").upper() for m in mk.values() if m.get("base")})
    return [f.format(b=b) for b in bases for f in FORMS] + list(mk)


def test_parse_symbol_forms():
    assert C.parse_symbol("BTC/USDT") == {"base": "BTC", "quote": "USDT", "settle": None, "form": "spot"}
    assert C.parse_symbol("BTC/USDT:USDT")["form"] == "swap"
    assert C.parse_symbol("BTC/USD:BTC") == {"base": "BTC", "quote": "USD", "settle": "BTC", "form": "swap"}
    assert C.parse_symbol("BTC/USDT:USDT-261225")["form"] == "future"
    assert C.parse_symbol("BTC/USD:USD-261117-54000-C")["form"] == "option"
    for bad in ("", "BTC", "BTC/USDT/X", "BTC/USDT:", "BTC/USDT:USDT-12"):
        assert C.parse_symbol(bad) is None


@pytest.mark.parametrize("exchange_id", EXCHANGES)
def test_resolution_never_changes_base_type_settle_or_expiry(markets, exchange_id):
    mk = markets[exchange_id]
    perps_venue = exchange_id in C.PERPS_VENUES
    for s in _inputs(mk):
        r = C.resolve_market(mk, s, exchange_id)
        if r["symbol"] is None:
            assert r["match"] == "missing" and not r["tradable"]
            continue
        m = mk[r["symbol"]]
        p = C.parse_symbol(s)
        exact = r["match"] == "exact"
        assert exact == (r["symbol"] == s), s
        if r["tradable"]:
            assert exact, f"{s}: only an exact market may be tradable"
        if p is None:
            assert exact
            continue
        assert (m.get("base") or "").upper() == p["base"], (s, r["symbol"])
        kind = C.market_kind(m)
        if p["form"] == "spot":
            assert kind == ("swap" if perps_venue else "spot"), (s, r["symbol"])
            if perps_venue:
                assert C.is_perpetual(m) and not exact
        elif p["form"] == "swap":
            assert kind == "swap", (s, r["symbol"])
            assert (m.get("settle") or "").upper() == p["settle"], (s, r["symbol"])
            assert bool(m.get("inverse")) == (p["settle"] == p["base"]), (s, r["symbol"])
            if not exact:
                assert not m.get("expiry"), (s, r["symbol"])
        else:
            assert exact and kind == p["form"], (s, r["symbol"])
        if not exact:
            assert kind not in ("future", "option"), (s, r["symbol"])


@pytest.mark.parametrize("exchange_id", EXCHANGES)
def test_stream_and_daemon_resolve_identically(markets, exchange_id):
    import exchange_daemon as D
    import ws_stream as W
    W.emit = lambda obj: None
    mk = markets[exchange_id]
    venue = _Venue(mk)
    for s in _inputs(mk):
        assert W.resolve_symbol(venue, s, exchange_id) == D._resolve_market_symbol(venue, s, exchange_id), s


def test_kraken_uni_usdt_shows_uni_usd_never_uni_btc(markets):
    mk = markets["kraken"]
    r = C.resolve_market(mk, "UNI/USDT", "kraken")
    assert r["symbol"] == "UNI/USD" and r["match"] == "substituted" and not r["tradable"]
    assert "UNI/USD" in r["reason"]
    assert C.resolve_market(mk, "UNI/EUR", "kraken")["match"] == "exact"
    gbp = C.resolve_market(mk, "UNI/GBP", "kraken")          # no non-USD substitution
    assert gbp["symbol"] is None and "UNI/USD" in gbp["alternatives"]
    assert C.resolve_market(mk, "UNI/USDT:USDT", "kraken")["symbol"] is None


def test_perp_request_never_becomes_option_or_future(markets):
    cases = [("okx", "BTC/USDC:USDC"), ("okx", "ETH/USDC:USDC"), ("cryptocom", "BTC/USDT:USDT"),
             ("binance", "BTC/USD:USD"), ("bybit", "ETH/USD:USD"), ("gate", "ADA/USDC:USDC"),
             ("coinbase", "ADA/USDT:USDT"), ("htx", "BTC/USDC:USDC"), ("kucoin", "BTC/USD:USD"),
             ("binance", "BNB/USD:USD")]
    for ex, s in cases:
        r = C.resolve_market(markets[ex], s, ex)
        if r["symbol"]:
            m = markets[ex][r["symbol"]]
            assert C.market_kind(m) == "swap" and not m.get("expiry"), (ex, s, r["symbol"])
            assert (m.get("settle") or "").upper() == s.split(":")[1], (ex, s, r["symbol"])


def test_explicit_dated_and_option_symbols_resolve_exactly(markets):
    seen = 0
    for ex, mk in markets.items():
        for s, m in mk.items():
            if C.market_kind(m) in ("future", "option"):
                r = C.resolve_market(mk, s, ex)
                assert r["match"] == "exact" and r["symbol"] == s
                seen += 1
    assert seen > 20


def test_hyperliquid_bare_pairs_mean_the_perpetual_never_a_spot_token(markets):
    mk = markets["hyperliquid"]
    assert "BTC/USDC" in mk and C.market_kind(mk["BTC/USDC"]) == "spot"   # the spot token exists...
    r = C.resolve_market(mk, "BTC/USDC", "hyperliquid")                   # ...but is never chosen
    assert r["symbol"] == "BTC/USDC:USDC" and r["match"] == "substituted" and not r["tradable"]
    r = C.resolve_market(mk, "BTC/USDT", "hyperliquid")
    assert r["symbol"] == "BTC/USDC:USDC" and not r["tradable"]
    assert C.resolve_market(mk, "BTC/USDC:USDC", "hyperliquid")["tradable"]
    pepe = C.resolve_market(mk, "PEPE/USDC", "hyperliquid")               # unrelated spot "PEPE"
    assert pepe["symbol"] is None


def test_inactive_exact_market_is_shown_not_traded(markets):
    for ex, mk in markets.items():
        for s, m in mk.items():
            if m.get("active") is False:
                r = C.resolve_market(mk, s, ex)
                if r["match"] == "exact":
                    assert not r["tradable"] and "not active" in r["reason"]
                    return
    pytest.skip("no inactive market in the fixture")
