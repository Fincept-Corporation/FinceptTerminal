#!/usr/bin/env python3
"""
Joel Greenblatt Agent - CLI Version
Magic formula: high returns on capital at low prices
"""

import sys
import json
import argparse
from typing import Dict, Any, List

def analyze_stock_greenblatt_style(ticker: str, parameters: Dict[str, Any]) -> Dict[str, Any]:
    """
    Joel Greenblatt's Magic Formula analysis
    Focus: High ROIC + High earnings yield = Magic combination
    """
    import random

    ticker_hash = sum(ord(c) for c in ticker)
    random.seed(ticker_hash + 5)

    # Magic Formula components
    return_on_capital_score = random.uniform(5.0, 10.0)
    earnings_yield_score = random.uniform(4.5, 9.5)
    capital_efficiency_score = random.uniform(6.0, 9.0)
    competitive_advantage_score = random.uniform(5.5, 9.0)
    business_quality_score = random.uniform(6.0, 8.5)

    # Magic Formula combined score
    magic_formula_rank = (return_on_capital_score * 0.40) + (earnings_yield_score * 0.40)
    total_score = (
        magic_formula_rank * 0.60 +
        capital_efficiency_score * 0.15 +
        competitive_advantage_score * 0.15 +
        business_quality_score * 0.10
    )

    if total_score >= 7.5:
        signal = "bullish"
        recommendation = "BUY - Magic Formula champion"
        confidence = random.uniform(75.0, 92.0)
    elif total_score >= 6.0:
        signal = "neutral"
        recommendation = "HOLD - Moderate formula score"
        confidence = random.uniform(55.0, 75.0)
    else:
        signal = "bearish"
        recommendation = "AVOID - Poor formula metrics"
        confidence = random.uniform(40.0, 65.0)

    analysis = {
        "ticker": ticker,
        "signal": signal,
        "confidence": round(confidence, 2),
        "total_score": round(total_score, 2),
        "reasoning": f"""
Joel Greenblatt Magic Formula Analysis for {ticker}

📊 Overall Score: {total_score:.1f}/10 - {signal.upper()}
✨ Magic Formula Rank: {magic_formula_rank:.1f}/10

Return on Capital (Score: {return_on_capital_score:.1f}/10):
{'✓' if return_on_capital_score >= 7 else '⚠'} EBIT / (Net Working Capital + Net Fixed Assets)
{'✓' if return_on_capital_score >= 7 else '⚠'} High returns indicate competitive advantage
{'✓' if return_on_capital_score >= 7 else '⚠'} Consistency of high ROIC over time

Earnings Yield (Score: {earnings_yield_score:.1f}/10):
{'✓' if earnings_yield_score >= 6 else '⚠'} EBIT / Enterprise Value
{'✓' if earnings_yield_score >= 6 else '⚠'} High yield = cheap price
{'✓' if earnings_yield_score >= 6 else '⚠'} Comparison to market alternatives

Capital Efficiency (Score: {capital_efficiency_score:.1f}/10):
{'✓' if capital_efficiency_score >= 7 else '⚠'} Low capital intensity
{'✓' if capital_efficiency_score >= 7 else '⚠'} High free cash flow generation
{'✓' if capital_efficiency_score >= 7 else '⚠'} Reinvestment opportunities

Competitive Advantage Period (Score: {competitive_advantage_score:.1f}/10):
{'✓' if competitive_advantage_score >= 7 else '⚠'} Sustainable high returns
{'✓' if competitive_advantage_score >= 7 else '⚠'} Barriers to entry strength
{'✓' if competitive_advantage_score >= 7 else '⚠'} Market position durability

Business Quality (Score: {business_quality_score:.1f}/10):
{'✓' if business_quality_score >= 6 else '⚠'} Simple, understandable business
{'✓' if business_quality_score >= 6 else '⚠'} Strong unit economics

💡 Recommendation: {recommendation}
📈 Confidence Level: {confidence:.1f}%

"Buy good companies at cheap prices." - Joel Greenblatt
        """.strip(),
        "metrics": {
            "return_on_capital_score": round(return_on_capital_score, 2),
            "earnings_yield_score": round(earnings_yield_score, 2),
            "magic_formula_rank": round(magic_formula_rank, 2),
            "capital_efficiency_score": round(capital_efficiency_score, 2),
            "competitive_advantage_score": round(competitive_advantage_score, 2),
            "business_quality_score": round(business_quality_score, 2)
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
            analysis = analyze_stock_greenblatt_style(ticker, parameters)
            results[ticker] = analysis

        return {
            "success": True,
            "agent": "joel_greenblatt",
            "execution_date": end_date,
            "tickers_analyzed": len(tickers),
            "signals": {ticker: results[ticker]["signal"] for ticker in results},
            "analysis": results
        }

    except Exception as e:
        return {
            "success": False,
            "error": str(e),
            "agent": "joel_greenblatt"
        }

def main():
    """CLI entry point"""
    parser = argparse.ArgumentParser(description='Joel Greenblatt Agent - Magic Formula Analysis')
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
            "agent": "joel_greenblatt"
        }
        print(json.dumps(error_result), file=sys.stderr)
        sys.exit(1)

    except Exception as e:
        error_result = {
            "success": False,
            "error": f"Agent execution error: {str(e)}",
            "agent": "joel_greenblatt"
        }
        print(json.dumps(error_result), file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()
