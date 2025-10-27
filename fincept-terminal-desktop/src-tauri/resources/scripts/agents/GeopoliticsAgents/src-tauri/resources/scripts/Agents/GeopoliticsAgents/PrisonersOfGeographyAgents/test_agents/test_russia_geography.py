"""
Test Framework for Russia Geography Agent
Validates Marshall's geographic determinism compliance
"""

import json
from datetime import datetime
import sys
import os

# Add parent directories to path for imports
sys.path.append(os.path.dirname(os.path.dirname(os.path.dirname(__file__))))

def test_russia_geography_agent():
    """
    Test Russia Geography Agent with sample events
    Validates Marshall's geographic determinism compliance (70%+ required)
    """

    print("🧪 Testing Russia Geography Agent - Marshall's Geographic Determinism Framework")
    print("=" * 80)

    # Test event data focusing on Russian geographic constraints
    test_events = [
        {
            "title": "Russia strengthens Baltic Sea military presence",
            "description": "Russian naval deployments in Baltic Sea increase amid NATO expansion concerns",
            "source": "Geopolitical Monitor",
            "date": "2024-10-15",
            "region": "europe/russia"
        },
        {
            "title": "Ukraine grain export disruption affects global food security",
            "description": "Conflict in Ukraine's agricultural heartland impacts worldwide grain supplies",
            "source": "Agricultural Report",
            "date": "2024-10-10",
            "region": "europe/russia"
        },
        {
            "title": "Russia-China Arctic cooperation expands",
            "description": "Joint development of Arctic shipping routes and resource extraction projects",
            "source": "Arctic Affairs",
            "date": "2024-10-05",
            "region": "arctic/russia"
        },
        {
            "title": "NATO discusses eastern European troop deployments",
            "description": "Alliance considers permanent military presence in Poland and Baltic states",
            "source": "Security Briefing",
            "date": "2024-09-28",
            "region": "europe/nato"
        },
        {
            "title": "Russian energy diplomacy through pipeline control",
            "description": "Moscow leverages natural gas infrastructure for political influence in Europe",
            "source": "Energy Analysis",
            "date": "2024-09-20",
            "region": "europe/russia"
        }
    ]

    # Create mock agent state
    mock_state = {
        "current_events": test_events,
        "analysis_parameters": {
            "framework": "marshalls_geographic_determinism",
            "region": "russia",
            "compliance_threshold": 0.7
        }
    }

    try:
        # Import the Russia geography agent
        from region_agents.russia_geography_agent import russia_geography_agent

        # Run the agent
        print("📊 Running Russia Geography Agent analysis...")
        analysis_result = russia_geography_agent(mock_state)

        # Validate the results
        print("\n🔍 Analysis Results:")
        print(f"Agent: {analysis_result.get('agent', 'Unknown')}")
        print(f"Framework: {analysis_result.get('framework', 'Unknown')}")
        print(f"Region: {analysis_result.get('region', 'Unknown')}")

        # Check Marshall compliance
        marshall_compliance = analysis_result.get('marshall_compliance_score', 0.0)
        print(f"\n📈 Marshall Thesis Compliance Score: {marshall_compliance:.3f}")

        if marshall_compliance >= 0.7:
            print("✅ PASS - Meets Marshall's geographic determinism requirements")
        else:
            print("❌ FAIL - Below 70% Marshall compliance threshold")
            print("   - Need stronger geographic determinism focus")
            print("   - Improve historical pattern recognition")
            print("   - Enhance strategic imperative identification")

        # Analyze constraint levels
        event_analyses = analysis_result.get('event_analyses', [])
        if event_analyses:
            print(f"\n🎯 Event Analysis Summary:")
            for i, event_analysis in enumerate(event_analyses[:3], 1):
                event_name = event_analysis.get('event', 'Unknown')
                constraint_level = event_analysis.get('constraint_level', 0.0)
                geographic_determinants = event_analysis.get('geographic_determinants', [])

                print(f"  {i}. {event_name}")
                print(f"     Geographic Determinants: {', '.join(geographic_determinants)}")
                print(f"     Constraint Level: {constraint_level:.2f}")

        # Check strategic imperatives
        strategic_imperatives = analysis_result.get('strategic_imperatives', {})
        primary_imperatives = strategic_imperatives.get('primary_imperatives', [])

        print(f"\n🎯 Strategic Imperatives Identified: {len(primary_imperatives)}")
        for imperative in primary_imperatives:
            imperative_name = imperative.get('imperative', 'Unknown')
            geographic_driver = imperative.get('geographic_driver', 'Unknown')
            print(f"  - {imperative_name}")
            print(f"    Driver: {geographic_driver}")

        # Validate geographic determinism focus
        determinism_focus = analysis_result.get('determinism_focus', 0.0)
        print(f"\n🏔️ Geographic Determinism Focus: {determinism_focus:.3f}")

        # Check historical patterns
        historical_patterns = analysis_result.get('historical_patterns', {})
        print(f"\n📚 Historical Patterns Identified:")
        for pattern_type, pattern_data in historical_patterns.items():
            if isinstance(pattern_data, dict) and pattern_data.get('geographic_determinism'):
                pattern_name = pattern_data.get('pattern', 'Unknown')
                print(f"  - {pattern_type}: {pattern_name}")

        # Test predictions
        predictions = analysis_result.get('geographic_predictions', [])
        print(f"\n🔮 Geographic Predictions: {len(predictions)}")
        for prediction in predictions[:2]:  # Show first 2
            pred_behavior = prediction.get('prediction', 'Unknown')
            confidence = prediction.get('confidence', 0.0)
            geographic_driver = prediction.get('geographic_driver', 'Unknown')
            print(f"  - {pred_behavior}")
            print(f"    Driver: {geographic_driver}, Confidence: {confidence:.2f}")

        # Overall test result
        print("\n" + "=" * 80)
        print("🧪 TEST SUMMARY:")

        test_passed = (
            marshall_compliance >= 0.7 and
            len(primary_imperatives) >= 2 and
            determinism_focus >= 0.5
        )

        if test_passed:
            print("✅ RUSSIA GEOGRAPHY AGENT TEST PASSED")
            print("   - Marshall thesis compliance achieved")
            print("   - Strategic imperatives identified")
            print("   - Geographic determinism focus maintained")
        else:
            print("❌ RUSSIA GEOGRAPHY AGENT TEST FAILED")
            if marshall_compliance < 0.7:
                print("   - Marshall compliance below threshold")
            if len(primary_imperatives) < 2:
                print("   - Insufficient strategic imperatives")
            if determinism_focus < 0.5:
                print("   - Weak geographic determinism focus")

        return test_passed

    except ImportError as e:
        print(f"❌ Import Error: {e}")
        print("   - Check module imports and file structure")
        return False
    except Exception as e:
        print(f"❌ Test Error: {e}")
        return False

