"""
DexScreener Data Fetcher
DEX pair prices, volume, liquidity across 80+ chains.
No API key required.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

BASE_URL = "https://api.dexscreener.com"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)


def _make_request(endpoint: str, params: Dict = None) -> Any:
    """Make HTTP request with error handling."""
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    try:
        response = session.get(url, params=params, timeout=30)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {str(e)}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}


def get_pairs_by_chain(chain_id: str) -> Any:
    """Get top pairs on a specific chain (e.g. ethereum, bsc, solana, arbitrum)."""
    data = _make_request(f"latest/dex/pairs/{chain_id}")
    if isinstance(data, dict) and "pairs" in data:
        pairs = data["pairs"] or []
        return {"chain": chain_id, "pairs": pairs[:100], "count": len(pairs)}
    return data


def get_pair(chain: str, address: str) -> Any:
    """Get detailed data for a specific pair by chain and pair address."""
    data = _make_request(f"latest/dex/pairs/{chain}/{address}")
    if isinstance(data, dict) and "pairs" in data:
        pairs = data["pairs"] or []
        return {"chain": chain, "address": address, "pair": pairs[0] if pairs else None}
    return data


def search_pairs(query: str) -> Any:
    """Search for DEX pairs by token name or symbol across all chains."""
    params = {"q": query}
    data = _make_request("latest/dex/search", params=params)
    if isinstance(data, dict) and "pairs" in data:
        pairs = data["pairs"] or []
        return {"query": query, "pairs": pairs[:50], "count": len(pairs)}
    return data


def get_token_pairs(chain: str, token_address: str) -> Any:
    """Get all pairs for a specific token address on a chain."""
    data = _make_request(f"latest/dex/tokens/{token_address}")
    if isinstance(data, dict) and "pairs" in data:
        pairs = data["pairs"] or []
        # Filter by chain if specified
        if chain and chain.lower() != "all":
            pairs = [p for p in pairs if p.get("chainId", "").lower() == chain.lower()]
        return {"chain": chain, "token_address": token_address, "pairs": pairs[:50], "count": len(pairs)}
    return data


def get_latest_token_profiles() -> Any:
    """Get latest boosted token profiles with metadata."""
    data = _make_request("token-profiles/latest/v1")
    if isinstance(data, list):
        return {"profiles": data[:50], "count": len(data)}
    return data


def get_boosted_tokens() -> Any:
    """Get currently boosted/featured tokens across all chains."""
    data = _make_request("token-boosts/active/v1")
    if isinstance(data, list):
        return {"boosted": data[:50], "count": len(data)}
    return data


def get_top_boosted() -> Any:
    """Get top boosted tokens by boost amount."""
    data = _make_request("token-boosts/top/v1")
    if isinstance(data, list):
        return {"top_boosted": data[:50], "count": len(data)}
    return data


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided. Available: pairs, pair, search, token_pairs, profiles, boosted, top_boosted"}))
        return

    command = args[0]

    if command == "pairs":
        if len(args) < 2:
            result = {"error": "Usage: pairs <chain_id> (e.g. ethereum, bsc, solana, arbitrum)"}
        else:
            result = get_pairs_by_chain(args[1])
    elif command == "pair":
        if len(args) < 3:
            result = {"error": "Usage: pair <chain> <pair_address>"}
        else:
            result = get_pair(args[1], args[2])
    elif command == "search":
        if len(args) < 2:
            result = {"error": "Usage: search <query>"}
        else:
            result = search_pairs(args[1])
    elif command == "token_pairs":
        if len(args) < 3:
            result = {"error": "Usage: token_pairs <chain> <token_address>"}
        else:
            result = get_token_pairs(args[1], args[2])
    elif command == "profiles":
        result = get_latest_token_profiles()
    elif command == "boosted":
        result = get_boosted_tokens()
    elif command == "top_boosted":
        result = get_top_boosted()
    else:
        result = {"error": f"Unknown command: {command}. Available: pairs, pair, search, token_pairs, profiles, boosted, top_boosted"}

    print(json.dumps(result))


if __name__ == "__main__":
    main()
