"""
Unified exchange client — shared initialization for all exchange scripts.
Handles exchange instantiation, credential loading, and error normalization.

Called by C++ python_runner as subprocess — outputs JSON to stdout.
"""

import os
import re
import sys
import json
import hashlib
import threading
import time
import ccxt

# Markets cache. A market list changes rarely, so a cached copy younger than
# a day is used as-is, and an older one is still used when the exchange cannot
# be reached — a stale symbol list beats a dead trading screen.
_MARKETS_CACHE_TTL = 24 * 3600
_MARKETS_CACHE_VERSION = 2


def _markets_cache_dir() -> str:
    """User-writable cache directory: <data root>/cache/exchange_markets.

    The scripts tree is read-only in installed builds (Program Files, /opt,
    an AppImage), so the cache lives under the app's data directory, never in
    the scripts tree. When it cannot be created the cache is simply skipped
    (loads find nothing, saves fail silently).
    """
    # The terminal always sets FINCEPT_DATA_DIR. Run by hand: the edition's own
    # default data root (fincept_paths), never com.fincept.terminal (a separate
    # private build's).
    scripts_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    if scripts_root not in sys.path:
        sys.path.append(scripts_root)
    import fincept_paths
    d = os.path.join(str(fincept_paths.data_root()), "cache", "exchange_markets")
    try:
        os.makedirs(d, exist_ok=True)
    except OSError:
        pass
    return d


def get_markets_cache_path(exchange_id: str) -> str:
    """Return path to the per-exchange markets cache file."""
    return os.path.join(_markets_cache_dir(), f"markets_{exchange_id}.json")


def _diag(message: str) -> None:
    """One diagnostic line on stderr (stdout is the JSON result channel; never data or credentials)."""
    sys.stderr.write(f"[exchange_client] {message}\n")
    sys.stderr.flush()


def load_cached_market_data(exchange_id: str, allow_stale: bool = False):
    """Return (markets, currencies) from the cache, or (None, None).

    `currencies` may be None for a cache written before currencies were stored.
    With allow_stale the TTL is ignored (offline fallback).
    """
    path = get_markets_cache_path(exchange_id)
    try:
        if os.path.exists(path):
            age = time.time() - os.path.getmtime(path)
            if allow_stale or age < _MARKETS_CACHE_TTL:
                with open(path, "r", encoding="utf-8") as f:
                    data = json.load(f)
                if isinstance(data, dict) and data.get("version") == _MARKETS_CACHE_VERSION:
                    return data.get("markets") or None, data.get("currencies") or None
                if isinstance(data, dict) and data:
                    return data, None  # legacy file: a bare markets dict
    except Exception as exc:
        _diag(f"WARNING Markets cache unreadable, reloading from the exchange exchange={exchange_id} "
              f"file={os.path.basename(path)} error={type(exc).__name__}")
    return None, None


def load_cached_markets(exchange_id: str, allow_stale: bool = False) -> dict | None:
    """Return cached markets dict if still fresh (within TTL), else None."""
    return load_cached_market_data(exchange_id, allow_stale)[0]


def save_markets_cache(exchange_id: str, markets: dict, currencies: dict | None = None) -> None:
    """Persist markets (and currencies) so the next start can skip load_markets()."""
    if not markets:
        return
    path = get_markets_cache_path(exchange_id)
    # Unique per writer: the exchange daemon and each ws_stream process share
    # this directory and may save the same exchange's list at the same moment.
    tmp = f"{path}.{os.getpid()}.{threading.get_ident()}.tmp"
    try:
        with open(tmp, "w", encoding="utf-8") as f:
            json.dump({"version": _MARKETS_CACHE_VERSION, "markets": markets,
                       "currencies": currencies or {}}, f, default=str)
        os.replace(tmp, path)
    except Exception:
        try:
            os.remove(tmp)
        except OSError:
            pass


def apply_cached_markets(exchange, markets: dict, currencies: dict | None) -> bool:
    """Install cached markets via set_markets() so ccxt rebuilds its indexes
    (markets_by_id, symbols, currencies). Returns True on success."""
    try:
        exchange.set_markets(markets, currencies)
        return bool(exchange.markets)
    except Exception as exc:
        _diag(f"WARNING Cached markets could not be applied exchange={getattr(exchange, 'id', '?')} "
              f"error={type(exc).__name__}")
        return False


def _is_retryable(exc) -> bool:
    # RequestTimeout: a connection that hung. A bare NetworkError (not one of
    # its subclasses such as DDoSProtection / RateLimitExceeded /
    # ExchangeNotAvailable) is a dropped or refused connection.
    return isinstance(exc, ccxt.RequestTimeout) or type(exc) is ccxt.NetworkError


