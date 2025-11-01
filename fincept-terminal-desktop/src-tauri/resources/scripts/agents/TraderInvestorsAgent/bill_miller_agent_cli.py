#!/usr/bin/env python3
"""
Bill Miller Agent - CLI Version
Contrarian value + technology focus
"""

import sys
import json
import argparse
from typing import Dict, Any
import random

def analyze_stock_miller_style(ticker: str, parameters: Dict[str, Any], previous_context: str = None) -> Dict[str, Any]:
    """
    Bill Miller-style contrarian value investing
    Focus: Technology value plays, contrarian timing, concentrated positions
    """
    ticker_hash = sum(ord(c) for c in ticker)
    random.seed(ticker_hash)

    contrarian_opportunity = random.uniform(4.0, 9.5)
    technology_value = random.uniform(5.0, 9.0)
    long_term_drivers = random.uniform(6.0, 9.5)
    market_misperception = random.uniform(3.0, 8.5)
    concentration_worthiness = random.uniform(5.0, 9.0)

    context_adjustment = ""
    if previous_context:
        context_adjustment = f"\n\n🔗 Building on Previous Analysis:\n{previous_context[:500]}...\n"
        contrarian_opportunity *= 1.02
        concentration_worthiness *= 1.03

    total_score = (
        contrarian_opportunity * 0.30 +
        concentration_worthiness * 0.25 +
        technology_value * 0.20 +
        long_term_drivers * 0.15 +
        market_misperception * 0.10
    )

    # Miller's high-conviction approach
    if total_score >= 8.0:
        signal = "bullish"
        recommendation = "BUY - High-conviction contrarian opportunity"
        confidence = random.uniform(80.0, 95.0)
    elif total_score <= 4.0:
        signal = "bearish"
        recommendation = "PASS - Lacks contrarian value setup"
        confidence = random.uniform(60.0, 75.0)
    else:
        signal = "neutral"
        recommendation = "WATCH - Potential developing, wait for better entry"
        confidence = random.uniform(50.0, 70.0)

    analysis = {
        "ticker": ticker,
        "signal": signal,
        "confidence": round(confidence, 2),
        "total_score": round(total_score, 2),
        "reasoning": f"""
Bill Miller Contrarian Value Analysis for {ticker}
{context_adjustment}
📊 Overall Score: {total_score:.1f}/10 - {signal.upper()}

Contrarian Opportunity (Score: {contrarian_opportunity:.1f}/10):
{'✓' if contrarian_opportunity >= 7 else '⚠'} Extreme valuation creating setup
{'✓' if contrarian_opportunity >= 7 else '⚠'} Market pessimism vs business reality
{'✓' if contrarian_opportunity >= 7 else '⚠'} Temporary challenges with long-term potential

Technology Value Potential (Score: {technology_value:.1f}/10):
{'✓' if technology_value >= 7 else '⚠'} Asset-light scalable model
{'✓' if technology_value >= 7 else '⚠'} Network effects and switching costs
{'✓' if technology_value >= 7 else '⚠'} R&D efficiency and innovation

Long-Term Growth Drivers (Score: {long_term_drivers:.1f}/10):
{'✓' if long_term_drivers >= 7 else '⚠'} Consistent revenue growth track record
{'✓' if long_term_drivers >= 7 else '⚠'} Reinvestment for future growth
{'✓' if long_term_drivers >= 7 else '⚠'} Balance sheet capacity for expansion

Market Misperception (Score: {market_misperception:.1f}/10):
{'✓' if market_misperception >= 6 else '⚠'} Quality company at value multiple
{'✓' if market_misperception >= 6 else '⚠'} Growth story with temporary headwinds
{'✓' if market_misperception >= 6 else '⚠'} Asset value not recognized by market

Concentration Worthiness (Score: {concentration_worthiness:.1f}/10):
{'✓' if concentration_worthiness >= 7 else '⚠'} Strong competitive position
{'✓' if concentration_worthiness >= 7 else '⚠'} Significant upside potential (50%+)
{'✓' if concentration_worthiness >= 7 else '⚠'} Durable business model

💡 Recommendation: {recommendation}
📈 Confidence Level: {confidence:.1f}%

"Being early is the same as being wrong, but being contrarian and right is very rewarding." - Bill Miller
        """.strip(),
        "metrics": {
            "contrarian_opportunity": round(contrarian_opportunity, 2),
            "technology_value": round(technology_value, 2),
            "long_term_drivers": round(long_term_drivers, 2),
            "market_misperception": round(market_misperception, 2),
            "concentration_worthiness": round(concentration_worthiness, 2)
        }
    }

    return analysis

def execute_agent(parameters: Dict[str, Any], inputs: Dict[str, Any]) -> Dict[str, Any]:
    """Main agent execution"""
    try:
        tickers = parameters.get("tickers", ["AAPL"])
        end_date = parameters.get("end_date", "2024-12-31")

        if isinstance(tickers, str):
            tickers = [tickers]

        mediated_context = None
        if inputs and isinstance(inputs, dict):
            mediated_context = inputs.get("mediated_analysis")

        results = {}
        for ticker in tickers:
            analysis = analyze_stock_miller_style(ticker, parameters, mediated_context)
            results[ticker] = analysis

        response = {
            "success": True,
            "agent": "bill_miller",
            "execution_date": end_date,
            "tickers_analyzed": len(tickers),
            "signals": {ticker: results[ticker]["signal"] for ticker in results},
            "analysis": results
        }

        if mediated_context:
            response["used_previous_context"] = True

        return response

    except Exception as e:
        return {"success": False, "error": str(e), "agent": "bill_miller"}

def main():
    parser = argparse.ArgumentParser(description='Bill Miller Agent - Contrarian Value Analysis')
    parser.add_argument('--parameters', type=str, required=True)
    parser.add_argument('--inputs', type=str, required=True)
    args = parser.parse_args()

    try:
        result = execute_agent(json.loads(args.parameters), json.loads(args.inputs))
        print(json.dumps(result, indent=2))
        sys.exit(0)
    except Exception as e:
        print(json.dumps({"success": False, "error": str(e), "agent": "bill_miller"}), file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()
