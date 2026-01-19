"""
Workflow Module - Complex task orchestration

Provides:
- Sequential and parallel step execution
- Conditional branching
- Loop support
- Step dependencies
- Agent workflow integration
"""

from typing import Dict, Any, Optional, List, Callable, Union
import logging

logger = logging.getLogger(__name__)


class WorkflowModule:
    """
    Workflow orchestration for complex multi-step tasks.

    Supports:
    - Sequential steps
    - Parallel execution
    - Conditional branching
    - Loops
    - Agent integration
    """

    def __init__(
        self,
        name: str = "Workflow",
        description: Optional[str] = None,
        **kwargs
    ):
        """
        Initialize workflow module.

        Args:
            name: Workflow name
            description: Workflow description
            **kwargs: Additional configuration
        """
        self.name = name
        self.description = description
        self.config = kwargs

        self._steps: List[Dict[str, Any]] = []
        self._workflow = None

    def add_step(
        self,
        name: str,
        agent: Optional[Any] = None,
        function: Optional[Callable] = None,
        prompt: Optional[str] = None,
        **kwargs
    ) -> "WorkflowModule":
        """
        Add a step to the workflow.

        Args:
            name: Step name
            agent: Agno Agent for this step
            function: Python function to execute
            prompt: Prompt template for agent
            **kwargs: Additional step configuration

        Returns:
            self for chaining
        """
        step = {
            "type": "step",
            "name": name,
            "agent": agent,
            "function": function,
            "prompt": prompt,
            **kwargs
        }
        self._steps.append(step)
        return self

    def add_parallel(
        self,
        name: str,
        steps: List[Dict[str, Any]],
        **kwargs
    ) -> "WorkflowModule":
        """
        Add parallel execution steps.

        Args:
            name: Parallel group name
            steps: List of steps to run in parallel
            **kwargs: Additional configuration

        Returns:
            self for chaining
        """
        parallel_step = {
            "type": "parallel",
            "name": name,
            "steps": steps,
            **kwargs
        }
        self._steps.append(parallel_step)
        return self

    def add_condition(
        self,
        name: str,
        condition: Callable[[Any], bool],
        if_true: Dict[str, Any],
        if_false: Optional[Dict[str, Any]] = None,
        **kwargs
    ) -> "WorkflowModule":
        """
        Add conditional branching.

        Args:
            name: Condition name
            condition: Function that returns True/False
            if_true: Step to execute if condition is True
            if_false: Step to execute if condition is False
            **kwargs: Additional configuration

        Returns:
            self for chaining
        """
        condition_step = {
            "type": "condition",
            "name": name,
            "condition": condition,
            "if_true": if_true,
            "if_false": if_false,
            **kwargs
        }
        self._steps.append(condition_step)
        return self

    def add_loop(
        self,
        name: str,
        steps: List[Dict[str, Any]],
        condition: Optional[Callable[[Any], bool]] = None,
        max_iterations: int = 10,
        **kwargs
    ) -> "WorkflowModule":
        """
        Add loop execution.

        Args:
            name: Loop name
            steps: Steps to repeat
            condition: Continue condition (loop while True)
            max_iterations: Maximum iterations
            **kwargs: Additional configuration

        Returns:
            self for chaining
        """
        loop_step = {
            "type": "loop",
            "name": name,
            "steps": steps,
            "condition": condition,
            "max_iterations": max_iterations,
            **kwargs
        }
        self._steps.append(loop_step)
        return self

    def add_router(
        self,
        name: str,
        routes: Dict[str, Dict[str, Any]],
        router_function: Callable[[Any], str],
        **kwargs
    ) -> "WorkflowModule":
        """
        Add router step for dynamic routing.

        Args:
            name: Router name
            routes: Dict of route_key -> step configuration
            router_function: Function that returns route key
            **kwargs: Additional configuration

        Returns:
            self for chaining
        """
        router_step = {
            "type": "router",
            "name": name,
            "routes": routes,
            "router_function": router_function,
            **kwargs
        }
        self._steps.append(router_step)
        return self

    def build(self) -> Any:
        """
        Build the Agno Workflow.

        Returns:
            Agno Workflow instance
        """
        if self._workflow:
            return self._workflow

        if not self._steps:
            raise ValueError("Cannot build workflow without steps")

        try:
            from agno.workflow import Workflow, Step, Parallel, Condition, Loop, Router

            # Convert steps to Agno format
            agno_steps = self._convert_steps_to_agno(self._steps)

            self._workflow = Workflow(
                name=self.name,
                description=self.description,
                steps=agno_steps
            )

            logger.debug(f"Built workflow '{self.name}' with {len(self._steps)} steps")
            return self._workflow

        except ImportError as e:
            logger.warning(f"Agno Workflow not available: {e}")
            # Return a simple executor fallback
            return self._create_fallback_executor()
        except Exception as e:
            raise RuntimeError(f"Failed to build workflow: {e}")

    def _convert_steps_to_agno(self, steps: List[Dict[str, Any]]) -> List[Any]:
        """Convert internal step format to Agno steps"""
        from agno.workflow import Step, Parallel, Condition, Loop, Router

        agno_steps = []

        for step_config in steps:
            step_type = step_config.get("type", "step")

            if step_type == "step":
                agno_step = self._create_step(step_config)
            elif step_type == "parallel":
                agno_step = self._create_parallel(step_config)
            elif step_type == "condition":
                agno_step = self._create_condition(step_config)
            elif step_type == "loop":
                agno_step = self._create_loop(step_config)
            elif step_type == "router":
                agno_step = self._create_router(step_config)
            else:
                logger.warning(f"Unknown step type: {step_type}")
                continue

            if agno_step:
                agno_steps.append(agno_step)

        return agno_steps

    def _create_step(self, config: Dict[str, Any]) -> Any:
        """Create an Agno Step"""
        from agno.workflow import Step

        step_kwargs = {"name": config["name"]}

        if config.get("agent"):
            step_kwargs["agent"] = config["agent"]
        if config.get("function"):
            step_kwargs["function"] = config["function"]
        if config.get("prompt"):
            step_kwargs["prompt"] = config["prompt"]

        return Step(**step_kwargs)

    def _create_parallel(self, config: Dict[str, Any]) -> Any:
        """Create an Agno Parallel step"""
        from agno.workflow import Parallel

        inner_steps = self._convert_steps_to_agno(config.get("steps", []))
        return Parallel(name=config["name"], steps=inner_steps)

    def _create_condition(self, config: Dict[str, Any]) -> Any:
        """Create an Agno Condition step"""
        from agno.workflow import Condition

        return Condition(
            name=config["name"],
            condition=config["condition"],
            if_true=self._create_step(config["if_true"]) if config.get("if_true") else None,
            if_false=self._create_step(config["if_false"]) if config.get("if_false") else None,
        )

    def _create_loop(self, config: Dict[str, Any]) -> Any:
        """Create an Agno Loop step"""
        from agno.workflow import Loop

        inner_steps = self._convert_steps_to_agno(config.get("steps", []))
        return Loop(
            name=config["name"],
            steps=inner_steps,
            condition=config.get("condition"),
            max_iterations=config.get("max_iterations", 10),
        )

    def _create_router(self, config: Dict[str, Any]) -> Any:
        """Create an Agno Router step"""
        from agno.workflow import Router

        routes = {}
        for key, step_config in config.get("routes", {}).items():
            routes[key] = self._create_step(step_config)

        return Router(
            name=config["name"],
            routes=routes,
            router=config["router_function"],
        )

    def _create_fallback_executor(self) -> "SimpleWorkflowExecutor":
        """Create a simple fallback executor if Agno Workflow is not available"""
        return SimpleWorkflowExecutor(self._steps)

    def run(self, input_data: Optional[Dict[str, Any]] = None, **kwargs) -> Any:
        """
        Run the workflow.

        Args:
            input_data: Initial input data
            **kwargs: Additional arguments

        Returns:
            Workflow result
        """
        workflow = self.build()

        run_kwargs = {}
        if input_data:
            run_kwargs["input"] = input_data
        run_kwargs.update(kwargs)

        return workflow.run(**run_kwargs)

    async def arun(self, input_data: Optional[Dict[str, Any]] = None, **kwargs) -> Any:
        """
        Run the workflow asynchronously.

        Args:
            input_data: Initial input data
            **kwargs: Additional arguments

        Returns:
            Workflow result
        """
        workflow = self.build()

        run_kwargs = {}
        if input_data:
            run_kwargs["input"] = input_data
        run_kwargs.update(kwargs)

        return await workflow.arun(**run_kwargs)

    def get_steps(self) -> List[Dict[str, Any]]:
        """Get list of workflow steps"""
        return self._steps.copy()

    @classmethod
    def from_config(cls, config: Dict[str, Any]) -> "WorkflowModule":
        """
        Create WorkflowModule from configuration.

        Args:
            config: Workflow configuration

        Returns:
            WorkflowModule instance
        """
        module = cls(
            name=config.get("name", "Workflow"),
            description=config.get("description"),
        )

        for step_config in config.get("steps", []):
            step_type = step_config.get("type", "step")

            if step_type == "step":
                module.add_step(**step_config)
            elif step_type == "parallel":
                module.add_parallel(**step_config)
            elif step_type == "condition":
                module.add_condition(**step_config)
            elif step_type == "loop":
                module.add_loop(**step_config)
            elif step_type == "router":
                module.add_router(**step_config)

        return module


