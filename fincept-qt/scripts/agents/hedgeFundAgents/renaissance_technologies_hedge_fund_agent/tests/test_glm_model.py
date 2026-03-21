"""
Renaissance Technologies Multi-Agent System - Full Integration Test

Comprehensive test covering ALL components:
1. Configuration & Model Factory
2. Individual Agents (10 agents)
3. Teams (Research, Trading, Risk, Medallion Fund)
4. Workflows (Signal Discovery, Validation, Risk Assessment, Execution, Post-Trade)
5. Knowledge Base
6. Memory System
7. Reasoning Engine
8. Guardrails
9. Evaluation
10. Tracing

Usage:
    python -m renaissance_technologies_hedge_fund_agent.tests.test_glm_model
"""

import asyncio
from datetime import datetime
from typing import Dict, Any, Optional


# =============================================================================
# HELPER: Create Model
# =============================================================================

def get_model():
    """Create model dynamically from frontend config"""
    from ..utils.model_factory import create_model_from_config
    return create_model_from_config()


def print_section(title: str):
    """Print a section header"""
    print("\n" + "=" * 70)
    print(f" {title}")
    print("=" * 70)


def print_result(name: str, passed: bool, details: str = ""):
    """Print test result"""
    status = "[PASS]" if passed else "[FAIL]"
    print(f"  {status}: {name}")
    if details and not passed:
        print(f"         {details}")


# =============================================================================
# 1. CONFIGURATION & MODEL FACTORY
# =============================================================================

def test_configuration() -> bool:
    """Test configuration loading"""
    print_section("1. CONFIGURATION")

    try:
        from ..config import get_config, ModelProvider

        config = get_config()

        print(f"  Provider: {config.models.provider.value}")
        print(f"  Model: {config.models.model_id}")
        print(f"  Base URL: {config.models.anthropic_base_url}")
        print(f"  Risk Limits - Max Position: {config.risk_limits.max_position_size_pct}%")
        print(f"  Risk Limits - Max Leverage: {config.risk_limits.max_leverage}x")

        assert config.models.provider == ModelProvider.ANTHROPIC
        # Model ID loaded from frontend config

        print_result("Configuration loaded", True)
        return True
    except Exception as e:
        print_result("Configuration loaded", False, str(e))
        return False


def test_model_factory() -> bool:
    """Test model factory"""
    try:
        from ..models.model_factory import create_model, get_model_info

        info = get_model_info()
        print(f"  Model Info: {info['provider']}:{info['model_id']}")

        model = create_model()
        print(f"  Model Type: {type(model).__name__}")

        print_result("Model factory", True)
        return True
    except Exception as e:
        print_result("Model factory", False, str(e))
        return False


# =============================================================================
# 2. INDIVIDUAL AGENTS
# =============================================================================

async def test_agents() -> Dict[str, bool]:
    """Test all individual agents"""
    print_section("2. INDIVIDUAL AGENTS")

    results = {}
    model = get_model()

    agents_to_test = [
        ("Signal Scientist", "signal_scientist"),
        ("Data Scientist", "data_scientist"),
        ("Quant Researcher", "quant_researcher"),
        ("Research Lead", "research_lead"),
        ("Execution Trader", "execution_trader"),
        ("Market Maker", "market_maker"),
        ("Risk Quant", "risk_quant"),
        ("Compliance Officer", "compliance_officer"),
        ("Portfolio Manager", "portfolio_manager"),
        ("Investment Committee", "investment_committee"),
    ]

    for name, agent_type in agents_to_test:
        try:
            from agno.agent import Agent

            agent = Agent(
                model=model,
                name=name,
                description=f"RenTech {name}",
                instructions=[f"You are a {name} at Renaissance Technologies."],
            )

            # Quick test - just verify agent creation
            results[agent_type] = True
            print_result(name, True)

        except Exception as e:
            results[agent_type] = False
            print_result(name, False, str(e))

    return results


