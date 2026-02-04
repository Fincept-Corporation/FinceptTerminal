"""
Fincept Orchestrator - Multi-step agentic workflow using Fincept LLM API.

Uses the async polling pattern:
  1. POST /research/llm/async  -> get task_id instantly
  2. GET  /research/llm/status/{task_id}  -> poll until completed

This avoids gateway timeouts on long LLM calls.
"""

import json
import time
import requests
import logging
import traceback
from typing import Any, Dict, List, Optional

logger = logging.getLogger(__name__)

BASE_URL = "https://finceptbackend.share.zrok.io"
POLL_INTERVAL = 4       # seconds between polls
POLL_MAX_WAIT = 180     # max seconds to wait for a single LLM call


class FinceptOrchestrator:
    """
    Orchestrates multi-step financial workflows by making iterative
    async LLM calls with heartbeat polling.
    """

    def __init__(self, api_key: Optional[str] = None, temperature: float = 0.7):
        self.api_key = api_key
        self.temperature = temperature
        self._headers = {"Content-Type": "application/json"}
        if api_key:
            self._headers["X-API-Key"] = api_key

    # ------------------------------------------------------------------
    # LLM call with async polling
    # ------------------------------------------------------------------

    def _submit_llm(self, prompt: str, max_tokens: int = 2000) -> Optional[str]:
        """Submit an async LLM request, return task_id or None."""
        payload = {
            "prompt": prompt,
            "temperature": self.temperature,
            "max_tokens": max_tokens
        }
        try:
            resp = requests.post(
                f"{BASE_URL}/research/llm/async",
                json=payload,
                headers=self._headers,
                timeout=30
            )
            resp.raise_for_status()
            data = resp.json()
            if data.get("success"):
                return data.get("task_id")
            return None
        except requests.exceptions.RequestException as e:
            logger.error(f"Async submit failed: {e}")
            return None

    def _poll_result(self, task_id: str) -> str:
        """Poll until the task completes, return the response text."""
        elapsed = 0
        while elapsed < POLL_MAX_WAIT:
            time.sleep(POLL_INTERVAL)
            elapsed += POLL_INTERVAL
            try:
                resp = requests.get(
                    f"{BASE_URL}/research/llm/status/{task_id}",
                    headers=self._headers,
                    timeout=15
                )
                resp.raise_for_status()
                data = resp.json()
                status = data.get("status")

                if status == "completed":
                    result_data = data.get("data", {})
                    if isinstance(result_data, dict):
                        return result_data.get("response") or result_data.get("content") or ""
                    return str(result_data)

                if status == "failed":
                    error = data.get("error", "Unknown error")
                    return f"[LLM task failed: {error}]"

                # status == "processing" -> keep polling
            except requests.exceptions.RequestException as e:
                logger.warning(f"Poll request failed (will retry): {e}")
                # Don't break, just retry on next interval

        return "[Error: LLM task timed out after polling]"

    def _call_llm(self, prompt: str, max_tokens: int = 2000) -> str:
        """Submit async + poll for result. Falls back to sync if async fails."""
        task_id = self._submit_llm(prompt, max_tokens)

        if task_id:
            return self._poll_result(task_id)

        # Fallback: try sync endpoint directly
        logger.warning("Async submit failed, falling back to sync endpoint")
        return self._call_llm_sync(prompt, max_tokens)

    def _call_llm_sync(self, prompt: str, max_tokens: int = 2000) -> str:
        """Fallback: direct sync call to /research/llm."""
        payload = {
            "prompt": prompt,
            "temperature": self.temperature,
            "max_tokens": max_tokens
        }
        try:
            resp = requests.post(
                f"{BASE_URL}/research/llm",
                json=payload,
                headers=self._headers,
                timeout=120
            )
            resp.raise_for_status()
            result = resp.json()
            if result.get("success"):
                data = result.get("data", {})
                return data.get("response") or data.get("content") or ""
            return result.get("response") or result.get("content") or ""
        except requests.exceptions.RequestException as e:
            return f"[Error calling LLM: {str(e)}]"

    # ------------------------------------------------------------------
    # Planning
    # ------------------------------------------------------------------

    def _create_plan(self, task: str, agent_type: str, subagent_names: List[str]) -> List[Dict[str, str]]:
        """Ask the LLM to break the task into steps."""
        subagents_str = ", ".join(subagent_names)
        prompt = f"""You are a financial planning agent. Break down this task into 3-5 concrete research steps.

Task: {task}

Available specialist areas: {subagents_str}

Return ONLY a JSON array of steps. Each step should have:
- "step": brief title
- "specialist": which specialist area handles this (pick from the available list)
- "prompt": the specific question/instruction for that specialist

Example format:
[
  {{"step": "Analyze market structure", "specialist": "data_analyst", "prompt": "Analyze the market..."}},
  {{"step": "Assess risks", "specialist": "risk_analyzer", "prompt": "Evaluate the key risks..."}}
]

Return ONLY valid JSON, no other text."""

        response = self._call_llm(prompt, max_tokens=1000)

        try:
            response = response.strip()
            if response.startswith("```"):
                lines = response.split("\n")
                response = "\n".join(
                    l for l in lines if not l.strip().startswith("```")
                )

            start = response.find("[")
            end = response.rfind("]") + 1
            if start >= 0 and end > start:
                plan = json.loads(response[start:end])
                if isinstance(plan, list):
                    return plan
        except (json.JSONDecodeError, ValueError):
            pass

        # Fallback plan
        return [
            {
                "step": "Comprehensive Analysis",
                "specialist": "data_analyst",
                "prompt": f"Provide a thorough, comprehensive analysis for: {task}"
            },
            {
                "step": "Generate Report",
                "specialist": "reporter",
                "prompt": f"Write a professional financial report on: {task}"
            }
        ]

    # ------------------------------------------------------------------
    # Execution
    # ------------------------------------------------------------------

    SUBAGENT_PROMPTS = {
        "research": "You are a financial research specialist. Provide thorough, well-sourced analysis with CFA-level standards.",
        "data_analyst": "You are a quantitative data analyst. Provide detailed numerical analysis, statistics, metrics, and data-driven insights.",
        "trading": "You are a quantitative trading strategist. Focus on strategy design, signal generation, and execution planning.",
        "risk_analyzer": "You are a risk analyst. Focus on VaR, CVaR, drawdown analysis, stress testing, and risk factors.",
        "portfolio_optimizer": "You are a portfolio optimization specialist. Focus on asset allocation, mean-variance optimization, and rebalancing.",
        "backtester": "You are a backtesting specialist. Focus on historical simulation, performance attribution, and strategy validation.",
        "reporter": "You are a financial report writer. Synthesize the analysis into a clear, professional, well-structured report with actionable insights."
    }

    def execute(
        self,
        task: str,
        agent_type: str = "research",
        subagent_names: Optional[List[str]] = None
    ) -> Dict[str, Any]:
        """Execute a multi-step agentic workflow with async polling."""

        if not subagent_names:
            subagent_names = ["data_analyst", "reporter"]

        todos = []
        step_results = []

        try:
            # Step 1: Create plan
            plan = self._create_plan(task, agent_type, subagent_names)

            for i, step in enumerate(plan):
                todos.append({
                    "id": f"todo-{i+1}",
                    "task": step.get("step", f"Step {i+1}"),
                    "status": "pending",
                    "subtasks": []
                })

            # Step 2: Execute each step
            for i, step in enumerate(plan):
                todos[i]["status"] = "in_progress"

                specialist = step.get("specialist", "data_analyst")
                step_prompt = step.get("prompt", step.get("step", task))
                system_context = self.SUBAGENT_PROMPTS.get(
                    specialist, self.SUBAGENT_PROMPTS["data_analyst"]
                )

                full_prompt = f"""{system_context}

Context: You are working as part of a multi-agent team analyzing a financial task.
Overall task: {task}
Your specific assignment: {step_prompt}

Provide a thorough, detailed analysis. Include specific data points, company names, numbers, and actionable insights where relevant. Be comprehensive."""

                result = self._call_llm(full_prompt, max_tokens=1500)
                step_results.append({
                    "step": step.get("step", f"Step {i+1}"),
                    "specialist": specialist,
                    "result": result
                })
                todos[i]["status"] = "completed"

            # Step 3: Synthesize
            synthesis_prompt = f"""You are a senior financial analyst writing a comprehensive report.

Original task: {task}

Below are the results from your specialist team:

"""
            for sr in step_results:
                synthesis_prompt += f"--- {sr['step']} (by {sr['specialist']}) ---\n{sr['result']}\n\n"

            synthesis_prompt += """
Now synthesize all the above into a single, cohesive, well-structured report.
Use clear headings, bullet points, and organize the information logically.
Include a brief executive summary at the top and key recommendations at the bottom.
Be thorough and professional."""

            final_report = self._call_llm(synthesis_prompt, max_tokens=3000)

            # Fallback if synthesis failed
            if final_report.startswith("[Error") or final_report.startswith("[LLM task failed"):
                valid_results = [
                    sr for sr in step_results
                    if not sr["result"].startswith("[Error") and not sr["result"].startswith("[LLM task failed")
                ]
                if valid_results:
                    parts = [f"## {sr['step']}\n\n{sr['result']}" for sr in valid_results]
                    final_report = "\n\n---\n\n".join(parts)

            return {
                "success": True,
                "result": final_report,
                "todos": todos,
                "files": {},
                "step_results": step_results
            }

        except Exception as e:
            logger.error(f"Orchestration error: {e}\n{traceback.format_exc()}")
            return {
                "success": False,
                "error": str(e),
                "details": traceback.format_exc(),
                "todos": todos
            }
