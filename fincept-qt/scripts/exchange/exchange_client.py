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

# Markets cache. A market list changes rarely, so a cached copy younger than
# a day is used as-is, and an older one is still used when the exchange cannot
# be reached — a stale symbol list beats a dead trading screen.
_MARKETS_CACHE_TTL = 24 * 3600
_MARKETS_CACHE_VERSION = 2


def _markets_cache_dir() -> str:
    """User-writable cache directory.

    The scripts tree is read-only in installed builds (Program Files, /opt,
    an AppImage), so the cache lives under the app's data directory.
    """
    base = os.environ.get("FINCEPT_DATA_DIR")
    if not base:
        local = os.environ.get("LOCALAPPDATA")
        base = (os.path.join(local, "com.fincept.terminal") if local
                else os.path.join(os.path.expanduser("~"), ".cache", "fincept-terminal"))
    candidates = [os.path.join(base, "cache", "exchange_markets"),
                  os.path.join(os.path.dirname(os.path.abspath(__file__)), ".cache")]
    for d in candidates:
        try:
            os.makedirs(d, exist_ok=True)
            return d
        except OSError:
            continue
    return candidates[-1]


def get_markets_cache_path(exchange_id: str) -> str:
    """Return path to the per-exchange markets cache file."""
    return os.path.join(_markets_cache_dir(), f"markets_{exchange_id}.json")


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
    except Exception:
        pass
    return None, None


def load_cached_markets(exchange_id: str, allow_stale: bool = False) -> dict | None:
    """Return cached markets dict if still fresh (within TTL), else None."""
    return load_cached_market_data(exchange_id, allow_stale)[0]


def save_markets_cache(exchange_id: str, markets: dict, currencies: dict | None = None) -> None:
    """Persist markets (and currencies) so the next start can skip load_markets()."""
    if not markets:
        return
    path = get_markets_cache_path(exchange_id)
    tmp = path + ".tmp"
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
    except Exception:
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
