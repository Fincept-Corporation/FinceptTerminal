#!/usr/bin/env python3
"""Kalshi trading bridge for Fincept Terminal.

Invocation:
    python prediction_kalshi.py <command> <json_payload>

Commands:
    balance           — GET /portfolio/balance
    positions         — GET /portfolio/positions
    open_orders       — GET /portfolio/orders?status=resting
    fills             — GET /portfolio/fills
    place_order       — POST /portfolio/orders
    cancel_order      — DELETE /portfolio/orders/{order_id}
    decrease_order    — POST /portfolio/orders/{order_id}/decrease
    settlements       — GET /portfolio/settlements

Payload shape for every command:
    {
        "api_key_id":      "UUID",
        "private_key_pem": "-----BEGIN …-----\n…\n-----END …-----",
        "use_demo":        false,
        …command-specific fields…
    }

place_order fields:
        "ticker":          "KXFOO-25DEC-T3.00",
        "action":          "buy" | "sell",
        "side":            "yes" | "no",
        "count":           10,                  // number of contracts
        "order_type":      "limit" | "market",
        "yes_price_cents": 52,                   // integer cents 1-99 (limit only)
        "no_price_cents":  48,                   // alternative — limit sell NO side
        "expiration_ts":   1735689600,            // 0/omitted = GTC
        "client_order_id": "uuid-…"

All responses are a single JSON line on stdout.
"""
from __future__ import annotations

import base64
import datetime
import json
import sys
import traceback
import uuid


def _emit(obj: dict) -> None:
    sys.stdout.write(json.dumps(obj, separators=(",", ":")))
    sys.stdout.write("\n")
    sys.stdout.flush()


def _fail(msg: str, **extra) -> None:
    _emit({"ok": False, "error": msg, **extra})


def _require_crypto():
    try:
        from cryptography.hazmat.primitives import hashes  # noqa: F401
        import requests  # noqa: F401
        return True
    except Exception as exc:
        _fail("cryptography / requests is not installed", detail=str(exc))
        return False


_API_PREFIX = "/trade-api/v2"


def _base_url(use_demo: bool) -> str:
    return "https://demo-api.kalshi.co" + _API_PREFIX if use_demo else \
           "https://api.elections.kalshi.com" + _API_PREFIX


def _load_private_key(pem: str):
    from cryptography.hazmat.primitives.serialization import load_pem_private_key
    return load_pem_private_key(pem.encode("utf-8"), password=None)


def _sign_text(priv, text: str) -> str:
    from cryptography.hazmat.primitives import hashes
    from cryptography.hazmat.primitives.asymmetric import padding

    sig = priv.sign(
        text.encode("utf-8"),
        padding.PSS(
            mgf=padding.MGF1(hashes.SHA256()),
            salt_length=padding.PSS.DIGEST_LENGTH,
        ),
        hashes.SHA256(),
    )
    return base64.b64encode(sig).decode("utf-8")


def _auth_headers(payload: dict, method: str, signing_path: str) -> dict:
    api_key_id = payload["api_key_id"]
    pem = payload["private_key_pem"]
    priv = _load_private_key(pem)

    ts = str(int(datetime.datetime.now().timestamp() * 1000))
    # Kalshi signs `timestamp_ms + METHOD + path` where path is the full path
    # under the host *including* the /trade-api/v2 prefix and *excluding* the
    # query string. Confirmed against docs.kalshi.com quickstart (Apr 2026).
    signature = _sign_text(priv, ts + method.upper() + signing_path)
    return {
        "KALSHI-ACCESS-KEY": api_key_id,
        "KALSHI-ACCESS-SIGNATURE": signature,
        "KALSHI-ACCESS-TIMESTAMP": ts,
        "Content-Type": "application/json",
    }


def _request(payload: dict, method: str, path: str, *, params=None, body=None):
    import requests
    base = _base_url(bool(payload.get("use_demo")))
    url = base + path
    signing_path = _API_PREFIX + path  # full path under host, no query string
    headers = _auth_headers(payload, method, signing_path)
    resp = requests.request(method, url, headers=headers, params=params, json=body, timeout=30)
    ok = resp.status_code in (200, 201)
    try:
        data = resp.json()
    except Exception:
        data = {"text": resp.text}
    return ok, resp.status_code, data


# ── Commands ────────────────────────────────────────────────────────────────


def cmd_balance(payload: dict) -> None:
    ok, code, data = _request(payload, "GET", "/portfolio/balance")
    if not ok:
        _fail(f"HTTP {code}", detail=data)
        return
    cents = int(data.get("balance", 0))
    _emit({
        "ok": True,
        "available": cents / 100.0,
        "total_value": cents / 100.0,
        "currency": "USD",
    })


def _to_float(v, default=0.0):
    try:
        return float(v) if v is not None else default
    except (TypeError, ValueError):
        return default


def cmd_positions(payload: dict) -> None:
    ok, code, data = _request(payload, "GET", "/portfolio/positions",
                              params={"limit": 500})
    if not ok:
        _fail(f"HTTP {code}", detail=data)
        return
    positions = []
    # As of Mar 12, 2026 Kalshi removed integer cent fields from responses.
    # Use position_fp (signed contract count, 2dp) and *_dollars (decimal $).
    for p in data.get("market_positions", []) or []:
        position_fp = _to_float(p.get("position_fp"))
        market_exposure = _to_float(p.get("market_exposure_dollars"))
        size = abs(position_fp)
        avg_price = market_exposure / size if size > 0 else 0.0
        positions.append({
            "asset_id": (p.get("ticker") or "") + (":yes" if position_fp >= 0 else ":no"),
            "market_id": p.get("ticker", ""),
            "outcome": "YES" if position_fp >= 0 else "NO",
            "size": size,
            "avg_price": avg_price,
            "realized_pnl": _to_float(p.get("realized_pnl_dollars")),
            "unrealized_pnl": _to_float(p.get("total_traded_dollars")),
            "current_value": market_exposure,
        })
    _emit({"ok": True, "positions": positions})


