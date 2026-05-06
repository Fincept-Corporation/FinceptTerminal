#!/usr/bin/env python3
"""
option_greeks_daemon.py — persistent worker for batched IV + Greeks.

Frame protocol matches scripts/yfinance_data.py: 4-byte big-endian length
prefix followed by a UTF-8 JSON body. Reads requests on stdin, writes
responses on stdout. Sends a {"ready": true, "pid": <pid>} handshake once
imports complete (py_vollib pulls in scipy, which is the slow part).

Scope: implied vol + Greeks for European options via py_vollib. Black-Scholes-
Merton is the default model — works uniformly for index (q=0) and stock
options. Pass model="black" for futures/forwards (USDINR).

Sole action: option_greeks_batch.

Request payload:
    {
        "contracts": [
            {
                "token": <int>,           # echoed back; opaque to the daemon
                "S": <float>,             # underlying spot
                "K": <float>,             # strike
                "t": <float>,             # time to expiry, years (actual/365)
                "r": <float>,             # risk-free rate, decimal (0.067 = 6.7%)
                "q": <float>,             # dividend yield, decimal (0 for indices)
                "flag": "c" | "p",
                "market_price": <float>,  # mid or LTP for IV solve
                "model": "bsm" | "black"  # optional, default "bsm"
            },
            ...
        ]
    }

Response result (one entry per input contract, same order):
    {
        "results": [
            {
                "token": <int>,
                "iv": <float>,            # decimal (0.142 = 14.2%); 0 if invalid
                "delta": <float>,
                "gamma": <float>,
                "theta": <float>,         # PER CALENDAR DAY (py_vollib convention)
                "vega": <float>,          # PER 1.00 σ (py_vollib value × 100)
                "rho": <float>,           # PER 1.00 r (py_vollib value × 100)
                "valid": <bool>,
                "error": <string>         # only when valid=false
            },
            ...
        ]
    }

Note on Greek scaling — py_vollib's analytical greeks return:
  - vega   per 1% absolute vol change
  - rho    per 1% absolute rate change
  - theta  per CALENDAR DAY (already day-scaled, NOT per year)

The C++ OptionGreeks struct documents vega/rho as "per 1.00 σ" / "per 1.00 r",
so the daemon multiplies vega and rho by 100 before returning. Theta is
already per-day and is returned unscaled.
"""

import json
import os
import sys
import traceback

# Lazy imports — kept inside main so the bad-pythonpath stderr message lands
# before the slow scipy import on a misconfigured machine.
def _do_imports():
    global black_scholes_merton, bsm_iv, bsm_delta, bsm_gamma, bsm_vega, bsm_theta, bsm_rho
    global black, b_iv, b_delta, b_gamma, b_vega, b_theta, b_rho
    from py_vollib.black_scholes_merton import black_scholes_merton  # noqa: F401
    from py_vollib.black_scholes_merton.greeks.analytical import (
        delta as bsm_delta,
        gamma as bsm_gamma,
        vega as bsm_vega,
        theta as bsm_theta,
        rho as bsm_rho,
    )
    from py_vollib.black_scholes_merton.implied_volatility import implied_volatility as bsm_iv

    from py_vollib.black import black  # noqa: F401
    from py_vollib.black.greeks.analytical import (
        delta as b_delta,
        gamma as b_gamma,
        vega as b_vega,
        theta as b_theta,
        rho as b_rho,
    )
    from py_vollib.black.implied_volatility import implied_volatility as b_iv

    # Stash into module globals so _compute_one can reach them without
    # threading them through every call.
    g = globals()
    g["bsm_iv"] = bsm_iv
    g["bsm_delta"] = bsm_delta
    g["bsm_gamma"] = bsm_gamma
    g["bsm_vega"] = bsm_vega
    g["bsm_theta"] = bsm_theta
    g["bsm_rho"] = bsm_rho
    g["b_iv"] = b_iv
    g["b_delta"] = b_delta
    g["b_gamma"] = b_gamma
    g["b_vega"] = b_vega
    g["b_theta"] = b_theta
    g["b_rho"] = b_rho


# ─── Framing helpers (mirror scripts/yfinance_data.py) ──────────────────────

def _read_frame(stream):
    header = b""
    while len(header) < 4:
        chunk = stream.read(4 - len(header))
        if not chunk:
            return None
        header += chunk
    n = int.from_bytes(header, byteorder="big", signed=False)
    if n == 0:
        return b""
    if n > 64 * 1024 * 1024:
        return None
    buf = b""
    while len(buf) < n:
        chunk = stream.read(n - len(buf))
        if not chunk:
            return None
        buf += chunk
    return buf


def _write_frame(stream, data_bytes):
    n = len(data_bytes)
    stream.write(n.to_bytes(4, byteorder="big", signed=False))
    stream.write(data_bytes)
    stream.flush()


# ─── Per-contract IV + Greeks ───────────────────────────────────────────────

