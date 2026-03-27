"""
orchestrator.py — Fallback for non-tool-calling LLMs (Fincept API, Ollama).

Single responsibility:
  - Execute multi-step agent workflows via prompt-loop when the configured LLM
    does not support LangChain tool calling (e.g. Fincept hosted LLM, Ollama)
  - Produce the same output format as the deepagents library path so cli.py
    routing is transparent to the caller

Does NOT use deepagents library — pure HTTP + prompt engineering.
"""

from __future__ import annotations

import json
import logging
import time
from typing import Any

import requests

logger = logging.getLogger(__name__)

# Fincept hosted LLM endpoint
_FINCEPT_LLM_URL   = "https://api.fincept.in/research/llm"
_FINCEPT_LLM_ASYNC = "https://api.fincept.in/research/llm/async"
_DEFAULT_TIMEOUT   = 120  # seconds

# System prompts for each specialist role
SUBAGENT_PROMPTS: dict[str, str] = {
    "research": (
        "You are a financial research specialist. Gather and synthesize comprehensive "
        "information on the given topic. Be thorough, cite specific facts and figures, "
        "and note data recency."
    ),
    "data-analyst": (
        "You are a quantitative financial analyst. Analyze the data provided, compute "
        "relevant metrics, identify trends, and derive actionable insights. "
        "Be precise with numbers and explain your methodology."
    ),
    "trading": (
        "You are a trading strategy specialist. Design specific entry/exit criteria, "
        "signal logic, and risk parameters. Provide concrete, actionable strategy details "
        "with clear rationale."
    ),
    "risk-analyzer": (
        "You are a risk management specialist. Assess VaR, drawdown, concentration risk, "
        "and tail risk. Provide severity ratings and specific mitigation recommendations."
    ),
    "portfolio-optimizer": (
        "You are a portfolio optimization specialist. Apply mean-variance analysis, "
        "factor exposures, and rebalancing logic. Provide specific allocation recommendations."
    ),
    "backtester": (
        "You are a backtesting specialist. Evaluate strategy performance historically, "
        "flag biases, compute standard metrics, and assess statistical significance."
    ),
    "reporter": (
        "You are a senior financial analyst writing a professional report. "
        "Synthesize all findings into a structured document with Executive Summary, "
        "Analysis, Risks, and Recommendations sections."
    ),
    "macro-economist": (
        "You are a macroeconomic analyst. Interpret economic indicators, central bank policy, "
        "and global trends. Connect macro regime to asset class implications."
    ),
}