def cmd_open_orders(payload: dict) -> None:
    ok, code, data = _request(payload, "GET", "/portfolio/orders",
                              params={"status": "resting", "limit": 500})
    if not ok:
        _fail(f"HTTP {code}", detail=data)
        return
    orders = []
    # Mar 12, 2026: bare `count` / `yes_price` / `no_price` removed from order
    # responses. Use `*_fp` for counts and `*_dollars` for prices.
    for o in data.get("orders", []) or []:
        side = (o.get("side") or "yes").lower()
        price_field = "yes_price_dollars" if side == "yes" else "no_price_dollars"
        initial = _to_float(o.get("initial_count_fp"))
        remaining = _to_float(o.get("remaining_count_fp"))
        # ISO 8601 created_time → epoch ms
        created_ms = 0
        ct = o.get("created_time") or ""
        if ct:
            try:
                created_ms = int(datetime.datetime.fromisoformat(
                    ct.replace("Z", "+00:00")).timestamp() * 1000)
            except ValueError:
                created_ms = 0
        expires_ms = 0
        et = o.get("expiration_time") or ""
        if et:
            try:
                expires_ms = int(datetime.datetime.fromisoformat(
                    et.replace("Z", "+00:00")).timestamp() * 1000)
            except ValueError:
                expires_ms = 0
        orders.append({
            "order_id": o.get("order_id", ""),
            "asset_id": (o.get("ticker") or "") + ":" + side,
            "market_id": o.get("ticker", ""),
            "outcome": side.upper(),
            "side": (o.get("action") or "buy").upper(),
            "order_type": (o.get("type") or "limit").upper(),
            "price": _to_float(o.get(price_field)),
            "size": remaining,
            "filled": max(0.0, initial - remaining),
            "status": (o.get("status") or "").upper(),
            "created_ms": created_ms,
            "expires_ms": expires_ms,
            "client_order_id": o.get("client_order_id", ""),
        })
    _emit({"ok": True, "orders": orders})


def cmd_fills(payload: dict) -> None:
    ok, code, data = _request(payload, "GET", "/portfolio/fills",
                              params={"limit": int(payload.get("limit", 100))})
    if not ok:
        _fail(f"HTTP {code}", detail=data)
        return
    _emit({"ok": True, "fills": data.get("fills", []) or []})


def cmd_settlements(payload: dict) -> None:
    ok, code, data = _request(payload, "GET", "/portfolio/settlements",
                              params={"limit": int(payload.get("limit", 100))})
    if not ok:
        _fail(f"HTTP {code}", detail=data)
        return
    _emit({"ok": True, "settlements": data.get("settlements", []) or []})


def cmd_place_order(payload: dict) -> None:
    body = {
        "ticker": payload["ticker"],
        "action": str(payload.get("action", "buy")).lower(),
        "side": str(payload.get("side", "yes")).lower(),
        "count": int(payload["count"]),
        "type": str(payload.get("order_type", "limit")).lower(),
        "client_order_id": payload.get("client_order_id") or str(uuid.uuid4()),
    }
    if body["type"] == "limit":
        if "yes_price_cents" in payload:
            body["yes_price"] = int(payload["yes_price_cents"])
        elif "no_price_cents" in payload:
            body["no_price"] = int(payload["no_price_cents"])
        else:
            _fail("limit order requires yes_price_cents or no_price_cents")
            return
    if payload.get("expiration_ts"):
        body["expiration_ts"] = int(payload["expiration_ts"])

    ok, code, data = _request(payload, "POST", "/portfolio/orders", body=body)
    if not ok:
        _fail(f"HTTP {code}", detail=data)
        return
    order = data.get("order", {}) or {}
    _emit({
        "ok": True,
        "order_id": order.get("order_id", ""),
        "status": (order.get("status") or "").upper(),
        "client_order_id": body["client_order_id"],
        "raw": data,
    })


def cmd_cancel_order(payload: dict) -> None:
    order_id = str(payload["order_id"])
    ok, code, data = _request(payload, "DELETE", f"/portfolio/orders/{order_id}")
    if not ok:
        _fail(f"HTTP {code}", detail=data)
        return
    _emit({"ok": True, "order_id": order_id, "raw": data})


def cmd_decrease_order(payload: dict) -> None:
    order_id = str(payload["order_id"])
    body = {"reduce_by": int(payload.get("reduce_by", 0))}
    ok, code, data = _request(payload, "POST",
                              f"/portfolio/orders/{order_id}/decrease", body=body)
    if not ok:
        _fail(f"HTTP {code}", detail=data)
        return
    _emit({"ok": True, "raw": data})


COMMANDS = {
    "balance": cmd_balance,
    "positions": cmd_positions,
    "open_orders": cmd_open_orders,
    "fills": cmd_fills,
    "settlements": cmd_settlements,
    "place_order": cmd_place_order,
    "cancel_order": cmd_cancel_order,
    "decrease_order": cmd_decrease_order,
}


def main() -> int:
    if len(sys.argv) < 2:
        _fail("usage: prediction_kalshi.py <command> [<json_payload>]")
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

    if not _require_crypto():
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
