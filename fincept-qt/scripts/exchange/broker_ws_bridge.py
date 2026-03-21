"""
broker_ws_bridge.py — Equity broker WebSocket streaming bridge.

Spawned as a subprocess by ExchangeService (same pattern as ws_stream.py).
Imports the openalgo broker streaming adapters directly, captures their
ZMQ output, and re-emits normalized JSON lines to stdout.

Usage:
  python broker_ws_bridge.py <broker> <api_key> <access_token> \
      [user_id] [--symbols SYM:EXCH:TOKEN ...]

Where TOKEN is the broker-specific instrument token (int).
If no TOKEN is known, pass 0 and we fall back to a symbol-only subscribe.

Each output line is a JSON object:
  {"type":"tick",   "symbol":"RELIANCE", "exchange":"NSE", "ltp":2450.5,
   "open":2400.0, "high":2460.0, "low":2390.0, "close":2440.0,
   "volume":1234567, "bid":2450.0, "ask":2451.0, "timestamp":1712345678000}
  {"type":"status", "connected":true,  "broker":"zerodha", "message":"..."}
  {"type":"error",  "broker":"zerodha", "message":"..."}
"""

import json
import os
import sys
import threading
import time
import argparse

OPENALGO_PATH = r"C:\projects\openalgo"


def emit(obj):
    """Write JSON line to stdout, flush immediately."""
    try:
        print(json.dumps(obj, default=str), flush=True)
    except Exception:
        pass


def emit_status(broker, connected, message=""):
    emit({"type": "status", "connected": connected, "broker": broker, "message": message})


def emit_error(broker, message):
    emit({"type": "error", "broker": broker, "message": message})


def emit_tick(symbol, exchange, data: dict):
    """Normalize market data dict → unified tick format."""
    ltp = (data.get("ltp") or data.get("last_price") or
           data.get("last_traded_price") or data.get("LastTradedPrice") or 0)
    # Some brokers (AngelOne) send prices in paise
    if isinstance(ltp, (int, float)) and ltp > 1_000_000:
        ltp /= 100.0

    ohlc = data.get("ohlc") or {}
    emit({
        "type":      "tick",
        "symbol":    symbol,
        "exchange":  exchange,
        "ltp":       float(ltp),
        "open":      float(data.get("open")  or ohlc.get("open")  or 0),
        "high":      float(data.get("high")  or ohlc.get("high")  or 0),
        "low":       float(data.get("low")   or ohlc.get("low")   or 0),
        "close":     float(data.get("close") or ohlc.get("close") or 0),
        "volume":    float(data.get("volume") or data.get("Volume") or 0),
        "bid":       float(data.get("bid")   or data.get("best_bid")  or 0),
        "ask":       float(data.get("ask")   or data.get("best_ask")  or 0),
        "timestamp": int(data.get("timestamp") or data.get("ltt") or
                        int(time.time() * 1000)),
    })


# ─────────────────────────────────────────────────────────────────────────────
# ZMQ subscriber thread — listens to the adapter's PUB socket and re-emits
# ─────────────────────────────────────────────────────────────────────────────

def zmq_listener(zmq_port: int, broker: str, sym_map: dict, stop_event: threading.Event):
    """
    Subscribe to the adapter's ZMQ PUB socket and re-emit ticks as JSON lines.
    sym_map: {topic_prefix: (symbol, exchange)}  e.g. {"NSE_RELIANCE": ("RELIANCE","NSE")}
    """
    try:
        import zmq
    except ImportError:
        emit_error(broker, "pyzmq not installed — cannot receive market data")
        return

    ctx = zmq.Context()
    sock = ctx.socket(zmq.SUB)
    sock.connect(f"tcp://127.0.0.1:{zmq_port}")
    sock.setsockopt(zmq.SUBSCRIBE, b"")  # subscribe to all topics
    sock.setsockopt(zmq.RCVTIMEO, 500)  # 500 ms poll timeout

    emit_status(broker, True, f"ZMQ subscriber connected on port {zmq_port}")

    while not stop_event.is_set():
        try:
            parts = sock.recv_multipart()
            if len(parts) < 2:
                continue
            topic_bytes, data_bytes = parts[0], parts[1]
            topic = topic_bytes.decode("utf-8", errors="ignore")
            data = json.loads(data_bytes.decode("utf-8", errors="ignore"))

            # Resolve symbol/exchange from topic or data payload
            symbol = data.get("symbol") or data.get("trading_symbol") or ""
            exchange = data.get("exchange") or ""

            # Fallback: match topic prefix against sym_map
            if not symbol:
                for prefix, (sym, exch) in sym_map.items():
                    if topic.startswith(prefix):
                        symbol, exchange = sym, exch
                        break

            if symbol:
                emit_tick(symbol, exchange, data)

        except Exception as e:
            err = str(e)
            if "Resource temporarily unavailable" in err or "EAGAIN" in err:
                continue  # timeout — normal, keep looping
            if not stop_event.is_set():
                emit_error(broker, f"ZMQ receive error: {err}")
            break

    sock.close()
    ctx.term()