class FinceptOrchestrator:
    """
    Prompt-loop multi-agent orchestrator for non-tool-calling LLMs.

    Simulates subagent delegation via sequential prompting.
    Produces the same output schema as the deepagents library path.
    """

    def __init__(self, api_key: str | None = None):
        self.api_key = api_key
        self._session = requests.Session()
        if api_key:
            self._session.headers.update({"Authorization": f"Bearer {api_key}"})

    # ------------------------------------------------------------------
    # Public API — matches cli.py expectations
    # ------------------------------------------------------------------

    def execute(
        self,
        task: str,
        agent_type: str,
        subagent_names: list[str],
    ) -> dict[str, Any]:
        """
        Execute a full multi-step task.

        Returns dict with: success, result, todos, files, error
        """
        try:
            plan = self._create_plan(task, agent_type, subagent_names)
            todos = []
            step_results = []

            for i, step in enumerate(plan):
                todo_id = f"todo-{i + 1}"
                todos.append({
                    "id":       todo_id,
                    "task":     step["step"],
                    "status":   "in_progress",
                    "subtasks": [],
                })

                result = self._execute_step(
                    task=task,
                    step_prompt=step["prompt"],
                    specialist=step["specialist"],
                    previous_results=step_results,
                )

                step_results.append({
                    "step":       step["step"],
                    "specialist": step["specialist"],
                    "result":     result,
                })
                todos[i]["status"] = "completed"

            final_report = self._synthesize(task, step_results)

            return {
                "success":   True,
                "result":    final_report,
                "todos":     todos,
                "files":     {},
                "error":     None,
            }

        except Exception as exc:
            logger.error("Orchestrator execute failed: %s", exc)
            return {
                "success": False,
                "result":  "",
                "todos":   [],
                "files":   {},
                "error":   str(exc),
            }

    def create_plan(
        self,
        task: str,
        agent_type: str,
        subagent_names: list[str],
    ) -> dict[str, Any]:
        """
        Create an execution plan without running it.

        Returns dict with: success, todos, plan, error
        """
        try:
            plan = self._create_plan(task, agent_type, subagent_names)
            todos = [
                {
                    "id":         f"todo-{i + 1}",
                    "task":       step["step"],
                    "status":     "pending",
                    "specialist": step["specialist"],
                    "prompt":     step["prompt"],
                    "subtasks":   [],
                }
                for i, step in enumerate(plan)
            ]
            return {"success": True, "todos": todos, "plan": plan, "error": None}

        except Exception as exc:
            logger.error("Orchestrator create_plan failed: %s", exc)
            return {"success": False, "todos": [], "plan": [], "error": str(exc)}

    def execute_step(
        self,
        task: str,
        step_prompt: str,
        specialist: str,
        previous_results: list[dict[str, Any]],
    ) -> dict[str, Any]:
        """
        Execute a single step.

        Returns dict with: success, result, error
        """
        try:
            result = self._execute_step(task, step_prompt, specialist, previous_results)
            return {"success": True, "result": result, "error": None}
        except Exception as exc:
            logger.error("Orchestrator execute_step failed: %s", exc)
            return {"success": False, "result": "", "error": str(exc)}

    def synthesize(
        self,
        task: str,
        step_results: list[dict[str, Any]],
    ) -> dict[str, Any]:
        """
        Synthesize step results into final report.

        Returns dict with: success, result, error
        """
        try:
            result = self._synthesize(task, step_results)
            return {"success": True, "result": result, "error": None}
        except Exception as exc:
            logger.error("Orchestrator synthesize failed: %s", exc)
            return {"success": False, "result": "", "error": str(exc)}

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------

    def _create_plan(
        self,
        task: str,
        agent_type: str,
        subagent_names: list[str],
    ) -> list[dict[str, str]]:
        """Generate a list of steps with specialist assignments."""
        specialists = subagent_names if subagent_names else ["data-analyst", "reporter"]

        prompt = (
            f"You are a planning agent for a financial analysis task.\n\n"
            f"Task: {task}\n\n"
            f"Available specialists: {', '.join(specialists)}\n\n"
            f"Create a concise execution plan as a JSON array. Each item must have:\n"
            f'  "step": short step title\n'
            f'  "specialist": one of the available specialists\n'
            f'  "prompt": specific instructions for that specialist\n\n'
            f"Return ONLY the JSON array, no other text."
        )

        raw = self._call_llm(prompt, max_tokens=800)

        # Extract JSON array from response
        try:
            start = raw.find("[")
            end   = raw.rfind("]") + 1
            if start >= 0 and end > start:
                return json.loads(raw[start:end])
        except (json.JSONDecodeError, ValueError):
            pass

        # Fallback: generate a minimal plan
        logger.warning("Plan parsing failed, using fallback plan")
        return [
            {
                "step":       "Research and Analysis",
                "specialist": specialists[0] if specialists else "data-analyst",
                "prompt":     task,
            },
            {
                "step":       "Generate Report",
                "specialist": "reporter",
                "prompt":     f"Summarize findings for: {task}",
            },
        ]

    def _execute_step(
        self,
        task: str,
        step_prompt: str,
        specialist: str,
        previous_results: list[dict[str, Any]],
    ) -> str:
        """Run a single specialist step."""
        system = SUBAGENT_PROMPTS.get(
            specialist,
            SUBAGENT_PROMPTS["data-analyst"],
        )

        prev_context = ""
        if previous_results:
            prev_context = "\n\nPrevious findings from the team:\n"
            for pr in previous_results:
                prev_context += (
                    f"--- {pr.get('step', '')} "
                    f"(by {pr.get('specialist', '')}) ---\n"
                    f"{pr.get('result', '')}\n\n"
                )

        full_prompt = (
            f"{system}\n\n"
            f"Overall task: {task}\n"
            f"Your assignment: {step_prompt}"
            f"{prev_context}\n\n"
            f"Provide thorough, detailed analysis with specific data points and insights."
        )

        return self._call_llm(full_prompt, max_tokens=1500)

    def _synthesize(
        self,
        task: str,
        step_results: list[dict[str, Any]],
    ) -> str:
        """Combine all step results into a final report."""
        findings = ""
        for sr in step_results:
            findings += (
                f"--- {sr.get('step', '')} "
                f"(by {sr.get('specialist', '')}) ---\n"
                f"{sr.get('result', '')}\n\n"
            )

        prompt = (
            f"You are a senior financial analyst writing a comprehensive report.\n\n"
            f"Original task: {task}\n\n"
            f"Specialist findings:\n{findings}\n"
            f"Synthesize all findings into a single cohesive, well-structured report.\n"
            f"Include: Executive Summary, Analysis, Key Risks, Recommendations.\n"
            f"Be thorough and professional."
        )

        return self._call_llm(prompt, max_tokens=3000)

    def _call_llm(self, prompt: str, max_tokens: int = 1000) -> str:
        """Call the Fincept LLM endpoint with retry on async failure."""
        payload = {
            "prompt":     prompt,
            "max_tokens": max_tokens,
        }

        # Try async endpoint first
        try:
            resp = self._session.post(
                _FINCEPT_LLM_ASYNC,
                json=payload,
                timeout=_DEFAULT_TIMEOUT,
            )
            resp.raise_for_status()
            data = resp.json()
            if data.get("success") and data.get("result"):
                return data["result"]
        except requests.RequestException as exc:
            logger.warning("Async LLM endpoint failed: %s — falling back to sync", exc)

        # Fall back to sync endpoint
        try:
            resp = self._session.post(
                _FINCEPT_LLM_URL,
                json=payload,
                timeout=_DEFAULT_TIMEOUT,
            )
            resp.raise_for_status()
            data = resp.json()
            result = data.get("result") or data.get("response") or data.get("text", "")
            if not result:
                raise ValueError(f"Empty response from LLM: {data}")
            return result
        except requests.RequestException as exc:
            raise RuntimeError(f"LLM call failed: {exc}") from exc