def install_get_retry(exchange, attempts: int = 3) -> None:
    """Retry idempotent GET requests that hit a hung or dropped connection.

    Some networks lose a share of new connections to an exchange (seen with
    Kraken: roughly half of fresh TLS connections stall until timeout). One
    stalled connection must not fail a quote, a candle fetch or load_markets.
    Only GET is retried: a POST may be an order, and retrying one could place
    it twice. Works for both the sync and the async (ccxt.pro) classes.
    """
    original = exchange.fetch
    import inspect
    if inspect.iscoroutinefunction(original):
        async def fetch(url, method="GET", headers=None, body=None):
            if (method or "GET").upper() != "GET":
                return await original(url, method, headers, body)
            for i in range(attempts):
                try:
                    return await original(url, method, headers, body)
                except Exception as e:
                    if i == attempts - 1 or not _is_retryable(e):
                        raise
    else:
        def fetch(url, method="GET", headers=None, body=None):
            if (method or "GET").upper() != "GET":
                return original(url, method, headers, body)
            for i in range(attempts):
                try:
                    return original(url, method, headers, body)
                except Exception as e:
                    if i == attempts - 1 or not _is_retryable(e):
                        raise
    exchange.fetch = fetch


def get_default_type(exchange_id: str) -> str:
    """Return the appropriate defaultType for an exchange.
    Hyperliquid is a perps DEX — its primary market type is swap.
    All others default to spot.
    """
    return "swap" if exchange_id in ("hyperliquid",) else "spot"


# ── Market resolution ───────────────────────────────────────────────────────
# ONE function decides which market a symbol the app shows means, for the live
# stream (ws_stream.py), every REST handler of the daemon and the order path.
# Two resolvers used to disagree: on Kraken "UNI/USDT" streamed the UNI/BTC book
# while an order went to UNI/USD.
#
# Rules (correctness over convenience):
#   * The market TYPE always matches the symbol's form: "B/Q" is spot, "B/Q:S" a
#     perpetual swap settled in S (linear, or inverse when S == B), and dated
#     futures / options are only ever matched by their full ccxt symbol.
#   * On a perps venue (Hyperliquid) a bare "B/Q" means the perpetual; it never
#     resolves to one of its spot tokens, which are user-deployed and may share a
#     ticker with an unrelated coin (spot "PEPE/USDC" is not PEPE).
#   * No exact market: the base never changes and the settle currency never
#     changes; the quote may be swapped within the USD family (USDT/USDC/USD...)
#     so a price can be SHOWN — match "substituted", tradable False. The order
#     path accepts only "exact" matches: it never trades a substitute.
#   * Nothing matching: "missing", with the nearest markets as choices.

USD_QUOTES = frozenset({"USD", "USDT", "USDC", "BUSD", "FDUSD", "TUSD", "USDE", "DAI"})
PREFERRED_USD_QUOTES = ("USDT", "USDC", "USD")
PERPS_VENUES = frozenset({"hyperliquid"})

_DATE_RX = re.compile(r"^\d{6}$")


def parse_symbol(symbol):
    """Split a ccxt unified symbol into base / quote / settle and its form:
    "spot" (B/Q), "swap" (B/Q:S), "future" (B/Q:S-YYMMDD) or "option"
    (B/Q:S-YYMMDD-STRIKE-C|P). None when it is not in one of those forms."""
    if not isinstance(symbol, str):
        return None
    s = symbol.strip()
    if s.count("/") != 1:
        return None
    base, rest = s.split("/", 1)
    settle = None
    suffix = None
    if ":" in rest:
        quote, tail = rest.split(":", 1)
        parts = tail.split("-")
        settle, suffix = parts[0], parts[1:]
    else:
        quote = rest
    if not base or not quote or (settle is not None and not settle):
        return None
    if suffix is None:
        form = "spot"
    elif not suffix:
        form = "swap"
    elif len(suffix) == 1 and _DATE_RX.match(suffix[0]):
        form = "future"
    elif len(suffix) == 3 and _DATE_RX.match(suffix[0]) and suffix[2] in ("C", "P"):
        form = "option"
    else:
        return None
    return {"base": base.upper(), "quote": quote.upper(),
            "settle": settle.upper() if settle else None, "form": form}


