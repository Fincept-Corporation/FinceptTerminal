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

# Fincept hosted LLM endpoint. Contract (apiback research.py):
#   POST /research/llm/async  {prompt,max_tokens} -> {"success","task_id","message"}   (no result here)
#   GET  /research/llm/status/{task_id}            -> {"status": processing|completed|failed,
#                                                      "data": {"response": "...", ...}, "error": ...}
#   POST /research/llm        {prompt,max_tokens} -> {"success","message","data": {"response": "...", ...}}
# Auth is the X-API-Key header only; "Authorization: Bearer" is never accepted and every 401 counts
# toward the server's per-IP auto-block, so a rejected key must stop the run, not be retried.
_FINCEPT_LLM_URL    = "https://api.fincept.in/research/llm"
_FINCEPT_LLM_ASYNC  = "https://api.fincept.in/research/llm/async"
_FINCEPT_LLM_STATUS = "https://api.fincept.in/research/llm/status/"
_DEFAULT_TIMEOUT    = 120  # seconds, one HTTP exchange
_TASK_TIMEOUT       = 300  # seconds, one async task (submit -> completed)
_POLL_INTERVAL      = 3.0  # seconds; the endpoint asks for 3-5 s
_MIN_SUBMIT_GAP     = 1.2  # seconds; the server allows 1 LLM request/second per user
_RETRY_STATUSES     = (429, 502, 503, 504)

# The prompt-loop fallback can only talk to the Fincept LLM. Any other provider's key must never be
# sent to api.fincept.in.
FINCEPT_PROVIDER = "fincept"


class FinceptLLMError(RuntimeError):
    """A Fincept LLM call failed with a reason worth showing to the user."""

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

    def __init__(self, api_key: str | None = None, provider: str = FINCEPT_PROVIDER):
        self.provider = (provider or "").lower().strip()
        # Only a Fincept key is ever attached; for any other provider nothing is sent (see execute()).
        self.api_key = api_key if self.provider == FINCEPT_PROVIDER else None
        self._session = requests.Session()
        if self.api_key:
            self._session.headers.update({"X-API-Key": self.api_key})
        self._last_submit = 0.0

    # ------------------------------------------------------------------
    # Public API — cli.py execute_task (non-tool-calling LLM fallback)
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
        if self.provider != FINCEPT_PROVIDER:
            shown = self.provider or "none"
            return self._failure(
                f"LLM provider '{shown}' does not support tool calling, and the fallback agent runs only "
                f"on the Fincept LLM. Choose a tool-calling provider or Fincept for AI Quant Lab."
            )
        if not self.api_key:
            return self._failure(
                "No Fincept API key for the Fincept LLM. Sign in to Fincept (or set the key on the "
                "Fincept LLM profile) and run the agent again."
            )
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
            return self._failure(str(exc))

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------

    @staticmethod
    def _failure(error: str) -> dict[str, Any]:
        return {
            "success": False,
            "result":  "",
            "todos":   [],
            "files":   {},
            "error":   error,
        }

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
        """Return the Fincept LLM's text for `prompt`.

        Submits to /research/llm/async and polls /research/llm/status/{task_id}
        (long generations outlive a synchronous gateway request). The sync
        /research/llm route is used only when the async route is absent.
        """
        payload = {
            "prompt":     prompt,
            "max_tokens": max_tokens,
        }
        try:
            self._respect_rate_limit()
            resp = self._request("POST", _FINCEPT_LLM_ASYNC, json=payload)
            if resp.status_code in (404, 405):
                logger.warning("Async LLM endpoint unavailable (HTTP %s), using %s",
                               resp.status_code, _FINCEPT_LLM_URL)
                return self._call_sync(payload)
            if resp.status_code != 200:
                self._raise_for(resp)
            body = resp.json()
            task_id = body.get("task_id") if isinstance(body, dict) else None
            if not task_id:
                raise FinceptLLMError(f"Fincept LLM did not return a task id: {str(body)[:200]}")
            return self._poll(task_id)
        except requests.RequestException as exc:
            raise RuntimeError(f"LLM call failed: {exc}") from exc

    def _call_sync(self, payload: dict[str, Any]) -> str:
        self._respect_rate_limit()
        resp = self._request("POST", _FINCEPT_LLM_URL, json=payload)
        if resp.status_code != 200:
            self._raise_for(resp)
        body = resp.json()
        return self._response_text(body.get("data") if isinstance(body, dict) else None)

    def _poll(self, task_id: str) -> str:
        deadline = time.monotonic() + _TASK_TIMEOUT
        while True:
            time.sleep(_POLL_INTERVAL)
            try:
                resp = self._request("GET", _FINCEPT_LLM_STATUS + task_id)
            except requests.RequestException as exc:  # transient: keep polling until the deadline
                logger.warning("Polling LLM task %s failed: %s", task_id, exc)
                resp = None
            if resp is not None:
                if resp.status_code != 200:
                    self._raise_for(resp)
                body = resp.json()
                status = body.get("status") if isinstance(body, dict) else None
                if status == "completed":
                    return self._response_text(body.get("data"))
                if status == "failed":
                    raise FinceptLLMError(f"Fincept LLM task failed: {body.get('error') or 'no reason given'}")
            if time.monotonic() > deadline:
                raise FinceptLLMError(f"Fincept LLM task did not finish within {_TASK_TIMEOUT} s")

    def _request(self, method: str, url: str, **kwargs: Any) -> requests.Response:
        """One exchange; 429/502/503/504 are retried twice with a growing wait. Never retries a 401."""
        resp = None
        for attempt in range(3):
            resp = self._session.request(method, url, timeout=_DEFAULT_TIMEOUT, **kwargs)
            if resp.status_code not in _RETRY_STATUSES:
                return resp
            if attempt < 2:
                time.sleep(3.0 * (attempt + 1))
        return resp

    def _respect_rate_limit(self) -> None:
        wait = _MIN_SUBMIT_GAP - (time.monotonic() - self._last_submit)
        if wait > 0:
            time.sleep(wait)
        self._last_submit = time.monotonic()

    @staticmethod
    def _response_text(data: Any) -> str:
        text = data.get("response") if isinstance(data, dict) else None
        if not text:
            raise FinceptLLMError(f"Empty response from the Fincept LLM: {str(data)[:200]}")
        return text

    @staticmethod
    def _raise_for(resp: requests.Response) -> None:
        """Raise with the server's own message ({"message": str | {"message": str}})."""
        try:
            body = resp.json()
        except ValueError:
            body = None
        msg = body.get("message") if isinstance(body, dict) else None
        if isinstance(msg, dict):
            msg = msg.get("message")
        msg = str(msg or (resp.text or "")[:300] or resp.reason)
        if resp.status_code == 401:
            raise FinceptLLMError(f"Fincept API rejected the API key (HTTP 401: {msg}). Sign in again.")
        raise FinceptLLMError(f"Fincept LLM request failed (HTTP {resp.status_code}): {msg}")
