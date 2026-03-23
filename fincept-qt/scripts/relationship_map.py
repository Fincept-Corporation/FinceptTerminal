#!/usr/bin/env python3
"""
Relationship Map Data Fetcher
Fetches corporate intelligence data: company info, peers, institutional holders,
insiders, events from Yahoo Finance.

Usage: python relationship_map.py <TICKER>
Output: JSON to stdout
"""

import json
import sys

def fetch_company_data(ticker: str) -> dict:
    """Fetch comprehensive company data from yfinance."""
    try:
        import yfinance as yf
    except ImportError:
        return {"error": "yfinance not installed. Run: pip install yfinance"}

    result = {
        "company": {},
        "institutional_holders": [],
        "insider_holders": [],
        "peers": [],
        "events": [],
        "supply_chain": [],
        "data_quality": 0,
    }

    try:
        stock = yf.Ticker(ticker)
        info = stock.info or {}

        # ── Company Info ──────────────────────────────────────────────────
        result["company"] = {
            "ticker": ticker.upper(),
            "name": info.get("longName", info.get("shortName", ticker)),
            "sector": info.get("sector", ""),
            "industry": info.get("industry", ""),
            "market_cap": info.get("marketCap", 0),
            "current_price": info.get("currentPrice", info.get("regularMarketPrice", 0)),
            "pe_ratio": info.get("trailingPE", 0),
            "forward_pe": info.get("forwardPE", 0),
            "price_to_book": info.get("priceToBook", 0),
            "roe": info.get("returnOnEquity", 0),
            "revenue_growth": info.get("revenueGrowth", 0),
            "profit_margins": info.get("profitMargins", 0),
            "revenue": info.get("totalRevenue", 0),
            "ebitda": info.get("ebitda", 0),
            "free_cashflow": info.get("freeCashflow", 0),
            "total_cash": info.get("totalCash", 0),
            "total_debt": info.get("totalDebt", 0),
            "insider_percent": info.get("heldPercentInsiders", 0),
            "institutional_percent": info.get("heldPercentInstitutions", 0),
            "employees": info.get("fullTimeEmployees", 0),
            "recommendation": info.get("recommendationKey", ""),
            "target_price": info.get("targetMeanPrice", 0),
            "analyst_count": info.get("numberOfAnalystOpinions", 0),
        }

        quality = 30  # base for having company info

        # ── Institutional Holders ─────────────────────────────────────────
        try:
            inst = stock.institutional_holders
            if inst is not None and not inst.empty:
                quality += 20
                for _, row in inst.head(25).iterrows():
                    holder = {
                        "name": str(row.get("Holder", "")),
                        "shares": float(row.get("Shares", 0)),
                        "value": float(row.get("Value", 0)),
                        "percentage": float(row.get("% Out", row.get("pctHeld", 0))) * 100
                            if float(row.get("% Out", row.get("pctHeld", 0))) < 1
                            else float(row.get("% Out", row.get("pctHeld", 0))),
                        "change_percent": 0,
                        "fund_family": "",
                    }
                    if holder["name"]:
                        result["institutional_holders"].append(holder)
        except Exception:
            pass

        # ── Insider Holders ───────────────────────────────────────────────
        try:
            insiders = stock.insider_holders
            if insiders is not None and not insiders.empty:
                quality += 15
                for _, row in insiders.head(15).iterrows():
                    insider = {
                        "name": str(row.get("Name", row.get("Insider", ""))),
                        "title": str(row.get("Position", row.get("Relation", ""))),
                        "shares": float(row.get("Shares", row.get("sharesOwned", 0))),
                        "percentage": 0,
                        "last_transaction": "",
                    }
                    if insider["name"]:
                        result["insider_holders"].append(insider)
        except Exception:
            pass

        # Try insider transactions for more detail
        try:
            txns = stock.insider_transactions
            if txns is not None and not txns.empty:
                for _, row in txns.head(10).iterrows():
                    name = str(row.get("Insider", row.get("Name", "")))
                    tx_type = str(row.get("Transaction", row.get("Text", ""))).lower()
                    # Update existing insider with transaction info
                    for ins in result["insider_holders"]:
                        if name and name.split()[0].lower() in ins["name"].lower():
                            ins["last_transaction"] = "buy" if "purchase" in tx_type or "buy" in tx_type else "sell"
                            break
        except Exception:
            pass

        # ── Major Holders Summary ─────────────────────────────────────────
        try:
            major = stock.major_holders
            if major is not None and not major.empty:
                for _, row in major.iterrows():
                    val = row.iloc[0] if len(row) > 0 else 0
                    desc = str(row.iloc[1]) if len(row) > 1 else ""
                    if "insider" in desc.lower():
                        try:
                            result["company"]["insider_percent"] = float(str(val).replace('%', '')) / 100
                        except (ValueError, TypeError):
                            pass
                    elif "institution" in desc.lower():
                        try:
                            result["company"]["institutional_percent"] = float(str(val).replace('%', '')) / 100
                        except (ValueError, TypeError):
                            pass
        except Exception:
            pass

        # ── Peer Companies ────────────────────────────────────────────────
        INDUSTRY_PEERS = {
            "Technology": ["AAPL", "MSFT", "GOOGL", "META", "AMZN", "NVDA", "CRM", "ORCL", "ADBE"],
            "Software": ["MSFT", "CRM", "ORCL", "ADBE", "NOW", "INTU", "SNOW", "PLTR"],
            "Semiconductors": ["NVDA", "AMD", "INTC", "AVGO", "QCOM", "TXN", "MU"],
            "Electric Vehicles": ["TSLA", "RIVN", "LCID", "NIO", "XPEV"],
            "Auto Manufacturers": ["TSLA", "F", "GM", "TM", "HMC", "STLA"],
            "Banks": ["JPM", "BAC", "WFC", "C", "GS", "MS"],
            "Pharmaceuticals": ["JNJ", "PFE", "MRK", "ABBV", "LLY", "BMY"],
            "Oil & Gas": ["XOM", "CVX", "COP", "EOG", "SLB"],
            "Retail": ["WMT", "COST", "TGT", "HD", "LOW"],
            "Payments": ["V", "MA", "PYPL", "SQ"],
            "Cloud Computing": ["AMZN", "MSFT", "GOOGL", "CRM", "SNOW"],
        }

        industry = result["company"]["industry"]
        sector = result["company"]["sector"]
        peer_tickers = []

        for key, peers in INDUSTRY_PEERS.items():
            if key.lower() in industry.lower() or key.lower() in sector.lower():
                peer_tickers = [p for p in peers if p != ticker.upper()][:8]
                break

        if not peer_tickers:
            peer_tickers = ["AAPL", "MSFT", "GOOGL", "AMZN", "META"]
            peer_tickers = [p for p in peer_tickers if p != ticker.upper()][:5]

        for pt in peer_tickers:
            try:
                pi = yf.Ticker(pt).info or {}
                peer = {
                    "ticker": pt,
                    "name": pi.get("longName", pi.get("shortName", pt)),
                    "market_cap": pi.get("marketCap", 0),
                    "pe_ratio": pi.get("trailingPE", 0),
                    "roe": pi.get("returnOnEquity", 0),
                    "revenue_growth": pi.get("revenueGrowth", 0),
                    "profit_margins": pi.get("profitMargins", 0),
                    "current_price": pi.get("currentPrice", pi.get("regularMarketPrice", 0)),
                    "sector": pi.get("sector", ""),
                }
                result["peers"].append(peer)
                quality += 5
            except Exception:
                pass

        quality = min(quality, 100)
        result["data_quality"] = quality

    except Exception as e:
        result["error"] = str(e)

    return result


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(json.dumps({"error": "Usage: relationship_map.py <TICKER>"}))
        sys.exit(1)

    ticker = sys.argv[1].strip().upper()
    data = fetch_company_data(ticker)
    print(json.dumps(data, default=str))