def market_kind(m):
    """"spot" | "swap" | "future" | "option" (or the raw type) of a ccxt market."""
    m = m or {}
    if m.get("option") or m.get("type") == "option":
        return "option"
    if m.get("future") or m.get("type") in ("future", "delivery"):
        return "future"
    if m.get("swap") or m.get("type") == "swap":
        return "swap"
    if m.get("spot") or m.get("type") == "spot":
        return "spot"
    return m.get("type") or "other"


def is_perpetual(m):
    return market_kind(m) == "swap" and not m.get("expiry")


def _upper(v):
    return (v or "").upper() if isinstance(v, str) else ""


def _active(m):
    return m.get("active", True) is not False


def _quote_rank(code, wanted):
    code = _upper(code)
    if code == wanted:
        return 0
    if code in PREFERRED_USD_QUOTES:
        return 1 + PREFERRED_USD_QUOTES.index(code)
    return 8 if code in USD_QUOTES else 9


def _same_family(code, wanted):
    code = _upper(code)
    if wanted in USD_QUOTES:
        return code in USD_QUOTES
    return code == wanted


def _alternatives(markets, base, group, limit=8):
    """Active markets for `base` of the same kind group ("spot" / "perp" / a kind),
    USD quotes first — offered to the user when a pair cannot be traded as typed."""
    out = []
    for sym, m in markets.items():
        if _upper(m.get("base")) != base or not _active(m):
            continue
        if group == "perp":
            if not is_perpetual(m):
                continue
        elif market_kind(m) != group:
            continue
        out.append(m)
    out.sort(key=lambda m: (_quote_rank(m.get("settle") or m.get("quote"), "USDT"),
                            _quote_rank(m.get("quote"), "USDT"), m.get("symbol") or ""))
    return [m.get("symbol") for m in out[:limit]]


def resolve_market(markets, requested, exchange_id):
    """Resolve the symbol the app shows to a market of this exchange.

    Returns {"requested", "symbol", "match", "tradable", "reason", "alternatives"}:
      match "exact"       — `symbol` == `requested`; tradable unless inactive
      match "substituted" — `symbol` is a related market (same base, type and
                            settle; quote within the USD family) that may be
                            SHOWN with `reason`; never tradable
      match "missing"     — `symbol` None; `alternatives` lists the nearest markets
    """
    markets = markets or {}
    out = {"requested": requested, "symbol": None, "match": "missing", "tradable": False,
           "reason": "", "alternatives": []}
    if not isinstance(requested, str) or not requested.strip():
        out["reason"] = "no symbol given"
        return out
    req = requested.strip()
    out["requested"] = req
    venue = exchange_id or "this exchange"
    p = parse_symbol(req)

    if p is None:
        m = markets.get(req)
        if m is not None:
            out.update(symbol=req, match="exact", tradable=_active(m),
                       reason="" if _active(m) else f"{req} is not active on {venue}")
        else:
            out["reason"] = f"{req} is not a BASE/QUOTE symbol listed on {venue}"
        return out

    base, quote, settle, form = p["base"], p["quote"], p["settle"], p["form"]
    perps_venue = exchange_id in PERPS_VENUES

    def substitute(cands, why):
        cands.sort(key=lambda m: (_quote_rank(m.get("settle") or m.get("quote"), settle or quote),
                                  _quote_rank(m.get("quote"), quote), m.get("symbol") or ""))
        best = cands[0]["symbol"]
        out.update(symbol=best, match="substituted", tradable=False, reason=why.format(best=best))
        return out

    if perps_venue and form == "spot":
        group = "perp"
        cands = [m for m in markets.values()
                 if _upper(m.get("base")) == base and _active(m) and is_perpetual(m)
                 and m.get("linear") is not False and _same_family(m.get("settle"), quote)]
        if cands:
            substitute(cands, f"{req} is not a market on {venue}; its perpetual is {{best}}")
        else:
            out["reason"] = f"{venue} lists no {base} perpetual settled in {quote}"
    elif req in markets:
        m = markets[req]
        kind = market_kind(m)
        expected = {"spot": "spot", "swap": "swap", "future": "future", "option": "option"}[form]
        if kind != expected or _upper(m.get("base")) != base:
            out["reason"] = f"{req} on {venue} is a {kind} market, not what the symbol says"
            group = expected
        else:
            out.update(symbol=req, match="exact", tradable=_active(m),
                       reason="" if _active(m) else f"{req} is not active on {venue}")
            return out
    elif form == "spot":
        group = "spot"
        if quote in USD_QUOTES:
            cands = [m for m in markets.values()
                     if _upper(m.get("base")) == base and _active(m) and market_kind(m) == "spot"
                     and _upper(m.get("quote")) in USD_QUOTES]
            if cands:
                substitute(cands, f"{req} is not listed on {venue}; showing {{best}}")
        if out["match"] == "missing":
            out["reason"] = f"{req} is not listed on {venue}"
    elif form == "swap":
        group = "perp"
        inverse = settle == base
        cands = [m for m in markets.values()
                 if _upper(m.get("base")) == base and _active(m) and is_perpetual(m)
                 and _upper(m.get("settle")) == settle
                 and bool(m.get("inverse")) == inverse and bool(m.get("linear")) == (not inverse)
                 and _same_family(m.get("quote"), quote)]
        if cands:
            substitute(cands, f"{req} is not listed on {venue}; showing {{best}}")
        else:
            out["reason"] = f"{venue} lists no {base} perpetual settled in {settle}"
    else:
        group = form
        out["reason"] = (f"{req} is not listed on {venue} (dated futures and options are only "
                         f"matched by their exact symbol)")

    out["alternatives"] = [s for s in _alternatives(markets, base, group) if s != out["symbol"]]
    if out["symbol"]:
        out["alternatives"].insert(0, out["symbol"])
    return out


