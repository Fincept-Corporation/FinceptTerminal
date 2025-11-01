#!/usr/bin/env python3
"""
Capitalism Economic Agent - CLI Version
For Node Editor integration
"""

import sys
import json
import argparse
from typing import Dict, Any
from capitalism_agent import CapitalismAgent
from base_agent import EconomicData
from datetime import datetime

def execute_agent(parameters: Dict[str, Any], inputs: Dict[str, Any]) -> Dict[str, Any]:
    """
    Execute Capitalism economic analysis

    Args:
        parameters: {
            "gdp": 3.5,
            "inflation": 2.5,
            "unemployment": 4.5,
            "interest_rate": 5.0,
            "trade_balance": -2.0,
            "government_spending": 35.0,
            "tax_rate": 30.0,
            "private_investment": 25.0,
            "consumer_confidence": 70.0
        }
        inputs: Data from connected nodes (if any)

    Returns:
        Economic analysis from capitalist perspective
    """
    try:
        agent = CapitalismAgent()

        # Extract parameters with defaults
        gdp = float(parameters.get("gdp", 3.0))
        inflation = float(parameters.get("inflation", 2.5))
        unemployment = float(parameters.get("unemployment", 5.0))
        interest_rate = float(parameters.get("interest_rate", 5.0))
        trade_balance = float(parameters.get("trade_balance", 0.0))
        government_spending = float(parameters.get("government_spending", 35.0))
        tax_rate = float(parameters.get("tax_rate", 30.0))
        private_investment = float(parameters.get("private_investment", 20.0))
        consumer_confidence = float(parameters.get("consumer_confidence", 65.0))

        # Build EconomicData
        economic_data = EconomicData(
            gdp=gdp,
            inflation=inflation,
            unemployment=unemployment,
            interest_rate=interest_rate,
            trade_balance=trade_balance,
            government_spending=government_spending,
            tax_rate=tax_rate,
            private_investment=private_investment,
            consumer_confidence=consumer_confidence,
            timestamp=datetime.now()
        )

        # Perform analysis
        analysis = agent.analyze_economy(economic_data)
        recommendations = agent.get_policy_recommendations(economic_data, analysis)

        # Format recommendations
        formatted_recs = []
        for rec in recommendations:
            formatted_recs.append({
                "title": rec.title,
                "description": rec.description,
                "priority": rec.priority,
                "expected_impact": rec.expected_impact,
                "implementation_time": rec.implementation_time,
                "risk_level": rec.risk_level
            })

        # Build comprehensive response
        response = {
            "success": True,
            "agent": "capitalism",
            "ideology": "Free Market Capitalism",
            "analysis": analysis,
            "policy_recommendations": formatted_recs,
            "input_data": {
                "gdp_growth": gdp,
                "inflation_rate": inflation,
                "unemployment_rate": unemployment,
                "interest_rate": interest_rate,
                "trade_balance": trade_balance,
                "government_spending": government_spending,
                "tax_rate": tax_rate,
                "private_investment": private_investment,
                "consumer_confidence": consumer_confidence
            }
        }

        # Include input context if available
        if inputs and isinstance(inputs, dict):
            mediated_context = inputs.get("mediated_analysis")
            if mediated_context:
                response["used_previous_context"] = True
                response["previous_context"] = mediated_context[:200] + "..."

        return response

    except Exception as e:
        return {
            "success": False,
            "error": str(e),
            "agent": "capitalism"
        }

def main():
    """CLI entry point"""
    parser = argparse.ArgumentParser(description='Capitalism Economic Agent')
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
            "agent": "capitalism"
        }
        print(json.dumps(error_result), file=sys.stderr)
        sys.exit(1)

    except Exception as e:
        error_result = {
            "success": False,
            "error": f"Agent execution error: {str(e)}",
            "agent": "capitalism"
        }
        print(json.dumps(error_result), file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()