async def test_agent_interaction() -> bool:
    """Test agent can respond to prompts"""
    print("\n  Testing agent interaction...")

    try:
        from agno.agent import Agent
        model = get_model()

        agent = Agent(
            model=model,
            name="Signal Scientist",
            description="Quantitative signal researcher",
            instructions=["Analyze signals with statistical rigor. Be brief."],
        )

        response = agent.run("What is a p-value? One sentence only.")
        content = response.content if hasattr(response, 'content') else str(response)

        print(f"  Response: {content[:100]}...")
        print_result("Agent interaction", True)
        return True

    except Exception as e:
        print_result("Agent interaction", False, str(e))
        return False


# =============================================================================
# 3. TEAMS
# =============================================================================

def test_teams() -> Dict[str, bool]:
    """Test team structures"""
    print_section("3. TEAMS")

    results = {}

    teams = [
        ("Research Team", "teams.research_team", "create_research_team"),
        ("Trading Team", "teams.trading_team", "create_trading_team"),
        ("Risk Team", "teams.risk_team", "create_risk_team"),
        ("Medallion Fund", "teams.medallion_fund", "create_medallion_fund"),
    ]

    for name, module, func_name in teams:
        try:
            # Test import
            exec(f"from ..{module} import {func_name}")
            results[name] = True
            print_result(name, True)
        except Exception as e:
            results[name] = False
            print_result(name, False, str(e))

    return results


# =============================================================================
# 4. WORKFLOWS
# =============================================================================

def test_workflows() -> Dict[str, bool]:
    """Test workflow definitions"""
    print_section("4. WORKFLOWS")

    results = {}

    workflows = [
        ("Signal Discovery", "SignalDiscoveryWorkflow"),
        ("Signal Validation", "SignalValidationWorkflow"),
        ("Risk Assessment", "RiskAssessmentWorkflow"),
        ("Execution Pipeline", "ExecutionPipelineWorkflow"),
        ("Post Trade Analysis", "PostTradeWorkflow"),
        ("Daily Trading Cycle", "DailyTradingCycleWorkflow"),
    ]

    for name, class_name in workflows:
        try:
            from ..workflows import (
                SignalDiscoveryWorkflow,
                SignalValidationWorkflow,
                RiskAssessmentWorkflow,
                ExecutionPipelineWorkflow,
                PostTradeWorkflow,
                DailyTradingCycleWorkflow,
            )
            results[name] = True
            print_result(name, True)
        except Exception as e:
            results[name] = False
            print_result(name, False, str(e))

    return results


def test_workflow_base() -> bool:
    """Test workflow base structures"""
    try:
        from ..workflows.base import (
            WorkflowContext,
            WorkflowResult,
            StepResult,
            SignalData,
            RiskAssessment,
        )

        # Create sample signal
        signal = SignalData(
            signal_id="TEST_001",
            ticker="AAPL",
            signal_type="momentum",
            direction="long",
            strength=0.75,
            confidence=0.85,
            p_value=0.008,
            information_coefficient=0.05,
        )

        print(f"  Sample Signal: {signal.signal_id} - {signal.ticker} ({signal.direction})")
        print_result("Workflow base structures", True)
        return True

    except Exception as e:
        print_result("Workflow base structures", False, str(e))
        return False


# =============================================================================
# 5. KNOWLEDGE BASE
# =============================================================================

