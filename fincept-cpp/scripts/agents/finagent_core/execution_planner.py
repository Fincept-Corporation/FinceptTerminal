"""
ExecutionPlanner - Multi-step task planning for complex operations

Plans and orchestrates multi-step tasks with dependency management.
Similar to ValueCell's task planning capabilities.
"""

from typing import Dict, Any, Optional, List, Callable, Union
from dataclasses import dataclass, field
from enum import Enum
import logging
import json

logger = logging.getLogger(__name__)


class StepStatus(Enum):
    """Status of a plan step"""
    PENDING = "pending"
    RUNNING = "running"
    COMPLETED = "completed"
    FAILED = "failed"
    SKIPPED = "skipped"


class StepType(Enum):
    """Type of step"""
    AGENT = "agent"           # Execute with an agent
    TOOL = "tool"             # Execute a tool directly
    CONDITION = "condition"   # Conditional branching
    PARALLEL = "parallel"     # Parallel execution
    LOOP = "loop"             # Loop execution
    WAIT = "wait"             # Wait for user input
    CHECKPOINT = "checkpoint" # Save state checkpoint


@dataclass
class PlanStep:
    """A single step in an execution plan"""
    id: str
    name: str
    step_type: StepType
    config: Dict[str, Any] = field(default_factory=dict)
    dependencies: List[str] = field(default_factory=list)
    status: StepStatus = StepStatus.PENDING
    result: Any = None
    error: Optional[str] = None

    def to_dict(self) -> Dict[str, Any]:
        return {
            "id": self.id,
            "name": self.name,
            "step_type": self.step_type.value,
            "config": self.config,
            "dependencies": self.dependencies,
            "status": self.status.value,
            "result": self.result if isinstance(self.result, (dict, list, str, int, float, bool, type(None))) else str(self.result),
            "error": self.error
        }


@dataclass
class ExecutionPlan:
    """A complete execution plan"""
    id: str
    name: str
    description: str
    steps: List[PlanStep] = field(default_factory=list)
    context: Dict[str, Any] = field(default_factory=dict)
    status: StepStatus = StepStatus.PENDING
    current_step_index: int = 0

    def add_step(self, step: PlanStep) -> None:
        """Add a step to the plan"""
        self.steps.append(step)

    def get_ready_steps(self) -> List[PlanStep]:
        """Get steps that are ready to execute (dependencies met)"""
        completed_ids = {s.id for s in self.steps if s.status == StepStatus.COMPLETED}
        ready = []

        for step in self.steps:
            if step.status != StepStatus.PENDING:
                continue
            if all(dep in completed_ids for dep in step.dependencies):
                ready.append(step)

        return ready

    def get_step(self, step_id: str) -> Optional[PlanStep]:
        """Get step by ID"""
        for step in self.steps:
            if step.id == step_id:
                return step
        return None

    def is_complete(self) -> bool:
        """Check if plan is complete"""
        return all(s.status in (StepStatus.COMPLETED, StepStatus.SKIPPED) for s in self.steps)

    def has_failed(self) -> bool:
        """Check if any step has failed"""
        return any(s.status == StepStatus.FAILED for s in self.steps)

    def to_dict(self) -> Dict[str, Any]:
        return {
            "id": self.id,
            "name": self.name,
            "description": self.description,
            "steps": [s.to_dict() for s in self.steps],
            "context": self.context,
            "status": self.status.value,
            "current_step_index": self.current_step_index,
            "is_complete": self.is_complete(),
            "has_failed": self.has_failed()
        }