# ─────────────────────────────────────────────────────────────────────────────
# Token resolution — fetch broker-specific instrument tokens via REST
# ─────────────────────────────────────────────────────────────────────────────

def _resolve_tokens(broker: str, api_key: str, access_token: str, user_id: str,
                    symbols: list) -> dict:
    """
    Resolve (symbol, exchange) → (token, brexchange) via broker REST API.
    Returns dict of {(symbol, exchange): (token, brexchange)}.
    Only implemented for brokers that expose a symbol search/lookup endpoint.
    """
    result = {}
    try:
        if broker == "angelone":
            result = _resolve_tokens_angelone(api_key, access_token, user_id, symbols)
        # Other brokers can be added here
    except Exception as e:
        emit_error(broker, f"Token resolution failed: {e}")
    return result


def _resolve_tokens_angelone(api_key: str, access_token: str, user_id: str,
                              symbols: list) -> dict:
    """
    AngelOne: download the instrument master CSV and look up tokens by symbol+exchange.
    Instrument list URL: https://margincalculator.angelbroking.com/OpenAPI_File/files/OpenAPIScripMaster.json
    This is a public endpoint — no auth needed.
    """
    try:
        import urllib.request
        url = "https://margincalculator.angelbroking.com/OpenAPI_File/files/OpenAPIScripMaster.json"
        emit_status("angelone", False, "Downloading instrument master for token resolution...")
        with urllib.request.urlopen(url, timeout=15) as resp:
            instruments = json.loads(resp.read().decode("utf-8"))
    except Exception as e:
        emit_error("angelone", f"Could not download instrument master: {e}")
        return {}

    # Build lookup: (symbol, extype) → token
    # extype in master: "NSE" = NSE EQ, "BSE" = BSE EQ, "NFO" = F&O, etc.
    lookup = {}
    for inst in instruments:
        sym   = inst.get("symbol") or inst.get("name") or ""
        exch  = inst.get("exch_seg") or inst.get("exchange") or ""
        token = inst.get("token") or inst.get("symboltoken") or ""
        if sym and exch and token:
            lookup[(sym.upper(), exch.upper())] = (str(token), exch.upper())

    result = {}
    for sym, exch in symbols:
        key = (sym.upper(), exch.upper())
        if key in lookup:
            result[(sym, exch)] = lookup[key]
        else:
            emit_error("angelone", f"Token not found in master for {sym}/{exch}")
    return result


# ─────────────────────────────────────────────────────────────────────────────
# Per-broker adapter launchers
# ─────────────────────────────────────────────────────────────────────────────

