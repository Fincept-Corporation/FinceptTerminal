"""Order preparation (exchange_daemon._prepare_order): base-coin quantity ->
exchange units, rounding, limits, refusals, idempotency ids — no network."""

import base64
import copy

import ccxt
import pytest

import exchange_client as C
import exchange_daemon as D

PRICES = {"BTC": 50000.0, "ETH": 2500.0, "DOGE": 0.1, "PEPE": 0.00001, "UNI": 8.0, "ADA": 0.5, "SOL": 150.0}
TOKEN = "0123456789abcdef0123456789abcdef"
SECRET = base64.b64encode(b"0123456789abcdef0123456789abcdef").decode()


@pytest.fixture
def venue(markets, monkeypatch):
    """A real ccxt client of `exchange_id` holding the saved market list, with
    dummy credentials and a fixed reference price — nothing reaches the network."""
    monkeypatch.setattr(D, "_reference_price",
                        lambda ex, e, s: PRICES.get((ex.markets[s].get("base") or "").upper(), 1.0))

    def make(exchange_id):
        ex = getattr(ccxt, exchange_id)({"options": D._exchange_options(exchange_id)})
        ex.set_markets(copy.deepcopy(markets[exchange_id]))
        ex._fincept_markets_stale = False
        if exchange_id == "hyperliquid":
            D._disable_hyperliquid_builder(ex)
        if exchange_id == "kucoin":
            ex.options["uta"] = False
        D._credentials[exchange_id] = (
            {"wallet_address": "0x" + "11" * 20, "private_key": "0x" + "22" * 32} if exchange_id == "hyperliquid"
            else {"api_key": "dummykey0123456789", "secret": SECRET, "password": "p"})
        return ex
    return make


def prep(ex, symbol, **kw):
    args = {"symbol": symbol, "side": "buy", "type": "limit", "amount": 1,
            "price": PRICES[symbol.split("/")[0]], "client_order_token": TOKEN, "margin_mode": "cross",
            "leverage": 3}
    args.update(kw)
    return D._prepare_order(ex, ex.id, args)


@pytest.mark.parametrize("exchange_id,symbol,coins,contracts", [
    ("okx", "DOGE/USDT:USDT", 1000, 1.0),          # contractSize 1000 DOGE
    ("okx", "PEPE/USDT:USDT", 10_000_000, 1.0),    # contractSize 10,000,000 PEPE
    ("okx", "PEPE/USDT:USDT", 1_000_000, 0.1),
    ("okx", "BTC/USDT:USDT", 1, 100.0),            # contractSize 0.01 BTC
    ("gate", "DOGE/USDT:USDT", 1000, 100.0),
    ("gate", "PEPE/USDT:USDT", 10_000_000, 1.0),
    ("mexc", "DOGE/USDT:USDT", 1000, 10.0),
    ("htx", "DOGE/USDT:USDT", 1000, 10.0),
    ("kucoin", "PEPE/USDT:USDT", 1_040_000, 2.0),
    ("binance", "DOGE/USDT:USDT", 1000, 1000.0),
    ("bybit", "DOGE/USDT:USDT", 1000, 1000.0),
])
def test_base_coins_become_contracts(venue, exchange_id, symbol, coins, contracts):
    p = prep(venue(exchange_id), symbol, amount=coins)
    assert float(p["amount_text"]) == contracts
    assert p["create_order"]["amount"] == p["amount_text"]
    assert p["base_amount"] == pytest.approx(coins, rel=1e-12)
    assert p["amount_unit"] == "contracts"


def test_inverse_contracts_sized_in_usd(venue):
    p = prep(venue("okx"), "BTC/USD:BTC", amount=0.01, price=50000)
    assert float(p["amount_text"]) == 5.0                 # 0.01 BTC x 50000 / 100 USD
    assert p["notional"] == pytest.approx(500.0) and p["notional_ccy"] == "USD"
    for ex in ("bybit", "kucoin", "gate"):                # contractSize 1: unit not provable
        with pytest.raises(C.OrderRefused, match="inverse"):
            prep(venue(ex), "BTC/USD:BTC", amount=0.01, price=50000)


def test_contract_size_on_unverified_exchange_is_refused(markets):
    m = copy.deepcopy(markets["binance"]["DOGE/USDT:USDT"])
    m["contractSize"] = 10.0
    assert "not verified" in C.contract_refusal("binance", m)
    assert C.contract_refusal("okx", markets["okx"]["DOGE/USDT:USDT"]) == ""


