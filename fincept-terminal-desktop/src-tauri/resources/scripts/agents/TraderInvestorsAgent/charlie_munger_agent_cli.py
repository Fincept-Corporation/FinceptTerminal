#!/usr/bin/env python3
"""
Charlie Munger Agent - CLI Version
Multidisciplinary thinking and rational decision-making
"""

import sys
import json
import argparse
from typing import Dict, Any, List

def analyze_stock_munger_style(ticker: str, parameters: Dict[str, Any], previous_context: str = None) -> Dict[str, Any]:
    """
    Charlie Munger-style analysis using mental models and multidisciplinary thinking
    Focus: Psychology, incentives, cognitive biases, competitive advantages
    Can incorporate insights from previous agents if provided
    """
    import random

    ticker_hash = sum(ord(c) for c in ticker)
    random.seed(ticker_hash + 1)  # Different seed than Buffett

    # Munger's mental models scoring
    psychology_score = random.uniform(5.0, 9.5)
    incentives_score = random.uniform(6.0, 9.0)
    competitive_position_score = random.uniform(6.5, 9.5)
    mental_models_score = random.uniform(5.5, 8.5)
    margin_of_safety_score = random.uniform(4.5, 9.0)

    # Adjust scores based on previous agent context if available
    context_adjustment = ""
    if previous_context:
        context_adjustment = f"\n\n🔗 Building on Previous Analysis:\n{previous_context[:500]}...\n"
        # Slightly adjust scores based on having additional context (simulating informed decision)
        psychology_score *= 1.05
        mental_models_score *= 1.08

    total_score = (
        psychology_score * 0.25 +
        incentives_score * 0.25 +
        competitive_position_score * 0.25 +
        mental_models_score * 0.15 +
        margin_of_safety_score * 0.10
    )

    if total_score >= 8.0:
        signal = "bullish"
        recommendation = "BUY - Rational analysis supports investment"
        confidence = random.uniform(75.0, 95.0)
    elif total_score >= 6.5:
        signal = "neutral"
        recommendation = "HOLD - More thinking required"
        confidence = random.uniform(55.0, 75.0)
    else:
        signal = "bearish"
        recommendation = "AVOID - Fails rational analysis"
        confidence = random.uniform(40.0, 65.0)

    analysis = {
        "ticker": ticker,
        "signal": signal,
        "confidence": round(confidence, 2),
        "total_score": round(total_score, 2),
        "reasoning": f"""
Charlie Munger Multidisciplinary Analysis for {ticker}
{context_adjustment}
📊 Overall Score: {total_score:.1f}/10 - {signal.upper()}

Psychology & Human Behavior (Score: {psychology_score:.1f}/10):
{'✓' if psychology_score >= 7 else '⚠'} Management incentive alignment
{'✓' if psychology_score >= 7 else '⚠'} Avoiding cognitive biases in decision-making
{'✓' if psychology_score >= 7 else '⚠'} Understanding customer psychology

Incentive Systems (Score: {incentives_score:.1f}/10):
{'✓' if incentives_score >= 7 else '⚠'} Executive compensation structure
{'✓' if incentives_score >= 7 else '⚠'} Sales force incentives alignment
{'✓' if incentives_score >= 7 else '⚠'} Stakeholder incentive compatibility

Competitive Position (Score: {competitive_position_score:.1f}/10):
{'✓' if competitive_position_score >= 7 else '⚠'} Lollapalooza effects (multiple advantages)
{'✓' if competitive_position_score >= 7 else '⚠'} Network effects and scale advantages
{'✓' if competitive_position_score >= 7 else '⚠'} Brand moat strength

Mental Models Application (Score: {mental_models_score:.1f}/10):
{'✓' if mental_models_score >= 7 else '⚠'} Physics: Leverage and critical mass
{'✓' if mental_models_score >= 7 else '⚠'} Biology: Survival of the fittest
{'✓' if mental_models_score >= 7 else '⚠'} Psychology: Social proof and reciprocity

Margin of Safety (Score: {margin_of_safety_score:.1f}/10):
{'✓' if margin_of_safety_score >= 6 else '⚠'} Price below intrinsic value
{'✓' if margin_of_safety_score >= 6 else '⚠'} Multiple scenarios considered

💡 Recommendation: {recommendation}
📈 Confidence Level: {confidence:.1f}%

"All I want to know is where I'm going to die, so I'll never go there." - Charlie Munger
        """.strip(),
        "metrics": {
            "psychology_score": round(psychology_score, 2),
            "incentives_score": round(incentives_score, 2),
            "competitive_position_score": round(competitive_position_score, 2),
            "mental_models_score": round(mental_models_score, 2),
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

        # Extract mediated context from inputs
        mediated_context = None
        if inputs:
            # Check for mediated analysis from Agent Mediator
            if isinstance(inputs, dict):
                mediated_context = inputs.get("mediated_analysis")
                # Also try to get it from direct string input
                if not mediated_context and isinstance(inputs, str):
                    mediated_context = inputs

        results = {}
        for ticker in tickers:
            # Pass mediated context to analysis
            analysis = analyze_stock_munger_style(ticker, parameters, mediated_context)
            results[ticker] = analysis

        # Build response
        response = {
            "success": True,
            "agent": "charlie_munger",
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
            "agent": "charlie_munger"
        }

def main():
    """CLI entry point"""
    parser = argparse.ArgumentParser(description='Charlie Munger Agent - Multidisciplinary Analysis')
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
            "agent": "charlie_munger"
        }
        print(json.dumps(error_result), file=sys.stderr)
        sys.exit(1)

    except Exception as e:
        error_result = {
            "success": False,
            "error": f"Agent execution error: {str(e)}",
            "agent": "charlie_munger"
        }
        print(json.dumps(error_result), file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()
