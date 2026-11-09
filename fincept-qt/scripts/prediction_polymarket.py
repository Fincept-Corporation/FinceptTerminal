#!/usr/bin/env python3
"""Polymarket trading bridge for Fincept Terminal.

Invocation:
    python prediction_polymarket.py <command> <json_payload>

Commands:
    derive_api_creds  — POST /auth/api-key via L1 EIP-712 headers; returns {apiKey, secret, passphrase}
    balance           — get available USDC + total value
    positions         — user's open positions (Data API v2, all pages)
    open_orders       — user's live orders
    activity          — trade / split / merge / redeem / reward history
                        (raw Data API v2 rows, snake_case keys)
    place_order       — sign + submit a limit/market order
    cancel_order      — cancel a single order by id
    cancel_market     — cancel all user orders in a (market, asset_id) pair

Signing uses py_clob_client_v2. Polymarket's CLOB V2 went live 2026-04-28 with
no V1 compatibility: orders signed by the legacy `py_clob_client` are rejected.

Payload shape for `place_order`:
    {
        "private_key":    "0x…",          // Polygon signer
        "funder":          "0x…",          // the Polymarket account wallet; required unless EOA
        "signature_type":  0 | 1 | 2 | 3,  // 0=EOA 1=POLY_PROXY 2=POLY_GNOSIS_SAFE
                                           // 3=POLY_1271 (Deposit Wallet: every account since 2026-05-04)
        "api_key":         "…",             // L2 creds (from derive_api_creds)
        "api_secret":      "…",
        "api_passphrase":  "…",
        "token_id":        "asset id",
        "price":           0.65,
        "size":            100,
        "side":            "BUY" | "SELL",
        "order_type":      "GTC" | "FOK" | "GTD" | "FAK",
        "tick_size":       "0.01",          // hint only — the market tick is read from the API
        "neg_risk":        false,           // hint only — read from the API as well
        "expiration":      0                // 0=GTC; unix seconds for GTD
    }

All responses are a single-line JSON object on stdout. Errors come back as
  {"ok": false, "error": "..."}
Success responses are {"ok": true, ...command-specific fields...}.

The script deliberately avoids raising uncaught exceptions — anything that
slips through is turned into a stderr trace + `ok:false` stdout line so the
C++ caller can still parse it.
"""
from __future__ import annotations

import json
import sys
import traceback


def _emit(obj: dict) -> None:
    sys.stdout.write(json.dumps(obj, separators=(",", ":")))
    sys.stdout.write("\n")
    sys.stdout.flush()


def _fail(msg: str, **extra) -> None:
    _emit({"ok": False, "error": msg, **extra})


def _require_pyclob():
    try:
        from py_clob_client_v2 import ClobClient  # noqa: F401
        return True
    except Exception as exc:
        _fail(
            "py_clob_client_v2 is not installed. Re-run the terminal's Python setup, "
            "or run: pip install py-clob-client-v2 eth-account",
            detail=str(exc),
        )
        return False


def _build_client(payload: dict):
    from py_clob_client_v2 import ApiCreds, ClobClient

    host = payload.get("host") or "https://clob.polymarket.com"
    chain_id = int(payload.get("chain_id") or 137)
    private_key = payload.get("private_key") or ""
    if not private_key:
        raise ValueError("private_key is required")
    funder = payload.get("funder") or None
    sig_type = int(payload.get("signature_type", 1))

    creds = None
    if payload.get("api_key") and payload.get("api_secret") and payload.get("api_passphrase"):
        creds = ApiCreds(
            api_key=payload["api_key"],
            api_secret=payload["api_secret"],
            api_passphrase=payload["api_passphrase"],
        )

    # Always pass signature_type: the client sends it on balance reads and signs
    # with it. Smart wallets (1/2/3) hold the funds at `funder`; without it the
    # SDK falls back to the signer address and orders are rejected.
    kwargs = {
        "host": host,
        "chain_id": chain_id,
        "key": private_key,
        "signature_type": sig_type,
    }
    if creds is not None:
        kwargs["creds"] = creds
    if sig_type != 0 and funder:
        kwargs["funder"] = funder
    return ClobClient(**kwargs)


# ── Commands ────────────────────────────────────────────────────────────────


def cmd_derive_api_creds(payload: dict) -> None:
    """Derive L2 API credentials from L1 (EIP-712) signature.

    py_clob_client_v2's `create_or_derive_api_key()` handles both paths.
    """
    client = _build_client(payload)
    creds = client.create_or_derive_api_key()
    _emit({
        "ok": True,
        "api_key": creds.api_key,
        "api_secret": creds.api_secret,
        "api_passphrase": creds.api_passphrase,
    })


