"""
COMPREHENSIVE TEST FRAMEWORK
Complete Testing System for Grand Chessboard Agents

This module provides comprehensive testing capabilities for all Grand Chessboard agents,
including individual agent testing, inter-agent communication testing,
framework compliance validation, and integration testing.
"""

import logging
import asyncio
import json
from typing import Dict, List, Any, Optional, Callable
from dataclasses import dataclass, field
from datetime import datetime
import time
import unittest
from enum import Enum

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

class TestType(Enum):
    """Types of tests for Grand Chessboard agents"""
    UNIT_TEST = "unit_test"                    # Individual component testing
    INTEGRATION_TEST = "integration_test"        # Agent integration testing
    FRAMEWORK_COMPLIANCE = "framework_compliance"  # Brzezinski framework compliance
    INTER_AGENT_COMMUNICATION = "inter_agent_communication"  # Communication between agents
    COORDINATION_TEST = "coordination_test"     # Multi-agent coordination
    PERFORMANCE_TEST = "performance_test"         # Performance and scalability
    END_TO_END_TEST = "end_to_end_test"      # Complete workflow testing

class TestResult(Enum):
    """Test result status"""
    PASSED = "passed"
    FAILED = "failed"
    SKIPPED = "skipped"
    ERROR = "error"
    TIMEOUT = "timeout"

@dataclass
class TestScenario:
    """Test scenario definition"""
    id: str
    name: str
    description: str
    test_type: TestType
    test_data: Dict[str, Any]
    expected_results: Dict[str, Any]
    agents_involved: List[str]
    timeout: int = 300  # 5 minutes default
    priority: str = "normal"  # high, normal, low

@dataclass
class TestExecution:
    """Result of test execution"""
    scenario_id: str
    agent_id: Optional[str]
    test_type: TestType
    start_time: datetime
    end_time: Optional[datetime]
    result: TestResult
    actual_results: Optional[Dict[str, Any]]
    expected_results: Dict[str, Any]
    errors: List[str] = field(default_factory=list)
    performance_metrics: Dict[str, float] = field(default_factory=dict)
    compliance_score: Optional[float] = None

@dataclass
class TestSuite:
    """Collection of test scenarios for specific testing"""
    name: str
    description: str
    scenarios: List[TestScenario]
    setup_functions: Dict[str, Callable] = field(default_factory=dict)
    teardown_functions: Dict[str, Callable] = field(default_factory=dict)
    test_data_generators: Dict[str, Callable] = field(default_factory=dict)