class PlanBuilder:
    """Builder for creating execution plans"""

    def __init__(self, plan_id: str, name: str, description: str = ""):
        self.plan = ExecutionPlan(
            id=plan_id,
            name=name,
            description=description
        )
        self._step_counter = 0

    def _next_id(self) -> str:
        self._step_counter += 1
        return f"step_{self._step_counter}"

    def add_agent_step(
        self,
        name: str,
        query: str,
        agent_config: Dict[str, Any] = None,
        dependencies: List[str] = None,
        step_id: str = None
    ) -> str:
        """Add an agent execution step"""
        step_id = step_id or self._next_id()
        step = PlanStep(
            id=step_id,
            name=name,
            step_type=StepType.AGENT,
            config={
                "query": query,
                "agent_config": agent_config or {}
            },
            dependencies=dependencies or []
        )
        self.plan.add_step(step)
        return step_id

    def add_tool_step(
        self,
        name: str,
        tool_name: str,
        tool_args: Dict[str, Any] = None,
        dependencies: List[str] = None,
        step_id: str = None
    ) -> str:
        """Add a tool execution step"""
        step_id = step_id or self._next_id()
        step = PlanStep(
            id=step_id,
            name=name,
            step_type=StepType.TOOL,
            config={
                "tool_name": tool_name,
                "tool_args": tool_args or {}
            },
            dependencies=dependencies or []
        )
        self.plan.add_step(step)
        return step_id

    def add_condition(
        self,
        name: str,
        condition: str,
        if_true: str,
        if_false: str,
        dependencies: List[str] = None,
        step_id: str = None
    ) -> str:
        """Add a conditional branch"""
        step_id = step_id or self._next_id()
        step = PlanStep(
            id=step_id,
            name=name,
            step_type=StepType.CONDITION,
            config={
                "condition": condition,
                "if_true": if_true,
                "if_false": if_false
            },
            dependencies=dependencies or []
        )
        self.plan.add_step(step)
        return step_id

    def add_parallel(
        self,
        name: str,
        step_ids: List[str],
        dependencies: List[str] = None,
        step_id: str = None
    ) -> str:
        """Add parallel execution of multiple steps"""
        step_id = step_id or self._next_id()
        step = PlanStep(
            id=step_id,
            name=name,
            step_type=StepType.PARALLEL,
            config={"parallel_steps": step_ids},
            dependencies=dependencies or []
        )
        self.plan.add_step(step)
        return step_id

    def add_checkpoint(
        self,
        name: str,
        save_keys: List[str] = None,
        dependencies: List[str] = None,
        step_id: str = None
    ) -> str:
        """Add a checkpoint to save state"""
        step_id = step_id or self._next_id()
        step = PlanStep(
            id=step_id,
            name=name,
            step_type=StepType.CHECKPOINT,
            config={"save_keys": save_keys or []},
            dependencies=dependencies or []
        )
        self.plan.add_step(step)
        return step_id

    def build(self) -> ExecutionPlan:
        """Build and return the plan"""
        return self.plan