def cmd_balance(payload: dict) -> None:
    client = _build_client(payload)
    # get_balance_allowance returns {balance, allowances} for the collateral token
    # (pUSD since CLOB V2, a USDC-backed 6-decimal ERC-20).
    from py_clob_client_v2 import AssetType, BalanceAllowanceParams
    params = BalanceAllowanceParams(asset_type=AssetType.COLLATERAL)
    ba = client.get_balance_allowance(params)
    # Balances are in 6-decimal base units.
    available = float(ba.get("balance", "0")) / 1_000_000.0
    _emit({
        "ok": True,
        "available": available,
        "total_value": available,
        "currency": "USDC",
    })


# Data API v1 was retired on 2026-11-02 (every v1 route now answers 410 Gone).
# v2 wraps each payload as {"data": ..., "pagination": {"next_cursor", ...}},
# uses snake_case fields, pages by opaque cursor only, and rejects `offset` /
# `market` with HTTP 400.
_DATA_API = "https://data-api.polymarket.com/v2"
_MAX_PAGES = 50  # 50 x 1000 rows: a loop guard, not an expected size


def _resolve_user(payload: dict) -> str:
    """The wallet that holds the positions.

    Smart wallets (proxy / safe / deposit, signature_type 1-3) hold positions at the
    funder address, not at the signer — querying the signer showed an empty book.
    Only a plain EOA (0) holds them at the key's own address.
    """
    user = payload.get("user") or ""
    if not user and int(payload.get("signature_type", 1)) != 0:
        user = payload.get("funder") or ""
    if not user:
        from eth_account import Account
        user = Account.from_key(payload["private_key"]).address
    return user


def _data_api_get(path: str, params: dict):
    """GET one Data API v2 page. Returns (rows, next_cursor or None)."""
    import requests

    r = requests.get(_DATA_API + path, params=params, timeout=30)
    r.raise_for_status()
    body = r.json() or {}
    # A documented miss is `data: null` or `[]`, never an error.
    rows = body.get("data") or []
    cursor = (body.get("pagination") or {}).get("next_cursor")
    return rows, cursor


def _num(v) -> float:
    # v2 may send null for economics a position does not have.
    return float(v or 0)


def cmd_positions(payload: dict) -> None:
    user = _resolve_user(payload)
    # status defaults to OPEN, which v2 defines as the superset that still holds
    # settled-but-unredeemed winners — what v1 /positions returned. filter_amount
    # replaces v1's sizeThreshold; v2's default floor would hide holdings < 0.1 share.
    params = {"user": user, "limit": 1000, "filter_type": "TOKENS", "filter_amount": 0.0001}
    items = []
    for _ in range(_MAX_PAGES):
        rows, cursor = _data_api_get("/positions", params)
        items.extend(rows)
        if not cursor:
            break
        # Resend the same anchor/filters with the cursor (v2's resume contract).
        params = {**params, "cursor": cursor}
    else:
        sys.stderr.write(f"prediction_polymarket: positions truncated after {_MAX_PAGES} pages\n")
    positions = []
    for p in items:
        positions.append({
            "asset_id": p.get("token_id"),
            "market_id": p.get("condition_id"),
            "outcome": p.get("outcome") or "",
            "size": _num(p.get("current_size")),
            "avg_price": _num(p.get("avg_price")),
            "realized_pnl": _num(p.get("realized_pnl")),
            "unrealized_pnl": _num(p.get("unrealized_pnl")),
            "current_value": _num(p.get("current_value")),
        })
    _emit({"ok": True, "positions": positions})


def cmd_open_orders(payload: dict) -> None:
    client = _build_client(payload)
    orders = client.get_open_orders() or []  # walks every cursor page
    normalized = []
    for o in orders:
        normalized.append({
            "order_id": o.get("id"),
            "asset_id": o.get("asset_id"),
            "market_id": o.get("market"),
            "outcome": o.get("outcome"),
            "side": o.get("side"),
            "order_type": o.get("order_type") or "GTC",
            "price": _num(o.get("price")),
            "size": _num(o.get("original_size")),
            "filled": _num(o.get("size_matched")),
            "status": o.get("status") or "",
            "created_ms": int(o.get("created_at") or 0) * 1000,
            "expires_ms": int(o.get("expiration") or 0) * 1000,
        })
    _emit({"ok": True, "orders": normalized})


