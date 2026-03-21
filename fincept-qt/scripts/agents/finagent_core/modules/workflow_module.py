"""
Workflow Module - Complex task orchestration using Agno's native subclass pattern.

Each financial workflow is a proper Agno Workflow subclass with:
- WorkflowAgent for LLM execution
- Step / Parallel / Loop / Condition / Router primitives
- StepInput / StepOutput typed I/O
- Session state + streaming support
"""

from __future__ import annotations

from datetime import date
from typing import Any, Dict, Iterator, List, Optional, Callable, Union
import logging

logger = logging.getLogger(__name__)

# ─── Helpers ──────────────────────────────────────────────────────────────────

def _today() -> str:
    return date.today().strftime("%B %d, %Y")


def _resolve_provider(api_keys: Dict[str, str]) -> str:
    """Pick first configured provider from api_keys."""
    preferred = ["fincept", "ollama", "anthropic", "google", "groq",
                 "deepseek", "openai", "openrouter"]
    for p in preferred:
        if api_keys.get(p) or api_keys.get(f"{p.upper()}_API_KEY"):
            return p
    for k, v in api_keys.items():
        if k.endswith("_API_KEY") and v:
            return k[:-8].lower()
    return "openai"


def _make_model(api_keys: Dict[str, str], model_config: Optional[Dict] = None):
    """Create an agno Model from api_keys + optional model_config dict."""
    from finagent_core.registries import ModelsRegistry
    cfg = model_config or {}
    provider = cfg.get("provider") or _resolve_provider(api_keys)
    return ModelsRegistry.create_model(
        provider=provider,
        model_id=cfg.get("model_id"),
        api_keys=api_keys,
        temperature=cfg.get("temperature"),
        max_tokens=cfg.get("max_tokens"),
    )


def _load_workflow_tools(api_keys: Dict[str, str],
                          terminal_endpoint: Optional[str] = None,
                          terminal_tool_defs: Optional[List] = None) -> List[Any]:
    """
    Load tools for workflow agents:
    - yfinance (real-time market data, prices, financials)
    - duckduckgo (news and web search)
    - TerminalToolkit (internal MCP: get_quote, portfolio, etc.)
    Falls back gracefully if any tool fails to load.
    """
    tools = []

    # Core financial data tools via ToolsRegistry
    try:
        from finagent_core.registries import ToolsRegistry
        financial_tools = ToolsRegistry.get_tools(
            ["yfinance", "duckduckgo"],
            api_keys=api_keys
        )
        tools.extend(financial_tools)
        logger.debug(f"WorkflowTools: loaded {len(financial_tools)} financial tools")
    except Exception as e:
        logger.warning(f"WorkflowTools: ToolsRegistry load failed: {e}")

    # Terminal MCP bridge tools (get_quote, portfolio, navigation, etc.)
    if terminal_endpoint and terminal_tool_defs:
        try:
            from finagent_core.tools.terminal_toolkit import TerminalToolkit
            toolkit = TerminalToolkit(
                endpoint=terminal_endpoint,
                tool_definitions=terminal_tool_defs,
            )
            tools.extend(toolkit.get_tools())
            logger.debug(f"WorkflowTools: loaded {len(toolkit.functions)} terminal MCP tools")
        except Exception as e:
            logger.warning(f"WorkflowTools: TerminalToolkit load failed: {e}")

    return tools


def _make_workflow_agent(api_keys: Dict[str, str],
                         instructions: str,
                         model_config: Optional[Dict] = None,
                         tools: Optional[List] = None) -> Any:
    """
    Create an agent for workflow step execution.
    Uses Agent (supports tools) when tools are provided,
    falls back to WorkflowAgent when no tools are needed.
    """
    model = _make_model(api_keys, model_config)
    if tools:
        from agno.agent import Agent
        return Agent(model=model, instructions=instructions, tools=tools, markdown=False)
    else:
        from agno.workflow import WorkflowAgent
        return WorkflowAgent(model=model, instructions=instructions)


def _step_output(content: Any) -> Any:
    from agno.workflow.types import StepOutput
    return StepOutput(content=content)


def _extract_content(step_input: Any) -> Any:
    """Extract the input payload from a StepInput."""
    return getattr(step_input, "input", None) or getattr(step_input, "previous_step_content", None) or {}