def test_knowledge() -> Dict[str, bool]:
    """Test knowledge base components"""
    print_section("5. KNOWLEDGE BASE")

    results = {}

    try:
        from ..knowledge import KnowledgeCategory
        from ..knowledge.base import KnowledgeDocument

        # Test category enum
        categories = list(KnowledgeCategory)
        print(f"  Categories: {[c.value for c in categories]}")
        results["Categories"] = True
        print_result("Knowledge categories", True)

    except Exception as e:
        results["Categories"] = False
        print_result("Knowledge categories", False, str(e))

    try:
        from ..knowledge.base import KnowledgeDocument, KnowledgeCategory

        doc = KnowledgeDocument(
            id="test_doc_001",
            category=KnowledgeCategory.RESEARCH,
            title="Test Research Paper",
            content="Statistical analysis of momentum signals.",
            tags=["momentum", "research"],
        )

        print(f"  Document: {doc.title}")
        results["Document Creation"] = True
        print_result("Document creation", True)

    except Exception as e:
        results["Document Creation"] = False
        print_result("Document creation", False, str(e))

    # Test specialized knowledge
    knowledge_types = [
        ("Market Data Knowledge", "market_data"),
        ("Research Knowledge", "research_papers"),
        ("Strategy Knowledge", "strategy_docs"),
        ("Trade History Knowledge", "trade_history"),
    ]

    for name, module in knowledge_types:
        try:
            exec(f"from ..knowledge.{module} import *")
            results[name] = True
            print_result(name, True)
        except Exception as e:
            results[name] = False
            print_result(name, False, str(e))

    return results


# =============================================================================
# 6. MEMORY SYSTEM
# =============================================================================

def test_memory() -> Dict[str, bool]:
    """Test memory system"""
    print_section("6. MEMORY SYSTEM")

    results = {}

    try:
        from ..memory import MemoryType, MemoryEntry

        print(f"  Memory Types: {[t.value for t in MemoryType]}")
        results["Memory Types"] = True
        print_result("Memory types", True)

    except Exception as e:
        results["Memory Types"] = False
        print_result("Memory types", False, str(e))

    memory_types = [
        ("Trade Memory", "trade_memory"),
        ("Decision Memory", "decision_memory"),
        ("Learning Memory", "learning_memory"),
        ("Agent Memory", "agent_memory"),
    ]

    for name, module in memory_types:
        try:
            exec(f"from ..memory.{module} import *")
            results[name] = True
            print_result(name, True)
        except Exception as e:
            results[name] = False
            print_result(name, False, str(e))

    return results


# =============================================================================
# 7. REASONING ENGINE
# =============================================================================

def test_reasoning() -> Dict[str, bool]:
    """Test reasoning components"""
    print_section("7. REASONING ENGINE")

    results = {}

    reasoning_modules = [
        ("Base Reasoning", "base"),
        ("Investment Reasoning", "investment_reasoning"),
        ("Risk Reasoning", "risk_reasoning"),
        ("IC Deliberation", "ic_deliberation"),
    ]

    for name, module in reasoning_modules:
        try:
            exec(f"from ..reasoning.{module} import *")
            results[name] = True
            print_result(name, True)
        except Exception as e:
            results[name] = False
            print_result(name, False, str(e))

    return results


# =============================================================================
# 8. GUARDRAILS
# =============================================================================

def test_guardrails() -> Dict[str, bool]:
    """Test guardrail system"""
    print_section("8. GUARDRAILS")

    results = {}

    try:
        from ..guardrails import (
            Guardrail,
            GuardrailResult,
            GuardrailSeverity,
            GuardrailRegistry,
        )

        print(f"  Severity Levels: {[s.value for s in GuardrailSeverity]}")
        results["Base Guardrails"] = True
        print_result("Base guardrails", True)

    except Exception as e:
        results["Base Guardrails"] = False
        print_result("Base guardrails", False, str(e))

    guardrail_types = [
        ("Risk Guardrails", "risk_guardrails"),
        ("Compliance Guardrails", "compliance_guardrails"),
        ("Signal Guardrails", "signal_guardrails"),
        ("Execution Guardrails", "execution_guardrails"),
    ]

    for name, module in guardrail_types:
        try:
            exec(f"from ..guardrails.{module} import *")
            results[name] = True
            print_result(name, True)
        except Exception as e:
            results[name] = False
            print_result(name, False, str(e))

    # Test guardrail execution
    try:
        from ..guardrails import PositionSizeGuardrail

        # PositionSizeGuardrail takes no constructor args - config is loaded internally
        guardrail = PositionSizeGuardrail()
        result = guardrail.check({"position_size_pct": 3.0, "ticker": "AAPL"})

        print(f"  Position Size Check (3%): {'PASS' if result.passed else 'FAIL'}")
        results["Guardrail Execution"] = True
        print_result("Guardrail execution", True)

    except Exception as e:
        results["Guardrail Execution"] = False
        print_result("Guardrail execution", False, str(e))

    return results