class SimpleWorkflowExecutor:
    """
    Simple fallback workflow executor when Agno Workflow is not available.
    Executes steps sequentially.
    """

    def __init__(self, steps: List[Dict[str, Any]]):
        self.steps = steps

    def run(self, input: Optional[Dict[str, Any]] = None, **kwargs) -> Dict[str, Any]:
        """Execute workflow steps sequentially"""
        context = input or {}
        results = []

        for step in self.steps:
            step_type = step.get("type", "step")

            if step_type == "step":
                result = self._execute_step(step, context)
                results.append(result)
                context["last_result"] = result
            elif step_type == "parallel":
                # Execute sequentially as fallback
                for inner_step in step.get("steps", []):
                    result = self._execute_step(inner_step, context)
                    results.append(result)
            # Add other step types as needed

        return {
            "success": True,
            "results": results,
            "context": context
        }

    def _execute_step(self, step: Dict[str, Any], context: Dict[str, Any]) -> Any:
        """Execute a single step"""
        if step.get("function"):
            return step["function"](context)
        elif step.get("agent"):
            prompt = step.get("prompt", "")
            return step["agent"].run(prompt.format(**context))
        return None

    async def arun(self, input: Optional[Dict[str, Any]] = None, **kwargs) -> Dict[str, Any]:
        """Async execution (falls back to sync)"""
        return self.run(input=input, **kwargs)


