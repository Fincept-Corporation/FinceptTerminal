"""
WESTPHALIAN ORDER AGENT TEST SUITE - Henry Kissinger Framework

===== TESTING FRAMEWORK =====
# INPUT:
#   - dummy_geopolitical_events
#   - historical_test_cases
#   - framework_compliance_checks
#
# OUTPUT:
#   - Agent performance validation
#   - Kissinger framework compliance score
#   - Historical continuity verification
#   - Diplomatic nuance assessment
#
# PARAMETERS:
#   - test_type="comprehensive"
#   - compliance_threshold="70%"
#   - validation_depth="thorough"
"""

import sys
import os
import json
from typing import Dict, List, Any

# Add the parent directories to the path
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from civilization_agents.westphalian_europe_agent import westphalian_europe_agent
from diplomatic_modules.realpolitik_analysis import RealpolitikAnalyzer
from diplomatic_modules.balance_power_calculations import BalancePowerCalculator

class WestphalianOrderTestSuite:
    """
    TEST SUITE FOR WESTPHALIAN ORDER AGENT
    Validates agent performance against Kissinger's framework requirements
    """

    def __init__(self):
        self.test_cases = self._initialize_test_cases()
        self.compliance_threshold = 70.0
        self.test_results = []

    def _initialize_test_cases(self) -> List[Dict[str, Any]]:
        """Initialize comprehensive test cases for Westphalian order analysis"""

        return [
            {
                'name': 'Sovereignty Crisis Test',
                'description': 'Test agent analysis of sovereignty violations',
                'events': [
                    {
                        'type': 'border_conflict',
                        'actors': ['Russia', 'Ukraine', 'NATO'],
                        'location': 'Eastern Europe',
                        'description': 'Conflict over territorial borders and sovereign boundaries',
                        'date': '2024-01-15'
                    }
                ],
                'expected_framework_elements': [
                    'sovereignty_assessment',
                    'westphalian_principles',
                    'border_integrity',
                    'non_interference'
                ],
                'min_compliance_score': 70.0
            },
            {
                'name': 'Balance of Power Test',
                'description': 'Test agent ability to analyze European balance dynamics',
                'events': [
                    {
                        'type': 'great_power_competition',
                        'actors': ['Germany', 'France', 'UK'],
                        'location': 'Europe',
                        'description': 'European powers managing regional balance and cooperation',
                        'date': '2024-02-20'
                    }
                ],
                'expected_framework_elements': [
                    'balance_power',
                    'hegemony_prevention',
                    'equilibrium_maintenance',
                    'concert_of_europe'
                ],
                'min_compliance_score': 75.0
            },
            {
                'name': 'Diplomatic Tradition Test',
                'description': 'Test agent understanding of European diplomatic continuity',
                'events': [
                    {
                        'type': 'diplomatic_negotiation',
                        'actors': ['EU', 'UK', 'European_States'],
                        'location': 'Brussels',
                        'description': 'Traditional European diplomatic negotiations and treaty-making',
                        'date': '2024-03-10'
                    }
                ],
                'expected_framework_elements': [
                    'historical_continuity',
                    'diplomatic_protocols',
                    'westphalian_traditions',
                    'european_coordination'
                ],
                'min_compliance_score': 70.0
            },
            {
                'name': 'Multi-actor Crisis Test',
                'description': 'Test complex multi-actor European crisis analysis',
                'events': [
                    {
                        'type': 'regional_crisis',
                        'actors': ['NATO', 'EU', 'Russia', 'Multiple_European_States'],
                        'location': 'Eastern Europe',
                        'description': 'Complex crisis involving multiple actors and alliance systems',
                        'date': '2024-04-05'
                    }
                ],
                'expected_framework_elements': [
                    'alliance_systems',
                    'coalition_formations',
                    'crisis_diplomacy',
                    'security_guarantees'
                ],
                'min_compliance_score': 75.0
            },
            {
                'name': 'Historical Pattern Recognition Test',
                'description': 'Test agent recognition of historical European patterns',
                'events': [
                    {
                        'type': 'power_transition',
                        'actors': ['European_Powers', 'Rising_Powers'],
                        'location': 'Europe',
                        'description': 'Power transition patterns reminiscent of historical European changes',
                        'date': '2024-05-12'
                    }
                ],
                'expected_framework_elements': [
                    'historical_patterns',
                    'power_transition_dynamics',
                    'european_state_system',
                    'balance_powershifts'
                ],
                'min_compliance_score': 80.0
            }
        ]

    def run_all_tests(self) -> Dict[str, Any]:
        """Run all test cases and return comprehensive results"""
        print("=== WESTPHALIAN ORDER AGENT TEST SUITE ===")
        print(f"Running {len(self.test_cases)} test cases...\n")

        overall_results = {
            'total_tests': len(self.test_cases),
            'passed_tests': 0,
            'failed_tests': 0,
            'average_compliance': 0.0,
            'framework_validation': {},
            'test_details': []
        }

        total_compliance = 0.0

        for i, test_case in enumerate(self.test_cases, 1):
            print(f"Test {i}/{len(self.test_cases)}: {test_case['name']}")
            print(f"Description: {test_case['description']}")

            # Run the test case
            test_result = self._run_single_test(test_case)
            self.test_results.append(test_result)

            # Update overall results
            if test_result['passed']:
                overall_results['passed_tests'] += 1
                print(f"✅ PASSED (Compliance: {test_result['compliance_score']:.1f}%)")
            else:
                overall_results['failed_tests'] += 1
                print(f"❌ FAILED (Compliance: {test_result['compliance_score']:.1f}%)")
                print(f"   Issues: {', '.join(test_result['missing_elements'])}")

            total_compliance += test_result['compliance_score']
            print()

        # Calculate overall statistics
        overall_results['average_compliance'] = total_compliance / len(self.test_cases)
        overall_results['success_rate'] = (overall_results['passed_tests'] / overall_results['total_tests']) * 100

        # Validate framework compliance
        overall_results['framework_validation'] = self._validate_framework_compliance()

        return overall_results

    def _run_single_test(self, test_case: Dict[str, Any]) -> Dict[str, Any]:
        """Run a single test case and return results"""
        events = test_case['events']
        expected_elements = test_case['expected_framework_elements']
        min_compliance = test_case['min_compliance_score']

        # Initialize test state
        test_state = {
            'current_events': events,
            'analysis_framework': 'kissinger_world_order',
            'test_mode': True
        }

        # Run the Westphalian Europe agent
        try:
            agent_result = westphalian_europe_agent(test_state)

            # Calculate compliance score
            compliance_score = agent_result.get('framework_compliance', {}).get('kissinger_score', 0.0)

            # Check for expected framework elements
            result_elements = self._extract_framework_elements(agent_result)
            missing_elements = [elem for elem in expected_elements if elem not in result_elements]

            # Validate Kissinger framework requirements
            framework_validation = self._validate_kissinger_requirements(agent_result)

            # Test passed if compliance meets threshold and required elements present
            passed = (compliance_score >= min_compliance and len(missing_elements) == 0)

            return {
                'test_name': test_case['name'],
                'passed': passed,
                'compliance_score': compliance_score,
                'expected_elements': expected_elements,
                'found_elements': result_elements,
                'missing_elements': missing_elements,
                'framework_validation': framework_validation,
                'agent_result': agent_result
            }

        except Exception as e:
            return {
                'test_name': test_case['name'],
                'passed': False,
                'compliance_score': 0.0,
                'error': str(e),
                'expected_elements': expected_elements,
                'found_elements': [],
                'missing_elements': expected_elements
            }

    def _extract_framework_elements(self, agent_result: Dict[str, Any]) -> List[str]:
        """Extract framework elements from agent result"""
        elements = []

        # Check for Westphalian sovereignty analysis
        if agent_result.get('westphalian_analysis'):
            elements.append('sovereignty_assessment')
            if agent_result['westphalian_analysis'].get('sovereignty_impact'):
                elements.append('border_integrity')

        # Check for balance of power analysis
        if agent_result.get('balance_power_analysis'):
            elements.append('balance_power')
            if agent_result['balance_power_analysis'].get('hegemony_threats'):
                elements.append('hegemony_prevention')

        # Check for historical continuity
        if agent_result.get('historical_continuity'):
            elements.append('historical_continuity')
            if any('westphalian' in str(pattern).lower()
                   for pattern in agent_result['historical_continuity'].get('westphalian_patterns', [])):
                elements.append('westphalian_traditions')

        # Check for diplomatic recommendations
        if agent_result.get('diplomatic_recommendations'):
            elements.append('diplomatic_recommendations')
            recommendations = agent_result['diplomatic_recommendations']
            if recommendations.get('westphalian_solutions'):
                elements.append('westphalian_solutions')

        # Check for specific Westphalian concepts
        result_str = json.dumps(agent_result).lower()
        if 'sovereign' in result_str:
            elements.append('sovereignty')
        if 'non-interference' in result_str:
            elements.append('non_interference')
        if 'diplomatic' in result_str:
            elements.append('diplomatic_protocols')

        return list(set(elements))  # Remove duplicates

    def _validate_kissinger_requirements(self, agent_result: Dict[str, Any]) -> Dict[str, bool]:
        """Validate specific Kissinger framework requirements"""
        validation = {
            'historical_continuity_present': False,
            'realpolitik_analysis_present': False,
            'diplomatic_nuance_present': False,
            'balance_power_logic_present': False,
            'european_understanding_present': False
        }

        # Check historical continuity
        if agent_result.get('historical_continuity', {}).get('westphalian_patterns'):
            validation['historical_continuity_present'] = True

        # Check realpolitik analysis
        if agent_result.get('balance_power_analysis', {}).get('hegemony_threats'):
            validation['realpolitik_analysis_present'] = True

        # Check diplomatic nuance
        if agent_result.get('diplomatic_recommendations'):
            validation['diplomatic_nuance_present'] = True

        # Check balance of power logic
        power_analysis = agent_result.get('balance_power_analysis', {})
        if power_analysis.get('hegemony_threats') or power_analysis.get('power_equilibrium_status'):
            validation['balance_power_logic_present'] = True

        # Check European understanding
        result_str = json.dumps(agent_result).lower()
        if any(term in result_str for term in ['europe', 'westphalian', 'concert', 'sovereign']):
            validation['european_understanding_present'] = True

        return validation

    def _validate_framework_compliance(self) -> Dict[str, Any]:
        """Validate overall framework compliance across all tests"""
        validation_summary = {
            'kissinger_framework_compliance': 0.0,
            'historical_continuity_score': 0.0,
            'realpolitik_application_score': 0.0,
            'diplomatic_nuance_score': 0.0,
            'european_expertise_score': 0.0,
            'overall_framework_health': 'unknown'
        }

        if not self.test_results:
            return validation_summary

        # Calculate average scores for each framework component
        total_tests = len(self.test_results)
        historical_count = 0
        realpolitik_count = 0
        diplomatic_count = 0
        european_count = 0

        for result in self.test_results:
            if result.get('framework_validation'):
                validation = result['framework_validation']
                if validation.get('historical_continuity_present'):
                    historical_count += 1
                if validation.get('realpolitik_analysis_present'):
                    realpolitik_count += 1
                if validation.get('diplomatic_nuance_present'):
                    diplomatic_count += 1
                if validation.get('european_understanding_present'):
                    european_count += 1

        validation_summary['historical_continuity_score'] = (historical_count / total_tests) * 100
        validation_summary['realpolitik_application_score'] = (realpolitik_count / total_tests) * 100
        validation_summary['diplomatic_nuance_score'] = (diplomatic_count / total_tests) * 100
        validation_summary['european_expertise_score'] = (european_count / total_tests) * 100

        # Calculate overall framework health
        framework_scores = [
            validation_summary['historical_continuity_score'],
            validation_summary['realpolitik_application_score'],
            validation_summary['diplomatic_nuance_score'],
            validation_summary['european_expertise_score']
        ]

        validation_summary['kissinger_framework_compliance'] = sum(framework_scores) / len(framework_scores)

        if validation_summary['kissinger_framework_compliance'] >= 80:
            validation_summary['overall_framework_health'] = 'excellent'
        elif validation_summary['kissinger_framework_compliance'] >= 70:
            validation_summary['overall_framework_health'] = 'good'
        elif validation_summary['kissinger_framework_compliance'] >= 60:
            validation_summary['overall_framework_health'] = 'fair'
        else:
            validation_summary['overall_framework_health'] = 'poor'

        return validation_summary

    def generate_test_report(self, results: Dict[str, Any]) -> str:
        """Generate comprehensive test report"""
        report = []
        report.append("=" * 60)
        report.append("WESTPHALIAN ORDER AGENT TEST REPORT")
        report.append("=" * 60)
        report.append(f"Total Tests: {results['total_tests']}")
        report.append(f"Passed: {results['passed_tests']}")
        report.append(f"Failed: {results['failed_tests']}")
        report.append(f"Success Rate: {results['success_rate']:.1f}%")
        report.append(f"Average Compliance: {results['average_compliance']:.1f}%")
        report.append("")

        # Framework validation summary
        framework_validation = results['framework_validation']
        report.append("FRAMEWORK VALIDATION SUMMARY:")
        report.append(f"  Kissinger Framework Compliance: {framework_validation['kissinger_framework_compliance']:.1f}%")
        report.append(f"  Historical Continuity: {framework_validation['historical_continuity_score']:.1f}%")
        report.append(f"  Realpolitik Application: {framework_validation['realpolitik_application_score']:.1f}%")
        report.append(f"  Diplomatic Nuance: {framework_validation['diplomatic_nuance_score']:.1f}%")
        report.append(f"  European Expertise: {framework_validation['european_expertise_score']:.1f}%")
        report.append(f"  Overall Framework Health: {framework_validation['overall_framework_health'].upper()}")
        report.append("")

        # Individual test details
        report.append("INDIVIDUAL TEST RESULTS:")
        for i, test_detail in enumerate(results['test_details'], 1):
            report.append(f"  {i}. {test_detail['test_name']}")
            report.append(f"     Status: {'PASSED' if test_detail['passed'] else 'FAILED'}")
            report.append(f"     Compliance: {test_detail['compliance_score']:.1f}%")
            if test_detail.get('missing_elements'):
                report.append(f"     Missing Elements: {', '.join(test_detail['missing_elements'])}")
            report.append("")

        # Recommendations
        report.append("RECOMMENDATIONS:")
        if results['average_compliance'] < 70:
            report.append("  - Agent needs significant improvement to meet Kissinger framework standards")
            report.append("  - Focus on historical continuity and European diplomatic traditions")
            report.append("  - Strengthen realpolitik analysis components")
        elif results['average_compliance'] < 80:
            report.append("  - Agent performs adequately but could be enhanced")
            report.append("  - Consider adding more European historical context")
            report.append("  - Refine diplomatic nuance in recommendations")
        else:
            report.append("  - Agent demonstrates strong Kissinger framework compliance")
            report.append("  - Ready for production deployment")
            report.append("  - Consider additional edge case testing")

        return "\n".join(report)