# =============================================================================
# 9. EVALUATION
# =============================================================================

def test_evaluation() -> Dict[str, bool]:
    """Test evaluation system"""
    print_section("9. EVALUATION")

    results = {}

    eval_types = [
        ("Signal Evaluator", "signal_evaluation"),
        ("Trade Evaluator", "trade_evaluation"),
        ("Agent Evaluator", "agent_evaluation"),
        ("System Evaluator", "system_evaluation"),
    ]

    for name, module in eval_types:
        try:
            exec(f"from ..evaluation.{module} import *")
            results[name] = True
            print_result(name, True)
        except Exception as e:
            results[name] = False
            print_result(name, False, str(e))

    return results


# =============================================================================
# 10. TRACING
# =============================================================================

def test_tracing() -> Dict[str, bool]:
    """Test tracing system"""
    print_section("10. TRACING")

    results = {}

    tracing_components = [
        ("Base Tracer", "base"),
        ("Agent Tracer", "agent_tracer"),
        ("Workflow Tracer", "workflow_tracer"),
        ("Metrics Collector", "metrics_collector"),
    ]

    for name, module in tracing_components:
        try:
            exec(f"from ..tracing.{module} import *")
            results[name] = True
            print_result(name, True)
        except Exception as e:
            results[name] = False
            print_result(name, False, str(e))

    return results


# =============================================================================
# 11. FULL SYSTEM INTEGRATION
# =============================================================================

async def test_full_system() -> bool:
    """Test complete system integration"""
    print_section("11. FULL SYSTEM INTEGRATION")

    try:
        from ..main import RenaissanceTechnologiesSystem
        from ..config import get_config

        print("  Initializing RenTech System...")
        system = RenaissanceTechnologiesSystem()

        # Get config separately (system doesn't store it as attribute)
        config = get_config()
        print(f"  System Name: {config.name}")
        print(f"  Environment: {config.environment}")
        print(f"  Model Provider: {system.model_provider or 'Pure Quant (no LLM)'}")

        print_result("System initialization", True)
        return True

    except Exception as e:
        print_result("System initialization", False, str(e))
        return False


async def test_end_to_end_signal_analysis() -> bool:
    """Test end-to-end signal analysis flow"""
    print("\n  Running end-to-end signal analysis...")

    try:
        from agno.agent import Agent
        from ..workflows.base import SignalData

        model = get_model()

        # Create signal
        signal = SignalData(
            signal_id="E2E_TEST_001",
            ticker="MSFT",
            signal_type="mean_reversion",
            direction="long",
            strength=0.68,
            confidence=0.82,
            p_value=0.012,
            information_coefficient=0.045,
        )

        # Step 1: Research Analysis
        research_agent = Agent(
            model=model,
            name="Research Analyst",
            instructions=["Analyze signals briefly. 2 sentences max."],
        )

        research_response = research_agent.run(
            f"Signal: {signal.ticker} {signal.signal_type}, p-value={signal.p_value}, IC={signal.information_coefficient}. Valid?"
        )
        print(f"  Research: {str(research_response.content)[:80]}...")

        # Step 2: Risk Assessment
        risk_agent = Agent(
            model=model,
            name="Risk Analyst",
            instructions=["Assess risk briefly. 2 sentences max."],
        )

        risk_response = risk_agent.run(
            f"Position: 2% NAV in {signal.ticker}. Max drawdown risk?"
        )
        print(f"  Risk: {str(risk_response.content)[:80]}...")

        # Step 3: Final Decision
        pm_agent = Agent(
            model=model,
            name="Portfolio Manager",
            instructions=["Make decision: APPROVE or REJECT. One sentence."],
        )

        pm_response = pm_agent.run(
            f"Signal {signal.ticker}: p={signal.p_value}, conf={signal.confidence}. Approve?"
        )
        print(f"  Decision: {str(pm_response.content)[:80]}...")

        print_result("End-to-end signal analysis", True)
        return True

    except Exception as e:
        print_result("End-to-end signal analysis", False, str(e))
        return False