class GrandChessboardTestFramework:
    """
    Comprehensive testing framework for Grand Chessboard agents

    Features:
    - Individual agent testing with framework compliance validation
    - Inter-agent communication and coordination testing
    - Brzezinski framework compliance scoring
    - Performance benchmarking and scalability testing
    - Test data generation and management
    - Automated test execution and reporting
    - Integration testing with real LLM providers
    """

    def __init__(self):
        """Initialize the comprehensive test framework"""
        self.framework_id = "comprehensive_test_framework"

        # Test execution tracking
        self.test_executions = []
        self.test_suites = {}

        # Mock data and providers
        self.mock_data_providers = {}
        self.llm_providers = {}

        # Test configuration
        self.default_timeout = 300  # 5 minutes
        self.parallel_execution = True
        self.max_parallel_tests = 5

        # Initialize test suites
        self._initialize_test_suites()

    def _initialize_test_suites(self) -> None:
        """Initialize all test suites for Grand Chessboard agents"""

        # Individual Agent Test Suite
        self.test_suites["individual_agents"] = TestSuite(
            name="Individual Agent Tests",
            description="Test each Grand Chessboard agent independently",
            scenarios=self._create_individual_agent_scenarios()
        )

        # Framework Compliance Test Suite
        self.test_suites["framework_compliance"] = TestSuite(
            name="Brzezinski Framework Compliance",
            description="Validate compliance with Brzezinski's theoretical framework",
            scenarios=self._create_framework_compliance_scenarios()
        )

        # Inter-Agent Communication Test Suite
        self.test_suites["inter_agent_communication"] = TestSuite(
            name="Inter-Agent Communication Tests",
            description="Test communication and coordination between agents",
            scenarios=self._create_communication_test_scenarios()
        )

        # Integration Test Suite
        self.test_suites["integration"] = TestSuite(
            name="Integration Tests",
            description="Test complete workflow and data flow between components",
            scenarios=self._create_integration_test_scenarios()
        )

        # Performance Test Suite
        self.test_suites["performance"] = TestSuite(
            name="Performance Tests",
            description="Test performance, scalability, and resource usage",
            scenarios=self._create_performance_test_scenarios()
        )

        # End-to-End Test Suite
        self.test_suites["end_to_end"] = TestSuite(
            name="End-to-End Workflow Tests",
            description="Test complete Grand Chessboard analysis workflows",
            scenarios=self._create_end_to_end_scenarios()
        )

    def run_test_suite(
        self,
        suite_name: str,
        parallel: bool = None,
        include_compliance: bool = True
    ) -> Dict[str, Any]:
        """
        Run a complete test suite

        Args:
            suite_name: Name of test suite to run
            parallel: Whether to run tests in parallel (default from framework config)
            include_compliance: Whether to include framework compliance tests

        Returns:
            Comprehensive test results and analysis
        """
        try:
            if suite_name not in self.test_suites:
                raise ValueError(f"Test suite '{suite_name}' not found")

            logger.info(f"Running test suite: {suite_name}")
            start_time = datetime.now()

            test_suite = self.test_suites[suite_name]
            execution_results = []

            # Configure parallel execution
            use_parallel = parallel if parallel is not None else self.parallel_execution

            if use_parallel:
                execution_results = self._run_tests_parallel(test_suite.scenarios)
            else:
                execution_results = self._run_tests_sequential(test_suite.scenarios)

            end_time = datetime.now()
            total_duration = (end_time - start_time).total_seconds()

            # Run framework compliance tests if requested
            compliance_results = None
            if include_compliance and suite_name != "framework_compliance":
                compliance_results = self._run_framework_compliance_tests(
                    [scenario.id for scenario in test_suite.scenarios]
                )

            # Analyze results
            analysis = self._analyze_test_results(execution_results, compliance_results)

            return {
                "suite_name": suite_name,
                "total_scenarios": len(test_suite.scenarios),
                "executed_tests": len(execution_results),
                "passed_tests": len([e for e in execution_results if e.result == TestResult.PASSED]),
                "failed_tests": len([e for e in execution_results if e.result == TestResult.FAILED]),
                "total_duration": total_duration,
                "execution_results": execution_results,
                "compliance_results": compliance_results,
                "analysis": analysis,
                "summary": self._generate_test_summary(execution_results, analysis)
            }

        except Exception as e:
            logger.error(f"Error running test suite {suite_name}: {str(e)}")
            return {
                "suite_name": suite_name,
                "error": str(e),
                "total_scenarios": 0,
                "executed_tests": 0,
                "passed_tests": 0,
                "failed_tests": 0,
                "total_duration": 0,
                "execution_results": [],
                "compliance_results": None,
                "analysis": None,
                "summary": None
            }

    def run_framework_compliance_tests(self, agent_ids: List[str]) -> Dict[str, Any]:
        """
        Run Brzezinski framework compliance tests for specified agents

        Args:
            agent_ids: List of agent IDs to test for compliance

        Returns:
            Compliance test results and scores
        """
        try:
            logger.info("Running framework compliance tests")
            compliance_suite = self.test_suites["framework_compliance"]

            # Filter scenarios for specific agents
            relevant_scenarios = [
                scenario for scenario in compliance_suite.scenarios
                if any(agent_id in scenario.agents_involved for agent_id in agent_ids)
            ]

            # Run compliance tests
            execution_results = self._run_tests_sequential(relevant_scenarios)

            # Calculate compliance scores
            compliance_scores = {}
            for agent_id in agent_ids:
                agent_results = [
                    result for result in execution_results
                    if result.agent_id == agent_id
                ]
                if agent_results:
                    scores = [
                        result.compliance_score for result in agent_results
                        if result.compliance_score is not None
                    ]
                    if scores:
                        compliance_scores[agent_id] = sum(scores) / len(scores)
                    else:
                        compliance_scores[agent_id] = 0.0
                else:
                    compliance_scores[agent_id] = None

            # Analyze compliance results
            analysis = self._analyze_compliance_results(execution_results, compliance_scores)

            return {
                "suite_name": "framework_compliance",
                "tested_agents": agent_ids,
                "total_scenarios": len(relevant_scenarios),
                "executed_tests": len(execution_results),
                "passed_tests": len([e for e in execution_results if e.result == TestResult.PASSED]),
                "failed_tests": len([e for e in execution_results if e.result == TestResult.FAILED]),
                "compliance_scores": compliance_scores,
                "execution_results": execution_results,
                "analysis": analysis,
                "compliance_summary": self._generate_compliance_summary(compliance_scores, analysis)
            }

        except Exception as e:
            logger.error(f"Error running framework compliance tests: {str(e)}")
            return {
                "error": str(e),
                "tested_agents": agent_ids,
                "total_scenarios": 0,
                "executed_tests": 0,
                "passed_tests": 0,
                "failed_tests": 0,
                "compliance_scores": {},
                "execution_results": [],
                "analysis": None,
                "compliance_summary": None
            }

    def run_performance_benchmarks(self, agent_ids: List[str]) -> Dict[str, Any]:
        """
        Run performance benchmarks for specified agents

        Args:
            agent_ids: List of agent IDs to benchmark

        Returns:
            Performance benchmark results
        """
        try:
            logger.info(f"Running performance benchmarks for agents: {agent_ids}")
            performance_suite = self.test_suites["performance"]

            # Filter performance scenarios for specified agents
            relevant_scenarios = [
                scenario for scenario in performance_suite.scenarios
                if any(agent_id in scenario.agents_involved for agent_id in agent_ids)
            ]

            # Run performance tests
            execution_results = self._run_tests_sequential(relevant_scenarios)

            # Analyze performance metrics
            performance_analysis = self._analyze_performance_results(execution_results)

            return {
                "suite_name": "performance",
                "tested_agents": agent_ids,
                "total_scenarios": len(relevant_scenarios),
                "executed_tests": len(execution_results),
                "execution_results": execution_results,
                "performance_analysis": performance_analysis,
                "benchmark_summary": self._generate_performance_summary(performance_analysis)
            }

        except Exception as e:
            logger.error(f"Error running performance benchmarks: {str(e)}")
            return {
                "error": str(e),
                "tested_agents": agent_ids,
                "total_scenarios": 0,
                "executed_tests": 0,
                "execution_results": [],
                "performance_analysis": None,
                "benchmark_summary": None
            }

    def _create_individual_agent_scenarios(self) -> List[TestScenario]:
        """Create test scenarios for individual agent testing"""

        scenarios = []

        # Master Agent Tests
        scenarios.extend([
            TestScenario(
                id="master_agent_basic_analysis",
                name="Master Agent Basic Analysis",
                description="Test master agent's core analysis capabilities",
                test_type=TestType.UNIT_TEST,
                test_data=self._generate_master_agent_test_data(),
                expected_results={
                    "primacy_assessment": {"status": "calculated"},
                    "hegemony_analysis": {"status": "analyzed"},
                    "strategic_recommendations": {"count": ">0"}
                },
                agents_involved=["eurasian_chessboard_master"]
            ),
            TestScenario(
                id="master_agent_data_integration",
                name="Master Agent Data Integration",
                description="Test master agent's ability to integrate multiple data sources",
                test_type=TestType.INTEGRATION_TEST,
                test_data=self._generate_comprehensive_test_data(),
                expected_results={
                    "data_integration": {"status": "successful"},
                    "coherence_score": {"value": ">0.7"}
                },
                agents_involved=["eurasian_chessboard_master"]
            )
        ])

        # European Bridgehead Agent Tests
        scenarios.extend([
            TestScenario(
                id="european_integration_analysis",
                name="European Integration Analysis",
                description="Test European agent's integration assessment",
                test_type=TestType.UNIT_TEST,
                test_data=self._generate_european_test_data(),
                expected_results={
                    "integration_status": {"status": "determined"},
                    "nato_effectiveness": {"status": "analyzed"}
                },
                agents_involved=["democratic_bridgehead"]
            )
        ])

        # Black Hole Agent Tests
        scenarios.extend([
            TestScenario(
                id="russian_trajectory_analysis",
                name="Russian Trajectory Analysis",
                description="Test Black Hole agent's Russian trajectory assessment",
                test_type=TestType.UNIT_TEST,
                test_data=self._generate_russian_test_data(),
                expected_results={
                    "trajectory": {"status": "determined"},
                    "containment_needs": {"status": "assessed"}
                },
                agents_involved=["black_hole"]
            )
        ])

        # Eurasian Balkans Agent Tests
        scenarios.extend([
            TestScenario(
                id="central_asia_competition",
                name="Central Asia Competition Analysis",
                description="Test Balkans agent's competition assessment",
                test_type=TestType.UNIT_TEST,
                test_data=self._generate_central_asia_test_data(),
                expected_results={
                    "stability_level": {"status": "assessed"},
                    "power_distribution": {"status": "calculated"}
                },
                agents_involved=["eurasian_balkans"]
            )
        ])

        # Far Eastern Anchor Agent Tests
        scenarios.extend([
            TestScenario(
                id="east_asia_balance",
                name="East Asia Balance Analysis",
                description="Test Far Eastern agent's balance assessment",
                test_type=TestType.UNIT_TEST,
                test_data=self._generate_east_asia_test_data(),
                expected_results={
                    "balance_level": {"status": "determined"},
                    "china_trajectory": {"status": "analyzed"}
                },
                agents_involved=["far_eastern_anchor"]
            )
        ])

        return scenarios

    def _create_framework_compliance_scenarios(self) -> List[TestScenario]:
        """Create test scenarios for Brzezinski framework compliance"""

        scenarios = []

        # Core Framework Compliance Tests
        scenarios.extend([
            TestScenario(
                id="eurasian_primacy_focus",
                name="Eurasian Primacy Focus",
                description="Test agent focus on Eurasian competition dynamics",
                test_type=TestType.FRAMEWORK_COMPLIANCE,
                test_data=self._generate_framework_test_data(),
                expected_results={
                    "eurasian_focus": {"compliance_score": ">0.8"},
                    "brzezinski_concepts": {"recognition_rate": ">0.7"}
                },
                agents_involved=["eurasian_chessboard_master", "democratic_bridgehead",
                              "black_hole", "eurasian_balkans", "far_eastern_anchor"]
            ),
            TestScenario(
                id="american_primacy_maintenance",
                name="American Primacy Maintenance",
                description="Test focus on American global leadership",
                test_type=TestType.FRAMEWORK_COMPLIANCE,
                test_data=self._generate_framework_test_data(),
                expected_results={
                    "american_primacy": {"compliance_score": ">0.8"},
                    "balance_management": {"assessment": "present"}
                },
                agents_involved=["eurasian_chessboard_master", "american_hegemony"]
            )
        ])

        return scenarios

    def _create_communication_test_scenarios(self) -> List[TestScenario]:
        """Create test scenarios for inter-agent communication"""

        scenarios = []

        # Message Passing Tests
        scenarios.extend([
            TestScenario(
                id="insight_sharing_test",
                name="Agent Insight Sharing",
                description="Test agents sharing insights with each other",
                test_type=TestType.INTER_AGENT_COMMUNICATION,
                test_data=self._generate_communication_test_data(),
                expected_results={
                    "message_delivery": {"success_rate": "1.0"},
                    "insight_integration": {"status": "successful"}
                },
                agents_involved=["democratic_bridgehead", "black_hole", "eurasian_balkans"]
            ),
            TestScenario(
                id="coordination_request_test",
                name="Multi-Agent Coordination",
                description="Test coordination requests between agents",
                test_type=TestType.COORDINATION_TEST,
                test_data=self._generate_coordination_test_data(),
                expected_results={
                    "coordination_success": {"status": "completed"},
                    "result_synthesis": {"status": "successful"}
                },
                agents_involved=["eurasian_chessboard_master", "democratic_bridgehead", "far_eastern_anchor"]
            )
        ])

        return scenarios

    def _create_integration_test_scenarios(self) -> List[TestScenario]:
        """Create end-to-end integration test scenarios"""

        scenarios = []

        # Complete Analysis Workflow Tests
        scenarios.extend([
            TestScenario(
                id="complete_chessboard_analysis",
                name="Complete Chessboard Analysis",
                description="Test complete Grand Chessboard analysis workflow",
                test_type=TestType.END_TO_END_TEST,
                test_data=self._generate_integration_test_data(),
                expected_results={
                    "workflow_completion": {"status": "successful"},
                    "analysis_coherence": {"score": ">0.8"}
                },
                agents_involved=["eurasian_chessboard_master", "democratic_bridgehead",
                              "black_hole", "eurasian_balkans", "far_eastern_anchor"]
            )
        ])

        return scenarios

    def _create_performance_test_scenarios(self) -> List[TestScenario]:
        """Create performance testing scenarios"""

        scenarios = []

        # Load and Performance Tests
        scenarios.extend([
            TestScenario(
                id="large_data_processing",
                name="Large Data Processing",
                description="Test agent performance with large datasets",
                test_type=TestType.PERFORMANCE_TEST,
                test_data=self._generate_large_test_data(),
                expected_results={
                    "processing_time": {"max_seconds": 60},
                    "memory_usage": {"max_mb": 512}
                },
                agents_involved=["eurasian_chessboard_master"]
            ),
            TestScenario(
                id="concurrent_analysis",
                name="Concurrent Analysis",
                description="Test agent performance under concurrent requests",
                test_type=TestType.PERFORMANCE_TEST,
                test_data=self._generate_concurrent_test_data(),
                expected_results={
                    "response_time": {"max_ms": 5000},
                    "concurrent_handling": {"success_rate": "1.0"}
                },
                agents_involved=["democratic_bridgehead", "black_hole", "eurasian_balkans"]
            )
        ])

        return scenarios

    def _create_end_to_end_scenarios(self) -> List[TestScenario]:
        """Create end-to-end workflow test scenarios"""

        scenarios = []

        # Strategic Analysis Workflow
        scenarios.extend([
            TestScenario(
                id="strategic_assessment_workflow",
                name="Strategic Assessment Workflow",
                description="Test complete strategic assessment workflow",
                test_type=TestType.END_TO_END_TEST,
                test_data=self._generate_strategic_workflow_data(),
                expected_results={
                    "workflow_success": {"status": "completed"},
                    "strategic_output": {"completeness": ">0.9"}
                },
                agents_involved=["eurasian_chessboard_master", "all_agents"]
            )
        ])

        return scenarios

    def _run_tests_sequential(self, scenarios: List[TestScenario]) -> List[TestExecution]:
        """Run test scenarios sequentially"""
        execution_results = []

        for scenario in scenarios:
            execution = self._execute_single_test(scenario)
            execution_results.append(execution)

        return execution_results

    def _run_tests_parallel(self, scenarios: List[TestScenario]) -> List[TestExecution]:
        """Run test scenarios in parallel"""
        execution_results = []

        # Split scenarios into batches
        batch_size = min(self.max_parallel_tests, len(scenarios))
        batches = [scenarios[i:i + batch_size] for i in range(0, len(scenarios), batch_size)]

        for batch in batches:
            # Execute batch in parallel
            batch_results = []
            with ThreadPoolExecutor(max_workers=batch_size) as executor:
                futures = [
                    executor.submit(self._execute_single_test, scenario)
                    for scenario in batch
                ]

                for future in as_completed(futures):
                    try:
                        result = future.result(timeout=scenario.timeout)
                        batch_results.append(result)
                    except TimeoutError:
                        # Create timeout result
                        timeout_result = TestExecution(
                            scenario_id=scenario.id,
                            agent_id=None,
                            test_type=scenario.test_type,
                            start_time=result.start_time,
                            end_time=datetime.now(),
                            result=TestResult.TIMEOUT,
                            actual_results=None,
                            expected_results=scenario.expected_results,
                            errors=["Test execution timed out"]
                        )
                        batch_results.append(timeout_result)
                    except Exception as e:
                        # Create error result
                        error_result = TestExecution(
                            scenario_id=scenario.id,
                            agent_id=None,
                            test_type=scenario.test_type,
                            start_time=datetime.now(),
                            end_time=datetime.now(),
                            result=TestResult.ERROR,
                            actual_results=None,
                            expected_results=scenario.expected_results,
                            errors=[str(e)]
                        )
                        batch_results.append(error_result)

            execution_results.extend(batch_results)

        return execution_results

    def _execute_single_test(self, scenario: TestScenario) -> TestExecution:
        """Execute a single test scenario"""
        start_time = datetime.now()

        try:
            # Import and execute the appropriate agent
            agent_result = self._execute_agent_test(scenario)

            # Compare with expected results
            test_result = self._compare_results(agent_result, scenario.expected_results)

            end_time = datetime.now()
            execution_time = (end_time - start_time).total_seconds()

            return TestExecution(
                scenario_id=scenario.id,
                agent_id=scenario.agents_involved[0] if scenario.agents_involved else None,
                test_type=scenario.test_type,
                start_time=start_time,
                end_time=end_time,
                result=test_result,
                actual_results=agent_result,
                expected_results=scenario.expected_results,
                performance_metrics={"execution_time": execution_time},
                compliance_score=self._calculate_compliance_score(agent_result, scenario)
            )

        except Exception as e:
            return TestExecution(
                scenario_id=scenario.id,
                agent_id=scenario.agents_involved[0] if scenario.agents_involved else None,
                test_type=scenario.test_type,
                start_time=start_time,
                end_time=datetime.now(),
                result=TestResult.ERROR,
                actual_results=None,
                expected_results=scenario.expected_results,
                errors=[str(e)],
                performance_metrics={},
                compliance_score=0.0
            )

    # Test data generation methods
    def _generate_master_agent_test_data(self) -> Dict[str, Any]:
        """Generate test data for master agent"""
        return {
            "current_events": [
                {"id": "1", "type": "geopolitical", "description": "China expands military presence"},
                {"id": "2", "type": "economic", "description": "NATO announces new initiatives"}
            ],
            "strategic_moves_by_powers": {
                "military_moves": {"china": "expansion", "russia": "containment"},
                "economic_moves": {"us": "sanctions", "eu": "cooperation"}
            },
            "military_deployments": [
                {"location": "pacific", "force": "carrier_group"},
                {"location": "europe", "force": "enhanced_forward_presence"}
            ],
            "economic_indicators": {
                "us_growth": 2.1, "china_growth": 4.8, "eu_growth": 1.4
            }
        }

    def _generate_framework_test_data(self) -> Dict[str, Any]:
        """Generate test data for framework compliance"""
        return {
            "test_scenarios": [
                {"focus": "eurasian_competition", "complexity": "high"},
                {"focus": "american_primacy", "complexity": "medium"},
                {"focus": "balance_management", "complexity": "high"}
            ],
            "brzezinski_concepts": [
                "eurasian_chessboard", "geostrategic_players", "geopolitical_pivots",
                "democratic_bridgehead", "black_hole", "eurasian_balkans",
                "far_eastern_anchor", "american_hegemony"
            ]
        }

    # Additional test data generation methods...
    def _generate_european_test_data(self) -> Dict[str, Any]:
        """Generate test data for European agent"""
        return {
            "european_events": [
                {"id": "1", "type": "political", "description": "EU announces defense integration"},
                {"id": "2", "type": "military", "description": "NATO expansion discussions"}
            ],
            "nato_developments": [
                {"id": "1", "type": "expansion", "status": "in_progress"}
            ],
            "eu_integration_data": {
                "policy_harmonization": 0.7,
                "economic_integration": 0.8
            }
        }

    def _generate_russian_test_data(self) -> Dict[str, Any]:
        """Generate test data for Black Hole agent"""
        return {
            "russian_events": [
                {"id": "1", "type": "military", "description": "Russia military modernization"},
                {"id": "2", "type": "political", "description": "Near abroad policy statements"}
            ],
            "russian_capabilities": {
                "military_capability": 0.7,
                "economic_strength": 0.5,
                "internal_stability": 0.6
            }
        }

    # Additional test data generators would be implemented here...

    def _compare_results(self, actual: Dict[str, Any], expected: Dict[str, Any]) -> TestResult:
        """Compare actual test results with expected results"""
        # Simplified comparison logic
        try:
            for key, expected_value in expected.items():
                if key in actual:
                    # Simple validation - could be enhanced
                    if isinstance(expected_value, dict):
                        if "status" in expected_value:
                            expected_status = expected_value["status"]
                            actual_status = actual[key].get("status", "")
                            if expected_status == actual_status:
                                continue
                            else:
                                return TestResult.FAILED
                    elif actual[key] == expected_value:
                        continue
                    else:
                        return TestResult.FAILED
                else:
                    return TestResult.FAILED

            return TestResult.PASSED

        except Exception as e:
            logger.error(f"Error comparing results: {str(e)}")
            return TestResult.ERROR

    def _calculate_compliance_score(self, result: Dict[str, Any], scenario: TestScenario) -> float:
        """Calculate compliance score for framework compliance tests"""
        if scenario.test_type != TestType.FRAMEWORK_COMPLIANCE:
            return None

        # Simplified compliance scoring
        try:
            compliance_factors = []

            # Check for Brzezinski concept recognition
            if "brzezinski_concepts" in result:
                concepts = result["brzezinski_concepts"]
                recognized = sum(1 for concept in concepts if concept.get("recognized", False))
                compliance_factors.append(recognized / len(concepts))

            # Check for Eurasian focus
            if "eurasian_focus" in result:
                focus_score = result["eurasian_focus"].get("score", 0)
                compliance_factors.append(focus_score)

            # Check for American primacy focus
            if "american_primacy" in result:
                primacy_score = result["american_primacy"].get("score", 0)
                compliance_factors.append(primacy_score)

            if compliance_factors:
                return sum(compliance_factors) / len(compliance_factors)
            else:
                return 0.5  # Default score

        except Exception as e:
            logger.error(f"Error calculating compliance score: {str(e)}")
            return 0.0

    # Analysis methods
    def _analyze_test_results(self, executions: List[TestExecution], compliance_results: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
        """Analyze test execution results"""
        passed_count = len([e for e in executions if e.result == TestResult.PASSED])
        failed_count = len([e for e in executions if e.result == TestResult.FAILED])
        error_count = len([e for e in executions if e.result == TestResult.ERROR])

        # Calculate average compliance scores
        compliance_scores = [e.compliance_score for e in executions if e.compliance_score is not None]
        avg_compliance = sum(compliance_scores) / len(compliance_scores) if compliance_scores else 0

        # Performance analysis
        execution_times = [e.performance_metrics.get("execution_time", 0) for e in executions]
        avg_execution_time = sum(execution_times) / len(execution_times) if execution_times else 0

        return {
            "total_tests": len(executions),
            "passed_tests": passed_count,
            "failed_tests": failed_count,
            "error_tests": error_count,
            "success_rate": passed_count / len(executions) if executions else 0,
            "average_compliance": avg_compliance,
            "average_execution_time": avg_execution_time,
            "performance_analysis": self._analyze_performance_metrics(executions),
            "test_coverage": self._calculate_test_coverage(executions)
        }

    def _generate_test_summary(self, executions: List[TestExecution], analysis: Dict[str, Any]) -> Dict[str, Any]:
        """Generate comprehensive test summary"""
        return {
            "test_summary": {
                "total_executed": len(executions),
                "success_rate": analysis.get("success_rate", 0),
                "framework_compliance": analysis.get("average_compliance", 0),
                "performance_rating": self._calculate_performance_rating(analysis.get("average_execution_time", 0))
            },
            "recommendations": self._generate_test_recommendations(executions, analysis),
            "next_steps": self._generate_next_steps(executions, analysis)
        }

# Main execution function
def run_grand_chessboard_tests(
    test_suites: Optional[List[str]] = None,
    parallel_execution: bool = True,
    include_compliance: bool = True
) -> Dict[str, Any]:
    """
    Main function to run Grand Chessboard agent tests

    Args:
        test_suites: List of test suite names to run (None for all)
        parallel_execution: Whether to run tests in parallel
        include_compliance: Whether to include framework compliance tests

    Returns:
        Comprehensive test results and analysis
    """
    framework = GrandChessboardTestFramework()

    # Determine which test suites to run
    if test_suites is None:
        test_suites = ["individual_agents", "framework_compliance", "inter_agent_communication",
                      "integration", "performance", "end_to_end"]
    else:
        test_suites = test_suites

    # Run specified test suites
    results = {}
    for suite_name in test_suites:
        if suite_name in framework.test_suites:
            results[suite_name] = framework.run_test_suite(
                suite_name, parallel_execution, include_compliance
            )

    return {
        "test_execution_summary": {
            "total_suites_run": len(results),
            "suite_results": results,
            "overall_success_rate": sum(
                suite_result.get("success_rate", 0) for suite_result in results.values()
            ) / len(results) if results else 0
        },
        "detailed_results": results
    }

if __name__ == "__main__":
    # Example usage
    results = run_grand_chessboard_tests(
        test_suites=["individual_agents", "framework_compliance"],
        parallel_execution=True,
        include_compliance=True
    )

    print("Grand Chessboard Agent Test Results:")
    print(f"Total Test Suites: {len(results['detailed_results'])}")
    print(f"Overall Success Rate: {results['test_execution_summary']['overall_success_rate']:.2%}")

    for suite_name, suite_results in results['detailed_results'].items():
        print(f"\n{suite_name} Results:")
        print(f"  Passed: {suite_results.get('passed_tests', 0)}")
        print(f"  Failed: {suite_results.get('failed_tests', 0)}")
        if suite_results.get('compliance_scores'):
            avg_compliance = sum(suite_results['compliance_scores'].values()) / len(suite_results['compliance_scores'])
            print(f"  Avg Compliance: {avg_compliance:.2%}")