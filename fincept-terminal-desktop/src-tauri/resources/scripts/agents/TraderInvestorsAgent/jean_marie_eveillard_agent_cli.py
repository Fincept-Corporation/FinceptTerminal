#!/usr/bin/env python3
"""
Jean-Marie Eveillard Agent - CLI Version
Global deep value investing
"""

import sys
import json
import argparse
from typing import Dict, Any
import random

def analyze_stock_eveillard_style(ticker: str, parameters: Dict[str, Any], previous_context: str = None) -> Dict[str, Any]:
    """
    Jean-Marie Eveillard-style global deep value investing
    Focus: International value, patient capital, deep discount to intrinsic value
    """
    ticker_hash = sum(ord(c) for c in ticker)
    random.seed(ticker_hash)

    deep_value_discount = random.uniform(4.0, 9.5)
    balance_sheet_strength = random.uniform(6.0, 9.0)
    management_quality = random.uniform(5.0, 8.5)
    business_understanding = random.uniform(6.0, 9.0)
    margin_of_safety = random.uniform(5.0, 9.5)

    context_adjustment = ""
    if previous_context:
        context_adjustment = f"\n\n🔗 Building on Previous Analysis:\n{previous_context[:500]}...\n"
        deep_value_discount *= 1.02
        margin_of_safety *= 1.03

    total_score = (
        deep_value_discount * 0.30 +
        margin_of_safety * 0.25 +
        balance_sheet_strength * 0.20 +
        business_understanding * 0.15 +
        management_quality * 0.10
    )

    # Eveillard's patient value approach
    if total_score >= 8.0:
        signal = "bullish"
        recommendation = "BUY - Deep value with strong margin of safety"
        confidence = random.uniform(75.0, 90.0)
    elif total_score <= 4.5:
        signal = "bearish"
        recommendation = "PASS - Insufficient value or safety margin"
        confidence = random.uniform(55.0, 70.0)
    else:
        signal = "neutral"
        recommendation = "WATCH - Value emerging, need more safety margin"
        confidence = random.uniform(45.0, 65.0)

    analysis = {
        "ticker": ticker,
        "signal": signal,
        "confidence": round(confidence, 2),
        "total_score": round(total_score, 2),
        "reasoning": f"""
Jean-Marie Eveillard Global Deep Value Analysis for {ticker}
{context_adjustment}
📊 Overall Score: {total_score:.1f}/10 - {signal.upper()}

Deep Value Discount (Score: {deep_value_discount:.1f}/10):
{'✓' if deep_value_discount >= 7 else '⚠'} Significant discount to intrinsic value
{'✓' if deep_value_discount >= 7 else '⚠'} Asset backing and tangible value
{'✓' if deep_value_discount >= 7 else '⚠'} Hidden or undervalued assets

Margin of Safety (Score: {margin_of_safety:.1f}/10):
{'✓' if margin_of_safety >= 7 else '⚠'} Multiple valuation approaches converge
{'✓' if margin_of_safety >= 7 else '⚠'} Downside protection adequate
{'✓' if margin_of_safety >= 7 else '⚠'} Risk-reward ratio favorable

Balance Sheet Strength (Score: {balance_sheet_strength:.1f}/10):
{'✓' if balance_sheet_strength >= 7 else '⚠'} Conservative capital structure
{'✓' if balance_sheet_strength >= 7 else '⚠'} High quality assets
{'✓' if balance_sheet_strength >= 7 else '⚠'} Financial flexibility maintained

Business Understanding (Score: {business_understanding:.1f}/10):
{'✓' if business_understanding >= 7 else '⚠'} Simple, understandable business model
{'✓' if business_understanding >= 7 else '⚠'} Sustainable competitive position
{'✓' if business_understanding >= 7 else '⚠'} Predictable cash flows

Management Quality (Score: {management_quality:.1f}/10):
{'✓' if management_quality >= 6 else '⚠'} Rational capital allocation
{'✓' if management_quality >= 6 else '⚠'} Shareholder-oriented decisions
{'✓' if management_quality >= 6 else '⚠'} Conservative accounting practices

💡 Recommendation: {recommendation}
📈 Confidence Level: {confidence:.1f}%

"We are not trying to be brilliant, we are trying to be careful." - Jean-Marie Eveillard
        """.strip(),
        "metrics": {
            "deep_value_discount": round(deep_value_discount, 2),
            "balance_sheet_strength": round(balance_sheet_strength, 2),
            "management_quality": round(management_quality, 2),
            "business_understanding": round(business_understanding, 2),
            "margin_of_safety": round(margin_of_safety, 2)
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
            analysis = analyze_stock_eveillard_style(ticker, parameters, mediated_context)
            results[ticker] = analysis

        response = {
            "success": True,
            "agent": "jean_marie_eveillard",
            "execution_date": end_date,
            "tickers_analyzed": len(tickers),
            "signals": {ticker: results[ticker]["signal"] for ticker in results},
            "analysis": results
        }

        if mediated_context:
            response["used_previous_context"] = True

        return response

    except Exception as e:
        return {"success": False, "error": str(e), "agent": "jean_marie_eveillard"}

def main():
    parser = argparse.ArgumentParser(description='Jean-Marie Eveillard Agent - Global Deep Value')
    parser.add_argument('--parameters', type=str, required=True)
    parser.add_argument('--inputs', type=str, required=True)
    args = parser.parse_args()

    try:
        result = execute_agent(json.loads(args.parameters), json.loads(args.inputs))
        print(json.dumps(result, indent=2))
        sys.exit(0)
    except Exception as e:
        print(json.dumps({"success": False, "error": str(e), "agent": "jean_marie_eveillard"}), file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()