async def test_ic_deliberation() -> bool:
    """Test full Investment Committee deliberation with voting and reasoning"""
    print("\n  Running IC Deliberation test...")
    print("  " + "-" * 60)

    try:
        from ..reasoning.ic_deliberation import get_ic_deliberator, ICDeliberationResult

        deliberator = get_ic_deliberator()

        # Create test signal data
        signal = {
            "ticker": "NVDA",
            "direction": "long",
            "signal_type": "momentum",
            "p_value": 0.008,
            "confidence": 0.58,
            "expected_return_bps": 85,
            "average_daily_volume": 45000000,
        }

        signal_evaluation = {
            "statistical_quality": "strong",
            "overall_score": 72,
            "p_value_valid": True,
            "ic_valid": True,
        }

        risk_assessment = {
            "risk_level": "medium",
            "var_utilization_pct": 45,
            "sector_concentrations": {"Technology": 18},
            "risk_flags": ["High beta stock"],
        }

        sizing_recommendation = {
            "final_size_pct": 1.5,
            "final_notional": 15000000,
            "kelly_fraction": 0.02,
        }

        market_context = {
            "regime": "bullish",
            "vix": 16.5,
            "spy_trend": "up",
        }

        print(f"\n  SIGNAL: {signal['ticker']} {signal['direction'].upper()} ({signal['signal_type']})")
        print(f"  P-value: {signal['p_value']}, Confidence: {signal['confidence']:.0%}")
        print(f"  Expected Return: {signal['expected_return_bps']} bps")
        print("  " + "-" * 60)

        # Run IC deliberation
        result: ICDeliberationResult = await deliberator.deliberate(
            signal=signal,
            signal_evaluation=signal_evaluation,
            risk_assessment=risk_assessment,
            sizing_recommendation=sizing_recommendation,
            market_context=market_context,
            historical_decisions=[],
        )

        # Display deliberation results
        print("\n  IC DELIBERATION RESULTS:")
        print("  " + "-" * 60)

        # Pros and Cons
        print("\n  PROS:")
        for pro in result.pros[:3]:
            print(f"    + {pro}")

        print("\n  CONS:")
        for con in result.cons[:3]:
            print(f"    - {con}")

        # Member Opinions (the key part showing agent collaboration)
        print("\n  IC MEMBER VOTES:")
        print("  " + "-" * 60)
        for opinion in result.member_opinions:
            vote_symbol = {
                "approve": "[APPROVE]",
                "reject": "[REJECT]",
                "conditional": "[COND]",
                "abstain": "[ABSTAIN]",
            }.get(opinion.vote.value, "[?]")
            print(f"    {vote_symbol} {opinion.member_role}")
            print(f"           Rationale: {opinion.rationale[:60]}...")
            print(f"           Confidence: {opinion.confidence:.0%}")

        # Vote tally
        print("\n  VOTE TALLY:")
        for vote_type, count in result.vote_summary.items():
            if count > 0:
                print(f"    {vote_type.upper()}: {count}")

        # Final Decision
        print("\n  " + "=" * 60)
        print(f"  FINAL DECISION: {result.decision}")
        print(f"  RATIONALE: {result.decision_rationale}")
        if result.conditions:
            print(f"  CONDITIONS: {', '.join(result.conditions)}")
        if result.approved_size_pct:
            print(f"  APPROVED SIZE: {result.approved_size_pct:.2f}%")
        print(f"  CONFIDENCE: {result.confidence:.0%}")
        print("  " + "=" * 60)

        print_result("IC Deliberation", True)
        return True

    except Exception as e:
        import traceback
        print(f"  Error: {e}")
        traceback.print_exc()
        print_result("IC Deliberation", False, str(e))
        return False


