"""
Example Usage of Economic Agents
Demonstrates how to use different economic ideology agents to analyze the same economic data
"""

from datetime import datetime
from base_agent import EconomicData
from capitalism_agent import CapitalismAgent
from socialism_agent import SocialismAgent
from mixed_economy_agent import MixedEconomyAgent
from keynesian_agent import KeynesianAgent
from neoliberal_agent import NeoliberalAgent
from mercantilist_agent import MercantilistAgent

def main():
    """Example usage of all economic agents"""

    # Create sample economic data
    sample_data = EconomicData(
        gdp=3.2,                    # GDP growth rate (%)
        inflation=2.8,              # Inflation rate (%)
        unemployment=5.5,            # Unemployment rate (%)
        interest_rate=4.2,          # Interest rate (%)
        trade_balance=2.1,          # Trade balance (% of GDP, positive = surplus)
        government_spending=32.5,   # Government spending (% of GDP)
        tax_rate=28.0,              # Average tax rate (%)
        private_investment=22.3,    # Private investment (% of GDP)
        consumer_confidence=67.8,   # Consumer confidence index (0-100)
        timestamp=datetime.now()
    )

    # Initialize all agents
    agents = {
        "Capitalism": CapitalismAgent(),
        "Socialism": SocialismAgent(),
        "Mixed Economy": MixedEconomyAgent(),
        "Keynesian": KeynesianAgent(),
        "Neoliberal": NeoliberalAgent(),
        "Mercantilism": MercantilistAgent()
    }

    print("=" * 80)
    print("ECONOMIC ANALYSIS USING DIFFERENT IDEOLOGICAL PERSPECTIVES")
    print("=" * 80)
    print(f"\nAnalyzing economic data from {sample_data.timestamp.strftime('%Y-%m-%d %H:%M:%S')}")
    print(f"GDP Growth: {sample_data.gdp}%")
    print(f"Inflation: {sample_data.inflation}%")
    print(f"Unemployment: {sample_data.unemployment}%")
    print(f"Trade Balance: {sample_data.trade_balance}% of GDP")
    print(f"Government Spending: {sample_data.government_spending}% of GDP")
    print(f"Tax Rate: {sample_data.tax_rate}%")
    print(f"Private Investment: {sample_data.private_investment}% of GDP")
    print("-" * 80)

    # Analyze with each agent
    results = {}
    for name, agent in agents.items():
        print(f"\n{name.upper()} PERSPECTIVE:")
        print("-" * 40)

        # Get analysis
        analysis = agent.analyze_economy(sample_data)
        results[name] = analysis

        # Print key results
        print(f"Overall Health Score: {analysis['overall_health_score']:.1f}/100")
        print(f"Assessment: {analysis['assessment']}")

        # Print strengths
        if analysis['strengths']:
            print("\nStrengths:")
            for strength in analysis['strengths']:
                print(f"  ✓ {strength}")

        # Print concerns
        if analysis['concerns']:
            print("\nConcerns:")
            for concern in analysis['concerns']:
                print(f"  ⚠ {concern}")

        # Get top policy recommendations
        policies = agent.get_policy_recommendations(sample_data, analysis)
        if policies:
            print("\nTop Policy Recommendations:")
            for i, policy in enumerate(policies[:3], 1):
                print(f"  {i}. {policy.title}")
                print(f"     Priority: {policy.priority.upper()}")
                print(f"     Expected Impact: {policy.expected_impact}")

    # Compare policy evaluation
    print("\n" + "=" * 80)
    print("POLICY EVALUATION COMPARISON")
    print("=" * 80)

    test_policies = [
        {"name": "Tax Cuts for Businesses", "type": "tax_cut"},
        {"name": "Increase Infrastructure Spending", "type": "infrastructure_spending"},
        {"name": "Implement Trade Tariffs", "type": "import_tariff"},
        {"name": "Free Trade Agreement", "type": "trade_liberalization"}
    ]

    for policy in test_policies:
        print(f"\nPolicy: {policy['name']}")
        print("-" * 50)

        for name, agent in agents.items():
            evaluation = agent.evaluate_policy(policy, sample_data)
            print(f"{name:12}: {evaluation['recommendation']:20} (Score: {evaluation['agent_specific_score']:.1f})")

    # Summary comparison
    print("\n" + "=" * 80)
    print("IDEOLOGY COMPARISON SUMMARY")
    print("=" * 80)
    print(f"{'Ideology':<15} {'Health Score':<15} {'Key Focus':<50}")
    print("-" * 80)

    focus_areas = {
        "Capitalism": "Market efficiency, private investment, economic freedom",
        "Socialism": "Economic equality, social welfare, workers' protection",
        "Mixed Economy": "Balance of public and private sectors, stability",
        "Keynesian": "Demand management, counter-cyclical policies",
        "Neoliberal": "Deregulation, privatization, minimal government",
        "Mercantilism": "Trade surplus, protectionism, national wealth"
    }

    for name, analysis in results.items():
        health_score = analysis['overall_health_score']
        focus = focus_areas.get(name, "")
        print(f"{name:<15} {health_score:<15.1f} {focus:<50}")

def compare_specific_scenario():
    """Compare agents on a specific economic scenario"""

    print("\n" + "=" * 80)
    print("RECESSION SCENARIO ANALYSIS")
    print("=" * 80)

    # Create recession data
    recession_data = EconomicData(
        gdp=1.2,                    # Low growth
        inflation=1.5,              # Low inflation
        unemployment=8.5,            # High unemployment
        interest_rate=1.8,          # Low interest rates
        trade_balance=-4.2,          # Trade deficit
        government_spending=38.5,   # High government spending
        tax_rate=35.0,              # High taxes
        private_investment=15.3,    # Low private investment
        consumer_confidence=42.1,   # Low confidence
        timestamp=datetime.now()
    )

    print(f"\nRecession Scenario Data:")
    print(f"GDP Growth: {recession_data.gdp}% (Low)")
    print(f"Unemployment: {recession_data.unemployment}% (High)")
    print(f"Private Investment: {recession_data.private_investment}% (Low)")
    print(f"Consumer Confidence: {recession_data.consumer_confidence} (Low)")
    print("-" * 80)

    # Get Keynesian analysis specifically for recession
    keynesian = KeynesianAgent()
    keynesian_analysis = keynesian.analyze_economy(recession_data)

    print(f"\nKeynesian Analysis for Recession:")
    print(f"Economic Phase: {keynesian_analysis['economic_phase']}")
    print(f"Overall Score: {keynesian_analysis['overall_health_score']:.1f}/100")

    # Get recession-specific policies
    policies = keynesian.get_policy_recommendations(recession_data, keynesian_analysis)
    print("\nKeynesian Policy Recommendations for Recession:")
    for i, policy in enumerate(policies, 1):
        print(f"\n{i}. {policy.title}")
        print(f"   {policy.description}")
        print(f"   Priority: {policy.priority.upper()}")
        print(f"   Expected Impact: {policy.expected_impact}")
        print(f"   Implementation Time: {policy.implementation_time}")

if __name__ == "__main__":
    main()
    compare_specific_scenario()