_MIN_T = 1.0 / 365.0  # one calendar day; clamp expiry-day options


def _compute_one(c):
    """Solve IV from market_price, then return Greeks at that IV.

    Returns a result dict matching the response schema. Never raises —
    failures land in valid=false with a short error string.
    """
    token = c.get("token", 0)
    try:
        S = float(c["S"])
        K = float(c["K"])
        t = max(float(c["t"]), _MIN_T)
        r = float(c["r"])
        q = float(c.get("q", 0.0))
        flag = str(c["flag"]).lower()
        if flag not in ("c", "p"):
            raise ValueError(f"bad flag '{flag}'")
        price = float(c["market_price"])
        if price <= 0 or S <= 0 or K <= 0:
            raise ValueError("non-positive price/S/K")
        model = str(c.get("model", "bsm")).lower()

        if model == "black":
            iv = float(b_iv(price, S, K, t, r, flag))
            d = float(b_delta(flag, S, K, t, r, iv))
            g = float(b_gamma(flag, S, K, t, r, iv))
            v = float(b_vega(flag, S, K, t, r, iv))
            th = float(b_theta(flag, S, K, t, r, iv))
            rh = float(b_rho(flag, S, K, t, r, iv))
        else:
            iv = float(bsm_iv(price, S, K, t, r, q, flag))
            d = float(bsm_delta(flag, S, K, t, r, iv, q))
            g = float(bsm_gamma(flag, S, K, t, r, iv, q))
            v = float(bsm_vega(flag, S, K, t, r, iv, q))
            th = float(bsm_theta(flag, S, K, t, r, iv, q))
            rh = float(bsm_rho(flag, S, K, t, r, iv, q))

        # Sanity check — IV solver returns NaN for unsolvable inputs.
        if iv != iv or iv < 0 or iv > 5.0:
            raise ValueError(f"iv out of range: {iv}")

        return {
            "token": token,
            "iv": iv,
            "delta": d,
            "gamma": g,
            "theta": th,           # already per-day from py_vollib
            "vega": v * 100.0,     # py_vollib per-1% → struct per-1.00 σ
            "rho": rh * 100.0,     # py_vollib per-1% → struct per-1.00 r
            "valid": True,
        }
    except Exception as e:
        return {
            "token": token,
            "iv": 0.0,
            "delta": 0.0,
            "gamma": 0.0,
            "theta": 0.0,
            "vega": 0.0,
            "rho": 0.0,
            "valid": False,
            "error": str(e),
        }


# ─── Daemon dispatch ────────────────────────────────────────────────────────

def _dispatch(action, payload):
    if action == "option_greeks_batch":
        contracts = (payload or {}).get("contracts") or []
        return {"results": [_compute_one(c) for c in contracts]}
    return {"error": f"Unknown action: {action}"}


def run_daemon():
    try:
        _do_imports()
    except Exception as e:
        msg = json.dumps({
            "ready": False,
            "error": f"import failed: {e}",
            "traceback": traceback.format_exc(),
        }).encode("utf-8")
        try:
            _write_frame(sys.stdout.buffer, msg)
        except Exception:
            pass
        return

    stdin = sys.stdin.buffer
    stdout = sys.stdout.buffer

    ready = json.dumps({"ready": True, "pid": os.getpid()}).encode("utf-8")
    _write_frame(stdout, ready)

    while True:
        frame = _read_frame(stdin)
        if frame is None:
            break
        try:
            req = json.loads(frame.decode("utf-8"))
        except Exception as e:
            err = {"id": 0, "ok": False, "error": f"bad request JSON: {e}"}
            _write_frame(stdout, json.dumps(err).encode("utf-8"))
            continue

        req_id = req.get("id", 0)
        action = req.get("action", "")
        if action == "shutdown":
            resp = {"id": req_id, "ok": True, "result": {"shutdown": True}}
            _write_frame(stdout, json.dumps(resp).encode("utf-8"))
            break

        try:
            result = _dispatch(action, req.get("payload"))
            resp = {"id": req_id, "ok": True, "result": result}
        except Exception as e:
            resp = {"id": req_id, "ok": False, "error": str(e)}
        try:
            _write_frame(stdout, json.dumps(resp).encode("utf-8"))
        except Exception:
            break


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--daemon":
        run_daemon()
    else:
        # Smoke test — run a tiny batch and print the result.
        _do_imports()
        sample = {
            "contracts": [
                {"token": 1, "S": 24000, "K": 24000, "t": 30 / 365.0,
                 "r": 0.067, "q": 0.0, "flag": "c", "market_price": 250.0},
                {"token": 2, "S": 24000, "K": 24000, "t": 30 / 365.0,
                 "r": 0.067, "q": 0.0, "flag": "p", "market_price": 240.0},
            ]
        }
        print(json.dumps(_dispatch("option_greeks_batch", sample), indent=2))