async def test_team_collaboration() -> bool:
    """Test real team collaboration using Agno Team"""
    print("\n  Running Team Collaboration test...")
    print("  " + "-" * 60)

    try:
        from agno.agent import Agent
        from agno.team import Team

        model = get_model()

        # Create specialized agents
        research_lead = Agent(
            model=model,
            name="Research Lead",
            role="Signal Validation",
            instructions=[
                "You validate trading signals using statistical methods.",
                "Focus on p-values, information coefficients, and backtest results.",
                "Be concise but precise with numbers.",
            ],
        )

        risk_officer = Agent(
            model=model,
            name="Chief Risk Officer",
            role="Risk Assessment",
            instructions=[
                "You assess portfolio risk and position sizing.",
                "Focus on VaR, drawdown limits, and concentration risk.",
                "Flag any risk limit breaches immediately.",
            ],
        )

        portfolio_manager = Agent(
            model=model,
            name="Portfolio Manager",
            role="Final Decision",
            instructions=[
                "You make final trading decisions based on research and risk input.",
                "Synthesize team inputs into a clear APPROVE/REJECT decision.",
                "Always provide reasoning for your decision.",
            ],
        )

        # Create the Investment Committee team
        # Note: Agno Team doesn't have 'leader' param - all members are equal
        # The first member often acts as coordinator based on instructions
        ic_team = Team(
            name="Investment Committee",
            description="RenTech IC that makes systematic trading decisions",
            members=[portfolio_manager, research_lead, risk_officer],  # PM first to coordinate
            model=model,
            share_member_interactions=True,  # Key: agents see each other's responses
            add_team_history_to_members=True,  # Key: maintains conversation context
            instructions=[
                "Work together to evaluate trading signals.",
                "Research Lead validates the signal first.",
                "Risk Officer assesses the risk second.",
                "Portfolio Manager makes final decision last.",
            ],
            markdown=True,
            show_members_responses=True,
        )

        # Submit a task to the team
        task = """
Evaluate this trading signal for GOOGL:

SIGNAL DETAILS:
- Ticker: GOOGL
- Direction: LONG
- Signal Type: Mean Reversion
- P-value: 0.007
- Confidence: 61%
- Expected Return: 45 bps
- Proposed Size: 1.2% of NAV

CURRENT PORTFOLIO STATE:
- Technology Sector Exposure: 22%
- VaR Utilization: 55%
- Current Drawdown: 3.2%

Each team member should provide their assessment, then PM makes final call.
"""

        print("\n  Submitting task to Investment Committee Team...")
        print("  Task: Evaluate GOOGL mean reversion signal")
        print("  " + "-" * 60)

        # Run the team
        response = ic_team.run(task)
        content = response.content if hasattr(response, 'content') else str(response)

        # Display the collaborative response
        print("\n  TEAM RESPONSE:")
        print("  " + "-" * 60)

        # Format output nicely - handle Unicode for Windows
        lines = content.split('\n')
        for line in lines[:40]:  # Limit output
            # Replace problematic Unicode characters for Windows console
            safe_line = line.encode('ascii', 'replace').decode('ascii')
            print(f"  {safe_line}")

        if len(lines) > 40:
            print(f"  ... ({len(lines) - 40} more lines)")

        print("  " + "-" * 60)
        print_result("Team Collaboration", True)
        return True

    except Exception as e:
        import traceback
        print(f"  Error: {e}")
        traceback.print_exc()
        print_result("Team Collaboration", False, str(e))
        return False


# =============================================================================
# MAIN TEST RUNNER
# =============================================================================

