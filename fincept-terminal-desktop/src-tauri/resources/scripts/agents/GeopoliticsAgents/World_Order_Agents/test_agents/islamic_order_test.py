"""
ISLAMIC ORDER AGENT TEST SUITE - Henry Kissinger Framework

===== TESTING FRAMEWORK =====
# INPUT:
#   - dummy_islamic_world_events
#   - religious_political_test_cases
#   - islamic_civilization_checks
#
# OUTPUT:
#   - Islamic framework validation
#   - Ummah unity analysis verification
#   - Religious authority assessment
#   - Historical continuity from Islamic traditions
#
# PARAMETERS:
#   - test_type="islamic_civilization"
#   - compliance_threshold="70%"
#   - validation_depth="thorough"
"""

import sys
import os
import json
from typing import Dict, List, Any

# Add the parent directories to the path
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from civilization_agents.islamic_world_agent import islamic_world_agent

class IslamicOrderTestSuite:
    """
    TEST SUITE FOR ISLAMIC ORDER AGENT
    Validates agent performance against Kissinger's Islamic world order framework
    """

    def __init__(self):
        self.test_cases = self._initialize_test_cases()
        self.compliance_threshold = 70.0
        self.test_results = []

    def _initialize_test_cases(self) -> List[Dict[str, Any]]:
        """Initialize comprehensive test cases for Islamic order analysis"""

        return [
            {
                'name': 'Ummah Unity Test',
                'description': 'Test agent analysis of Muslim community unity beyond state boundaries',
                'events': [
                    {
                        'type': 'islamic_cooperation',
                        'actors': ['Saudi_Arabia', 'Iran', 'Turkey', 'Muslim_communities'],
                        'location': 'Middle_East',
                        'description': 'Islamic countries discussing pan-Islamic cooperation and unity',
                        'date': '2024-01-20'
                    }
                ],
                'expected_framework_elements': [
                    'ummah_unity',
                    'transnational_islamic_community',
                    'religious_bonds',
                    'community_beyond_borders'
                ],
                'min_compliance_score': 70.0
            },
            {
                'name': 'Religious-Political Authority Test',
                'description': 'Test agent understanding of Islamic religious-political dynamics',
                'events': [
                    {
                        'type': 'religious_political_dynamics',
                        'actors': ['Muslim_Brotherhood', 'Egyptian_Government', 'Religious_authorities'],
                        'location': 'Egypt',
                        'description': 'Tensions between religious authority and state power in governance',
                        'date': '2024-02-15'
                    }
                ],
                'expected_framework_elements': [
                    'religious_authority',
                    'political_islam',
                    'state_religion_balance',
                    'islamic_governance'
                ],
                'min_compliance_score': 75.0
            },
            {
                'name': 'Caliphate Tradition Test',
                'description': 'Test agent recognition of caliphate tradition influence',
                'events': [
                    {
                        'type': 'islamic_governance_discussion',
                        'actors': ['Islamic_movements', 'Regional_governments', 'Religious_scholars'],
                        'location': 'Various_Muslim_countries',
                        'description': 'Discussions about Islamic governance models and historical caliphate',
                        'date': '2024-03-10'
                    }
                ],
                'expected_framework_elements': [
                    'caliphate_legacy',
                    'historical_islamic_order',
                    'traditional_governance',
                    'islamic_political_theory'
                ],
                'min_compliance_score': 70.0
            },
            {
                'name': 'Dar al-Islam vs Dar al-Harb Test',
                'description': 'Test agent understanding of traditional Islamic worldview',
                'events': [
                    {
                        'type': 'islamic_worldview_analysis',
                        'actors': ['Muslim_countries', 'Non_muslim_powers', 'International_community'],
                        'location': 'Global',
                        'description': 'Analysis of Islamic world relations with non-Islamic world',
                        'date': '2024-04-05'
                    }
                ],
                'expected_framework_elements': [
                    'dar_al_islam_concept',
                    'islamic_worldview',
                    'civilizational_distinctions',
                    'traditional_categories'
                ],
                'min_compliance_score': 75.0
            },
            {
                'name': 'Sharia Law Influence Test',
                'description': 'Test agent analysis of Islamic legal framework influence',
                'events': [
                    {
                        'type': 'islamic_legal_framework',
                        'actors': ['Islamic_states', 'Legal_scholars', 'Reform_movements'],
                        'location': 'Muslim_world',
                        'description': 'Implementation and modernization of Islamic legal systems',
                        'date': '2024-05-12'
                    }
                ],
                'expected_framework_elements': [
                    'sharia_law',
                    'islamic_legal_traditions',
                    'religious_legitimacy',
                    'legal_frameworks'
                ],
                'min_compliance_score': 70.0
            }
        ]

    def run_all_tests(self) -> Dict[str, Any]:
        """Run all test cases and return comprehensive results"""
        print("=== ISLAMIC ORDER AGENT TEST SUITE ===")
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
        overall_results['framework_validation'] = self._validate_islamic_framework_compliance()

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
            'test_mode': True,
            'civilization_focus': 'islamic'
        }

        # Run the Islamic World agent
        try:
            agent_result = islamic_world_agent(test_state)

            # Calculate compliance score
            compliance_score = agent_result.get('framework_compliance', {}).get('kissinger_score', 0.0)

            # Check for expected framework elements
            result_elements = self._extract_framework_elements(agent_result)
            missing_elements = [elem for elem in expected_elements if elem not in result_elements]

            # Validate Islamic framework requirements
            framework_validation = self._validate_islamic_requirements(agent_result)

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
        """Extract Islamic framework elements from agent result"""
        elements = []

        # Check for Islamic order analysis
        if agent_result.get('islamic_order_analysis'):
            elements.append('islamic_framework')
            if agent_result['islamic_order_analysis'].get('ummah_unity_assessment'):
                elements.append('ummah_unity')
            if agent_result['islamic_order_analysis'].get('caliphate_tradition_influence'):
                elements.append('caliphate_legacy')

        # Check for regional dynamics
        if agent_result.get('regional_dynamics'):
            elements.append('regional_dynamics')
            if agent_result['regional_dynamics'].get('islamic_state_relations'):
                elements.append('islamic_state_relations')

        # Check for historical continuity
        if agent_result.get('historical_continuity'):
            elements.append('historical_continuity')
            if any('caliphate' in str(pattern).lower()
                   for pattern in agent_result['historical_continuity'].get('caliphate_legacy', [])):
                elements.append('caliphate_legacy')

        # Check for diplomatic recommendations
        if agent_result.get('diplomatic_recommendations'):
            elements.append('diplomatic_recommendations')
            recommendations = agent_result['diplomatic_recommendations']
            if recommendations.get('islamic_framework_solutions'):
                elements.append('islamic_solutions')

        # Check for specific Islamic concepts
        result_str = json.dumps(agent_result).lower()
        if 'ummah' in result_str:
            elements.append('ummah_unity')
        if 'caliphate' in result_str:
            elements.append('caliphate_legacy')
        if 'sharia' in result_str:
            elements.append('sharia_law')
        if 'dar al-islam' in result_str or 'dar al' in result_str:
            elements.append('dar_al_islam_concept')
        if 'religious authority' in result_str:
            elements.append('religious_authority')
        if 'islamic' in result_str:
            elements.append('islamic_framework')
        if 'transnational' in result_str:
            elements.append('transnational_islamic_community')

        return list(set(elements))  # Remove duplicates

    def _validate_islamic_requirements(self, agent_result: Dict[str, Any]) -> Dict[str, bool]:
        """Validate specific Islamic framework requirements"""
        validation = {
            'islamic_cultural_understanding': False,
            'historical_continuity_present': False,
            'religious_political_analysis': False,
            'ummah_concept_understanding': False,
            'regional_dynamics_analysis': False
        }

        # Check Islamic cultural understanding
        if agent_result.get('islamic_order_analysis'):
            validation['islamic_cultural_understanding'] = True

        # Check historical continuity
        if agent_result.get('historical_continuity', {}).get('caliphate_legacy'):
            validation['historical_continuity_present'] = True

        # Check religious-political analysis
        if agent_result.get('regional_dynamics', {}).get('religious_political_balance'):
            validation['religious_political_analysis'] = True

        # Check Ummah concept understanding
        ummah_assessment = agent_result.get('islamic_order_analysis', {}).get('ummah_unity_assessment')
        if ummah_assessment:
            validation['ummah_concept_understanding'] = True

        # Check regional dynamics analysis
        if agent_result.get('regional_dynamics', {}).get('islamic_state_relations'):
            validation['regional_dynamics_analysis'] = True

        return validation

    def _validate_islamic_framework_compliance(self) -> Dict[str, Any]:
        """Validate overall Islamic framework compliance across all tests"""
        validation_summary = {
            'islamic_framework_compliance': 0.0,
            'cultural_understanding_score': 0.0,
            'historical_continuity_score': 0.0,
            'religious_political_score': 0.0,
            'ummah_concept_score': 0.0,
            'overall_framework_health': 'unknown'
        }

        if not self.test_results:
            return validation_summary

        # Calculate average scores for each framework component
        total_tests = len(self.test_results)
        cultural_count = 0
        historical_count = 0
        religious_count = 0
        ummah_count = 0

        for result in self.test_results:
            if result.get('framework_validation'):
                validation = result['framework_validation']
                if validation.get('islamic_cultural_understanding'):
                    cultural_count += 1
                if validation.get('historical_continuity_present'):
                    historical_count += 1
                if validation.get('religious_political_analysis'):
                    religious_count += 1
                if validation.get('ummah_concept_understanding'):
                    ummah_count += 1

        validation_summary['cultural_understanding_score'] = (cultural_count / total_tests) * 100
        validation_summary['historical_continuity_score'] = (historical_count / total_tests) * 100
        validation_summary['religious_political_score'] = (religious_count / total_tests) * 100
        validation_summary['ummah_concept_score'] = (ummah_count / total_tests) * 100

        # Calculate overall framework health
        framework_scores = [
            validation_summary['cultural_understanding_score'],
            validation_summary['historical_continuity_score'],
            validation_summary['religious_political_score'],
            validation_summary['ummah_concept_score']
        ]

        validation_summary['islamic_framework_compliance'] = sum(framework_scores) / len(framework_scores)

        if validation_summary['islamic_framework_compliance'] >= 80:
            validation_summary['overall_framework_health'] = 'excellent'
        elif validation_summary['islamic_framework_compliance'] >= 70:
            validation_summary['overall_framework_health'] = 'good'
        elif validation_summary['islamic_framework_compliance'] >= 60:
            validation_summary['overall_framework_health'] = 'fair'
        else:
            validation_summary['overall_framework_health'] = 'poor'

        return validation_summary

    def generate_test_report(self, results: Dict[str, Any]) -> str:
        """Generate comprehensive test report"""
        report = []
        report.append("=" * 60)
        report.append("ISLAMIC ORDER AGENT TEST REPORT")
        report.append("=" * 60)
        report.append(f"Total Tests: {results['total_tests']}")
        report.append(f"Passed: {results['passed_tests']}")
        report.append(f"Failed: {results['failed_tests']}")
        report.append(f"Success Rate: {results['success_rate']:.1f}%")
        report.append(f"Average Compliance: {results['average_compliance']:.1f}%")
        report.append("")

        # Framework validation summary
        framework_validation = results['framework_validation']
        report.append("ISLAMIC FRAMEWORK VALIDATION SUMMARY:")
        report.append(f"  Islamic Framework Compliance: {framework_validation['islamic_framework_compliance']:.1f}%")
        report.append(f"  Cultural Understanding: {framework_validation['cultural_understanding_score']:.1f}%")
        report.append(f"  Historical Continuity: {framework_validation['historical_continuity_score']:.1f}%")
        report.append(f"  Religious-Political Analysis: {framework_validation['religious_political_score']:.1f}%")
        report.append(f"  Ummah Concept Understanding: {framework_validation['ummah_concept_score']:.1f}%")
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
            report.append("  - Agent needs significant improvement to meet Islamic framework standards")
            report.append("  - Focus on Islamic cultural and historical understanding")
            report.append("  - Strengthen religious-political dynamics analysis")
        elif results['average_compliance'] < 80:
            report.append("  - Agent performs adequately but could be enhanced")
            report.append("  - Consider adding more Islamic historical context")
            report.append("  - Refine Ummah concept analysis")
        else:
            report.append("  - Agent demonstrates strong Islamic framework compliance")
            report.append("  - Ready for production deployment")
            report.append("  - Consider additional Islamic civilization testing")

        return "\n".join(report)

# Example usage and testing
if __name__ == "__main__":
    # Initialize and run test suite
    test_suite = IslamicOrderTestSuite()
    results = test_suite.run_all_tests()

    # Generate and display report
    report = test_suite.generate_test_report(results)
    print(report)

    print(f"\nTest suite completed. Overall readiness: {'PRODUCTION READY' if results['average_compliance'] >= 70 else 'NEEDS IMPROVEMENT'}")