def run_broker(broker: str, api_key: str, access_token: str, user_id: str,
               symbols: list, stop_event: threading.Event, feed_token: str = ""):
    """
    Import the openalgo adapter for `broker`, initialize it with credentials,
    subscribe to symbols, then start a ZMQ listener that re-emits ticks.

    symbols: list of (symbol, exchange, token_str)
    """
    sys.path.insert(0, OPENALGO_PATH)

    # Build token map for SymbolMapper — use provided tokens where available.
    # For symbols with token "0", try to resolve via broker REST API.
    _BREXCH_MAP = {
        "NSE": "NSE", "BSE": "BSE", "NFO": "NFO",
        "MCX": "MCX", "CDS": "CDS", "BFO": "BFO",
    }
    symbol_token_map = {}
    for sym, exch, token in symbols:
        if token and token != "0":
            brexch = _BREXCH_MAP.get(exch.upper(), exch)
            symbol_token_map[(sym, exch)] = (token, brexch)

    # Resolve missing tokens via broker API where supported
    unresolved = [(sym, exch) for sym, exch, tok in symbols if not tok or tok == "0"]
    if unresolved:
        resolved = _resolve_tokens(broker, api_key, access_token, user_id, unresolved)
        for (sym, exch), (token, brexch) in resolved.items():
            symbol_token_map[(sym, exch)] = (token, brexch)
            emit_status(broker, False, f"Resolved token {sym}/{exch} → {token}")

    # Patch database deps — we pass auth_data directly so DB is never called
    _patch_db_modules(symbol_token_map=symbol_token_map)

    adapter = None
    zmq_thread = None

    try:
        adapter = _create_adapter(broker)
        if adapter is None:
            emit_error(broker, f"No adapter found for broker '{broker}'")
            return

        auth_data = {
            "auth_token":   access_token,
            "access_token": access_token,
            "api_key":      api_key,
            "feed_token":   feed_token if feed_token else access_token,
            "client_id":    user_id,
        }

        # Initialize
        result = adapter.initialize(broker, user_id, auth_data=auth_data)
        emit_status(broker, False, f"Adapter initialized: {result}")

        # Connect
        conn = adapter.connect()
        emit_status(broker, True, f"Connected: {conn}")

        # Build sym_map for ZMQ listener
        sym_map = {}
        for sym, exch, token in symbols:
            sym_map[f"{exch}_{sym}"] = (sym, exch)
            sym_map[f"{exch}_{sym}_LTP"] = (sym, exch)
            sym_map[f"{exch}_{sym}_QUOTE"] = (sym, exch)

        # Start ZMQ listener thread
        zmq_port = getattr(adapter, "zmq_port", None)
        if zmq_port:
            zmq_thread = threading.Thread(
                target=zmq_listener,
                args=(zmq_port, broker, sym_map, stop_event),
                daemon=True
            )
            zmq_thread.start()
        else:
            emit_error(broker, "Adapter has no zmq_port — cannot receive data")

        # Subscribe to all requested symbols
        for sym, exch, token in symbols:
            try:
                result = adapter.subscribe(sym, exch, mode=2)  # mode 2 = Quote
                emit_status(broker, True, f"Subscribed {sym}/{exch}: {result}")
            except Exception as e:
                emit_error(broker, f"Subscribe failed for {sym}/{exch}: {e}")

        # Keep alive — heartbeat every 30s until stop
        while not stop_event.is_set():
            stop_event.wait(30)
            if not stop_event.is_set():
                connected = getattr(adapter, "connected", False)
                emit_status(broker, bool(connected), "heartbeat")

    except Exception as e:
        emit_error(broker, f"Fatal: {e}")
        import traceback
        traceback.print_exc(file=sys.stderr)
    finally:
        if adapter:
            try:
                adapter.disconnect()
            except Exception:
                pass
        emit_status(broker, False, "disconnected")


def _patch_db_modules(symbol_token_map: dict = None):
    """
    Stub out openalgo's database modules so adapters that call
    get_auth_token() / get_feed_token() don't crash when no DB exists.
    We always pass auth_data directly so the DB path is never taken.

    symbol_token_map: optional dict of (symbol, exchange) -> (token, brexchange)
    used to answer get_token / get_brexchange calls from SymbolMapper.
    """
    import types

    # Build token lookup from the provided map
    _token_map = symbol_token_map or {}

    def _get_token(symbol, exchange, *a, **kw):
        return _token_map.get((symbol, exchange), (None, None))[0]

    def _get_brexchange(symbol, exchange, *a, **kw):
        return _token_map.get((symbol, exchange), (None, None))[1]

    def _make_stub(name):
        m = types.ModuleType(name)
        m.get_auth_token  = lambda *a, **kw: None
        m.get_feed_token  = lambda *a, **kw: None
        m.get_token       = _get_token
        m.get_brexchange  = _get_brexchange
        m.get_br_symbol   = lambda *a, **kw: None
        m.get_oa_symbol   = lambda *a, **kw: None
        return m

    for mod in ["database.auth_db", "database.token_db",
                "database.token_db_enhanced", "database.symbol"]:
        # Always re-patch so token map is fresh
        sys.modules[mod] = _make_stub(mod)

    # Stub utils.logging to use stdlib logging
    import logging
    m = types.ModuleType("utils.logging")
    m.get_logger = lambda name=None: logging.getLogger(name or __name__)
    sys.modules["utils.logging"] = m
    if "utils" not in sys.modules:
        sys.modules["utils"] = types.ModuleType("utils")
    sys.modules["utils"].logging = m