def market_descriptor(m):
    """The facts about a market the app shows next to an order."""
    m = m or {}
    limits = m.get("limits") or {}
    precision = m.get("precision") or {}
    return {
        "symbol": m.get("symbol"),
        "id": m.get("id"),
        "kind": market_kind(m),
        "base": m.get("base"), "quote": m.get("quote"), "settle": m.get("settle"),
        "linear": m.get("linear"), "inverse": m.get("inverse"),
        "contract_size": _num(m.get("contractSize")),
        "expiry": m.get("expiry"), "expiry_datetime": m.get("expiryDatetime"),
        "active": m.get("active"),
        "amount_precision": _num(precision.get("amount")),
        "price_precision": _num(precision.get("price")),
        "min_amount": _num((limits.get("amount") or {}).get("min")),
        "max_amount": _num((limits.get("amount") or {}).get("max")),
        "min_cost": _num((limits.get("cost") or {}).get("min")),
        "max_cost": _num((limits.get("cost") or {}).get("max")),
    }


def _num(v):
    try:
        f = float(v)
    except (TypeError, ValueError):
        return None
    return f if f == f and f not in (float("inf"), float("-inf")) else None


def order_size_limits(exchange_id, market):
    """(min, max) order size in the exchange's own unit — contracts for
    derivatives, coins for spot — plus the minimum in base coins (what the
    ticket's quantity is in). ccxt's HTX linear contracts state the minimum as
    one contract's size in coins (htx.py: minAmount = contractSize); converted."""
    d = market_descriptor(market)
    mn, mx = d["min_amount"], d["max_amount"]
    cs = d["contract_size"] or 1.0
    kind = market_kind(market)
    if exchange_id == "htx" and kind != "spot" and market.get("linear") and mn:
        mn = mn / cs
    if kind == "spot":
        min_base = mn
    elif market.get("inverse"):
        min_base = None        # depends on the price
    else:
        min_base = mn * cs if mn else None
    return mn, mx, min_base


# ── Order rules ─────────────────────────────────────────────────────────────
# Every combination below was checked against the request ccxt builds for it
# (scripts/exchange/tests/dry_run_orders.py captures the HTTP request without
# sending it). Anything not listed is refused before it is sent — never sent in
# a degraded form.

class OrderRefused(Exception):
    """An order that cannot be made provably right; nothing was sent."""

    def __init__(self, message, alternatives=None):
        super().__init__(message)
        self.alternatives = alternatives or []


# Exchanges whose contract-order size field is a number of CONTRACTS and whose
# ccxt contractSize is the base-currency amount of one linear contract (OKX sz,
# Gate size, MEXC vol, HTX volume, KuCoin size are all contract counts). On every
# other exchange a linear contract must have contractSize 1 (then contracts ==
# coins whatever the venue counts in), or the order is refused.
LINEAR_CONTRACT_COUNT_EXCHANGES = frozenset({"okx", "gate", "mexc", "htx", "kucoin"})
# Inverse (coin-margined) contracts: size is a number of contracts of
# contractSize USD each. Only where that face value is unambiguous in ccxt's
# market (100 USD BTC / 10 USD others); bybit/kucoin/gate/bingx/bitget report
# contractSize 1 for coin-margined markets, which does not say whether the size
# is in USD or in coins there — refused.
INVERSE_CONTRACT_EXCHANGES = frozenset({"binance", "okx", "htx"})

