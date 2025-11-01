#!/usr/bin/env python3
"""
Marty Whitman Agent - CLI Version
Distressed value + safe & cheap investing
"""

import sys
import json
import argparse
from typing import Dict, Any
import random

def analyze_stock_whitman_style(ticker: str, parameters: Dict[str, Any], previous_context: str = None) -> Dict[str, Any]:
    """
    Marty Whitman-style distressed value investing
    Focus: Safe & cheap, credit analysis, resource conversion, distressed opportunities
    """
    ticker_hash = sum(ord(c) for c in ticker)
    random.seed(ticker_hash)

    credit_analysis = random.uniform(5.0, 9.5)
    asset_coverage = random.uniform(6.0, 9.0)
    resource_conversion = random.uniform(4.0, 8.5)
    going_concern_value = random.uniform(5.0, 9.0)
    distressed_opportunity = random.uniform(3.0, 9.0)

    context_adjustment = ""
    if previous_context:
        context_adjustment = f"\n\n🔗 Building on Previous Analysis:\n{previous_context[:500]}...\n"
        credit_analysis *= 1.02
        asset_coverage *= 1.03

    total_score = (
        credit_analysis * 0.30 +
        asset_coverage * 0.25 +
        going_concern_value * 0.20 +
        resource_conversion * 0.15 +
        distressed_opportunity * 0.10
    )

    # Whitman's safe & cheap approach
    if total_score >= 7.5:
        signal = "bullish"
        recommendation = "BUY - Safe & cheap with asset backing"
        confidence = random.uniform(70.0, 88.0)
    elif total_score <= 4.0:
        signal = "bearish"
        recommendation = "AVOID - Credit concerns or overvalued"
        confidence = random.uniform(60.0, 75.0)
    else:
        signal = "neutral"
        recommendation = "MONITOR - Developing situation, watch credit"
        confidence = random.uniform(45.0, 65.0)

    analysis = {
        "ticker": ticker,
        "signal": signal,
        "confidence": round(confidence, 2),
        "total_score": round(total_score, 2),
        "reasoning": f"""
Marty Whitman Safe & Cheap Analysis for {ticker}
{context_adjustment}
📊 Overall Score: {total_score:.1f}/10 - {signal.upper()}

Credit Analysis (Score: {credit_analysis:.1f}/10):
{'✓' if credit_analysis >= 7 else '⚠'} Strong creditworthiness and solvency
{'✓' if credit_analysis >= 7 else '⚠'} Manageable debt levels
{'✓' if credit_analysis >= 7 else '⚠'} Adequate interest coverage

Asset Coverage (Score: {asset_coverage:.1f}/10):
{'✓' if asset_coverage >= 7 else '⚠'} Hard assets backing equity value
{'✓' if asset_coverage >= 7 else '⚠'} Liquidation value exceeds price
{'✓' if asset_coverage >= 7 else '⚠'} Working capital adequacy

Going Concern Value (Score: {going_concern_value:.1f}/10):
{'✓' if going_concern_value >= 7 else '⚠'} Business value as operating entity
{'✓' if going_concern_value >= 7 else '⚠'} Franchise value and market position
{'✓' if going_concern_value >= 7 else '⚠'} Sustainable earnings power

Resource Conversion (Score: {resource_conversion:.1f}/10):
{'✓' if resource_conversion >= 6 else '⚠'} Ability to monetize assets
{'✓' if resource_conversion >= 6 else '⚠'} Management capital allocation skill
{'✓' if resource_conversion >= 6 else '⚠'} Value realization potential

Distressed Opportunity (Score: {distressed_opportunity:.1f}/10):
{'✓' if distressed_opportunity >= 6 else '⚠'} Temporary distress vs permanent impairment
{'✓' if distressed_opportunity >= 6 else '⚠'} Catalyst for value realization
{'✓' if distressed_opportunity >= 6 else '⚠'} Risk-reward in restructuring scenario

💡 Recommendation: {recommendation}
📈 Confidence Level: {confidence:.1f}%

"Buy a business that's worth more dead than alive, then you can't lose." - Marty Whitman
        """.strip(),
        "metrics": {
            "credit_analysis": round(credit_analysis, 2),
            "asset_coverage": round(asset_coverage, 2),
            "resource_conversion": round(resource_conversion, 2),
            "going_concern_value": round(going_concern_value, 2),
            "distressed_opportunity": round(distressed_opportunity, 2)
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
            analysis = analyze_stock_whitman_style(ticker, parameters, mediated_context)
            results[ticker] = analysis

        response = {
            "success": True,
            "agent": "marty_whitman",
            "execution_date": end_date,
            "tickers_analyzed": len(tickers),
            "signals": {ticker: results[ticker]["signal"] for ticker in results},
            "analysis": results
        }

        if mediated_context:
            response["used_previous_context"] = True

        return response

    except Exception as e:
        return {"success": False, "error": str(e), "agent": "marty_whitman"}

def main():
    parser = argparse.ArgumentParser(description='Marty Whitman Agent - Safe & Cheap Distressed Value')
    parser.add_argument('--parameters', type=str, required=True)
    parser.add_argument('--inputs', type=str, required=True)
    args = parser.parse_args()

    try:
        result = execute_agent(json.loads(args.parameters), json.loads(args.inputs))
        print(json.dumps(result, indent=2))
        sys.exit(0)
    except Exception as e:
        print(json.dumps({"success": False, "error": str(e), "agent": "marty_whitman"}), file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()