class ExecutionPlanner:
    """
    Orchestrates multi-step task execution.

    Features:
    - Plan creation and management
    - Step-by-step execution
    - Dependency resolution
    - Result aggregation
    - Error handling and recovery
    """

    def __init__(self, api_keys: Optional[Dict[str, str]] = None):
        self.api_keys = api_keys or {}
        self.plans: Dict[str, ExecutionPlan] = {}
        self.checkpoints: Dict[str, Dict[str, Any]] = {}

    def create_plan(
        self,
        plan_id: str,
        name: str,
        description: str = ""
    ) -> PlanBuilder:
        """Create a new plan builder"""
        return PlanBuilder(plan_id, name, description)

    def register_plan(self, plan: ExecutionPlan) -> None:
        """Register a plan for execution"""
        self.plans[plan.id] = plan

    def get_plan(self, plan_id: str) -> Optional[ExecutionPlan]:
        """Get plan by ID"""
        return self.plans.get(plan_id)

    def execute_plan(
        self,
        plan: ExecutionPlan,
        initial_context: Dict[str, Any] = None
    ) -> Dict[str, Any]:
        """
        Execute a complete plan.

        Args:
            plan: The execution plan
            initial_context: Initial context data

        Returns:
            Execution results
        """
        if initial_context:
            plan.context.update(initial_context)

        plan.status = StepStatus.RUNNING
        self.register_plan(plan)

        while not plan.is_complete() and not plan.has_failed():
            ready_steps = plan.get_ready_steps()

            if not ready_steps:
                # Deadlock detection
                logger.warning("No ready steps but plan not complete - possible deadlock")
                break

            for step in ready_steps:
                result = self._execute_step(step, plan)

                if step.status == StepStatus.FAILED:
                    plan.status = StepStatus.FAILED
                    break

        if plan.is_complete():
            plan.status = StepStatus.COMPLETED

        return plan.to_dict()

    def execute_step(
        self,
        plan_id: str,
        step_id: str
    ) -> Dict[str, Any]:
        """Execute a single step"""
        plan = self.get_plan(plan_id)
        if not plan:
            return {"success": False, "error": f"Plan not found: {plan_id}"}

        step = plan.get_step(step_id)
        if not step:
            return {"success": False, "error": f"Step not found: {step_id}"}

        result = self._execute_step(step, plan)
        return {
            "success": step.status == StepStatus.COMPLETED,
            "step": step.to_dict(),
            "result": result
        }

    def _execute_step(self, step: PlanStep, plan: ExecutionPlan) -> Any:
        """Execute a single step based on its type"""
        step.status = StepStatus.RUNNING

        try:
            if step.step_type == StepType.AGENT:
                result = self._execute_agent_step(step, plan)
            elif step.step_type == StepType.TOOL:
                result = self._execute_tool_step(step, plan)
            elif step.step_type == StepType.CONDITION:
                result = self._execute_condition_step(step, plan)
            elif step.step_type == StepType.PARALLEL:
                result = self._execute_parallel_step(step, plan)
            elif step.step_type == StepType.CHECKPOINT:
                result = self._execute_checkpoint_step(step, plan)
            else:
                result = None

            step.result = result
            step.status = StepStatus.COMPLETED

            # Store result in context
            plan.context[f"{step.id}_result"] = result

            return result

        except Exception as e:
            step.status = StepStatus.FAILED
            step.error = str(e)
            logger.error(f"Step {step.id} failed: {e}")
            return None

    def _execute_agent_step(self, step: PlanStep, plan: ExecutionPlan) -> Any:
        """Execute an agent step"""
        from finagent_core.core_agent import CoreAgent

        query = step.config.get("query", "")
        agent_config = step.config.get("agent_config", {})

        # Interpolate context into query
        query = self._interpolate(query, plan.context)

        agent = CoreAgent(api_keys=self.api_keys)
        response = agent.run(query, agent_config)
        return agent.get_response_content(response)

    def _execute_tool_step(self, step: PlanStep, plan: ExecutionPlan) -> Any:
        """Execute a tool step"""
        tool_name = step.config.get("tool_name")
        tool_args = step.config.get("tool_args", {})

        # Interpolate context into args
        tool_args = {k: self._interpolate(v, plan.context) if isinstance(v, str) else v
                     for k, v in tool_args.items()}

        # Import and execute tool
        from finagent_core.registries import ToolsRegistry
        tools = ToolsRegistry.get_tools([tool_name])

        if not tools:
            raise ValueError(f"Tool not found: {tool_name}")

        tool = tools[0]
        return tool.run(**tool_args) if hasattr(tool, 'run') else str(tool)

    def _execute_condition_step(self, step: PlanStep, plan: ExecutionPlan) -> str:
        """Execute a condition step and return the chosen branch"""
        condition = step.config.get("condition", "")
        if_true = step.config.get("if_true")
        if_false = step.config.get("if_false")

        # Simple condition evaluation using context
        condition = self._interpolate(condition, plan.context)

        try:
            result = eval(condition, {"__builtins__": {}}, plan.context)
        except Exception:
            result = False

        next_step_id = if_true if result else if_false

        # Mark skipped branch
        skipped_id = if_false if result else if_true
        skipped_step = plan.get_step(skipped_id)
        if skipped_step:
            skipped_step.status = StepStatus.SKIPPED

        return next_step_id

    def _execute_parallel_step(self, step: PlanStep, plan: ExecutionPlan) -> List[Any]:
        """Execute parallel steps"""
        parallel_ids = step.config.get("parallel_steps", [])
        results = []

        # In real implementation, would use asyncio/threads
        for step_id in parallel_ids:
            parallel_step = plan.get_step(step_id)
            if parallel_step:
                result = self._execute_step(parallel_step, plan)
                results.append(result)

        return results

    def _execute_checkpoint_step(self, step: PlanStep, plan: ExecutionPlan) -> Dict[str, Any]:
        """Save a checkpoint"""
        save_keys = step.config.get("save_keys", [])

        checkpoint = {}
        if save_keys:
            for key in save_keys:
                if key in plan.context:
                    checkpoint[key] = plan.context[key]
        else:
            checkpoint = plan.context.copy()

        self.checkpoints[f"{plan.id}:{step.id}"] = checkpoint
        return checkpoint

    def _interpolate(self, text: str, context: Dict[str, Any]) -> str:
        """Interpolate context values into text"""
        if not isinstance(text, str):
            return text

        for key, value in context.items():
            placeholder = f"{{{key}}}"
            if placeholder in text:
                text = text.replace(placeholder, str(value))

        return text

    def restore_checkpoint(self, plan_id: str, step_id: str) -> Optional[Dict[str, Any]]:
        """Restore from a checkpoint"""
        key = f"{plan_id}:{step_id}"
        return self.checkpoints.get(key)

    # =========================================================================
    # Plan Templates for Common Tasks
    # =========================================================================

    @staticmethod
    def stock_analysis_plan(symbol: str) -> ExecutionPlan:
        """Create a stock analysis plan"""
        builder = PlanBuilder(
            plan_id=f"stock_analysis_{symbol}",
            name=f"Stock Analysis: {symbol}",
            description=f"Comprehensive analysis of {symbol}"
        )

        # Parallel data fetching
        fetch_price = builder.add_agent_step(
            name="Fetch Price Data",
            query=f"Get current price, 52-week high/low, and recent price history for {symbol}",
            agent_config={"tools": ["yfinance"]}
        )

        fetch_fundamentals = builder.add_agent_step(
            name="Fetch Fundamentals",
            query=f"Get P/E ratio, EPS, revenue, and other key fundamentals for {symbol}",
            agent_config={"tools": ["yfinance"]}
        )

        fetch_news = builder.add_agent_step(
            name="Fetch News",
            query=f"Get recent news and headlines for {symbol}",
            agent_config={"tools": ["duckduckgo"]}
        )

        # Analysis (depends on data)
        analyze = builder.add_agent_step(
            name="Analyze Stock",
            query=f"""Based on the data collected, provide a comprehensive analysis of {symbol}:
            - Technical analysis (price trends, support/resistance)
            - Fundamental analysis (valuation, growth)
            - News sentiment
            - Overall recommendation""",
            agent_config={"reasoning": True},
            dependencies=[fetch_price, fetch_fundamentals, fetch_news]
        )

        # Checkpoint
        builder.add_checkpoint(
            name="Save Analysis",
            dependencies=[analyze]
        )

        return builder.build()

    @staticmethod
    def portfolio_rebalance_plan(portfolio_id: str) -> ExecutionPlan:
        """Create a portfolio rebalancing plan"""
        builder = PlanBuilder(
            plan_id=f"rebalance_{portfolio_id}",
            name=f"Portfolio Rebalance: {portfolio_id}",
            description="Analyze and rebalance portfolio"
        )

        # Fetch current positions
        fetch_positions = builder.add_agent_step(
            name="Fetch Positions",
            query=f"Get current positions and allocations for portfolio {portfolio_id}",
            agent_config={"tools": ["yfinance"]}
        )

        # Analyze risk
        analyze_risk = builder.add_agent_step(
            name="Analyze Risk",
            query="Analyze portfolio risk metrics: volatility, correlation, concentration",
            dependencies=[fetch_positions]
        )

        # Generate recommendations
        recommend = builder.add_agent_step(
            name="Generate Recommendations",
            query="Based on the analysis, generate rebalancing recommendations",
            agent_config={"reasoning": True},
            dependencies=[analyze_risk]
        )

        # Calculate trades
        calculate = builder.add_agent_step(
            name="Calculate Trades",
            query="Calculate specific trades needed to implement the recommendations",
            agent_config={"tools": ["calculator"]},
            dependencies=[recommend]
        )

        return builder.build()