# STOP (stop-market) and STOP-LIMIT. The ticket's rule: a SELL stop triggers
# when the price FALLS to it (it must be below the market), a BUY stop when it
# RISES to it (above the market). How that reaches ccxt, per (exchange, kind
# "spot" | "contract") and order type; anything absent is refused:
#   "slp"      {"stopLossPrice": p} — ccxt derives the direction from the side
#              (checked: the request is a plain conditional order, not a
#              reduce-only / position stop-loss)
#   "trig_dir" {"triggerPrice": p, "triggerDirection": "descending"|"ascending"}
#   "trig"     {"triggerPrice": p} — the exchange triggers on the price
#              crossing p from where it is when the order is placed
#
# Refused after reading the request (ccxt 4.5.64): OKX spot (ccxt turns the
# algo order's tdMode "cash" into "cross" — a margin order); Bybit via
# stopLossPrice (reduce-only); Bitget contracts via stopLossPrice (a position
# stop-loss); Bitget spot (a stop-market BUY is sized as a spend at today's
# price); Gate spot (expires silently after 24 h; stop-market BUY sized as a
# 0.01 quote spend); MEXC (no stop support in ccxt); HTX contracts (no trigger
# direction in the request); BingX spot (direction not in the request);
# Bitfinex via stopLossPrice (sent as a stray field on a plain market order);
# Coinbase stop-market (ccxt: not supported).
_BOTH = lambda style: {"stop": style, "stop_limit": style}   # noqa: E731
STOP_STYLES = {
    ("kraken", "spot"): _BOTH("slp"),          # ordertype stop-loss / stop-loss-limit
    ("binance", "spot"): _BOTH("slp"),         # STOP_LOSS / STOP_LOSS_LIMIT
    ("binance", "contract"): _BOTH("slp"),     # algoOrder STOP_MARKET / STOP
    ("coinbase", "spot"): {"stop_limit": "slp"},       # stop_direction STOP_DOWN (sell) / STOP_UP (buy)
    ("coinbase", "contract"): {"stop_limit": "slp"},
    ("okx", "contract"): _BOTH("slp"),         # order-algo conditional slTriggerPx
    ("bybit", "spot"): _BOTH("trig"),          # orderFilter StopOrder
    ("bybit", "contract"): _BOTH("trig_dir"),  # triggerDirection 2 (falls) / 1 (rises)
    ("kucoin", "spot"): _BOTH("slp"),          # stop "loss" (sell) / "entry" (buy); unified: DOWN / UP
    ("kucoin", "contract"): _BOTH("trig_dir"), # stop "down" / "up"; unified: triggerDirection DOWN / UP
    ("bitget", "contract"): _BOTH("trig"),     # place-plan-order normal_plan
    ("gate", "contract"): _BOTH("slp"),        # price_orders rule 2 (<=) / 1 (>=)
    ("htx", "spot"): {"stop_limit": "trig"},   # sell-stop-limit operator lte / buy-stop-limit gte
    ("cryptocom", "spot"): _BOTH("slp"),       # STOP_LOSS / STOP_LIMIT with ref_price
    ("cryptocom", "contract"): _BOTH("slp"),
    ("bingx", "contract"): _BOTH("slp"),       # STOP_MARKET / STOP
    ("bitfinex", "spot"): _BOTH("trig"),       # EXCHANGE STOP / EXCHANGE STOP LIMIT
    ("bitfinex", "contract"): _BOTH("trig"),   # STOP / STOP LIMIT
    ("hyperliquid", "contract"): _BOTH("slp"), # trigger tpsl "sl", market bound by slippage
}
# Stop-loss / take-profit legs attached to the entry order (ccxt stopLoss /
# takeProfit params): (exchange, kind) -> entry order types where the request
# was checked to carry real legs. Refused: Binance, Kraken, Coinbase, Gate,
# MEXC, HTX, Crypto.com, Bitfinex (no legs in ccxt), KuCoin spot (sent as raw
# JSON fields), BingX spot (legs dropped from the request), Bybit spot market
# entries (ccxt refuses).
_MKT_LMT = frozenset({"market", "limit"})
ATTACHED_SLTP = {
    ("okx", "spot"): _MKT_LMT,              # attachAlgoOrds slTriggerPx / tpTriggerPx, market close
    ("okx", "contract"): _MKT_LMT,
    ("bybit", "spot"): frozenset({"limit"}),  # stopLoss / takeProfit, sl/tpOrderType Market
    ("bybit", "contract"): _MKT_LMT,
    ("kucoin", "contract"): _MKT_LMT,       # st-orders triggerStopDown/UpPrice; unified sl/tpTriggerPrice
    ("bitget", "contract"): _MKT_LMT,       # presetStopLossPrice / presetStopSurplusPrice
    ("bingx", "contract"): _MKT_LMT,        # stopLoss / takeProfit STOP_MARKET / TAKE_PROFIT_MARKET
    ("hyperliquid", "contract"): _MKT_LMT,  # normalTpsl grouping, reduce-only trigger legs
}
# Contract orders whose margin mode travels with each order (OKX tdMode, Bitget
# / KuCoin marginMode, MEXC openType, HTX margin_mode — ccxt otherwise sends
# "cross" whatever the user chose): the ticket's cross/isolated choice is sent.
MARGIN_MODE_PER_ORDER = frozenset({"okx", "bitget", "kucoin", "mexc", "htx"})
# Back-compat names (read by older scripts).
STOP_MARKET_VERIFIED = STOP_LIMIT_VERIFIED = ATTACHED_SLTP_VERIFIED = frozenset()
# Market orders that need a price: Hyperliquid (a slippage-bounded IOC limit).
MARKET_SLIPPAGE = "0.02"
# Margin-mode changes that apply to the whole account, not one market.
ACCOUNT_WIDE_MARGIN_MODE = frozenset({"bybit"})


