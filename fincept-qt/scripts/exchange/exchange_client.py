"""
Unified exchange client — shared initialization for all exchange scripts.
Handles exchange instantiation, credential loading, and error normalization.

Called by C++ python_runner as subprocess — outputs JSON to stdout.
"""

import os
import sys
import json
import time
import ccxt

# Markets cache TTL — 30 minutes is a safe balance between freshness and speed.
# Exchange market lists rarely change within a session.
_MARKETS_CACHE_TTL = 1800


def get_markets_cache_path(exchange_id: str) -> str:
    """Return path to the per-exchange markets cache file."""
    cache_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), ".cache")
    os.makedirs(cache_dir, exist_ok=True)
    return os.path.join(cache_dir, f"markets_{exchange_id}.json")


def load_cached_markets(exchange_id: str) -> dict | None:
    """Return cached markets dict if still fresh (within TTL), else None."""
    path = get_markets_cache_path(exchange_id)
    try:
        if os.path.exists(path):
            age = time.time() - os.path.getmtime(path)
            if age < _MARKETS_CACHE_TTL:
                with open(path, "r", encoding="utf-8") as f:
                    return json.load(f)
    except Exception:
        pass
    return None


def save_markets_cache(exchange_id: str, markets: dict) -> None:
    """Persist markets dict to disk so the next WS startup can skip load_markets()."""
    path = get_markets_cache_path(exchange_id)
    try:
        with open(path, "w", encoding="utf-8") as f:
            json.dump(markets, f)
    except Exception:
        pass


def get_default_type(exchange_id: str) -> str:
    """Return the appropriate defaultType for an exchange.
    Hyperliquid is a perps DEX — its primary market type is swap.
    All others default to spot.
    """
    return "swap" if exchange_id in ("hyperliquid",) else "spot"


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
    try:
        data = sys.stdin.read()
        if data.strip():
            return json.loads(data)
    except (json.JSONDecodeError, EOFError):
        pass
    return {}


def run_with_error_handling(func):
    """Decorator to catch ccxt exceptions and output normalized errors."""
    def wrapper(*args, **kwargs):
        try:
            return func(*args, **kwargs)
        except ccxt.AuthenticationError as e:
            output_error(f"Authentication failed: {e}", "AUTH_ERROR")
        except ccxt.InsufficientFunds as e:
            output_error(f"Insufficient funds: {e}", "INSUFFICIENT_FUNDS")
        except ccxt.InvalidOrder as e:
            output_error(f"Invalid order: {e}", "INVALID_ORDER")
        except ccxt.OrderNotFound as e:
            output_error(f"Order not found: {e}", "ORDER_NOT_FOUND")
        except ccxt.RateLimitExceeded as e:
            output_error(f"Rate limit exceeded: {e}", "RATE_LIMIT")
        except ccxt.NetworkError as e:
            output_error(f"Network error: {e}", "NETWORK_ERROR")
        except ccxt.ExchangeNotAvailable as e:
            output_error(f"Exchange unavailable: {e}", "EXCHANGE_UNAVAILABLE")
        except ccxt.ExchangeError as e:
            output_error(f"Exchange error: {e}", "EXCHANGE_ERROR")
        except Exception as e:
            output_error(f"Unexpected error: {e}", "UNKNOWN_ERROR")
    return wrapper