# ─── Stock Analysis Workflow ──────────────────────────────────────────────────

class StockAnalysisWorkflow:
    """
    Multi-step stock analysis pipeline.

    Steps:
    1. Fetch market data
    2. Technical + Fundamental analysis (Parallel)
    3. Generate recommendation
    4. Create report
    """

    def __init__(self, api_keys: Dict[str, str], model_config: Optional[Dict] = None,
                 tools: Optional[List] = None):
        self.api_keys = api_keys
        self._tools = tools
        self.model_config = model_config or {}

    def run(self, symbol: str) -> Dict[str, Any]:
        from agno.workflow import Workflow, Step, Parallel
        from agno.workflow.types import StepOutput

        today = _today()
        agent = _make_workflow_agent(
            self.api_keys,
            f"You are a professional financial analyst. Today is {today}. Always use today's date in reports.",
            self.model_config,
            tools=self._tools,
        )

        def fetch_step(step_input):
            inp = _extract_content(step_input)
            sym = inp.get("symbol", symbol) if isinstance(inp, dict) else symbol
            response = agent.run(
                f"Today is {today}. Fetch and summarize current market data for {sym} "
                f"including price, volume, recent news, and key metrics."
            )
            return _step_output(getattr(response, "content", str(response)))

        def tech_step(step_input):
            prev = getattr(step_input, "previous_step_content", "")
            response = agent.run(
                f"Today is {today}. Perform technical analysis for {symbol}. "
                f"Market data context: {prev}"
            )
            return _step_output(getattr(response, "content", str(response)))

        def fund_step(step_input):
            prev = getattr(step_input, "previous_step_content", "")
            response = agent.run(
                f"Today is {today}. Perform fundamental analysis for {symbol} "
                f"including valuation, financials, and competitive position. Context: {prev}"
            )
            return _step_output(getattr(response, "content", str(response)))

        def rec_step(step_input):
            prev = getattr(step_input, "previous_step_content", "")
            response = agent.run(
                f"Today is {today}. Based on the analysis, provide a clear "
                f"buy/sell/hold recommendation for {symbol}. Analysis: {prev}"
            )
            return _step_output(getattr(response, "content", str(response)))

        def report_step(step_input):
            prev = getattr(step_input, "previous_step_content", "")
            response = agent.run(
                f"Today is {today}. Generate a comprehensive investment report for {symbol}. "
                f"Report Date MUST be {today}. "
                f"Include executive summary, business overview, financials, risks, and recommendation. "
                f"Analysis: {prev}"
            )
            return _step_output(getattr(response, "content", str(response)))

        workflow = Workflow(
            name=f"Stock Analysis: {symbol}",
            steps=[
                Step(name="fetch_market_data", executor=fetch_step),
                Parallel(
                    Step(name="technical_analysis", executor=tech_step),
                    Step(name="fundamental_analysis", executor=fund_step),
                ),
                Step(name="generate_recommendation", executor=rec_step),
                Step(name="create_report", executor=report_step),
            ]
        )

        result = workflow.run(input={"symbol": symbol})
        return {"success": True, "response": getattr(result, "content", str(result))}


# ─── Portfolio Rebalancing Workflow ───────────────────────────────────────────

