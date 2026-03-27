"""
Blockchain.com Data Fetcher
Bitcoin network stats, hashrate, mempool, tx volumes, address data.
No API key required.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

BASE_URL = "https://api.blockchain.info"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)
session.headers.update({"Accept": "application/json"})


def _make_request(endpoint: str, params: Dict = None) -> Any:
    """Make HTTP request with error handling."""
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    try:
        response = session.get(url, params=params, timeout=30)
        response.raise_for_status()
        ct = response.headers.get("content-type", "")
        if "json" in ct:
            return response.json()
        # Some endpoints return plain text/numbers
        text = response.text.strip()
        try:
            return json.loads(text)
        except (json.JSONDecodeError, ValueError):
            return {"value": text}
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {str(e)}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}


def get_stats() -> Any:
    """Get comprehensive Bitcoin network statistics."""
    data = _make_request("stats")
    if isinstance(data, dict) and "error" not in data:
        return {
            "market_price_usd": data.get("market_price_usd"),
            "hash_rate": data.get("hash_rate"),
            "total_fees_btc": data.get("total_fees_btc"),
            "n_btc_mined": data.get("n_btc_mined"),
            "n_tx": data.get("n_tx"),
            "n_blocks_mined": data.get("n_blocks_mined"),
            "minutes_between_blocks": data.get("minutes_between_blocks"),
            "totalbc": data.get("totalbc"),
            "n_blocks_total": data.get("n_blocks_total"),
            "estimated_transaction_volume_usd": data.get("estimated_transaction_volume_usd"),
            "blocks_size": data.get("blocks_size"),
            "miners_revenue_usd": data.get("miners_revenue_usd"),
            "nextretarget": data.get("nextretarget"),
            "difficulty": data.get("difficulty"),
            "estimated_btc_sent": data.get("estimated_btc_sent"),
            "miners_revenue_btc": data.get("miners_revenue_btc"),
            "total_btc_sent": data.get("total_btc_sent"),
            "trade_volume_btc": data.get("trade_volume_btc"),
            "trade_volume_usd": data.get("trade_volume_usd"),
        }
    return data


def get_chart(chart_name: str, timespan: str = "1year") -> Any:
    """Get chart data for various Bitcoin metrics.
    Available charts: total-bitcoins, market-price, market-cap, trade-volume,
    blocks-size, avg-block-size, n-transactions, hash-rate, difficulty, etc.
    Timespans: 1week, 2weeks, 1month, 3months, 6months, 1year, 2year, all.
    """
    params = {"timespan": timespan, "format": "json", "cors": "true"}
    data = _make_request(f"charts/{chart_name}", params=params)
    if isinstance(data, dict) and "values" in data:
        return {
            "chart": chart_name,
            "timespan": timespan,
            "unit": data.get("unit"),
            "period": data.get("period"),
            "description": data.get("description"),
            "name": data.get("name"),
            "values": data["values"],
            "count": len(data["values"]),
        }
    return data


def get_pools(timespan: str = "4days") -> Any:
    """Get mining pool distribution by hashrate share.
    Timespans: 24hours, 48hours, 4days, 1week, 2week, 1month.
    """
    params = {"timespan": timespan}
    data = _make_request("pools", params=params)
    if isinstance(data, dict) and "error" not in data:
        pools = [{"pool": k, "blocks": v} for k, v in data.items()]
        pools.sort(key=lambda x: x["blocks"], reverse=True)
        return {"timespan": timespan, "pools": pools, "total_pools": len(pools)}
    return data


def get_address(address: str) -> Any:
    """Get address balance and transaction history."""
    params = {"format": "json"}
    data = _make_request(f"rawaddr/{address}", params=params)
    if isinstance(data, dict) and "error" not in data:
        txs = data.get("txs", [])
        return {
            "address": address,
            "final_balance": data.get("final_balance"),
            "n_tx": data.get("n_tx"),
            "total_received": data.get("total_received"),
            "total_sent": data.get("total_sent"),
            "recent_transactions": txs[:10],
        }
    return data


def get_transaction(tx_hash: str) -> Any:
    """Get details for a specific Bitcoin transaction."""
    data = _make_request(f"rawtx/{tx_hash}")
    if isinstance(data, dict) and "error" not in data:
        return {
            "hash": data.get("hash"),
            "time": data.get("time"),
            "block_height": data.get("block_height"),
            "fee": data.get("fee"),
            "size": data.get("size"),
            "inputs": data.get("inputs", [])[:5],
            "out": data.get("out", [])[:5],
            "input_count": len(data.get("inputs", [])),
            "output_count": len(data.get("out", [])),
        }
    return data


def get_ticker() -> Any:
    """Get Bitcoin ticker data in multiple currencies."""
    data = _make_request("ticker")
    if isinstance(data, dict) and "error" not in data:
        result = {}
        for currency, info in data.items():
            result[currency] = {
                "buy": info.get("buy"),
                "sell": info.get("sell"),
                "last": info.get("last"),
                "15m": info.get("15m"),
                "symbol": info.get("symbol"),
            }
        return {"ticker": result, "currencies": list(result.keys())}
    return data


def get_mempool() -> Any:
    """Get current mempool size and unconfirmed transaction count."""
    data = _make_request("q/unconfirmedcount")
    unconfirmed = data.get("value") if isinstance(data, dict) else data
    size_data = _make_request("q/getblockcount")
    block_count = size_data.get("value") if isinstance(size_data, dict) else size_data
    return {
        "unconfirmed_count": unconfirmed,
        "block_height": block_count,
    }


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided. Available: stats, chart, pools, address, transaction, ticker, mempool"}))
        return

    command = args[0]

    if command == "stats":
        result = get_stats()
    elif command == "chart":
        if len(args) < 2:
            result = {"error": "Usage: chart <chart_name> [timespan]. Charts: total-bitcoins, market-price, hash-rate, difficulty, n-transactions, trade-volume, blocks-size"}
        else:
            timespan = args[2] if len(args) > 2 else "1year"
            result = get_chart(args[1], timespan)
    elif command == "pools":
        timespan = args[1] if len(args) > 1 else "4days"
        result = get_pools(timespan)
    elif command == "address":
        if len(args) < 2:
            result = {"error": "Usage: address <bitcoin_address>"}
        else:
            result = get_address(args[1])
    elif command == "transaction":
        if len(args) < 2:
            result = {"error": "Usage: transaction <tx_hash>"}
        else:
            result = get_transaction(args[1])
    elif command == "ticker":
        result = get_ticker()
    elif command == "mempool":
        result = get_mempool()
    else:
        result = {"error": f"Unknown command: {command}. Available: stats, chart, pools, address, transaction, ticker, mempool"}

    print(json.dumps(result))


if __name__ == "__main__":
    main()