def cmd_activity(payload: dict) -> None:
    user = _resolve_user(payload)
    limit = int(payload.get("limit", 100))
    # v2 caps a page at 1000 and rejects anything larger.
    rows, _ = _data_api_get("/activity", {"user": user, "limit": min(max(limit, 1), 1000)})
    _emit({"ok": True, "activities": rows})


# Tick sizes the CLOB order builder supports — py_clob_client_v2's ROUNDING_CONFIG
# keys. 0.005 / 0.0025 are real (World Cup markets moved to 0.0025 on 2026-07-02).
_SUPPORTED_TICKS = ("0.1", "0.01", "0.005", "0.0025", "0.001", "0.0001")


def _is_protocol_v2_asset(asset_id: str):
    """True for a Polymarket Protocol V2 position id, False for a CTF token id,
    None when the Data API cannot say.

    V2 outcomes must be ordered by `position_id` (ExchangeV3, signing domain "3");
    CTF tokens by `token_id`. /v2/tokens reports `module`: `v1_ctf` is a CTF market,
    every other value a Protocol V2 module.
    """
    try:
        rows, _ = _data_api_get("/tokens", {"token_id": asset_id})
    except Exception as exc:
        sys.stderr.write(f"prediction_polymarket: token lookup failed ({exc}); assuming a CTF token\n")
        return None
    for row in rows:
        if str(row.get("token_id")) == asset_id:
            return row.get("module") not in (None, "", "v1_ctf")
    return None


def _normalize_tick(raw) -> str:
    """Canonical tick string ("0.001") from an API value (str or float)."""
    from decimal import Decimal, InvalidOperation

    try:
        d = Decimal(str(raw).strip())
    except (InvalidOperation, ValueError):
        raise ValueError(f"unreadable market tick size: {raw!r}")
    for t in _SUPPORTED_TICKS:
        if d == Decimal(t):
            return t
    raise ValueError(f"unsupported market tick size {raw!r} (expected one of {', '.join(_SUPPORTED_TICKS)})")


def _normalize_price(price, tick: str) -> float:
    """Validate a limit price against the market's tick grid and return it snapped
    EXACTLY onto the grid.

    Only float representation noise is absorbed (0.6549999999 -> 0.655). A price
    that is genuinely off the grid is REJECTED with the two nearest valid prices
    rather than rounded: the order ticket showed the user an exact price, and the
    order builder would otherwise silently round it (with a coarser tick than the
    market's, a 0.655 BUY became 0.66 — paying more than confirmed).
    """
    import math
    from decimal import ROUND_CEILING, ROUND_FLOOR, Decimal

    p = float(price)
    if not math.isfinite(p):
        raise ValueError("price must be a finite number")
    t = Decimal(tick)
    dp = Decimal(str(p))
    ticks = dp / t
    nearest = ticks.to_integral_value()
    if abs(ticks - nearest) > Decimal("1e-6"):
        lo = (ticks.to_integral_value(rounding=ROUND_FLOOR) * t).quantize(t)
        hi = (ticks.to_integral_value(rounding=ROUND_CEILING) * t).quantize(t)
        raise ValueError(f"price {p} is not a multiple of this market's tick size {tick} — use {lo} or {hi}")
    snapped = (nearest * t).quantize(t)
    if snapped < t or snapped > Decimal(1) - t:
        raise ValueError(f"price {snapped} is outside the tradable range {tick} .. {Decimal(1) - t}")
    return float(snapped)