class PortfolioRebalancingWorkflow:
    """Portfolio rebalancing using Condition for drift check."""

    def __init__(self, api_keys: Dict[str, str], model_config: Optional[Dict] = None,
                 tools: Optional[List] = None):
        self.api_keys = api_keys
        self.model_config = model_config or {}
        self._tools = tools

    def run(self, portfolio_data: Dict[str, Any]) -> Dict[str, Any]:
        from agno.workflow import Workflow, Step, Condition

        today = _today()
        agent = _make_workflow_agent(
            self.api_keys,
            f"You are a portfolio manager. Today is {today}.",
            self.model_config,
            tools=self._tools,
        )

        def analyze_step(step_input):
            response = agent.run(
                f"Today is {today}. Analyze this portfolio and calculate allocation drift: "
                f"{portfolio_data}"
            )
            return _step_output(getattr(response, "content", str(response)))

        def rebalance_step(step_input):
            prev = getattr(step_input, "previous_step_content", "")
            response = agent.run(
                f"Today is {today}. Calculate optimal rebalancing trades. Analysis: {prev}"
            )
            return _step_output(getattr(response, "content", str(response)))

        def no_action_step(step_input):
            return _step_output("Portfolio is within tolerance. No rebalancing needed.")

        def orders_step(step_input):
            prev = getattr(step_input, "previous_step_content", "")
            response = agent.run(
                f"Today is {today}. Generate specific trade orders for rebalancing. Plan: {prev}"
            )
            return _step_output(getattr(response, "content", str(response)))

        def needs_rebal(step_input):
            prev = str(getattr(step_input, "previous_step_content", ""))
            keywords = ["drift", "rebalance", "exceed", "above", "below", "overweight", "underweight"]
            return any(k in prev.lower() for k in keywords)

        workflow = Workflow(
            name="Portfolio Rebalancing",
            steps=[
                Step(name="analyze_portfolio", executor=analyze_step),
                Condition(
                    needs_rebal,
                    steps=[Step(name="rebalance_plan", executor=rebalance_step)],
                    name="check_drift",
                ),
                Step(name="generate_orders", executor=orders_step),
            ]
        )

        result = workflow.run(input=portfolio_data)
        return {"success": True, "response": getattr(result, "content", str(result))}


# ─── Risk Assessment Workflow ─────────────────────────────────────────────────

class RiskAssessmentWorkflow:
    """Risk assessment using Router to route to the right risk analysis."""

    def __init__(self, api_keys: Dict[str, str], model_config: Optional[Dict] = None,
                 tools: Optional[List] = None):
        self.api_keys = api_keys
        self.model_config = model_config or {}
        self._tools = tools

    def run(self, portfolio_data: Dict[str, Any]) -> Dict[str, Any]:
        from agno.workflow import Workflow, Step, Router

        today = _today()
        agent = _make_workflow_agent(
            self.api_keys,
            f"You are a risk analyst. Today is {today}.",
            self.model_config,
            tools=self._tools,
        )

        def identify_step(step_input):
            response = agent.run(
                f"Today is {today}. Identify the primary risk type (market/credit/liquidity/operational) "
                f"for this portfolio: {portfolio_data}"
            )
            return _step_output(getattr(response, "content", str(response)))

        def market_risk_step(step_input):
            prev = getattr(step_input, "previous_step_content", "")
            response = agent.run(
                f"Today is {today}. Perform market risk analysis (VaR, beta, volatility). Context: {prev}"
            )
            return _step_output(getattr(response, "content", str(response)))

        def credit_risk_step(step_input):
            prev = getattr(step_input, "previous_step_content", "")
            response = agent.run(
                f"Today is {today}. Perform credit risk analysis. Context: {prev}"
            )
            return _step_output(getattr(response, "content", str(response)))

        def liquidity_risk_step(step_input):
            prev = getattr(step_input, "previous_step_content", "")
            response = agent.run(
                f"Today is {today}. Analyze liquidity risk (bid-ask, volume). Context: {prev}"
            )
            return _step_output(getattr(response, "content", str(response)))

        def general_risk_step(step_input):
            prev = getattr(step_input, "previous_step_content", "")
            response = agent.run(
                f"Today is {today}. Perform general risk assessment. Context: {prev}"
            )
            return _step_output(getattr(response, "content", str(response)))

        def report_step(step_input):
            prev = getattr(step_input, "previous_step_content", "")
            response = agent.run(
                f"Today is {today}. Generate a comprehensive risk report. Report Date MUST be {today}. "
                f"Analysis: {prev}"
            )
            return _step_output(getattr(response, "content", str(response)))

        market_s  = Step(name="market_risk",    executor=market_risk_step)
        credit_s  = Step(name="credit_risk",    executor=credit_risk_step)
        liquid_s  = Step(name="liquidity_risk", executor=liquidity_risk_step)
        general_s = Step(name="general_risk",   executor=general_risk_step)
        choices   = [market_s, credit_s, liquid_s, general_s]

        def selector(step_input):
            prev = str(getattr(step_input, "previous_step_content", "")).lower()
            if "credit"    in prev: return [credit_s]
            if "liquidity" in prev: return [liquid_s]
            if "general"   in prev: return [general_s]
            return [market_s]

        workflow = Workflow(
            name="Risk Assessment",
            steps=[
                Step(name="identify_risk_type", executor=identify_step),
                Router(selector, choices=choices, name="risk_router"),
                Step(name="generate_risk_report", executor=report_step),
            ]
        )

        result = workflow.run(input=portfolio_data)
        return {"success": True, "response": getattr(result, "content", str(result))}