def validate_marshall_thesis_components(analysis_result):
    """
    Validate that all Marshall thesis components are present

    Marshall's Geographic Determinism Requirements:
    1. Geographic DETERMINISM (not just influence)
    2. Historical pattern recognition
    3. Strategic imperative identification
    4. Constraint over choice emphasis
    """

    components = {
        "geographic_determinism": False,
        "historical_patterns": False,
        "strategic_imperatives": False,
        "constraint_emphasis": False
    }

    # Check geographic determinism
    constraint_analysis = analysis_result.get('constraint_analysis', [])
    if constraint_analysis and any(analysis.get('marshall_determinism_score', 0) > 0.5
                                  for analysis in constraint_analysis):
        components["geographic_determinism"] = True

    # Check historical patterns
    historical_patterns = analysis_result.get('historical_patterns', {})
    if historical_patterns and any(pattern.get('geographic_determinism', False)
                                 for pattern in historical_patterns.values()
                                 if isinstance(pattern, dict)):
        components["historical_patterns"] = True

    # Check strategic imperatives
    strategic_imperatives = analysis_result.get('strategic_imperatives', {})
    primary_imperatives = strategic_imperatives.get('primary_imperatives', [])
    if len(primary_imperatives) >= 2:
        components["strategic_imperatives"] = True

    # Check constraint emphasis
    determinism_focus = analysis_result.get('determinism_focus', 0.0)
    if determinism_focus >= 0.5:
        components["constraint_emphasis"] = True

    return components

def run_detailed_validation():
    """
    Run detailed validation of Marshall thesis compliance
    """

    print("\n🔬 DETAILED MARSHALL THESIS VALIDATION")
    print("-" * 50)

    # Run basic test
    test_passed = test_russia_geography_agent()

    if test_passed:
        print("\n✅ All validation checks passed")
        print("🎉 Russia Geography Agent is ready for production")
    else:
        print("\n⚠️ Some validation checks failed")
        print("🔧 Agent needs refinement before production deployment")

    return test_passed

if __name__ == "__main__":
    print("🚀 Starting Russia Geography Agent Test Suite")
    print("Testing Tim Marshall's Geographic Determinism Framework")
    print("Required: 70%+ Marshall thesis compliance")
    print()

    success = run_detailed_validation()

    if success:
        print("\n🎯 Test completed successfully!")
        sys.exit(0)
    else:
        print("\n❌ Test failed - check output above for details")
        sys.exit(1)