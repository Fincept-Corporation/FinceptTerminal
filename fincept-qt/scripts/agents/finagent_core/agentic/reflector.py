"""
Reflector — post-step LLM critic that decides whether to continue, re-plan,
ask the user a clarifying question, or stop early.

Pattern: Reflexion (Shinn et al., arXiv:2303.11366) — verbal RL via natural
language self-reflection. We ask one focused critic call per step and parse a
structured JSON decision. Reflector is best-effort: any error falls back to
`continue` so a flaky critic never breaks a working run.

Decisions:
  continue   — proceed to next step as planned
  replan     — current plan is wrong; call planner again with the critique
  question   — clarifying question needed from user (handled by HITL layer)
  done       — goal materially satisfied; skip remaining steps, synthesize
"""
from __future__ import annotations

import json
import re
from dataclasses import dataclass
from typing import Any, Dict, List, Optional


_CRITIC_SYSTEM_PROMPT = """You are a critical reviewer of an autonomous AI agent's progress on a financial task.

After each step, your job is to decide ONE of:
  continue  — the step succeeded; proceed with the existing plan
  replan    — the result revealed something that requires the plan to change
  question  — the agent needs a clarifying question from the user to proceed well
  done      — the user's original goal is materially satisfied; stop and synthesize

Be strict about `replan`: only choose it when the prior plan is genuinely
wrong, not just imperfect. Be strict about `question`: only when proceeding
without clarification risks a wrong answer.

You must consider the remaining budget. If only a little budget remains, lean
toward `continue` or `done` rather than `replan`.

Respond ONLY with a JSON object of this exact shape (no markdown, no commentary):
{
  "decision": "continue" | "replan" | "question" | "done",
  "reason": "<one short sentence>",
  "question": "<question to the user, only if decision=question>",
  "replan_hint": "<what about the plan should change, only if decision=replan>"
}"""


@dataclass
class ReflectorDecision:
    decision: str  # continue | replan | question | done
    reason: str
    question: Optional[str] = None
    replan_hint: Optional[str] = None

    def to_dict(self) -> Dict[str, Any]:
        return {
            "decision": self.decision,
            "reason": self.reason,
            "question": self.question,
            "replan_hint": self.replan_hint,
        }


class Reflector:
    """LLM critic with budget-aware prompting and JSON-decision parsing."""

    def __init__(self, api_keys: Dict[str, str]):
        self.api_keys = api_keys

    def assess(
        self,
        query: str,
        plan: Dict[str, Any],
        step: Dict[str, Any],
        step_result: Dict[str, Any],
        prior_results: List[Dict[str, Any]],
        config: Dict[str, Any],
        budget_snapshot: Optional[Dict[str, Any]] = None,
    ) -> ReflectorDecision:
        # Cheap-critic fast-path: when the step result looks unambiguously
        # successful, skip the LLM round-trip and emit a synthetic `continue`.
        # Saves roughly one LLM call per happy-path step. (Task #24.)
        if _is_obviously_continue(step_result):
            return ReflectorDecision(decision="continue", reason="skip-obvious")

        # The critic should be a fast / cheap model — we don't want it
        # blowing budget. Override config tools to none (pure reasoning).
        critic_config: Dict[str, Any] = {
            **config,
            "instructions": _CRITIC_SYSTEM_PROMPT,
            "tools": [],
            "markdown": False,
            "reasoning": False,
        }
        # Route the critic to a cheaper/faster model when configured. The
        # reflector runs once per step, so this is the single biggest
        # latency / cost lever for long tasks. (Task #22.)
        reflector_model = (config.get("agentic", {}) or {}).get("reflector_model")
        if reflector_model:
            critic_config["model"] = reflector_model

        # Compact summary of prior steps so the critic has context but doesn't
        # blow its own context window.
        prior_summary_parts: List[str] = []
        for j, r in enumerate(prior_results[:8]):
            if isinstance(r, dict) and r.get("response"):
                prior_summary_parts.append(f"Step {j+1}: {r['response'][:300]}")
        prior_summary = "\n".join(prior_summary_parts) or "(none)"

        plan_outline = []
        for s in (plan.get("steps") or [])[:12]:
            status = s.get("status", "pending")
            plan_outline.append(f"- [{status}] {s.get('name', '')}")

        budget_block = ""
        if budget_snapshot:
            pct_tokens = 100 * budget_snapshot["tokens_used"] / max(budget_snapshot["max_tokens"], 1)
            pct_wall = 100 * budget_snapshot["wall_s"] / max(budget_snapshot["max_wall_s"], 1)
            pct_steps = 100 * budget_snapshot["steps_used"] / max(budget_snapshot["max_steps"], 1)
            budget_block = (
                f"Budget used: tokens {pct_tokens:.0f}%, wall {pct_wall:.0f}%, "
                f"steps {pct_steps:.0f}%, cost ${budget_snapshot['cost_used']:.2f} / "
                f"${budget_snapshot['max_cost_usd']:.2f}."
            )

        prompt = (
            f"USER GOAL:\n{query}\n\n"
            f"CURRENT PLAN:\n" + "\n".join(plan_outline) + "\n\n"
            f"PRIOR STEPS:\n{prior_summary}\n\n"
            f"JUST EXECUTED — step {step_result.get('step', '?')}:\n"
            f"{step_result.get('description', '')}\n"
            f"RESULT:\n{(step_result.get('response') or '')[:1500]}\n\n"
            f"{budget_block}\n\n"
            f"Return your JSON decision now."
        )

        try:
            from finagent_core.core_agent import CoreAgent
            agent = CoreAgent(api_keys=self.api_keys)
            response = agent.run(prompt, critic_config)
            raw = (agent.get_response_content(response) or "").strip()
            decision = _parse_decision(raw)
            return decision
        except Exception:
            # Reflector failures must NOT break the task. Default to continue.
            return ReflectorDecision(decision="continue", reason="reflector_error")


