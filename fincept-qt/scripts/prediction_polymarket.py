#!/usr/bin/env python3
"""Polymarket trading bridge for Fincept Terminal.

Invocation:
    python prediction_polymarket.py <command> <json_payload>

Commands:
    derive_api_creds  — POST /auth/api-key via L1 EIP-712 headers; returns {apiKey, secret, passphrase}
    balance           — get available USDC + total value
    positions         — user's open positions
    open_orders       — user's live orders
    activity          — trade / split / merge / redeem / reward history
    place_order       — sign + submit a limit/market order
    cancel_order      — cancel a single order by id
    cancel_market     — cancel all user orders in a (market, asset_id) pair

Payload shape for `place_order`:
    {
        "private_key":    "0x…",          // Polygon signer
        "funder":          "0x…",          // optional — auto-derived for Proxy
        "signature_type":  0 | 1 | 2,      // 0=EOA 1=POLY_PROXY 2=POLY_GNOSIS_SAFE
        "api_key":         "…",             // L2 creds (from derive_api_creds)
        "api_secret":      "…",
        "api_passphrase":  "…",
        "token_id":        "asset id",
        "price":           0.65,
        "size":            100,
        "side":            "BUY" | "SELL",
        "order_type":      "GTC" | "FOK" | "GTD" | "FAK",
        "tick_size":       "0.01",
        "neg_risk":        false,
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
        from py_clob_client.client import ClobClient  # noqa: F401
        return True
    except Exception as exc:
        _fail(
            "py_clob_client is not installed. Run: pip install py-clob-client eth-account",
            detail=str(exc),
        )
        return False


def _build_client(payload: dict):
    from py_clob_client.client import ClobClient
    from py_clob_client.clob_types import ApiCreds

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

    # Signature type 0 (EOA) does not need a funder; 1/2 do.
    kwargs = {
        "host": host,
        "chain_id": chain_id,
        "key": private_key,
    }
    if creds is not None:
        kwargs["creds"] = creds
    if sig_type != 0:
        kwargs["signature_type"] = sig_type
        if funder:
            kwargs["funder"] = funder
    return ClobClient(**kwargs)


# ── Commands ────────────────────────────────────────────────────────────────


def cmd_derive_api_creds(payload: dict) -> None:
    """Derive L2 API credentials from L1 (EIP-712) signature.

    py_clob_client's `create_or_derive_api_creds()` handles both paths.
    """
    client = _build_client(payload)
    creds = client.create_or_derive_api_creds()
    _emit({
        "ok": True,
        "api_key": creds.api_key,
        "api_secret": creds.api_secret,
        "api_passphrase": creds.api_passphrase,
    })


def cmd_balance(payload: dict) -> None:
    client = _build_client(payload)
    # get_balance_allowance returns {balance, allowance} for USDC collateral.
    from py_clob_client.clob_types import BalanceAllowanceParams, AssetType
    params = BalanceAllowanceParams(asset_type=AssetType.COLLATERAL)
    ba = client.get_balance_allowance(params)
    # Polymarket balances are in 6-decimal USDC units (microUSDC).
    available = float(ba.get("balance", "0")) / 1_000_000.0
    _emit({
        "ok": True,
        "available": available,
        "total_value": available,
        "currency": "USDC",
    })


def cmd_positions(payload: dict) -> None:
    import requests

    user = payload.get("user") or ""
    if not user:
        # If user address is not given, derive from private_key via eth_account.
        from eth_account import Account
        user = Account.from_key(payload["private_key"]).address
    r = requests.get(
        "https://data-api.polymarket.com/positions",
        params={"user": user, "sizeThreshold": 0.0001, "limit": 500},
        timeout=30,
    )
    r.raise_for_status()
    items = r.json() or []
    positions = []
    for p in items:
        positions.append({
            "asset_id": p.get("asset"),
            "market_id": p.get("conditionId"),
            "outcome": p.get("outcome", ""),
            "size": float(p.get("size", 0)),
            "avg_price": float(p.get("avgPrice", 0)),
            "realized_pnl": float(p.get("realizedPnl", 0)),
            "unrealized_pnl": float(p.get("cashPnl", 0)),
            "current_value": float(p.get("currentValue", 0)),
        })
    _emit({"ok": True, "positions": positions})


def cmd_open_orders(payload: dict) -> None:
    client = _build_client(payload)
    orders = client.get_orders() or []
    normalized = []
    for o in orders:
        normalized.append({
            "order_id": o.get("id"),
            "asset_id": o.get("asset_id"),
            "market_id": o.get("market"),
            "outcome": o.get("outcome"),
            "side": o.get("side"),
            "order_type": o.get("order_type", "GTC"),
            "price": float(o.get("price", 0)),
            "size": float(o.get("original_size", 0)) / 1_000_000.0,
            "filled": float(o.get("size_matched", 0)) / 1_000_000.0,
            "status": o.get("status", ""),
            "created_ms": int(o.get("created_at", 0)) * 1000,
            "expires_ms": int(o.get("expiration") or 0) * 1000,
        })
    _emit({"ok": True, "orders": normalized})


def cmd_activity(payload: dict) -> None:
    import requests
    from eth_account import Account

    user = payload.get("user") or Account.from_key(payload["private_key"]).address
    limit = int(payload.get("limit", 100))
    r = requests.get(
        "https://data-api.polymarket.com/activity",
        params={"user": user, "limit": min(max(limit, 1), 500)},
        timeout=30,
    )
    r.raise_for_status()
    _emit({"ok": True, "activities": r.json() or []})


def cmd_place_order(payload: dict) -> None:
    from py_clob_client.clob_types import OrderArgs, OrderType

    client = _build_client(payload)

    order_type_map = {
        "GTC": OrderType.GTC,
        "FOK": OrderType.FOK,
        "GTD": OrderType.GTD,
        "FAK": OrderType.FAK,
    }
    order_type = order_type_map.get(payload.get("order_type", "GTC"), OrderType.GTC)

    args = OrderArgs(
        token_id=str(payload["token_id"]),
        price=float(payload["price"]),
        size=float(payload["size"]),
        side=str(payload.get("side", "BUY")).upper(),
    )
    if payload.get("expiration"):
        args.expiration = int(payload["expiration"])

    options = {
        "tick_size": str(payload.get("tick_size", "0.01")),
        "neg_risk": bool(payload.get("neg_risk", False)),
    }

    result = client.create_and_post_order(args, options, order_type=order_type)
    _emit({
        "ok": bool(result.get("success", False)),
        "order_id": result.get("orderID") or result.get("order_id") or "",
        "status": result.get("status", ""),
        "error": result.get("errorMsg", ""),
        "raw": result,
    })


def cmd_cancel_order(payload: dict) -> None:
    client = _build_client(payload)
    oid = str(payload["order_id"])
    result = client.cancel(order_id=oid)
    _emit({
        "ok": bool(result.get("canceled")),
        "order_id": oid,
        "raw": result,
    })


def cmd_cancel_market(payload: dict) -> None:
    client = _build_client(payload)
    market = str(payload["market_id"])
    asset_id = str(payload["asset_id"])
    result = client.cancel_market_orders(market=market, asset_id=asset_id)
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
