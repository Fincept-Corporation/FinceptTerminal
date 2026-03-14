"""
Unified exchange client — shared initialization for all exchange scripts.
Handles exchange instantiation, credential loading, and error normalization.

Called by C++ python_runner as subprocess — outputs JSON to stdout.
"""

import sys
import json
import ccxt


def make_exchange(exchange_id: str, credentials: dict = None, sandbox: bool = False) -> ccxt.Exchange:
    """Create and configure a ccxt exchange instance."""
    if exchange_id not in ccxt.exchanges:
        raise ValueError(f"Unknown exchange: {exchange_id}. Available: {len(ccxt.exchanges)}")

    config = {
        "enableRateLimit": True,
        "timeout": 30000,
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
    """Print JSON result to stdout for C++ python_runner to extract."""
    print(json.dumps({"success": True, "data": data}, default=str))


def output_error(message: str, code: str = "EXCHANGE_ERROR"):
    """Print JSON error to stdout."""
    print(json.dumps({"success": False, "error": message, "code": code}, default=str))
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