class WorkflowBuilder:
    """
    Fluent builder for creating workflows.

    Example:
        workflow = (WorkflowBuilder("Data Pipeline")
            .add_step("fetch", function=fetch_data)
            .add_step("process", agent=processor_agent)
            .add_step("save", function=save_results)
            .build())
    """

    def __init__(self, name: str):
        """Initialize workflow builder"""
        self._module = WorkflowModule(name=name)

    def with_description(self, description: str) -> "WorkflowBuilder":
        """Set workflow description"""
        self._module.description = description
        return self

    def add_step(
        self,
        name: str,
        agent: Optional[Any] = None,
        function: Optional[Callable] = None,
        prompt: Optional[str] = None,
        **kwargs
    ) -> "WorkflowBuilder":
        """Add a step"""
        self._module.add_step(name, agent=agent, function=function, prompt=prompt, **kwargs)
        return self

    def add_parallel(self, name: str, steps: List[Dict[str, Any]]) -> "WorkflowBuilder":
        """Add parallel steps"""
        self._module.add_parallel(name, steps)
        return self

    def add_condition(
        self,
        name: str,
        condition: Callable,
        if_true: Dict,
        if_false: Optional[Dict] = None
    ) -> "WorkflowBuilder":
        """Add conditional branching"""
        self._module.add_condition(name, condition, if_true, if_false)
        return self

    def add_loop(
        self,
        name: str,
        steps: List[Dict],
        condition: Optional[Callable] = None,
        max_iterations: int = 10
    ) -> "WorkflowBuilder":
        """Add a loop"""
        self._module.add_loop(name, steps, condition, max_iterations)
        return self

    def build(self) -> Any:
        """Build and return the workflow"""
        return self._module.build()

    def get_module(self) -> WorkflowModule:
        """Get the underlying WorkflowModule"""
        return self._module