# =========================================================================
# Dynamic LLM-based Plan Generation
# =========================================================================

_PLAN_GENERATION_SYSTEM_PROMPT = """You are an expert financial analyst AI that creates structured execution plans.
Given a user's analysis request, generate a step-by-step execution plan as a JSON object.

The plan must follow this exact JSON schema:
{
  "name": "Human-readable plan title",
  "description": "Brief description of what this plan does",
  "steps": [
    {
      "id": "step_1",
      "name": "Step Name",
      "step_type": "agent",
      "query": "The specific prompt/query this step will execute",
      "tools": ["yfinance", "duckduckgo"],
      "dependencies": [],
      "reasoning": false
    }
  ]
}

Rules:
- step_type must be one of: agent, tool, checkpoint
- tools can include: yfinance, duckduckgo, calculator, edgar, fred, worldbank
- dependencies is a list of step IDs that must complete before this step
- Set reasoning=true for analysis/synthesis steps that require deep thinking
- Keep steps focused — each step should do one clear thing
- Use parallel steps (empty dependencies) for independent data fetching
- Final steps should synthesize earlier results
- Return ONLY valid JSON, no markdown fences, no extra text"""


def generate_dynamic_plan(query: str, api_keys: Dict[str, str] = None) -> Dict[str, Any]:
    """
    Use an LLM to dynamically generate an execution plan from a natural language query.
    Returns an ExecutionPlan dict ready for display and execution.
    """
    import time
    api_keys = api_keys or {}

    try:
        from finagent_core.core_agent import CoreAgent

        agent = CoreAgent(api_keys=api_keys)

        # Build a minimal model config preferring fast/cheap models
        config = {
            "instructions": _PLAN_GENERATION_SYSTEM_PROMPT,
            "markdown": False,
            "reasoning": False,
        }

        prompt = (
            f"Create an execution plan for the following financial analysis request:\n\n"
            f"{query}\n\n"
            f"Return ONLY the JSON plan object, no extra text."
        )

        response = agent.run(prompt, config)
        raw = agent.get_response_content(response)

        # Strip any accidental markdown fences
        raw = raw.strip()
        if raw.startswith("```"):
            raw = raw.split("```", 2)[1]
            if raw.startswith("json"):
                raw = raw[4:]
            raw = raw.rsplit("```", 1)[0].strip()

        plan_spec = json.loads(raw)

        # Build ExecutionPlan from LLM output
        plan_id = f"dynamic_{int(time.time())}"
        builder = PlanBuilder(
            plan_id=plan_id,
            name=plan_spec.get("name", "Custom Analysis Plan"),
            description=plan_spec.get("description", query),
        )

        # First pass: add all steps, collect id mapping
        step_id_map: Dict[str, str] = {}
        for i, s in enumerate(plan_spec.get("steps", []), start=1):
            logical_id = s.get("id", f"step_{i}")
            tools = s.get("tools", [])
            agent_cfg: Dict[str, Any] = {}
            if tools:
                agent_cfg["tools"] = tools
            if s.get("reasoning"):
                agent_cfg["reasoning"] = True

            # Resolve dependencies to builder step IDs
            deps = [step_id_map[d] for d in s.get("dependencies", []) if d in step_id_map]

            step_type = s.get("step_type", "agent")
            if step_type == "checkpoint":
                real_id = builder.add_checkpoint(
                    name=s.get("name", f"Step {i}"),
                    dependencies=deps,
                )
            else:
                real_id = builder.add_agent_step(
                    name=s.get("name", f"Step {i}"),
                    query=s.get("query", ""),
                    agent_config=agent_cfg,
                    dependencies=deps,
                )

            step_id_map[logical_id] = real_id

        plan = builder.build()
        return {"success": True, "plan": plan.to_dict(), "generated_by": "llm"}

    except json.JSONDecodeError as e:
        logger.warning(f"LLM returned invalid JSON for plan generation: {e}. Falling back to template.")
        return _fallback_plan_from_query(query)
    except Exception as e:
        logger.warning(f"Dynamic plan generation failed: {e}. Falling back to template.")
        return _fallback_plan_from_query(query)