# ─── Backwards-compatible factory (used by core_agent.py) ────────────────────

class FinancialWorkflowTemplates:
    """Factory that returns workflow instances — keeps core_agent.py unchanged."""

    @staticmethod
    def stock_analysis_pipeline(api_keys: Dict[str, str] = None,
                                 model_config: Optional[Dict] = None,
                                 tools: Optional[List] = None,
                                 terminal_endpoint: Optional[str] = None,
                                 terminal_tool_defs: Optional[List] = None,
                                 **_) -> "StockAnalysisWorkflow":
        _keys = api_keys or {}
        _tools = tools or _load_workflow_tools(_keys, terminal_endpoint, terminal_tool_defs)
        return StockAnalysisWorkflow(_keys, model_config, tools=_tools)

    @staticmethod
    def portfolio_rebalancing(api_keys: Dict[str, str] = None,
                               model_config: Optional[Dict] = None,
                               tools: Optional[List] = None,
                               terminal_endpoint: Optional[str] = None,
                               terminal_tool_defs: Optional[List] = None,
                               **_) -> "PortfolioRebalancingWorkflow":
        _keys = api_keys or {}
        _tools = tools or _load_workflow_tools(_keys, terminal_endpoint, terminal_tool_defs)
        return PortfolioRebalancingWorkflow(_keys, model_config, tools=_tools)

    @staticmethod
    def risk_assessment(api_keys: Dict[str, str] = None,
                        model_config: Optional[Dict] = None,
                        tools: Optional[List] = None,
                        terminal_endpoint: Optional[str] = None,
                        terminal_tool_defs: Optional[List] = None,
                        **_) -> "RiskAssessmentWorkflow":
        _keys = api_keys or {}
        _tools = tools or _load_workflow_tools(_keys, terminal_endpoint, terminal_tool_defs)
        return RiskAssessmentWorkflow(_keys, model_config, tools=_tools)


# ─── Kept for any direct usage ────────────────────────────────────────────────

class SimpleWorkflowExecutor:
    """Fallback sequential executor (backwards compat)."""
    def __init__(self, steps): self.steps = steps
    def run(self, input=None, **kwargs):
        context = input or {}
        results = []
        for step in self.steps:
            fn = step.get("function")
            agent = step.get("agent")
            if fn:
                r = fn(context)
            elif agent:
                r = agent.run(step.get("prompt", "").format(**context))
            else:
                r = None
            results.append(r)
            context["last_result"] = r
        return {"success": True, "results": results}
    async def arun(self, input=None, **kwargs): return self.run(input=input)


class WorkflowBuilder:
    """Fluent builder (backwards compat) — delegates to WorkflowModule."""
    def __init__(self, name: str):
        self._module = WorkflowModule(name=name)
    def with_description(self, d):
        self._module.description = d; return self
    def add_step(self, name, agent=None, function=None, prompt=None, **kw):
        self._module.add_step(name, agent=agent, function=function, prompt=prompt, **kw); return self
    def add_parallel(self, name, steps):
        self._module.add_parallel(name, steps); return self
    def add_condition(self, name, condition, if_true, if_false=None):
        self._module.add_condition(name, condition, if_true, if_false); return self
    def add_loop(self, name, steps, condition=None, max_iterations=10):
        self._module.add_loop(name, steps, condition, max_iterations); return self
    def build(self): return self._module.build()
    def get_module(self): return self._module