class FinancialWorkflowTemplates:
    """
    Pre-built workflow templates for common financial tasks.

    Templates:
    - Stock analysis pipeline
    - Portfolio rebalancing
    - Risk assessment
    - Market screening
    - Report generation
    """

    @staticmethod
    def stock_analysis_pipeline(
        fetch_agent: Any = None,
        analysis_agent: Any = None,
        report_agent: Any = None
    ) -> WorkflowModule:
        """
        Create stock analysis workflow.

        Steps:
        1. Fetch market data
        2. Perform technical analysis
        3. Perform fundamental analysis (parallel)
        4. Generate recommendation
        5. Create report

        Args:
            fetch_agent: Agent for fetching data
            analysis_agent: Agent for analysis
            report_agent: Agent for report generation

        Returns:
            Configured WorkflowModule
        """
        workflow = WorkflowModule(
            name="Stock Analysis Pipeline",
            description="Comprehensive stock analysis workflow"
        )

        # Step 1: Fetch data
        workflow.add_step(
            name="fetch_market_data",
            agent=fetch_agent,
            prompt="Fetch current market data for {symbol} including price, volume, and recent news"
        )

        # Step 2: Parallel analysis
        workflow.add_parallel(
            name="analysis",
            steps=[
                {
                    "type": "step",
                    "name": "technical_analysis",
                    "agent": analysis_agent,
                    "prompt": "Perform technical analysis on {symbol} using the market data: {last_result}"
                },
                {
                    "type": "step",
                    "name": "fundamental_analysis",
                    "agent": analysis_agent,
                    "prompt": "Perform fundamental analysis on {symbol} including valuation metrics"
                }
            ]
        )

        # Step 3: Generate recommendation
        workflow.add_step(
            name="generate_recommendation",
            agent=analysis_agent,
            prompt="Based on the technical and fundamental analysis, provide a buy/sell/hold recommendation for {symbol}"
        )

        # Step 4: Create report
        workflow.add_step(
            name="create_report",
            agent=report_agent,
            prompt="Generate a comprehensive investment report for {symbol} summarizing all analysis and recommendations"
        )

        return workflow

    @staticmethod
    def portfolio_rebalancing(
        analysis_agent: Any = None,
        optimization_agent: Any = None,
        execution_agent: Any = None,
        risk_threshold: float = 0.05
    ) -> WorkflowModule:
        """
        Create portfolio rebalancing workflow.

        Steps:
        1. Analyze current portfolio
        2. Check if rebalancing needed (condition)
        3. Calculate optimal allocation
        4. Generate trade orders
        5. Review and confirm

        Args:
            analysis_agent: Agent for portfolio analysis
            optimization_agent: Agent for optimization
            execution_agent: Agent for trade execution
            risk_threshold: Drift threshold for rebalancing

        Returns:
            Configured WorkflowModule
        """
        workflow = WorkflowModule(
            name="Portfolio Rebalancing",
            description="Automated portfolio rebalancing workflow"
        )

        # Step 1: Analyze current portfolio
        workflow.add_step(
            name="analyze_portfolio",
            agent=analysis_agent,
            prompt="Analyze current portfolio holdings and calculate drift from target allocation"
        )

        # Step 2: Check if rebalancing needed
        def needs_rebalancing(context: Dict) -> bool:
            drift = context.get("max_drift", 0)
            return drift > risk_threshold

        workflow.add_condition(
            name="check_rebalancing_needed",
            condition=needs_rebalancing,
            if_true={
                "type": "step",
                "name": "calculate_optimal_allocation",
                "agent": optimization_agent,
                "prompt": "Calculate optimal trade orders to rebalance portfolio to target allocation"
            },
            if_false={
                "type": "step",
                "name": "no_action_needed",
                "function": lambda ctx: {"message": "Portfolio within tolerance, no rebalancing needed"}
            }
        )

        # Step 3: Generate trade orders
        workflow.add_step(
            name="generate_orders",
            agent=execution_agent,
            prompt="Generate specific trade orders based on the optimal allocation: {last_result}"
        )

        return workflow

    @staticmethod
    def market_screening(
        screening_agent: Any = None,
        analysis_agent: Any = None,
        max_iterations: int = 5
    ) -> WorkflowModule:
        """
        Create market screening workflow with iterative refinement.

        Steps:
        1. Initial broad screen
        2. Apply filters (loop)
        3. Deep analysis on top candidates
        4. Rank and prioritize

        Args:
            screening_agent: Agent for screening
            analysis_agent: Agent for deep analysis
            max_iterations: Maximum screening iterations

        Returns:
            Configured WorkflowModule
        """
        workflow = WorkflowModule(
            name="Market Screening",
            description="Iterative market screening and analysis"
        )

        # Step 1: Initial screen
        workflow.add_step(
            name="initial_screen",
            agent=screening_agent,
            prompt="Screen the market for stocks matching criteria: {criteria}"
        )

        # Step 2: Iterative filtering (loop)
        def has_more_filters(context: Dict) -> bool:
            filters = context.get("remaining_filters", [])
            return len(filters) > 0

        workflow.add_loop(
            name="apply_filters",
            steps=[
                {
                    "type": "step",
                    "name": "apply_filter",
                    "agent": screening_agent,
                    "prompt": "Apply filter {current_filter} to the stock list: {candidates}"
                }
            ],
            condition=has_more_filters,
            max_iterations=max_iterations
        )

        # Step 3: Deep analysis on top candidates
        workflow.add_step(
            name="deep_analysis",
            agent=analysis_agent,
            prompt="Perform detailed analysis on top candidates: {last_result}"
        )

        # Step 4: Rank and prioritize
        workflow.add_step(
            name="rank_candidates",
            agent=analysis_agent,
            prompt="Rank and prioritize the analyzed candidates based on investment potential"
        )

        return workflow

    @staticmethod
    def risk_assessment(
        risk_agent: Any = None,
        market_agent: Any = None
    ) -> WorkflowModule:
        """
        Create risk assessment workflow with routing.

        Steps:
        1. Identify risk type
        2. Route to appropriate analysis
        3. Calculate risk metrics
        4. Generate risk report

        Args:
            risk_agent: Agent for risk analysis
            market_agent: Agent for market analysis

        Returns:
            Configured WorkflowModule
        """
        workflow = WorkflowModule(
            name="Risk Assessment",
            description="Comprehensive risk assessment with dynamic routing"
        )

        # Step 1: Identify risk type
        workflow.add_step(
            name="identify_risk_type",
            agent=risk_agent,
            prompt="Identify the primary risk type for the portfolio: market, credit, liquidity, or operational"
        )

        # Step 2: Route to appropriate analysis
        def route_risk_analysis(context: Dict) -> str:
            risk_type = context.get("risk_type", "market").lower()
            if "market" in risk_type:
                return "market_risk"
            elif "credit" in risk_type:
                return "credit_risk"
            elif "liquidity" in risk_type:
                return "liquidity_risk"
            return "general_risk"

        workflow.add_router(
            name="risk_router",
            routes={
                "market_risk": {
                    "type": "step",
                    "name": "market_risk_analysis",
                    "agent": market_agent,
                    "prompt": "Perform market risk analysis including VaR, beta, and volatility"
                },
                "credit_risk": {
                    "type": "step",
                    "name": "credit_risk_analysis",
                    "agent": risk_agent,
                    "prompt": "Perform credit risk analysis on holdings"
                },
                "liquidity_risk": {
                    "type": "step",
                    "name": "liquidity_risk_analysis",
                    "agent": risk_agent,
                    "prompt": "Analyze liquidity risk including bid-ask spreads and volume"
                },
                "general_risk": {
                    "type": "step",
                    "name": "general_risk_analysis",
                    "agent": risk_agent,
                    "prompt": "Perform general risk assessment"
                }
            },
            router_function=route_risk_analysis
        )

        # Step 3: Generate risk report
        workflow.add_step(
            name="generate_risk_report",
            agent=risk_agent,
            prompt="Generate comprehensive risk report based on the analysis: {last_result}"
        )

        return workflow

    @staticmethod
    def research_report_generation(
        research_agent: Any = None,
        writing_agent: Any = None
    ) -> WorkflowModule:
        """
        Create research report generation workflow.

        Steps:
        1. Gather research data (parallel)
        2. Analyze and synthesize
        3. Write report sections
        4. Review and finalize

        Args:
            research_agent: Agent for research
            writing_agent: Agent for writing

        Returns:
            Configured WorkflowModule
        """
        workflow = WorkflowModule(
            name="Research Report Generation",
            description="Generate comprehensive research reports"
        )

        # Step 1: Gather research data in parallel
        workflow.add_parallel(
            name="gather_research",
            steps=[
                {
                    "type": "step",
                    "name": "company_research",
                    "agent": research_agent,
                    "prompt": "Research company fundamentals, management, and competitive position for {symbol}"
                },
                {
                    "type": "step",
                    "name": "industry_research",
                    "agent": research_agent,
                    "prompt": "Research industry trends and competitive landscape for {symbol}'s sector"
                },
                {
                    "type": "step",
                    "name": "financial_research",
                    "agent": research_agent,
                    "prompt": "Analyze financial statements and metrics for {symbol}"
                }
            ]
        )

        # Step 2: Synthesize findings
        workflow.add_step(
            name="synthesize_findings",
            agent=research_agent,
            prompt="Synthesize all research findings into key insights and investment thesis"
        )

        # Step 3: Write report
        workflow.add_step(
            name="write_report",
            agent=writing_agent,
            prompt="Write a professional investment research report based on the synthesized findings"
        )

        # Step 4: Review and finalize
        workflow.add_step(
            name="review_report",
            agent=writing_agent,
            prompt="Review the report for accuracy, completeness, and clarity. Make necessary improvements."
        )

        return workflow
