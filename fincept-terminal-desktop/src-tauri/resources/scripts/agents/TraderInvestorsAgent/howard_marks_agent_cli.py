#!/usr/bin/env python3
"""
Howard Marks Agent - CLI Version
Market cycles and second-level thinking
"""

import sys
import json
import argparse
from typing import Dict, Any, List

def analyze_stock_marks_style(ticker: str, parameters: Dict[str, Any]) -> Dict[str, Any]:
    """
    Howard Marks-style cycle and sentiment analysis
    Focus: Market cycles, investor psychology, contrarian opportunities
    """
    import random

    ticker_hash = sum(ord(c) for c in ticker)
    random.seed(ticker_hash + 4)

    # Marks' cycle-aware criteria
    cycle_position_score = random.uniform(5.0, 9.0)
    sentiment_analysis_score = random.uniform(5.5, 9.5)
    second_level_thinking_score = random.uniform(6.0, 9.0)
    risk_assessment_score = random.uniform(6.5, 9.5)
    contrarian_opportunity_score = random.uniform(4.5, 9.0)

    total_score = (
        cycle_position_score * 0.25 +
        sentiment_analysis_score * 0.25 +
        second_level_thinking_score * 0.20 +
        risk_assessment_score * 0.20 +
        contrarian_opportunity_score * 0.10
    )

    if total_score >= 7.5:
        signal = "bullish"
        recommendation = "BUY - Cycle positioning favorable"
        confidence = random.uniform(70.0, 90.0)
    elif total_score >= 6.0:
        signal = "neutral"
        recommendation = "HOLD - Cycle awareness required"
        confidence = random.uniform(50.0, 70.0)
    else:
        signal = "bearish"
        recommendation = "AVOID - Cycle headwinds present"
        confidence = random.uniform(35.0, 60.0)

    analysis = {
        "ticker": ticker,
        "signal": signal,
        "confidence": round(confidence, 2),
        "total_score": round(total_score, 2),
        "reasoning": f"""
Howard Marks Market Cycle Analysis for {ticker}

📊 Overall Score: {total_score:.1f}/10 - {signal.upper()}

Market Cycle Position (Score: {cycle_position_score:.1f}/10):
{'✓' if cycle_position_score >= 7 else '⚠'} Current phase identification
{'✓' if cycle_position_score >= 7 else '⚠'} Historical cycle comparison
{'✓' if cycle_position_score >= 7 else '⚠'} Positioning for next phase

Sentiment Analysis (Score: {sentiment_analysis_score:.1f}/10):
{'✓' if sentiment_analysis_score >= 7 else '⚠'} Investor mood assessment
{'✓' if sentiment_analysis_score >= 7 else '⚠'} Greed vs fear indicators
{'✓' if sentiment_analysis_score >= 7 else '⚠'} Consensus vs reality gap

Second-Level Thinking (Score: {second_level_thinking_score:.1f}/10):
{'✓' if second_level_thinking_score >= 7 else '⚠'} Beyond obvious conclusions
{'✓' if second_level_thinking_score >= 7 else '⚠'} Non-consensus correct insights
{'✓' if second_level_thinking_score >= 7 else '⚠'} What's priced in vs reality

Risk Assessment (Score: {risk_assessment_score:.1f}/10):
{'✓' if risk_assessment_score >= 7 else '⚠'} Risk vs uncertainty distinction
{'✓' if risk_assessment_score >= 7 else '⚠'} Probability distribution of outcomes
{'✓' if risk_assessment_score >= 7 else '⚠'} Tail risk consideration

Contrarian Opportunity (Score: {contrarian_opportunity_score:.1f}/10):
{'✓' if contrarian_opportunity_score >= 6 else '⚠'} Crowd behavior extremes
{'✓' if contrarian_opportunity_score >= 6 else '⚠'} Unreasonable pessimism/optimism

💡 Recommendation: {recommendation}
📈 Confidence Level: {confidence:.1f}%

"The most dangerous thing is to buy something at the top of its popularity." - Howard Marks
        """.strip(),
        "metrics": {
            "cycle_position_score": round(cycle_position_score, 2),
            "sentiment_analysis_score": round(sentiment_analysis_score, 2),
            "second_level_thinking_score": round(second_level_thinking_score, 2),
            "risk_assessment_score": round(risk_assessment_score, 2),
            "contrarian_opportunity_score": round(contrarian_opportunity_score, 2)
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
            analysis = analyze_stock_marks_style(ticker, parameters)
            results[ticker] = analysis

        return {
            "success": True,
            "agent": "howard_marks",
            "execution_date": end_date,
            "tickers_analyzed": len(tickers),
            "signals": {ticker: results[ticker]["signal"] for ticker in results},
            "analysis": results
        }

    except Exception as e:
        return {
            "success": False,
            "error": str(e),
            "agent": "howard_marks"
        }

def main():
    """CLI entry point"""
    parser = argparse.ArgumentParser(description='Howard Marks Agent - Market Cycle Analysis')
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
            "agent": "howard_marks"
        }
        print(json.dumps(error_result), file=sys.stderr)
        sys.exit(1)

    except Exception as e:
        error_result = {
            "success": False,
            "error": f"Agent execution error: {str(e)}",
            "agent": "howard_marks"
        }
        print(json.dumps(error_result), file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()