# ── Heuristics ───────────────────────────────────────────────────────────────

# Substring patterns suggesting the step had a problem worth a real critique.
# Case-insensitive. Order doesn't matter — any match disables the fast-path.
_TROUBLE_MARKERS = (
    "error", "exception", "traceback", "failed", "i don't know", "i'm not sure",
    "cannot determine", "unable to", "insufficient information", "clarification",
    "i don't have enough", "please specify", "could you", "which ", "what specifically",
    "correction needed", "incorrect", "wrong", "misleading",
)


def _is_obviously_continue(step_result: Dict[str, Any]) -> bool:
    """Fast-path heuristic: did the step clearly succeed?

    Returns True only when the response looks unambiguously good — non-empty,
    reasonable length, no trouble markers. We bias to FALSE (i.e. invoke the
    LLM critic) whenever there's any signal of friction; cheap-critic should
    never let a bad step slip past unreviewed.
    """
    response = (step_result.get("response") or "").strip()
    if not response:
        return False
    # Length window: too short = probably an apology / refusal; too long = lots
    # of content the critic should actually look at.
    length = len(response)
    if length < 200 or length > 6000:
        return False
    lower = response.lower()
    for marker in _TROUBLE_MARKERS:
        if marker in lower:
            return False
    return True


# ── JSON parsing helpers ─────────────────────────────────────────────────────

_VALID = {"continue", "replan", "question", "done"}


def _parse_decision(raw: str) -> ReflectorDecision:
    """Tolerant JSON parser — strips markdown fences and extracts {...}."""
    text = raw.strip()
    text = re.sub(r"^```(?:json)?\s*", "", text)
    text = re.sub(r"\s*```$", "", text).strip()
    if not text.startswith("{"):
        m = re.search(r"\{.*\}", text, re.DOTALL)
        if m:
            text = m.group(0)
    try:
        obj = json.loads(text)
    except Exception:
        return ReflectorDecision(decision="continue", reason="parse_error")

    decision = (obj.get("decision") or "continue").lower()
    if decision not in _VALID:
        decision = "continue"
    return ReflectorDecision(
        decision=decision,
        reason=str(obj.get("reason") or "")[:280],
        question=obj.get("question"),
        replan_hint=obj.get("replan_hint"),
    )
