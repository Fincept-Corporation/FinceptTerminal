"""
Master Test Runner for Prisoners of Geography Agents
Tests all regional agents for Marshall's geographic determinism compliance
"""

import json
import sys
import os
from datetime import datetime

# Add parent directories to path for imports
sys.path.append(os.path.dirname(os.path.dirname(__file__)))

def test_all_geography_agents():
    """
    Test all implemented Prisoners of Geography agents
    Validates Marshall's geographic determinism compliance across all regions
    """

    print("🌍 Prisoners of Geography - Master Test Suite")
    print("=" * 80)
    print("Testing Tim Marshall's Geographic Determinism Framework")
    print("Required: 70%+ Marshall thesis compliance for all agents")
    print()

    # Define implemented agents
    implemented_agents = [
        {
            "name": "Russia",
            "file": "test_russia_geography",
            "region": "russia",
            "chapter": 1
        },
        {
            "name": "China",
            "file": "test_china_geography",
            "region": "china",
            "chapter": 2
        },
        {
            "name": "USA",
            "file": "test_usa_geography",
            "region": "usa",
            "chapter": 3
        },
        {
            "name": "Middle East",
            "file": "test_middle_east_geography",
            "region": "middle_east",
            "chapter": 6
        },
        {
            "name": "Europe",
            "file": "test_europe_geography",
            "region": "europe",
            "chapter": 8
        }
    ]

    test_results = {}
    overall_success = True

    for agent_info in implemented_agents:
        print(f"🧪 Testing {agent_info['name']} Geography Agent (Chapter {agent_info['chapter']})")
        print("-" * 60)

        try:
            # Import and run individual test
            test_module = __import__(agent_info['file'])
            test_passed = test_module.test_geography_agent()

            test_results[agent_info['region']] = {
                "name": agent_info['name'],
                "chapter": agent_info['chapter'],
                "passed": test_passed,
                "error": None
            }

            if test_passed:
                print(f"✅ {agent_info['name']} Agent PASSED")
            else:
                print(f"❌ {agent_info['name']} Agent FAILED")
                overall_success = False

        except ImportError as e:
            print(f"❌ Import Error for {agent_info['name']}: {e}")
            test_results[agent_info['region']] = {
                "name": agent_info['name'],
                "chapter": agent_info['chapter'],
                "passed": False,
                "error": f"Import error: {e}"
            }
            overall_success = False

        except Exception as e:
            print(f"❌ Test Error for {agent_info['name']}: {e}")
            test_results[agent_info['region']] = {
                "name": agent_info['name'],
                "chapter": agent_info['chapter'],
                "passed": False,
                "error": f"Test error: {e}"
            }
            overall_success = False

        print()

    # Print summary
    print("=" * 80)
    print("📊 TEST SUMMARY")
    print("=" * 80)

    total_agents = len(implemented_agents)
    passed_agents = sum(1 for result in test_results.values() if result['passed'])
    failed_agents = total_agents - passed_agents

    print(f"Total Agents Tested: {total_agents}")
    print(f"Passed: {passed_agents} ✅")
    print(f"Failed: {failed_agents} ❌")
    print(f"Success Rate: {(passed_agents/total_agents)*100:.1f}%")
    print()

    # Detailed results
    print("📋 DETAILED RESULTS:")
    for region, result in test_results.items():
        status = "✅ PASS" if result['passed'] else "❌ FAIL"
        print(f"  {result['name']} (Chapter {result['chapter']}): {status}")
        if result['error']:
            print(f"    Error: {result['error']}")

    print()

    # Marshall thesis compliance summary
    print("🎯 MARSHALL THESIS COMPLIANCE SUMMARY:")
    print("Geographic Determinism Framework Validation")

    if overall_success:
        print("✅ ALL AGENTS MEET MARSHALL'S GEOGRAPHIC DETERMINISM REQUIREMENTS")
        print("   - Geographic constraints drive policy choices")
        print("   - Historical patterns demonstrate determinism")
        print("   - Strategic imperatives identified correctly")
        print("   - 70%+ compliance achieved across all agents")
        print()
        print("🎉 Prisoners of Geography Agent Framework Ready for Production!")
    else:
        print("⚠️ SOME AGENTS FAIL MARSHALL COMPLIANCE REQUIREMENTS")
        print("   - Review failed agents for geographic determinism focus")
        print("   - Strengthen historical pattern recognition")
        print("   - Improve strategic imperative identification")
        print("   - Ensure 70%+ compliance threshold is met")
        print()
        print("🔧 Framework requires refinement before production deployment")

    return overall_success