def _features_create_order(ex, market):
    feats = getattr(ex, "features", None) or {}
    kind = market_kind(market)
    if kind == "spot":
        node = feats.get("spot")
    else:
        top = feats.get("swap" if kind == "swap" else "future")
        node = None
        if isinstance(top, dict):
            node = top.get("inverse" if market.get("inverse") else "linear")
    if not isinstance(node, dict):
        return {}
    return node.get("createOrder") or {}


def order_capabilities(ex, exchange_id, market):
    """What the ticket may send for this market, each refusal with its reason."""
    kind = market_kind(market)
    co = _features_create_order(ex, market)
    has = getattr(ex, "has", {}) or {}
    venue = exchange_id
    caps = {"kind": kind, "orders": True, "orders_reason": "",
            "market": True, "market_reason": "",
            "stop": False, "stop_reason": "", "stop_limit": False, "stop_limit_reason": "",
            "sl_tp": False, "sl_tp_reason": "",
            "reduce_only": kind != "spot",
            "leverage": kind in ("swap", "future") and bool(has.get("setLeverage")),
            "margin_mode": kind in ("swap", "future") and bool(has.get("setMarginMode")),
            "margin_mode_scope": "account" if exchange_id in ACCOUNT_WIDE_MARGIN_MODE else "market",
            "units": "base"}
    why = contract_refusal(exchange_id, market)
    if exchange_id in PERPS_VENUES and kind == "spot":
        why = (f"{venue} spot tokens are not traded from this terminal (their tickers can "
               f"belong to unrelated coins); use the perpetual")
    elif kind == "option":
        why = "options are not traded from this ticket"
    if why:
        caps.update(orders=False, orders_reason=why)
    if kind != "spot":
        caps["units"] = "contracts"
    group = "spot" if kind == "spot" else "contract"
    where = f"{venue} {'spot' if kind == 'spot' else 'contracts'}"
    if not has.get("createOrder", True):
        caps.update(orders=False, orders_reason=f"{venue} does not support placing orders")
    styles = STOP_STYLES.get((exchange_id, group)) or {}
    for otype, label in (("stop", "STOP (stop-market)"), ("stop_limit", "STOP-LIMIT")):
        style = styles.get(otype)
        ok = bool(style) and _feature_allows(co, style)
        caps[otype] = ok
        caps[otype + "_reason"] = "" if ok else f"{label} orders on {where} are not supported here"
    sltp = ATTACHED_SLTP.get((exchange_id, group)) or ()
    attached = co.get("attachedStopLossTakeProfit") or has.get("createOrderWithTakeProfitAndStopLoss")
    caps["sl_tp"] = bool(sltp) and bool(attached)
    caps["sl_tp_types"] = sorted(sltp) if caps["sl_tp"] else []
    caps["sl_tp_reason"] = "" if caps["sl_tp"] else (
        f"stop-loss / take-profit attached to an order is not supported on {where}; place a separate "
        f"STOP order after the entry fills")
    caps["margin_mode_per_order"] = kind != "spot" and exchange_id in MARGIN_MODE_PER_ORDER
    return caps