def test_size_is_rounded_down_and_reported(venue):
    p = prep(venue("okx"), "BTC/USDT:USDT", amount=0.00015)   # 0.015 contracts -> 0.01
    assert p["amount_text"] == "0.01" and p["base_amount"] == pytest.approx(0.0001)
    assert any("rounded down" in w for w in p["warnings"])
    with pytest.raises(C.OrderRefused, match="smallest order step"):
        prep(venue("okx"), "PEPE/USDT:USDT", amount=50_000)     # 0.005 contracts


def test_exchange_minimum_is_enforced_in_its_own_units(venue):
    with pytest.raises(C.OrderRefused, match="minimum of 0.00006"):
        prep(venue("kraken"), "BTC/USDT", amount=0.000001)
    ex = venue("htx")   # ccxt states HTX's minimum as one contract in coins
    mn, _mx, min_base = C.order_size_limits("htx", ex.markets["DOGE/USDT:USDT"])
    assert mn == 1.0 and min_base == 100.0


def test_off_grid_price_is_refused_not_rounded(venue):
    with pytest.raises(C.OrderRefused, match="nearest valid price"):
        prep(venue("binance"), "BTC/USDT", amount=0.001, price=50000.123)
    p = prep(venue("binance"), "BTC/USDT", amount=0.001, price=50000.12)
    assert p["create_order"]["price"] == "50000.12"


def test_quote_substitute_and_spot_token_are_refused_with_choices(venue):
    with pytest.raises(C.OrderRefused) as e:
        prep(venue("kraken"), "UNI/USDT", amount=1, price=8)
    assert "UNI/USD" in e.value.alternatives
    with pytest.raises(C.OrderRefused) as e:
        prep(venue("hyperliquid"), "BTC/USDT", amount=0.01, price=50000)
    assert e.value.alternatives == ["BTC/USDC:USDC"]


def test_stop_direction_and_capabilities(venue):
    ex = venue("binance")
    p = prep(ex, "BTC/USDT", side="sell", type="stop", amount=0.001, stop_price=40000)
    assert p["create_order"]["params"]["stopLossPrice"] == "40000" and p["create_order"]["type"] == "market"
    with pytest.raises(C.OrderRefused, match="must be below"):
        prep(ex, "BTC/USDT", side="sell", type="stop", amount=0.001, stop_price=60000)
    with pytest.raises(C.OrderRefused, match="must be above"):
        prep(ex, "BTC/USDT", side="buy", type="stop", amount=0.001, stop_price=40000)
    with pytest.raises(C.OrderRefused, match="not supported"):
        prep(venue("mexc"), "BTC/USDT", side="sell", type="stop", amount=0.001, stop_price=40000)
    bybit = prep(venue("bybit"), "BTC/USDT:USDT", side="sell", type="stop", amount=0.01, stop_price=40000)
    assert bybit["create_order"]["params"]["triggerDirection"] == "descending"
    assert "stopLossPrice" not in bybit["create_order"]["params"]


def test_sl_tp_only_where_attached_legs_exist(venue):
    with pytest.raises(C.OrderRefused, match="not supported on binance spot"):
        prep(venue("binance"), "BTC/USDT", amount=0.001, price=49000, sl=45000, tp=55000)
    p = prep(venue("okx"), "BTC/USDT:USDT", amount=0.01, price=49000, sl=45000, tp=55000)
    assert p["create_order"]["params"]["stopLoss"] == {"triggerPrice": "45000"}
    with pytest.raises(C.OrderRefused, match="stop-loss .* must be below"):
        prep(venue("okx"), "BTC/USDT:USDT", amount=0.01, price=49000, sl=50000, tp=55000)
    with pytest.raises(C.OrderRefused, match="LIMIT"):
        prep(venue("bybit"), "BTC/USDT", type="market", amount=0.001, sl=45000, tp=55000)


def test_hyperliquid_market_order_is_slippage_bounded_without_builder(venue):
    ex = venue("hyperliquid")
    p = prep(ex, "BTC/USDC:USDC", type="market", amount=0.01)
    assert p["create_order"]["price"] == "50000" and p["create_order"]["params"]["slippage"] == "0.02"
    assert p["market_bound"] == "51000"
    assert ex.options["approvedBuilderFee"] is False and ex.options["refSet"] is True
    assert ex.handle_builder_fee_approval() is True and ex.options["approvedBuilderFee"] is False
    # A Hyperliquid spot token is never an order target: by symbol it resolves to the perpetual
    # (refused as a substitute), and its market is flagged untradable.
    with pytest.raises(C.OrderRefused, match="BTC/USDC:USDC"):
        prep(ex, "BTC/USDC", amount=0.01, price=50000)
    caps = C.order_capabilities(ex, "hyperliquid", ex.markets["BTC/USDC"])
    assert not caps["orders"] and "spot tokens" in caps["orders_reason"]