def create_mock_event_data(region):
    """
    Create mock event data specific to each region for testing
    """

    event_templates = {
        "russia": [
            {
                "title": "Russia strengthens Black Sea naval presence",
                "description": "Russian fleet movements in response to regional security concerns",
                "source": "Naval Intelligence",
                "date": "2024-10-15"
            },
            {
                "title": "Arctic shipping route development accelerates",
                "description": "Northern Sea Route sees increased traffic as ice melts",
                "source": "Arctic Monitor",
                "date": "2024-10-10"
            }
        ],
        "china": [
            {
                "title": "South China Sea militarization continues",
                "description": "Island fortification and naval deployments expand",
                "source": "Asia Security",
                "date": "2024-10-12"
            },
            {
                "title": "Belt and Road infrastructure project completed",
                "description": "New railway connection enhances regional connectivity",
                "source": "Economic Report",
                "date": "2024-10-08"
            }
        ],
        "usa": [
            {
                "title": "US naval freedom of navigation operation",
                "description": "Warship transits disputed waters to assert international law",
                "source": "Pentagon Briefing",
                "date": "2024-10-14"
            },
            {
                "title": "Agricultural export levels reach new highs",
                "description": "American grain exports surge to meet global demand",
                "source": "Agriculture Department",
                "date": "2024-10-09"
            }
        ],
        "middle_east": [
            {
                "title": "OPEC discusses oil production quotas",
                "description": "Member nations consider output adjustments to stabilize prices",
                "source": "Energy Report",
                "date": "2024-10-13"
            },
            {
                "title": "Jordan River water sharing agreement disputed",
                "description": "Regional tensions rise over scarce water resources",
                "source": "Middle East Monitor",
                "date": "2024-10-07"
            }
        ],
        "europe": [
            {
                "title": "EU strengthens eastern border security",
                "description": "Defense cooperation enhanced amid regional tensions",
                "source": "Brussels Report",
                "date": "2024-10-11"
            },
            {
                "title": "Rhine River trade volume increases",
                "description": "Internal European commerce shows strong growth",
                "source": "EU Economic Report",
                "date": "2024-10-06"
            }
        ]
    }

    return event_templates.get(region, [])

def validate_framework_structure():
    """
    Validate that the Prisoners of Geography framework structure is correct
    """

    print("🏗️ FRAMEWORK STRUCTURE VALIDATION")
    print("-" * 40)

    required_structure = {
        "region_agents": [
            "russia_geography_agent.py",
            "china_geography_agent.py",
            "usa_geography_agent.py",
            "middle_east_geography_agent.py",
            "europe_geography_agent.py"
        ],
        "geographic_modules": [
            "geographic_constraints.py",
            "historical_patterns.py",
            "strategic_imperatives.py"
        ],
        "test_agents": [
            "test_russia_geography.py",
            "run_all_tests.py"
        ]
    }

    base_path = os.path.dirname(os.path.dirname(__file__))
    structure_valid = True

    for folder, required_files in required_structure.items():
        folder_path = os.path.join(base_path, folder)
        print(f"📁 Checking {folder}/:")

        if not os.path.exists(folder_path):
            print(f"  ❌ Folder missing: {folder}")
            structure_valid = False
            continue

        for file_name in required_files:
            file_path = os.path.join(folder_path, file_name)
            if os.path.exists(file_path):
                print(f"  ✅ {file_name}")
            else:
                print(f"  ❌ {file_name} - MISSING")
                structure_valid = False

    if structure_valid:
        print("\n✅ Framework structure validation PASSED")
    else:
        print("\n❌ Framework structure validation FAILED")
        print("   - Missing files or folders detected")
        print("   - Review framework structure before proceeding")

    return structure_valid

def main():
    """
    Main test runner
    """

    print("🚀 Prisoners of Geography - Master Test Suite")
    print(f"Started at: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print()

    # Validate framework structure first
    structure_valid = validate_framework_structure()
    print()

    if not structure_valid:
        print("❌ Framework structure issues detected - aborting tests")
        return False

    # Run all agent tests
    test_success = test_all_geography_agents()

    # Final result
    print("\n" + "=" * 80)
    print("🏁 FINAL TEST RESULT")

    if structure_valid and test_success:
        print("✅ ALL TESTS PASSED - FRAMEWORK READY FOR PRODUCTION")
        print("🌍 Prisoners of Geography agents successfully implement")
        print("   Tim Marshall's geographic determinism framework")
        return True
    else:
        print("❌ TESTS FAILED - FRAMEWORK NEEDS REFINEMENT")
        if not structure_valid:
            print("   - Fix framework structure issues")
        if not test_success:
            print("   - Improve agent Marshall compliance")
        return False

if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)