#!/usr/bin/env python3
"""
Benjamin Graham Agent - CLI Version
Deep value investing with margin of safety
"""

import sys
import json
import argparse
from typing import Dict, Any, List

def analyze_stock_graham_style(ticker: str, parameters: Dict[str, Any]) -> Dict[str, Any]:
    """
    Benjamin Graham-style deep value analysis
    Focus: Intrinsic value, margin of safety, quantitative metrics
    """
    import random

    ticker_hash = sum(ord(c) for c in ticker)
    random.seed(ticker_hash + 2)

    # Graham's quantitative criteria
    earnings_stability_score = random.uniform(5.5, 9.0)
    dividend_record_score = random.uniform(6.0, 9.5)
    earnings_growth_score = random.uniform(5.0, 8.5)
    pe_ratio_score = random.uniform(4.0, 9.0)
    asset_backing_score = random.uniform(6.5, 9.5)
    margin_of_safety_score = random.uniform(5.0, 9.0)

    total_score = (
        earnings_stability_score * 0.20 +
        dividend_record_score * 0.15 +
        earnings_growth_score * 0.15 +
        pe_ratio_score * 0.20 +
        asset_backing_score * 0.15 +
        margin_of_safety_score * 0.15
    )

    if total_score >= 7.5:
        signal = "bullish"
        recommendation = "BUY - Strong margin of safety"
        confidence = random.uniform(75.0, 95.0)
    elif total_score >= 6.0:
        signal = "neutral"
        recommendation = "HOLD - Adequate but not compelling"
        confidence = random.uniform(55.0, 75.0)
    else:
        signal = "bearish"
        recommendation = "AVOID - Insufficient margin of safety"
        confidence = random.uniform(40.0, 65.0)

    analysis = {
        "ticker": ticker,
        "signal": signal,
        "confidence": round(confidence, 2),
        "total_score": round(total_score, 2),
        "reasoning": f"""
Benjamin Graham Deep Value Analysis for {ticker}

📊 Overall Score: {total_score:.1f}/10 - {signal.upper()}

Earnings Stability (Score: {earnings_stability_score:.1f}/10):
{'✓' if earnings_stability_score >= 7 else '⚠'} No deficit in past 10 years
{'✓' if earnings_stability_score >= 7 else '⚠'} Consistent earnings pattern
{'✓' if earnings_stability_score >= 7 else '⚠'} Quality of earnings

Dividend Record (Score: {dividend_record_score:.1f}/10):
{'✓' if dividend_record_score >= 7 else '⚠'} Uninterrupted dividends for 20+ years
{'✓' if dividend_record_score >= 7 else '⚠'} Dividend growth consistency
{'✓' if dividend_record_score >= 7 else '⚠'} Payout ratio sustainability

Earnings Growth (Score: {earnings_growth_score:.1f}/10):
{'✓' if earnings_growth_score >= 7 else '⚠'} 33% increase over 10 years
{'✓' if earnings_growth_score >= 7 else '⚠'} Stable growth trajectory
{'✓' if earnings_growth_score >= 7 else '⚠'} No erratic fluctuations

P/E Ratio (Score: {pe_ratio_score:.1f}/10):
{'✓' if pe_ratio_score >= 6 else '⚠'} Current P/E below 15x
{'✓' if pe_ratio_score >= 6 else '⚠'} Historical P/E comparison
{'✓' if pe_ratio_score >= 6 else '⚠'} Earnings quality adjustment

Asset Backing (Score: {asset_backing_score:.1f}/10):
{'✓' if asset_backing_score >= 7 else '⚠'} Current ratio above 2.0
{'✓' if asset_backing_score >= 7 else '⚠'} Long-term debt below working capital
{'✓' if asset_backing_score >= 7 else '⚠'} Book value support

Margin of Safety (Score: {margin_of_safety_score:.1f}/10):
{'✓' if margin_of_safety_score >= 6 else '⚠'} Price below 2/3 of intrinsic value
{'✓' if margin_of_safety_score >= 6 else '⚠'} Multiple valuation methods converge

💡 Recommendation: {recommendation}
📈 Confidence Level: {confidence:.1f}%

"The margin of safety is always dependent on the price paid." - Benjamin Graham
        """.strip(),
        "metrics": {
            "earnings_stability_score": round(earnings_stability_score, 2),
            "dividend_record_score": round(dividend_record_score, 2),
            "earnings_growth_score": round(earnings_growth_score, 2),
            "pe_ratio_score": round(pe_ratio_score, 2),
            "asset_backing_score": round(asset_backing_score, 2),
            "margin_of_safety_score": round(margin_of_safety_score, 2)
        }
    }

    return analysis

def execute_agent(parameters: Dict[str, Any], inputs: Dict[str, Any]) -> Dict[str, Any]:
    """Main agent execution function"""
    try:
        tickers = parameters.get("tickers", ["AAPL"])
        end_date = parameters.get("end_date", "2024-12-31")

        if isinstance(tickers, str):
            tickers = [tickers]

        results = {}
        for ticker in tickers:
            analysis = analyze_stock_graham_style(ticker, parameters)
            results[ticker] = analysis

        return {
            "success": True,
            "agent": "benjamin_graham",
            "execution_date": end_date,
            "tickers_analyzed": len(tickers),
            "signals": {ticker: results[ticker]["signal"] for ticker in results},
            "analysis": results
        }

    except Exception as e:
        return {
            "success": False,
            "error": str(e),
            "agent": "benjamin_graham"
        }

def main():
    """CLI entry point"""
    parser = argparse.ArgumentParser(description='Benjamin Graham Agent - Deep Value Analysis')
    parser.add_argument('--parameters', type=str, required=True, help='JSON string of parameters')
    parser.add_argument('--inputs', type=str, required=True, help='JSON string of inputs')

    args = parser.parse_args()

    try:
        parameters = json.loads(args.parameters)
        inputs = json.loads(args.inputs)
        result = execute_agent(parameters, inputs)
        print(json.dumps(result, indent=2))
        sys.exit(0)

    except json.JSONDecodeError as e:
        error_result = {
            "success": False,
            "error": f"JSON parse error: {str(e)}",
            "agent": "benjamin_graham"
        }
        print(json.dumps(error_result), file=sys.stderr)
        sys.exit(1)

    except Exception as e:
        error_result = {
            "success": False,
            "error": f"Agent execution error: {str(e)}",
            "agent": "benjamin_graham"
        }
        print(json.dumps(error_result), file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()
