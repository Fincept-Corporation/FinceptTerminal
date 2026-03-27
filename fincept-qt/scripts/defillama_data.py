"""
DeFiLlama Data Fetcher
TVL for 3000+ DeFi protocols, chain TVL, yield pools, bridges, fees.
No API key required.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

BASE_URL = "https://api.llama.fi"
COINS_URL = "https://coins.llama.fi"
YIELDS_URL = "https://yields.llama.fi"
BRIDGES_URL = "https://bridges.llama.fi"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)


def _make_request(endpoint: str, params: Dict = None, base: str = None) -> Any:
    """Make HTTP request with error handling."""
    base_url = base if base else BASE_URL
    url = f"{base_url}/{endpoint}" if not endpoint.startswith('http') else endpoint
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


def get_protocols() -> Any:
    """Get list of all DeFi protocols with TVL data."""
    data = _make_request("protocols")
    if isinstance(data, list):
        return {"protocols": data[:200], "total_count": len(data)}
    return data


def get_protocol(name: str) -> Any:
    """Get detailed TVL data for a specific protocol."""
    return _make_request(f"protocol/{name}")


def get_chains() -> Any:
    """Get TVL for all chains."""
    data = _make_request("v2/chains")
    if isinstance(data, list):
        return {"chains": data, "total_count": len(data)}
    return data


def get_chain_tvl(chain: str) -> Any:
    """Get historical TVL data for a specific chain."""
    data = _make_request(f"v2/historicalChainTvl/{chain}")
    if isinstance(data, list):
        return {"chain": chain, "history": data}
    return data


def get_yield_pools() -> Any:
    """Get yield farming pools with APY and TVL data."""
    data = _make_request("pools", base=YIELDS_URL)
    if isinstance(data, dict) and "data" in data:
        pools = data["data"]
        return {"pools": pools[:200], "total_count": len(pools)}
    return data


def get_stablecoins() -> Any:
    """Get stablecoin circulating supply and peg data."""
    data = _make_request("stablecoins?includePrices=true")
    if isinstance(data, dict) and "peggedAssets" in data:
        assets = data["peggedAssets"]
        return {"stablecoins": assets, "total_count": len(assets)}
    return data


def get_bridges() -> Any:
    """Get bridge volume and TVL data."""
    data = _make_request("bridges?includeChains=true", base=BRIDGES_URL)
    if isinstance(data, dict) and "bridges" in data:
        bridges = data["bridges"]
        return {"bridges": bridges, "total_count": len(bridges)}
    return data


def get_protocol_fees(protocol: str = None) -> Any:
    """Get fees and revenue data for protocols."""
    if protocol:
        return _make_request(f"summary/fees/{protocol}")
    data = _make_request("overview/fees")
    if isinstance(data, dict) and "protocols" in data:
        return {"protocols": data["protocols"][:100], "total_count": len(data.get("protocols", []))}
    return data


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided. Available: protocols, protocol, chains, chain_tvl, yields, stablecoins, bridges, fees"}))
        return

    command = args[0]

    if command == "protocols":
        result = get_protocols()
    elif command == "protocol":
        if len(args) < 2:
            result = {"error": "Usage: protocol <name>"}
        else:
            result = get_protocol(args[1])
    elif command == "chains":
        result = get_chains()
    elif command == "chain_tvl":
        if len(args) < 2:
            result = {"error": "Usage: chain_tvl <chain_name>"}
        else:
            result = get_chain_tvl(args[1])
    elif command == "yields":
        result = get_yield_pools()
    elif command == "stablecoins":
        result = get_stablecoins()
    elif command == "bridges":
        result = get_bridges()
    elif command == "fees":
        protocol = args[1] if len(args) > 1 else None
        result = get_protocol_fees(protocol)
    else:
        result = {"error": f"Unknown command: {command}. Available: protocols, protocol, chains, chain_tvl, yields, stablecoins, bridges, fees"}

    print(json.dumps(result))


if __name__ == "__main__":
    main()