def _feature_allows(co, style):
    """ccxt's own feature table for this market must agree with the style used."""
    if style == "slp":
        return bool(co.get("stopLossPrice"))
    if style == "trig_dir":
        return bool(co.get("triggerPrice")) and bool(co.get("triggerDirection"))
    if style == "trig":
        return bool(co.get("triggerPrice"))
    return False


def stop_params(exchange_id, market, otype, side, stop_text):
    """ccxt params for a STOP / STOP-LIMIT (see STOP_STYLES); None when refused."""
    group = "spot" if market_kind(market) == "spot" else "contract"
    style = (STOP_STYLES.get((exchange_id, group)) or {}).get(otype)
    if style == "slp":
        return {"stopLossPrice": stop_text}
    if style == "trig_dir":
        return {"triggerPrice": stop_text, "triggerDirection": "descending" if side == "sell" else "ascending"}
    if style == "trig":
        return {"triggerPrice": stop_text}
    return None


def contract_refusal(exchange_id, market):
    """Why a base-coin quantity cannot be converted to this market's order size
    unambiguously (empty when it can)."""
    kind = market_kind(market)
    if kind == "spot":
        return ""
    if kind == "option":
        return "options are not traded from this ticket"
    cs = _num(market.get("contractSize"))
    if not cs or cs <= 0:
        return f"{market.get('symbol')}: the exchange reports no contract size, so the order size is unknown"
    if market.get("inverse"):
        if exchange_id not in INVERSE_CONTRACT_EXCHANGES:
            return (f"{market.get('symbol')} is a coin-margined (inverse) contract; its size unit on "
                    f"{exchange_id} cannot be confirmed from the market data, so it is not traded here")
        return ""
    if not market.get("linear"):
        return f"{market.get('symbol')}: neither linear nor inverse — contract size unit unknown"
    if cs != 1 and exchange_id not in LINEAR_CONTRACT_COUNT_EXCHANGES:
        return (f"{market.get('symbol')} has a contract size of {cs:g} {market.get('base')} and the "
                f"size unit on {exchange_id} is not verified, so it is not traded here")
    return ""


def client_order_id(exchange_id, market, token):
    """(param name, value) carrying the app's idempotency token in the form this
    exchange accepts, or (None, None) when it has none. Deterministic from
    `token`: the same ticket pressed again reuses the id."""
    t = re.sub(r"[^0-9a-f]", "", str(token or "").lower())
    if len(t) < 16:
        return None, None
    t = (t * 2)[:32]
    kind = market_kind(market)
    uuid = f"{t[:8]}-{t[8:12]}-{t[12:16]}-{t[16:20]}-{t[20:32]}"
    if exchange_id == "hyperliquid":
        return "clientOrderId", "0x" + t                # 128-bit hex cloid
    if exchange_id in ("kraken", "kucoin"):
        return "clientOrderId", uuid                    # cl_ord_id / clientOid
    if exchange_id == "coinbase":
        return "client_order_id", uuid                  # ccxt drops clientOrderId and sends its own
    if exchange_id == "gate":
        # Gate's "text" must start with "t-" (28 chars at most). ccxt adds the
        # prefix for plain orders but not for trigger orders, so send it.
        return "clientOrderId", "t-" + t[:26]
    if exchange_id == "htx":
        return ("clientOrderId", t) if kind == "spot" else ("clientOrderId", int(t[:13], 16))
    if exchange_id == "bitfinex":
        return "clientOrderId", int(t[:11], 16)         # cid: 45-bit integer
    if exchange_id == "cryptocom":
        return "client_oid", t                          # ccxt drops clientOrderId there
    if exchange_id in ("binance", "okx", "bybit", "bitget", "mexc", "bingx"):
        return "clientOrderId", t
    return None, None


def same_client_order_id(exchange_id, sent, reported):
    """Whether the clientOrderId an exchange reports is the one we sent."""
    if sent is None or reported is None:
        return False
    a, b = str(sent).lower(), str(reported).lower()
    if exchange_id == "gate":
        a, b = a[2:] if a.startswith("t-") else a, b[2:] if b.startswith("t-") else b
    return a == b


def credential_fingerprint(creds):
    """Same value as ExchangeDaemonPool::credential_fingerprint in the app: the
    first 16 hex chars of SHA-256(api_key|secret|password|wallet|private_key)."""
    c = creds or {}
    fields = [c.get(k) or "" for k in ("api_key", "secret", "password", "wallet_address", "private_key")]
    if not any(fields):
        return ""
    return hashlib.sha256("|".join(fields).encode("utf-8")).hexdigest()[:16]


