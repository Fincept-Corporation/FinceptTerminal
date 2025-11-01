#!/usr/bin/env python3
"""
Mixed Economy Agent - CLI Version
For Node Editor integration
"""

import sys
import json
import argparse
from typing import Dict, Any
from mixed_economy_agent import MixedEconomyAgent
from base_agent import EconomicData
from datetime import datetime

def execute_agent(parameters: Dict[str, Any], inputs: Dict[str, Any]) -> Dict[str, Any]:
    """Execute Mixed Economy analysis"""
    try:
        agent = MixedEconomyAgent()

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

        economic_data = EconomicData(
            gdp=gdp, inflation=inflation, unemployment=unemployment,
            interest_rate=interest_rate, trade_balance=trade_balance,
            government_spending=government_spending, tax_rate=tax_rate,
            private_investment=private_investment,
            consumer_confidence=consumer_confidence,
            timestamp=datetime.now()
        )

        analysis = agent.analyze_economy(economic_data)
        recommendations = agent.get_policy_recommendations(economic_data, analysis)

        formatted_recs = [{
            "title": rec.title, "description": rec.description,
            "priority": rec.priority, "expected_impact": rec.expected_impact,
            "implementation_time": rec.implementation_time, "risk_level": rec.risk_level
        } for rec in recommendations]

        response = {
            "success": True,
            "agent": "mixed_economy",
            "ideology": "Mixed Economy",
            "analysis": analysis,
            "policy_recommendations": formatted_recs,
            "input_data": {
                "gdp_growth": gdp, "inflation_rate": inflation,
                "unemployment_rate": unemployment, "interest_rate": interest_rate,
                "trade_balance": trade_balance, "government_spending": government_spending,
                "tax_rate": tax_rate, "private_investment": private_investment,
                "consumer_confidence": consumer_confidence
            }
        }

        if inputs and isinstance(inputs, dict):
            mediated_context = inputs.get("mediated_analysis")
            if mediated_context:
                response["used_previous_context"] = True
                response["previous_context"] = mediated_context[:200] + "..."

        return response

    except Exception as e:
        return {"success": False, "error": str(e), "agent": "mixed_economy"}

def main():
    parser = argparse.ArgumentParser(description='Mixed Economy Agent')
    parser.add_argument('--parameters', type=str, required=True)
    parser.add_argument('--inputs', type=str, required=True)
    args = parser.parse_args()

    try:
        result = execute_agent(json.loads(args.parameters), json.loads(args.inputs))
        print(json.dumps(result, indent=2))
        sys.exit(0)
    except Exception as e:
        print(json.dumps({"success": False, "error": str(e), "agent": "mixed_economy"}), file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()
