#!/usr/bin/env python3
"""
Warren Buffett Agent - CLI Version
Simplified for Node Editor integration
"""

import sys
import json
import argparse
from typing import Dict, Any, List

def analyze_stock_buffett_style(ticker: str, parameters: Dict[str, Any], previous_context: str = None) -> Dict[str, Any]:
    """
    Warren Buffett-style value investing analysis
    Focuses on: Economic moats, predictable earnings, financial strength, management quality
    Can incorporate insights from previous agents if provided
    """
    import random

    # Simulate varying analysis based on ticker (for demo purposes)
    # In production, replace with actual API calls to financial data providers
    ticker_hash = sum(ord(c) for c in ticker)
    random.seed(ticker_hash)

    moat_score = random.uniform(5.0, 9.5)
    earnings_score = random.uniform(6.0, 9.0)
    financial_score = random.uniform(7.0, 10.0)
    management_score = random.uniform(5.5, 8.5)
    valuation_score = random.uniform(4.0, 8.0)

    # Adjust scores if we have previous context
    context_adjustment = ""
    if previous_context:
        context_adjustment = f"\n\n🔗 Building on Previous Analysis:\n{previous_context[:500]}...\n"
        # Slightly improve scores when informed by other agents
        moat_score *= 1.03
        financial_score *= 1.02

    total_score = (
        moat_score * 0.30 +
        earnings_score * 0.25 +
        financial_score * 0.20 +
        management_score * 0.15 +
        valuation_score * 0.10
    )

    # Determine signal based on Buffett's criteria
    if total_score >= 8.0:
        signal = "bullish"
        recommendation = "BUY - Wonderful business at a fair price"
        confidence = random.uniform(75.0, 95.0)
    elif total_score >= 6.5:
        signal = "neutral"
        recommendation = "HOLD - Good business, but wait for better price"
        confidence = random.uniform(55.0, 75.0)
    else:
        signal = "bearish"
        recommendation = "PASS - Does not meet quality criteria"
        confidence = random.uniform(40.0, 65.0)

    analysis = {
        "ticker": ticker,
        "signal": signal,
        "confidence": round(confidence, 2),
        "total_score": round(total_score, 2),
        "reasoning": f"""
Warren Buffett Value Investment Analysis for {ticker}
{context_adjustment}
📊 Overall Score: {total_score:.1f}/10 - {signal.upper()}

Economic Moat Analysis (Score: {moat_score:.1f}/10):
{'✓' if moat_score >= 7 else '⚠'} Brand strength and competitive advantages
{'✓' if moat_score >= 7 else '⚠'} Pricing power in the market
{'✓' if moat_score >= 7 else '⚠'} High barriers to entry for competitors

Earnings Predictability (Score: {earnings_score:.1f}/10):
{'✓' if earnings_score >= 7 else '⚠'} Consistent profitability track record
{'✓' if earnings_score >= 7 else '⚠'} Stable and growing earnings trend
{'✓' if earnings_score >= 7 else '⚠'} Low earnings volatility

Financial Fortress (Score: {financial_score:.1f}/10):
{'✓' if financial_score >= 7 else '⚠'} Conservative debt levels
{'✓' if financial_score >= 7 else '⚠'} Strong balance sheet
{'✓' if financial_score >= 7 else '⚠'} Reliable cash flow generation

Management Quality (Score: {management_score:.1f}/10):
{'✓' if management_score >= 7 else '⚠'} Effective capital allocation
{'✓' if management_score >= 7 else '⚠'} Shareholder-friendly policies
{'✓' if management_score >= 7 else '⚠'} Long-term value creation focus

Valuation Assessment (Score: {valuation_score:.1f}/10):
{'✓' if valuation_score >= 6 else '⚠'} Current market price vs intrinsic value
{'✓' if valuation_score >= 6 else '⚠'} Margin of safety consideration

💡 Recommendation: {recommendation}
📈 Confidence Level: {confidence:.1f}%

"Price is what you pay, value is what you get." - Warren Buffett
        """.strip(),
        "metrics": {
            "moat_score": round(moat_score, 2),
            "earnings_score": round(earnings_score, 2),
            "financial_score": round(financial_score, 2),
            "management_score": round(management_score, 2),
            "valuation_score": round(valuation_score, 2)
        }
    }

    return analysis

def execute_agent(parameters: Dict[str, Any], inputs: Dict[str, Any]) -> Dict[str, Any]:
    """
    Main agent execution function

    Args:
        parameters: {
            "tickers": ["AAPL", "GOOGL"],
            "end_date": "2024-12-31"
        }
        inputs: Data from connected nodes (if any)

    Returns:
        {
            "success": true,
            "signals": {...},
            "analysis": {...}
        }
    """
    try:
        tickers = parameters.get("tickers", ["AAPL"])
        end_date = parameters.get("end_date", "2024-12-31")

        # Ensure tickers is a list
        if isinstance(tickers, str):
            tickers = [tickers]

        # Extract mediated context from inputs
        mediated_context = None
        if inputs:
            if isinstance(inputs, dict):
                mediated_context = inputs.get("mediated_analysis")
                if not mediated_context and isinstance(inputs, str):
                    mediated_context = inputs

        results = {}

        for ticker in tickers:
            # Pass mediated context to analysis
            analysis = analyze_stock_buffett_style(ticker, parameters, mediated_context)
            results[ticker] = analysis

        # Build response
        response = {
            "success": True,
            "agent": "warren_buffett",
            "execution_date": end_date,
            "tickers_analyzed": len(tickers),
            "signals": {ticker: results[ticker]["signal"] for ticker in results},
            "analysis": results
        }

        # Include metadata about previous context
        if mediated_context:
            response["used_previous_context"] = True
            response["context_source"] = inputs.get("source_agent") if isinstance(inputs, dict) else "unknown"
        else:
            response["used_previous_context"] = False

        return response

    except Exception as e:
        return {
            "success": False,
            "error": str(e),
            "agent": "warren_buffett"
        }

def main():
    """CLI entry point"""
    parser = argparse.ArgumentParser(description='Warren Buffett Agent - Value Investing Analysis')
    parser.add_argument('--parameters', type=str, required=True, help='JSON string of parameters')
    parser.add_argument('--inputs', type=str, required=True, help='JSON string of inputs')

    args = parser.parse_args()

    try:
        # Parse JSON inputs
        parameters = json.loads(args.parameters)
        inputs = json.loads(args.inputs)

        # Execute agent
        result = execute_agent(parameters, inputs)

        # Output JSON to stdout
        print(json.dumps(result, indent=2))
        sys.exit(0)

    except json.JSONDecodeError as e:
        error_result = {
            "success": False,
            "error": f"JSON parse error: {str(e)}",
            "agent": "warren_buffett"
        }
        print(json.dumps(error_result), file=sys.stderr)
        sys.exit(1)

    except Exception as e:
        error_result = {
            "success": False,
            "error": f"Agent execution error: {str(e)}",
            "agent": "warren_buffett"
        }
        print(json.dumps(error_result), file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()