def account_label(creds):
    """A non-secret label for the account an order goes to."""
    c = creds or {}
    if c.get("wallet_address"):
        w = str(c["wallet_address"])
        return f"wallet {w[:6]}…{w[-4:]}" if len(w) > 12 else f"wallet {w}"
    k = str(c.get("api_key") or "")
    if not k:
        return "no API key"
    return f"API key {k[:4]}…{k[-4:]}" if len(k) > 10 else "API key ****"


_URL_QUERY_RX = re.compile(r"(https?://[^\s\"'?]+)\?[^\s\"']*")


def scrub_message(message, creds=None):
    """Error text safe to show and log: URL query strings (HTX puts the API key
    id there) and any credential value removed."""
    text = _URL_QUERY_RX.sub(r"\1?…", str(message))
    for v in (creds or {}).values():
        if isinstance(v, str) and len(v) >= 6:
            text = text.replace(v, "***")
    return text


def make_exchange(exchange_id: str, credentials: dict = None,
                  sandbox: bool = False, timeout_ms: int = 10000) -> ccxt.Exchange:
    """Create and configure a ccxt exchange instance.

    timeout_ms defaults to 10s (was 30s) — fast failure on slow exchanges.
    WS stream sets its own timeout directly on the ccxt.pro instance.
    """
    if exchange_id not in ccxt.exchanges:
        raise ValueError(f"Unknown exchange: {exchange_id}. Available: {len(ccxt.exchanges)}")

    config = {
        "enableRateLimit": True,
        "timeout": timeout_ms,
        "options": {"defaultType": get_default_type(exchange_id)},
    }

    if credentials:
        if credentials.get("api_key"):
            config["apiKey"] = credentials["api_key"]
        if credentials.get("secret"):
            config["secret"] = credentials["secret"]
        if credentials.get("password"):
            config["password"] = credentials["password"]
        if credentials.get("uid"):
            config["uid"] = credentials["uid"]

    exchange_class = getattr(ccxt, exchange_id)
    exchange = exchange_class(config)
    install_get_retry(exchange)

    if sandbox:
        exchange.set_sandbox_mode(True)

    return exchange


def output_success(data: dict | list):
    """Print JSON result to stdout for C++ python_runner to extract.
    Uses compact separators to reduce payload size (no whitespace)."""
    print(json.dumps({"success": True, "data": data}, default=str, separators=(",", ":")))


def output_error(message: str, code: str = "EXCHANGE_ERROR"):
    """Print JSON error to stdout."""
    print(json.dumps({"success": False, "error": message, "code": code}, default=str, separators=(",", ":")))
    sys.exit(1)


def parse_credentials_from_stdin() -> dict:
    """Read credentials from stdin (passed by C++ via execute_with_stdin)."""
    data = ""
    try:
        data = sys.stdin.read()
        if data.strip():
            return json.loads(data)
    except (json.JSONDecodeError, EOFError) as exc:
        # Size only — never the credentials.
        _diag(f"WARNING Credentials on stdin are not valid JSON bytes={len(data)} error={type(exc).__name__}")
    return {}


def run_with_error_handling(func):
    """Decorator to catch ccxt exceptions and output normalized errors.

    Most specific class first: OrderNotFound derives from InvalidOrder and
    ExchangeNotAvailable from NetworkError, so the base-class clause used to
    catch them and their own codes were never produced."""
    def wrapper(*args, **kwargs):
        try:
            return func(*args, **kwargs)
        except ccxt.AuthenticationError as e:
            output_error(f"Authentication failed: {e}", "AUTH_ERROR")
        except ccxt.InsufficientFunds as e:
            output_error(f"Insufficient funds: {e}", "INSUFFICIENT_FUNDS")
        except ccxt.OrderNotFound as e:
            output_error(f"Order not found: {e}", "ORDER_NOT_FOUND")
        except ccxt.InvalidOrder as e:
            output_error(f"Invalid order: {e}", "INVALID_ORDER")
        except ccxt.RateLimitExceeded as e:
            output_error(f"Rate limit exceeded: {e}", "RATE_LIMIT")
        except ccxt.ExchangeNotAvailable as e:
            output_error(f"Exchange unavailable: {e}", "EXCHANGE_UNAVAILABLE")
        except ccxt.NetworkError as e:
            output_error(f"Network error: {e}", "NETWORK_ERROR")
        except ccxt.ExchangeError as e:
            output_error(f"Exchange error: {e}", "EXCHANGE_ERROR")
        except Exception as e:
            output_error(f"Unexpected error: {e}", "UNKNOWN_ERROR")
    return wrapper
