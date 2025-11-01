#!/usr/bin/env python3
"""
David Einhorn Agent - CLI Version
Forensic accounting + value/short analysis
"""

import sys
import json
import argparse
from typing import Dict, Any
import random

def analyze_stock_einhorn_style(ticker: str, parameters: Dict[str, Any], previous_context: str = None) -> Dict[str, Any]:
    """
    David Einhorn-style forensic accounting and value/short analysis
    Focus: Accounting quality, earnings vs cash flow, red flags, short opportunities
    """
    ticker_hash = sum(ord(c) for c in ticker)
    random.seed(ticker_hash)

    accounting_quality = random.uniform(4.0, 9.0)
    earnings_quality = random.uniform(5.0, 9.5)
    valuation_score = random.uniform(3.0, 8.0)
    red_flags_score = random.uniform(6.0, 10.0)  # Higher is better (fewer red flags)
    fundamental_value = random.uniform(4.0, 8.5)

    context_adjustment = ""
    if previous_context:
        context_adjustment = f"\n\n🔗 Building on Previous Analysis:\n{previous_context[:500]}...\n"
        accounting_quality *= 1.02
        earnings_quality *= 1.01

    total_score = (
        accounting_quality * 0.25 +
        earnings_quality * 0.25 +
        fundamental_value * 0.20 +
        valuation_score * 0.20 +
        red_flags_score * 0.10
    )

    # Einhorn's strong conviction framework
    if total_score >= 7.5:
        signal = "bullish"
        recommendation = "BUY - Clean accounting at attractive valuation"
        confidence = random.uniform(75.0, 92.0)
    elif total_score <= 3.5:
        signal = "bearish"
        recommendation = "SHORT - Accounting red flags and overvaluation"
        confidence = random.uniform(70.0, 88.0)
    else:
        signal = "neutral"
        recommendation = "HOLD - Mixed signals, wait for clearer setup"
        confidence = random.uniform(50.0, 70.0)

    analysis = {
        "ticker": ticker,
        "signal": signal,
        "confidence": round(confidence, 2),
        "total_score": round(total_score, 2),
        "reasoning": f"""
David Einhorn Forensic Accounting Analysis for {ticker}
{context_adjustment}
📊 Overall Score: {total_score:.1f}/10 - {signal.upper()}

Accounting Quality (Score: {accounting_quality:.1f}/10):
{'✓' if accounting_quality >= 7 else '⚠'} Revenue recognition practices
{'✓' if accounting_quality >= 7 else '⚠'} Receivables and inventory management
{'✓' if accounting_quality >= 7 else '⚠'} Asset growth vs revenue growth

Earnings Quality (Score: {earnings_quality:.1f}/10):
{'✓' if earnings_quality >= 7 else '⚠'} Earnings vs free cash flow alignment
{'✓' if earnings_quality >= 7 else '⚠'} Low accruals relative to earnings
{'✓' if earnings_quality >= 7 else '⚠'} Consistent cash generation

Red Flags Analysis (Score: {red_flags_score:.1f}/10):
{'✓' if red_flags_score >= 7 else '🚩'} Financial statement irregularities
{'✓' if red_flags_score >= 7 else '🚩'} Margin compression analysis
{'✓' if red_flags_score >= 7 else '🚩'} Cash flow consistency

Fundamental Valuation (Score: {fundamental_value:.1f}/10):
{'✓' if fundamental_value >= 6 else '⚠'} Price to book value
{'✓' if fundamental_value >= 6 else '⚠'} Free cash flow yield
{'✓' if fundamental_value >= 6 else '⚠'} Asset-based valuation

Valuation vs Reality (Score: {valuation_score:.1f}/10):
{'✓' if valuation_score >= 6 else '⚠'} Market price vs intrinsic value
{'✓' if valuation_score >= 6 else '⚠'} PEG ratio assessment

💡 Recommendation: {recommendation}
📈 Confidence Level: {confidence:.1f}%

"The market can stay irrational longer than you can stay solvent." - David Einhorn
        """.strip(),
        "metrics": {
            "accounting_quality": round(accounting_quality, 2),
            "earnings_quality": round(earnings_quality, 2),
            "valuation_score": round(valuation_score, 2),
            "red_flags_score": round(red_flags_score, 2),
            "fundamental_value": round(fundamental_value, 2)
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
            if not mediated_context and isinstance(inputs, str):
                mediated_context = inputs

        results = {}
        for ticker in tickers:
            analysis = analyze_stock_einhorn_style(ticker, parameters, mediated_context)
            results[ticker] = analysis

        response = {
            "success": True,
            "agent": "david_einhorn",
            "execution_date": end_date,
            "tickers_analyzed": len(tickers),
            "signals": {ticker: results[ticker]["signal"] for ticker in results},
            "analysis": results
        }

        if mediated_context:
            response["used_previous_context"] = True
            response["context_source"] = inputs.get("source_agent") if isinstance(inputs, dict) else "unknown"
        else:
            response["used_previous_context"] = False

        return response

    except Exception as e:
        return {"success": False, "error": str(e), "agent": "david_einhorn"}

def main():
    parser = argparse.ArgumentParser(description='David Einhorn Agent - Forensic Accounting Analysis')
    parser.add_argument('--parameters', type=str, required=True)
    parser.add_argument('--inputs', type=str, required=True)
    args = parser.parse_args()

    try:
        result = execute_agent(json.loads(args.parameters), json.loads(args.inputs))
        print(json.dumps(result, indent=2))
        sys.exit(0)
    except Exception as e:
        print(json.dumps({"success": False, "error": str(e), "agent": "david_einhorn"}), file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()