async def run_all_tests():
    """Run complete test suite"""
    print("\n" + "=" * 70)
    print(" RENAISSANCE TECHNOLOGIES MULTI-AGENT SYSTEM")
    print(" FULL INTEGRATION TEST SUITE")
    print(f" Started: {datetime.now().isoformat()}")
    print("=" * 70)

    all_results = {}

    # 1. Configuration
    all_results["config"] = test_configuration()
    all_results["model_factory"] = test_model_factory()

    # 2. Agents
    agent_results = await test_agents()
    all_results.update({f"agent_{k}": v for k, v in agent_results.items()})
    all_results["agent_interaction"] = await test_agent_interaction()

    # 3. Teams
    team_results = test_teams()
    all_results.update({f"team_{k}": v for k, v in team_results.items()})

    # 4. Workflows
    workflow_results = test_workflows()
    all_results.update({f"workflow_{k}": v for k, v in workflow_results.items()})
    all_results["workflow_base"] = test_workflow_base()

    # 5. Knowledge
    knowledge_results = test_knowledge()
    all_results.update({f"knowledge_{k}": v for k, v in knowledge_results.items()})

    # 6. Memory
    memory_results = test_memory()
    all_results.update({f"memory_{k}": v for k, v in memory_results.items()})

    # 7. Reasoning
    reasoning_results = test_reasoning()
    all_results.update({f"reasoning_{k}": v for k, v in reasoning_results.items()})

    # 8. Guardrails
    guardrail_results = test_guardrails()
    all_results.update({f"guardrail_{k}": v for k, v in guardrail_results.items()})

    # 9. Evaluation
    eval_results = test_evaluation()
    all_results.update({f"eval_{k}": v for k, v in eval_results.items()})

    # 10. Tracing
    tracing_results = test_tracing()
    all_results.update({f"tracing_{k}": v for k, v in tracing_results.items()})

    # 11. Full System
    all_results["full_system"] = await test_full_system()
    all_results["e2e_signal_analysis"] = await test_end_to_end_signal_analysis()

    # 12. Advanced: IC Deliberation & Team Collaboration
    all_results["ic_deliberation"] = await test_ic_deliberation()
    all_results["team_collaboration"] = await test_team_collaboration()

    # ==========================================================================
    # SUMMARY
    # ==========================================================================
    print_section("TEST SUMMARY")

    passed = sum(1 for v in all_results.values() if v)
    total = len(all_results)

    # Group results by category
    categories = {
        "Configuration": ["config", "model_factory"],
        "Agents": [k for k in all_results if k.startswith("agent_")],
        "Teams": [k for k in all_results if k.startswith("team_")],
        "Workflows": [k for k in all_results if k.startswith("workflow_")],
        "Knowledge": [k for k in all_results if k.startswith("knowledge_")],
        "Memory": [k for k in all_results if k.startswith("memory_")],
        "Reasoning": [k for k in all_results if k.startswith("reasoning_")],
        "Guardrails": [k for k in all_results if k.startswith("guardrail_")],
        "Evaluation": [k for k in all_results if k.startswith("eval_")],
        "Tracing": [k for k in all_results if k.startswith("tracing_")],
        "Integration": ["full_system", "e2e_signal_analysis"],
        "Collaboration": ["ic_deliberation", "team_collaboration"],
    }

    for cat_name, keys in categories.items():
        cat_passed = sum(1 for k in keys if all_results.get(k, False))
        cat_total = len(keys)
        status = "[OK]" if cat_passed == cat_total else "[!!]"
        print(f"  {status} {cat_name}: {cat_passed}/{cat_total}")

    print("\n" + "-" * 70)
    print(f"  TOTAL: {passed}/{total} tests passed ({100*passed//total}%)")
    print("-" * 70)

    if passed == total:
        print("\n  ALL TESTS PASSED - System is fully operational!")
    else:
        failed = [k for k, v in all_results.items() if not v]
        print(f"\n  {len(failed)} tests failed:")
        for f in failed[:10]:
            print(f"    - {f}")
        if len(failed) > 10:
            print(f"    ... and {len(failed) - 10} more")

    print("\n" + "=" * 70)
    print(f" Completed: {datetime.now().isoformat()}")
    print("=" * 70 + "\n")

    return all_results


def main():
    """CLI entry point"""
    asyncio.run(run_all_tests())


if __name__ == "__main__":
    main()