def test_reduce_only_spot_and_stale_markets_refused(venue):
    with pytest.raises(C.OrderRefused, match="reduce-only"):
        prep(venue("binance"), "BTC/USDT", amount=0.001, price=50000, reduce_only=True)
    ex = venue("binance")
    ex._fincept_markets_stale = True
    with pytest.raises(C.OrderRefused, match="older than a day"):
        prep(ex, "BTC/USDT", amount=0.001, price=50000)


def test_margin_mode_and_leverage_travel_with_orders_where_per_order(venue):
    p = prep(venue("okx"), "BTC/USDT:USDT", amount=0.01, margin_mode="isolated")
    assert p["create_order"]["params"]["marginMode"] == "isolated"
    with pytest.raises(C.OrderRefused, match="cross or isolated"):
        prep(venue("bitget"), "BTC/USDT:USDT", amount=0.01, margin_mode="")
    k = prep(venue("kucoin"), "BTC/USDT:USDT", amount=0.01, leverage=5)
    assert k["create_order"]["params"]["leverage"] == 5 and k["leverage_sent"] == 5
    assert "marginMode" not in prep(venue("binance"), "BTC/USDT:USDT", amount=0.01)["create_order"]["params"]


def test_confirm_hash_is_deterministic_and_binds_every_detail(venue):
    ex = venue("okx")
    a = prep(ex, "BTC/USDT:USDT", amount=0.01)
    b = prep(ex, "BTC/USDT:USDT", amount=0.01)
    assert a["confirm_hash"] == b["confirm_hash"]
    assert prep(ex, "BTC/USDT:USDT", amount=0.02)["confirm_hash"] != a["confirm_hash"]
    assert prep(ex, "BTC/USDT:USDT", amount=0.01, side="sell")["confirm_hash"] != a["confirm_hash"]
    assert prep(ex, "BTC/USDT:USDT", amount=0.01, client_order_token="f" * 32)["confirm_hash"] != a["confirm_hash"]


@pytest.mark.parametrize("exchange_id,symbol,key,check", [
    ("hyperliquid", "BTC/USDC:USDC", "clientOrderId", lambda v: v == "0x" + TOKEN),
    ("gate", "BTC/USDT", "clientOrderId", lambda v: v.startswith("t-") and len(v) == 28),
    ("htx", "BTC/USDT:USDT", "clientOrderId", lambda v: isinstance(v, int) and v < 2 ** 53),
    ("bitfinex", "BTC/USDT", "clientOrderId", lambda v: isinstance(v, int) and v < 2 ** 45),
    ("coinbase", "BTC/USDT", "client_order_id", lambda v: len(v) == 36 and v.count("-") == 4),
    ("cryptocom", "BTC/USDT", "client_oid", lambda v: v == TOKEN),
    ("okx", "BTC/USDT", "clientOrderId", lambda v: v == TOKEN and v.isalnum()),
])
def test_client_order_id_format_per_exchange(venue, exchange_id, symbol, key, check):
    p = prep(venue(exchange_id), symbol, amount=0.01, price=50000)
    assert check(p["create_order"]["params"][key])
    assert C.same_client_order_id(exchange_id, p["client_order_id"], p["client_order_id"])


def test_credential_fingerprint_matches_the_app():
    # ExchangeDaemonPool::credential_fingerprint: SHA-256 of the five fields joined by "|", 16 hex.
    import hashlib
    creds = {"api_key": "k", "secret": "s", "password": "", "wallet_address": "", "private_key": ""}
    assert C.credential_fingerprint(creds) == hashlib.sha256(b"k|s|||").hexdigest()[:16]
    assert C.credential_fingerprint({}) == ""


def test_error_text_is_scrubbed():
    msg = "htx GET https://api.huobi.pro/v1/order?AccessKeyId=abcdef123456&Signature=x failed for secretvalue"
    out = C.scrub_message(msg, {"secret": "secretvalue"})
    assert "abcdef123456" not in out and "secretvalue" not in out and "api.huobi.pro/v1/order" in out
