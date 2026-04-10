#!/usr/bin/env python3
"""
Relationship Map Data Fetcher — v2
Fetches comprehensive corporate intelligence: company info, governance,
officers, analysts, mutual funds, institutional/insider holders, peers,
earnings calendar, technicals, and short interest from Yahoo Finance.

Usage: python relationship_map.py <TICKER>
Output: JSON to stdout
"""

import json
import sys


def _safe_float(val, default=0.0):
    try:
        return float(val) if val is not None else default
    except (TypeError, ValueError):
        return default


def _safe_int(val, default=0):
    try:
        return int(val) if val is not None else default
    except (TypeError, ValueError):
        return default


def _safe_str(val, default=""):
    return str(val) if val is not None else default


def fetch_company_data(ticker: str) -> dict:
    try:
        import yfinance as yf
    except ImportError:
        return {"error": "yfinance not installed. Run: pip install yfinance"}

    result = {
        "company": {},
        "governance": {},
        "technicals": {},
        "short_interest": {},
        "enterprise": {},
        "margins": {},
        "analyst_targets": {},
        "recommendations_summary": [],
        "upgrades_downgrades": [],
        "officers": [],
        "institutional_holders": [],
        "mutualfund_holders": [],
        "insider_holders": [],
        "peers": [],
        "calendar": {},
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
            "website": info.get("website", ""),
            "description": info.get("longBusinessSummary", "")[:300],
            "employees": _safe_int(info.get("fullTimeEmployees")),
            "country": info.get("country", ""),
            "exchange": info.get("exchange", ""),
            "currency": info.get("currency", "USD"),
            "market_cap": _safe_float(info.get("marketCap")),
            "current_price": _safe_float(info.get("currentPrice", info.get("regularMarketPrice"))),
            "previous_close": _safe_float(info.get("previousClose")),
            "day_change_pct": _safe_float(info.get("regularMarketChangePercent")),
            "pe_ratio": _safe_float(info.get("trailingPE")),
            "forward_pe": _safe_float(info.get("forwardPE")),
            "price_to_book": _safe_float(info.get("priceToBook")),
            "roe": _safe_float(info.get("returnOnEquity")),
            "roa": _safe_float(info.get("returnOnAssets")),
            "revenue_growth": _safe_float(info.get("revenueGrowth")),
            "earnings_growth": _safe_float(info.get("earningsGrowth")),
            "profit_margins": _safe_float(info.get("profitMargins")),
            "revenue": _safe_float(info.get("totalRevenue")),
            "ebitda": _safe_float(info.get("ebitda")),
            "free_cashflow": _safe_float(info.get("freeCashflow")),
            "operating_cashflow": _safe_float(info.get("operatingCashflow")),
            "total_cash": _safe_float(info.get("totalCash")),
            "total_debt": _safe_float(info.get("totalDebt")),
            "insider_percent": _safe_float(info.get("heldPercentInsiders")),
            "institutional_percent": _safe_float(info.get("heldPercentInstitutions")),
            "recommendation": info.get("recommendationKey", ""),
            "recommendation_mean": _safe_float(info.get("recommendationMean")),
            "target_high": _safe_float(info.get("targetHighPrice")),
            "target_low": _safe_float(info.get("targetLowPrice")),
            "target_mean": _safe_float(info.get("targetMeanPrice")),
            "target_median": _safe_float(info.get("targetMedianPrice")),
            "analyst_count": _safe_int(info.get("numberOfAnalystOpinions")),
            "dividend_yield": _safe_float(info.get("dividendYield")),
            "payout_ratio": _safe_float(info.get("payoutRatio")),
            "trailing_eps": _safe_float(info.get("trailingEps")),
            "forward_eps": _safe_float(info.get("forwardEps")),
            "shares_outstanding": _safe_float(info.get("sharesOutstanding")),
        }

        quality = 30

        # ── Governance Risk ───────────────────────────────────────────────
        audit = _safe_int(info.get("auditRisk"))
        board = _safe_int(info.get("boardRisk"))
        comp  = _safe_int(info.get("compensationRisk"))
        shr   = _safe_int(info.get("shareHolderRightsRisk"))
        overall = _safe_int(info.get("overallRisk"))
        if overall > 0:
            quality += 5
            result["governance"] = {
                "audit_risk": audit,
                "board_risk": board,
                "compensation_risk": comp,
                "shareholder_rights_risk": shr,
                "overall_risk": overall,
            }

        # ── Technicals ────────────────────────────────────────────────────
        result["technicals"] = {
            "fifty_two_week_high": _safe_float(info.get("fiftyTwoWeekHigh")),
            "fifty_two_week_low": _safe_float(info.get("fiftyTwoWeekLow")),
            "fifty_day_avg": _safe_float(info.get("fiftyDayAverage")),
            "two_hundred_day_avg": _safe_float(info.get("twoHundredDayAverage")),
            "beta": _safe_float(info.get("beta")),
            "week52_change_pct": _safe_float(info.get("52WeekChange")),
            "sp500_52wk_change": _safe_float(info.get("SandP52WeekChange")),
            "avg_volume": _safe_int(info.get("averageVolume")),
            "avg_volume_10d": _safe_int(info.get("averageDailyVolume10Day")),
        }
        quality += 5

        # ── Short Interest ────────────────────────────────────────────────
        shares_short = _safe_float(info.get("sharesShort"))
        if shares_short > 0:
            quality += 5
            result["short_interest"] = {
                "shares_short": shares_short,
                "short_ratio": _safe_float(info.get("shortRatio")),
                "short_pct_float": _safe_float(info.get("shortPercentOfFloat")),
                "float_shares": _safe_float(info.get("floatShares")),
            }

        # ── Enterprise Metrics ────────────────────────────────────────────
        result["enterprise"] = {
            "enterprise_value": _safe_float(info.get("enterpriseValue")),
            "ev_to_revenue": _safe_float(info.get("enterpriseToRevenue")),
            "ev_to_ebitda": _safe_float(info.get("enterpriseToEbitda")),
            "peg_ratio": _safe_float(info.get("trailingPegRatio")),
            "price_to_sales": _safe_float(info.get("priceToSalesTrailing12Months")),
            "book_value": _safe_float(info.get("bookValue")),
        }

        # ── Margins ───────────────────────────────────────────────────────
        result["margins"] = {
            "gross": _safe_float(info.get("grossMargins")),
            "operating": _safe_float(info.get("operatingMargins")),
            "ebitda": _safe_float(info.get("ebitdaMargins")),
            "net": _safe_float(info.get("profitMargins")),
            "debt_to_equity": _safe_float(info.get("debtToEquity")),
            "current_ratio": _safe_float(info.get("currentRatio")),
            "quick_ratio": _safe_float(info.get("quickRatio")),
        }
        quality += 5

        # ── Analyst Price Targets ─────────────────────────────────────────
        try:
            apt = stock.analyst_price_targets
            if isinstance(apt, dict) and apt:
                quality += 5
                result["analyst_targets"] = {
                    "current": _safe_float(apt.get("current")),
                    "high": _safe_float(apt.get("high")),
                    "low": _safe_float(apt.get("low")),
                    "mean": _safe_float(apt.get("mean")),
                    "median": _safe_float(apt.get("median")),
                }
        except Exception:
            pass

        # ── Recommendations Summary ───────────────────────────────────────
        try:
            rec = stock.recommendations_summary
            if rec is not None and not rec.empty:
                quality += 5
                for _, row in rec.iterrows():
                    result["recommendations_summary"].append({
                        "period": _safe_str(row.get("period")),
                        "strong_buy": _safe_int(row.get("strongBuy")),
                        "buy": _safe_int(row.get("buy")),
                        "hold": _safe_int(row.get("hold")),
                        "sell": _safe_int(row.get("sell")),
                        "strong_sell": _safe_int(row.get("strongSell")),
                    })
        except Exception:
            pass

        # ── Upgrades / Downgrades ─────────────────────────────────────────
        try:
            ud = stock.upgrades_downgrades
            if ud is not None and not ud.empty:
                quality += 5
                for idx, row in ud.head(8).iterrows():
                    result["upgrades_downgrades"].append({
                        "date": _safe_str(idx)[:10],
                        "firm": _safe_str(row.get("Firm")),
                        "to_grade": _safe_str(row.get("ToGrade")),
                        "from_grade": _safe_str(row.get("FromGrade")),
                        "action": _safe_str(row.get("Action")),
                        "price_target": _safe_float(row.get("currentPriceTarget")),
                        "prior_target": _safe_float(row.get("priorPriceTarget")),
                    })
        except Exception:
            pass

        # ── Company Officers ──────────────────────────────────────────────
        try:
            officers = info.get("companyOfficers", [])
            if officers:
                quality += 5
                for o in officers[:8]:
                    result["officers"].append({
                        "name": _safe_str(o.get("name")),
                        "title": _safe_str(o.get("title")),
                        "total_pay": _safe_int(o.get("totalPay")),
                        "year_born": _safe_int(o.get("yearBorn")),
                    })
        except Exception:
            pass

        # ── Calendar (Earnings, Dividends) ────────────────────────────────
        try:
            cal = stock.calendar
            if isinstance(cal, dict) and cal:
                quality += 5
                result["calendar"] = {
                    "earnings_date": _safe_str(cal.get("Earnings Date", [None])[0] if isinstance(cal.get("Earnings Date"), list) else cal.get("Earnings Date")),
                    "earnings_avg": _safe_float(cal.get("Earnings Average")),
                    "earnings_low": _safe_float(cal.get("Earnings Low")),
                    "earnings_high": _safe_float(cal.get("Earnings High")),
                    "revenue_avg": _safe_float(cal.get("Revenue Average")),
                    "revenue_low": _safe_float(cal.get("Revenue Low")),
                    "revenue_high": _safe_float(cal.get("Revenue High")),
                    "ex_dividend_date": _safe_str(cal.get("Ex-Dividend Date")),
                    "dividend_date": _safe_str(cal.get("Dividend Date")),
                }
        except Exception:
            pass

        # ── Institutional Holders ─────────────────────────────────────────
        try:
            inst = stock.institutional_holders
            if inst is not None and not inst.empty:
                quality += 10
                for _, row in inst.head(20).iterrows():
                    pct = _safe_float(row.get("% Out", row.get("pctHeld", 0)))
                    if pct < 1:
                        pct *= 100
                    holder = {
                        "name": _safe_str(row.get("Holder")),
                        "shares": _safe_float(row.get("Shares")),
                        "value": _safe_float(row.get("Value")),
                        "percentage": pct,
                        "change_percent": 0.0,
                        "fund_family": "",
                        "type": "institutional",
                    }
                    if holder["name"]:
                        result["institutional_holders"].append(holder)
        except Exception:
            pass

        # ── Mutual Fund Holders ───────────────────────────────────────────
        try:
            mf = stock.mutualfund_holders
            if mf is not None and not mf.empty:
                quality += 5
                for _, row in mf.head(10).iterrows():
                    pct = _safe_float(row.get("pctHeld", 0))
                    if pct < 1:
                        pct *= 100
                    holder = {
                        "name": _safe_str(row.get("Holder")),
                        "shares": _safe_float(row.get("Shares")),
                        "value": _safe_float(row.get("Value")),
                        "percentage": pct,
                        "change_percent": _safe_float(row.get("pctChange", 0)) * 100,
                        "fund_family": "",
                        "type": "mutualfund",
                    }
                    if holder["name"]:
                        result["mutualfund_holders"].append(holder)
        except Exception:
            pass

        # ── Insider Holders ───────────────────────────────────────────────
        try:
            insiders = stock.insider_holders
            if insiders is not None and not insiders.empty:
                quality += 5
                for _, row in insiders.head(12).iterrows():
                    insider = {
                        "name": _safe_str(row.get("Name", row.get("Insider"))),
                        "title": _safe_str(row.get("Position", row.get("Relation"))),
                        "shares": _safe_float(row.get("Shares", row.get("sharesOwned", 0))),
                        "percentage": 0.0,
                        "last_transaction": "",
                    }
                    if insider["name"]:
                        result["insider_holders"].append(insider)
        except Exception:
            pass

        # enrich insiders with transaction direction
        try:
            txns = stock.insider_transactions
            if txns is not None and not txns.empty:
                for _, row in txns.head(15).iterrows():
                    name = _safe_str(row.get("Insider", row.get("Name")))
                    tx = _safe_str(row.get("Transaction", row.get("Text", ""))).lower()
                    for ins in result["insider_holders"]:
                        if name and name.split()[0].lower() in ins["name"].lower():
                            ins["last_transaction"] = "buy" if ("purchase" in tx or "buy" in tx) else "sell"
                            break
        except Exception:
            pass

        # ── Peer Companies ────────────────────────────────────────────────
        INDUSTRY_PEERS = {
            "Technology":        ["AAPL", "MSFT", "GOOGL", "META", "AMZN", "NVDA", "CRM", "ORCL", "ADBE"],
            "Software":          ["MSFT", "CRM", "ORCL", "ADBE", "NOW", "INTU", "SNOW", "PLTR"],
            "Semiconductors":    ["NVDA", "AMD", "INTC", "AVGO", "QCOM", "TXN", "MU", "AMAT"],
            "Electric Vehicles": ["TSLA", "RIVN", "LCID", "NIO", "XPEV", "LI"],
            "Auto Manufacturers":["TSLA", "F", "GM", "TM", "HMC", "STLA"],
            "Banks":             ["JPM", "BAC", "WFC", "C", "GS", "MS", "USB"],
            "Pharmaceuticals":   ["JNJ", "PFE", "MRK", "ABBV", "LLY", "BMY", "AMGN"],
            "Oil & Gas":         ["XOM", "CVX", "COP", "EOG", "SLB", "PSX"],
            "Retail":            ["WMT", "COST", "TGT", "HD", "LOW", "AMZN"],
            "Payments":          ["V", "MA", "PYPL", "SQ", "FIS", "FISV"],
            "Cloud Computing":   ["AMZN", "MSFT", "GOOGL", "CRM", "SNOW", "NOW"],
            "Telecom":           ["T", "VZ", "TMUS", "CMCSA"],
            "Healthcare":        ["UNH", "CVS", "HCA", "MCK", "ABC"],
            "Insurance":         ["BRK-B", "MET", "PRU", "AFL", "AIG"],
            "Real Estate":       ["AMT", "PLD", "CCI", "EQIX", "SPG"],
            "Consumer":          ["PG", "KO", "PEP", "UL", "CL", "MCD", "SBUX"],
            "Airlines":          ["DAL", "UAL", "AAL", "LUV", "ALK"],
            "Defense":           ["LMT", "RTX", "NOC", "GD", "BA"],
            "Energy":            ["NEE", "DUK", "SO", "AEP", "D"],
        }

        industry = result["company"].get("industry", "")
        sector   = result["company"].get("sector", "")
        peer_tickers = []

        for key, peers_list in INDUSTRY_PEERS.items():
            if key.lower() in industry.lower() or key.lower() in sector.lower():
                peer_tickers = [p for p in peers_list if p != ticker.upper()][:8]
                break

        if not peer_tickers:
            peer_tickers = [p for p in ["AAPL", "MSFT", "GOOGL", "AMZN", "META"] if p != ticker.upper()][:5]

        for pt in peer_tickers:
            try:
                pi = yf.Ticker(pt).info or {}
                peer = {
                    "ticker": pt,
                    "name": pi.get("longName", pi.get("shortName", pt)),
                    "market_cap": _safe_float(pi.get("marketCap")),
                    "pe_ratio": _safe_float(pi.get("trailingPE")),
                    "forward_pe": _safe_float(pi.get("forwardPE")),
                    "roe": _safe_float(pi.get("returnOnEquity")),
                    "revenue_growth": _safe_float(pi.get("revenueGrowth")),
                    "profit_margins": _safe_float(pi.get("profitMargins")),
                    "gross_margins": _safe_float(pi.get("grossMargins")),
                    "current_price": _safe_float(pi.get("currentPrice", pi.get("regularMarketPrice"))),
                    "sector": pi.get("sector", ""),
                    "beta": _safe_float(pi.get("beta")),
                    "ev_to_ebitda": _safe_float(pi.get("enterpriseToEbitda")),
                    "price_to_book": _safe_float(pi.get("priceToBook")),
                    "week52_change": _safe_float(pi.get("52WeekChange")),
                    "recommendation": pi.get("recommendationKey", ""),
                }
                result["peers"].append(peer)
                quality += 3
            except Exception:
                pass

        result["data_quality"] = min(quality, 100)

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