def _fallback_plan_from_query(query: str) -> Dict[str, Any]:
    """Fallback: derive a reasonable plan from keywords in the query."""
    import time
    query_lower = query.lower()

    # Detect symbol — first uppercase word-like token
    import re
    symbol_match = re.search(r'\b([A-Z]{1,5}(?:\.[A-Z]{1,2})?)\b', query)
    symbol = symbol_match.group(1) if symbol_match else "AAPL"

    if any(w in query_lower for w in ["portfolio", "rebalance", "holdings"]):
        plan = ExecutionPlanner.portfolio_rebalance_plan(symbol)
    else:
        plan = ExecutionPlanner.stock_analysis_plan(symbol)

    result = plan.to_dict()
    result["id"] = f"fallback_{int(time.time())}"
    return {"success": True, "plan": result, "generated_by": "fallback"}


# Entry points for Tauri commands
def create_stock_analysis_plan(symbol: str) -> Dict[str, Any]:
    """Create a stock analysis plan"""
    plan = ExecutionPlanner.stock_analysis_plan(symbol)
    return plan.to_dict()


def execute_plan(plan_dict: Dict[str, Any], api_keys: Dict[str, str] = None) -> Dict[str, Any]:
    """Execute a plan from dict"""
    # Reconstruct plan from dict
    plan = ExecutionPlan(
        id=plan_dict["id"],
        name=plan_dict["name"],
        description=plan_dict.get("description", ""),
        context=plan_dict.get("context", {})
    )

    for step_dict in plan_dict.get("steps", []):
        step = PlanStep(
            id=step_dict["id"],
            name=step_dict["name"],
            step_type=StepType(step_dict["step_type"]),
            config=step_dict.get("config", {}),
            dependencies=step_dict.get("dependencies", []),
            status=StepStatus(step_dict.get("status", "pending"))
        )
        plan.add_step(step)

    planner = ExecutionPlanner(api_keys=api_keys)
    return planner.execute_plan(plan)


def get_plan_status(plan_id: str) -> Dict[str, Any]:
    """Get plan status"""
    # In a real implementation, this would retrieve from storage
    return {"success": False, "error": "Plan not found - use execute_plan instead"}