def _create_adapter(broker: str):
    """Dynamically import and instantiate the broker's WebSocket adapter."""
    broker_lower = broker.lower()

    # Map our broker names → openalgo module names
    BROKER_MODULE_MAP = {
        "zerodha":   ("broker.zerodha.streaming.zerodha_adapter",   "ZerodhaWebSocketAdapter"),
        "fyers":     ("broker.fyers.streaming.fyers_websocket_adapter", "FyersWebSocketAdapter"),
        "upstox":    ("broker.upstox.streaming.upstox_adapter",     "UpstoxWebSocketAdapter"),
        "dhan":      ("broker.dhan.streaming.dhan_adapter",         "DhanWebSocketAdapter"),
        "kotak":     ("broker.kotak.streaming.kotak_adapter",       "KotakWebSocketAdapter"),
        "groww":     ("broker.groww.streaming.groww_adapter",       "GrowwWebSocketAdapter"),
        "aliceblue": ("broker.aliceblue.streaming.aliceblue_adapter", "AliceBlueWebSocketAdapter"),
        "angelone":  ("broker.angel.streaming.angel_adapter",       "AngelWebSocketAdapter"),
        "shoonya":   ("broker.shoonya.streaming.shoonya_adapter",   "ShoonyaWebSocketAdapter"),
        "iifl":      ("broker.iifl.streaming.iifl_adapter",         "IIFLWebSocketAdapter"),
        "motilal":   ("broker.motilal.streaming.motilal_adapter",   "MotilalWebSocketAdapter"),
        "fivepaisa": ("broker.fivepaisa.streaming.fivepaisa_adapter", "FivePaisaWebSocketAdapter"),
    }

    if broker_lower not in BROKER_MODULE_MAP:
        emit_error(broker, f"Broker '{broker}' not in bridge map")
        return None

    module_path, class_name = BROKER_MODULE_MAP[broker_lower]
    try:
        import importlib
        mod = importlib.import_module(module_path)
        cls = getattr(mod, class_name)
        return cls()
    except (ImportError, AttributeError) as e:
        emit_error(broker, f"Failed to import {module_path}.{class_name}: {e}")
        return None


# ─────────────────────────────────────────────────────────────────────────────
# Entry point
# ─────────────────────────────────────────────────────────────────────────────

def parse_args():
    p = argparse.ArgumentParser()
    p.add_argument("broker",       help="Broker name e.g. zerodha")
    p.add_argument("api_key",      help="API key")
    p.add_argument("access_token", help="Access/auth token")
    p.add_argument("user_id",      nargs="?", default="", help="User/client ID")
    p.add_argument("--feed-token", default="", dest="feed_token",
                   help="Separate feed token (AngelOne feedToken)")
    p.add_argument("--symbols",    nargs="*", default=[],
                   help="Symbols as SYMBOL:EXCHANGE[:TOKEN] e.g. RELIANCE:NSE:738561")
    return p.parse_args()


def main():
    args = parse_args()

    broker       = args.broker.lower()
    api_key      = args.api_key
    access_token = args.access_token
    user_id      = args.user_id or api_key
    feed_token   = args.feed_token or access_token  # fallback to access_token if not provided

    # Parse symbol list: "RELIANCE:NSE:738561" → ("RELIANCE","NSE","738561")
    symbols = []
    for s in (args.symbols or []):
        parts = s.split(":")
        sym   = parts[0] if len(parts) > 0 else s
        exch  = parts[1] if len(parts) > 1 else "NSE"
        token = parts[2] if len(parts) > 2 else "0"
        symbols.append((sym, exch, token))

    if not symbols:
        emit_error(broker, "No symbols provided — nothing to stream")
        sys.exit(1)

    emit_status(broker, False, f"Starting bridge for {len(symbols)} symbol(s)")

    stop_event = threading.Event()

    import signal
    def _shutdown(sig, frame):
        stop_event.set()
    try:
        signal.signal(signal.SIGTERM, _shutdown)
        signal.signal(signal.SIGINT, _shutdown)
    except Exception:
        pass

    run_broker(broker, api_key, access_token, user_id, symbols, stop_event,
               feed_token=feed_token)


if __name__ == "__main__":
    main()