class WorkflowModule:
    """Thin shim kept for backwards compatibility. Delegates to workflow classes."""

    def __init__(self, name: str = "Workflow", description: Optional[str] = None, **_):
        self.name = name
        self.description = description
        self._steps: List[Dict[str, Any]] = []

    def add_step(self, name, agent=None, function=None, prompt=None, **kwargs):
        self._steps.append({"type": "step", "name": name, "agent": agent,
                             "function": function, "prompt": prompt, **kwargs})
        return self

    def add_parallel(self, name, steps, **kwargs):
        self._steps.append({"type": "parallel", "name": name, "steps": steps, **kwargs})
        return self

    def add_condition(self, name, condition, if_true, if_false=None, **kwargs):
        self._steps.append({"type": "condition", "name": name, "condition": condition,
                             "if_true": if_true, "if_false": if_false, **kwargs})
        return self

    def add_loop(self, name, steps, condition=None, max_iterations=10, **kwargs):
        self._steps.append({"type": "loop", "name": name, "steps": steps,
                             "condition": condition, "max_iterations": max_iterations, **kwargs})
        return self

    def build(self) -> Any:
        from agno.workflow import Workflow, Step, Parallel, Loop, Condition, Router
        if not self._steps:
            raise ValueError("Cannot build workflow without steps")
        agno_steps = self._convert(self._steps)
        return Workflow(name=self.name, description=self.description, steps=agno_steps)

    def run(self, input_data=None, **kwargs):
        result = self.build().run(input=input_data, **kwargs)
        return getattr(result, "content", str(result))

    def _convert(self, steps):
        from agno.workflow import Step, Parallel, Loop, Condition, Router
        from agno.workflow.types import StepOutput
        out = []
        for cfg in steps:
            t = cfg.get("type", "step")
            if t == "step":
                out.append(self._make_step(cfg))
            elif t == "parallel":
                inner = self._convert(cfg.get("steps", []))
                out.append(Parallel(*inner, name=cfg["name"]))
            elif t == "condition":
                fn = cfg["condition"]
                branches = []
                if cfg.get("if_true"):  branches.append(self._make_step(cfg["if_true"]))
                if cfg.get("if_false"): branches.append(self._make_step(cfg["if_false"]))
                def _ev(si, _fn=fn):
                    ctx = _extract_content(si)
                    return bool(_fn(ctx if isinstance(ctx, dict) else {}))
                out.append(Condition(_ev, steps=branches, name=cfg["name"]))
            elif t == "loop":
                inner = self._convert(cfg.get("steps", []))
                kw: Dict = {"name": cfg["name"], "steps": inner,
                            "max_iterations": cfg.get("max_iterations", 10)}
                raw = cfg.get("condition")
                if raw:
                    def _ec(outputs, _r=raw):
                        ctx = {"last_result": getattr(outputs[-1], "content", None)} if outputs else {}
                        return not bool(_r(ctx))
                    kw["end_condition"] = _ec
                out.append(Loop(**kw))
            elif t == "router":
                routes = cfg.get("routes", {})
                rfn   = cfg["router_function"]
                keys  = list(routes.keys())
                choices = self._convert(list(routes.values()))
                def _sel(si, _keys=keys, _choices=choices, _rfn=rfn):
                    ctx = _extract_content(si)
                    key = _rfn(ctx if isinstance(ctx, dict) else {})
                    idx = _keys.index(key) if key in _keys else 0
                    return [_choices[idx]]
                out.append(Router(_sel, choices=choices, name=cfg["name"]))
        return out

    def _make_step(self, cfg):
        from agno.workflow import Step
        from agno.workflow.types import StepOutput
        name = cfg["name"]
        fn   = cfg.get("function")
        agent = cfg.get("agent")
        prompt = cfg.get("prompt")
        if fn:
            def _ex(si, _fn=fn):
                ctx = _extract_content(si)
                r = _fn(ctx if isinstance(ctx, dict) else {})
                return r if isinstance(r, StepOutput) else StepOutput(content=r)
            return Step(name=name, executor=_ex)
        elif agent and prompt:
            _a, _p = agent, prompt
            def _ex(si, _a=_a, _p=_p):
                ctx = _extract_content(si)
                ctx = ctx if isinstance(ctx, dict) else {}
                try:    fmt = _p.format(**ctx)
                except: fmt = _p
                resp = _a.run(fmt)
                return StepOutput(content=getattr(resp, "content", str(resp)))
            return Step(name=name, executor=_ex)
        elif agent:
            return Step(name=name, agent=agent)
        else:
            return Step(name=name, executor=lambda si: StepOutput(content=None))