# Example usage and testing
if __name__ == "__main__":
    # Initialize and run test suite
    test_suite = WestphalianOrderTestSuite()
    results = test_suite.run_all_tests()

    # Generate and display report
    report = test_suite.generate_test_report(results)
    print(report)

    # Additional validation with diplomatic modules
    print("\n" + "=" * 60)
    print("ADDITIONAL DIPLOMATIC MODULES TESTING")
    print("=" * 60)

    # Test Realpolitik Analyzer
    print("\nTesting Realpolitik Analysis Module:")
    realpolitik_analyzer = RealpolitikAnalyzer()
    test_event = {
        'type': 'sovereignty_conflict',
        'actors': ['Russia', 'Ukraine', 'NATO'],
        'location': 'Eastern Europe',
        'description': 'Conflict over territorial sovereignty and international borders'
    }
    realpolitik_result = realpolitik_analyzer.analyze_power_dynamics(test_event)
    print(f"Power Distribution: {realpolitik_result['power_distribution']['distribution_type']}")
    print(f"Framework Compliance: {realpolitik_result['framework_compliance']:.1f}%")

    # Test Balance Power Calculator
    print("\nTesting Balance of Power Calculator:")
    balance_calculator = BalancePowerCalculator()
    test_actors = [
        {
            'name': 'NATO',
            'capabilities': {
                'military': {'personnel': 85, 'equipment': 90, 'technology': 95, 'nuclear': 100},
                'economic': {'gdp': 80, 'trade': 85, 'technology': 90, 'resources': 75},
                'political': {'leadership': 80, 'institutions': 85, 'cohesion': 70, 'alliances': 95},
                'geographic': {'location': 85, 'size': 80, 'resources': 75, 'access': 90}
            }
        },
        {
            'name': 'Russia',
            'capabilities': {
                'military': {'personnel': 75, 'equipment': 70, 'technology': 75, 'nuclear': 95},
                'economic': {'gdp': 60, 'trade': 65, 'technology': 65, 'resources': 90},
                'political': {'leadership': 85, 'institutions': 70, 'cohesion': 80, 'alliances': 75},
                'geographic': {'location': 90, 'size': 95, 'resources': 95, 'access': 80}
            }
        }
    ]
    balance_result = balance_calculator.calculate_power_balance(test_actors)
    print(f"Balance Quality: {balance_result['distribution_analysis']['balance_quality']}")
    print(f"Stability Assessment: {balance_result['stability_assessment']['stability_rating']}")

    print(f"\nTest suite completed. Overall readiness: {'PRODUCTION READY' if results['average_compliance'] >= 70 else 'NEEDS IMPROVEMENT'}")