def cmd_place_order(payload: dict) -> None:
    from py_clob_client_v2 import OrderArgs, OrderType, PartialCreateOrderOptions

    token_id = str(payload.get("token_id") or "").strip()
    if not token_id:
        _fail("token_id is required")
        return
    if int(payload.get("signature_type", 1)) != 0 and not payload.get("funder"):
        _fail("funder is required for proxy / safe / deposit wallets — enter your Polymarket "
              "wallet address (shown in the polymarket.com profile menu) in the account settings")
        return

    client = _build_client(payload)
    try:
        size = float(payload["size"])
    except (KeyError, TypeError, ValueError):
        _fail("size must be a number")
        return
    import math

    # The order builder truncates size to 2 decimals; below 0.01 that is a 0-share order.
    if not math.isfinite(size) or size < 0.01:
        _fail(f"size must be at least 0.01 shares (got {payload.get('size')!r})")
        return

    # The MARKET's minimum tick is authoritative (GET /tick-size); the caller's value
    # is only a hint and was hard-coded to "0.01" — wrong for 0.001-tick markets.
    # Ticks also change live (e.g. when a price nears 0 or 1), so always re-read.
    try:
        market_tick = _normalize_tick(client.get_tick_size(token_id))
    except Exception as exc:  # network / unknown token / unsupported tick
        _fail(f"could not read the market's tick size: {exc}")
        return
    hint = payload.get("tick_size")
    if hint not in (None, "") and str(hint) != market_tick:
        sys.stderr.write(f"prediction_polymarket: caller tick {hint!r} ignored; market tick is {market_tick}\n")
    try:
        price = _normalize_price(payload.get("price"), market_tick)
    except (TypeError, ValueError) as exc:
        _fail(str(exc), tick_size=market_tick)
        return

    order_type_map = {
        "GTC": OrderType.GTC,
        "FOK": OrderType.FOK,
        "GTD": OrderType.GTD,
        "FAK": OrderType.FAK,
    }
    order_type = order_type_map.get(payload.get("order_type", "GTC"), OrderType.GTC)

    side = str(payload.get("side", "BUY")).upper()
    if side not in ("BUY", "SELL"):
        _fail(f"side must be BUY or SELL (got {payload.get('side')!r})")
        return
    protocol_v2 = _is_protocol_v2_asset(token_id) is True
    expiration = int(payload.get("expiration") or 0)
    if protocol_v2:
        # position_id selects ExchangeV3 signing; neg_risk does not apply.
        args = OrderArgs(position_id=token_id, price=price, size=size, side=side, expiration=expiration)
        options = PartialCreateOrderOptions(tick_size=market_tick)
    else:
        # neg_risk picks the exchange contract the order is signed for; a wrong value is
        # rejected. Like the tick, the market's own flag is authoritative.
        try:
            neg_risk = bool(client.get_neg_risk(token_id))
        except Exception as exc:
            _fail(f"could not read the market's neg-risk flag: {exc}")
            return
        hint_nr = payload.get("neg_risk")
        if hint_nr is not None and bool(hint_nr) != neg_risk:
            sys.stderr.write(f"prediction_polymarket: caller neg_risk {hint_nr!r} ignored; market says {neg_risk}\n")
        args = OrderArgs(token_id=token_id, price=price, size=size, side=side, expiration=expiration)
        options = PartialCreateOrderOptions(tick_size=market_tick, neg_risk=neg_risk)

    # Sign with create_order(), then publish with post_order() so the caller's
    # FOK/FAK/GTD/GTC time-in-force is honored.
    signed = client.create_order(args, options)
    result = client.post_order(signed, order_type)
    _emit({
        "ok": bool(result.get("success", False)),
        "order_id": result.get("orderID") or result.get("order_id") or "",
        "status": result.get("status", ""),
        "error": result.get("errorMsg", ""),
        "price": price,
        "tick_size": market_tick,
        "raw": result,
    })


def cmd_cancel_order(payload: dict) -> None:
    from py_clob_client_v2 import OrderPayload

    client = _build_client(payload)
    oid = str(payload["order_id"])
    result = client.cancel_order(OrderPayload(orderID=oid))
    _emit({
        "ok": bool(result.get("canceled")),
        "order_id": oid,
        "raw": result,
    })


def cmd_cancel_market(payload: dict) -> None:
    from py_clob_client_v2 import OrderMarketCancelParams

    client = _build_client(payload)
    market = str(payload["market_id"])
    asset_id = str(payload["asset_id"])
    result = client.cancel_market_orders(OrderMarketCancelParams(market=market, asset_id=asset_id))
    _emit({"ok": True, "raw": result})


COMMANDS = {
    "derive_api_creds": cmd_derive_api_creds,
    "balance": cmd_balance,
    "positions": cmd_positions,
    "open_orders": cmd_open_orders,
    "activity": cmd_activity,
    "place_order": cmd_place_order,
    "cancel_order": cmd_cancel_order,
    "cancel_market": cmd_cancel_market,
}


def main() -> int:
    if len(sys.argv) < 2:
        _fail("usage: prediction_polymarket.py <command> [<json_payload>]")
        return 2
    command = sys.argv[1]
    payload_str = sys.argv[2] if len(sys.argv) > 2 else "{}"
    try:
        payload = json.loads(payload_str)
    except json.JSONDecodeError as exc:
        _fail(f"invalid JSON payload: {exc}")
        return 2

    handler = COMMANDS.get(command)
    if handler is None:
        _fail(f"unknown command: {command}", available=list(COMMANDS.keys()))
        return 2

    if not _require_pyclob():
        return 3

    try:
        handler(payload)
        return 0
    except Exception as exc:
        traceback.print_exc(file=sys.stderr)
        _fail(f"{type(exc).__name__}: {exc}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
