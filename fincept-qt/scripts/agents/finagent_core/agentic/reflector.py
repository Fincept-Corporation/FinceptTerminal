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
    # `source` makes every verdict auditable. Values:
    #   "structural:<pattern>"  → deterministic pre-check fired
    #   "skip-obvious"          → cheap-critic fast-path
    #   "llm-critic"            → real LLM call
    #   "parse_error" / "reflector_error" → fallback paths
    source: str = "llm-critic"

    def to_dict(self) -> Dict[str, Any]:
        return {
            "decision": self.decision,
            "reason": self.reason,
            "question": self.question,
            "replan_hint": self.replan_hint,
            "source": self.source,
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
        # ── Layer 1: deterministic structural signals (model-agnostic) ──────
        # Pattern-match the step output for cues NO model should miss. Fires
        # for: explicit "plan is wrong" overrides, repeated tool/access errors,
        # surfaced unanswered questions. The point is to give predictable
        # verdicts that hold across MiniMax, Claude, GPT-4o, Llama — anything.
        struct = _structural_signals(step_result, plan, prior_results)
        if struct is not None:
            return struct

        # ── Layer 2: cheap-critic fast-path (heuristic, no LLM) ─────────────
        # When the step result looks unambiguously successful, skip the LLM
        # round-trip and emit a synthetic `continue`. Saves ~1 LLM call per
        # happy-path step.
        if _is_obviously_continue(step_result):
            return ReflectorDecision(decision="continue", reason="skip-obvious",
                                     source="skip-obvious")

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
            return _parse_decision(raw)  # source="llm-critic" baked in
        except Exception:
            # Reflector failures must NOT break the task. Default to continue.
            return ReflectorDecision(decision="continue", reason="reflector_error",
                                     source="reflector_error")


# ── Layer 1: structural signals (model-agnostic, deterministic) ─────────────

# Plan-override patterns: explicit language saying "this plan is wrong" or
# "we should take different steps". These are STRONG cues that should force
# replan regardless of what an LLM critic decides. Compiled once at import.
_PLAN_OVERRIDE_PATTERNS = (
    re.compile(r"\bWAIT\b", re.IGNORECASE),
    re.compile(r"\b(this|the) plan is (wrong|incorrect|flawed|broken)\b", re.IGNORECASE),
    re.compile(r"\bcorrection needed\b", re.IGNORECASE),
    re.compile(r"\b(we|you) (should|need to|must) (instead|actually|take different)\b", re.IGNORECASE),
    re.compile(r"\bdifferent steps\b", re.IGNORECASE),
    re.compile(r"\binstead of\b.+\bstep\b", re.IGNORECASE),
    re.compile(r"\bplan needs (to be )?(revis|chang|fix)", re.IGNORECASE),
)

# Tool-failure / capability-gap patterns. Repeated firing across consecutive
# steps means the plan is asking for things the agent can't do — also a
# replan signal (different plan, different tools).
_TOOL_ERROR_PATTERNS = (
    re.compile(r"\bTraceback\b"),
    re.compile(r"\b(API error|HTTP \d{3})\b"),
    re.compile(r"\bI (cannot|can't) (access|fetch|retrieve|reach)\b", re.IGNORECASE),
    re.compile(r"\b(tool not available|no such tool|tool failed)\b", re.IGNORECASE),
)

# Explicit unanswered question patterns. Step output containing a real
# question to the user → HITL.
_QUESTION_PATTERNS = (
    re.compile(r"(could you|can you) (please )?(specify|clarify|provide|tell me)", re.IGNORECASE),
    re.compile(r"(which|what) (specific |particular )?(ticker|stock|company|symbol|date|period|sector)", re.IGNORECASE),
    re.compile(r"please (provide|clarify|specify)", re.IGNORECASE),
    re.compile(r"\bi (don't|do not) (know|have) (which|what|the)", re.IGNORECASE),
)


def _structural_signals(
    step_result: Dict[str, Any],
    plan: Dict[str, Any],
    prior_results: List[Dict[str, Any]],
) -> Optional[ReflectorDecision]:
    """Deterministic pre-check that overrides the LLM critic when a strong
    cue is present in the step output.

    Returns None when no signal fires — caller falls through to skip-obvious
    or the LLM critic. When a signal fires, the returned decision carries
    `source="structural:<pattern>"` so downstream observability is clean.
    """
    response = step_result.get("response") or ""
    if not response:
        return None

    # 1. Plan-override → replan
    for pat in _PLAN_OVERRIDE_PATTERNS:
        if pat.search(response):
            snippet = (pat.search(response).group(0))[:80]
            return ReflectorDecision(
                decision="replan",
                reason=f"step output contains plan-override cue: {snippet!r}",
                replan_hint=response[:300],
                source="structural:plan_override",
            )

    # 2. Tool error: 2 consecutive steps showing access failures → replan
    if _tool_error_in(response):
        prior_response = ""
        if prior_results:
            prev = prior_results[-1]
            if isinstance(prev, dict):
                prior_response = prev.get("response") or ""
        if _tool_error_in(prior_response):
            return ReflectorDecision(
                decision="replan",
                reason="two consecutive steps hit tool/access errors — plan needs different tools",
                source="structural:tool_error_repeated",
            )

    # 3. Surfaced clarifying question → question
    matches = sum(1 for pat in _QUESTION_PATTERNS if pat.search(response))
    if matches >= 2:
        # Extract the first sentence containing a "?".
        q_match = re.search(r"([^.!?\n]*\?)", response)
        q_text = q_match.group(1).strip() if q_match else "Agent needs clarification."
        return ReflectorDecision(
            decision="question",
            reason="step output contains explicit unanswered question(s)",
            question=q_text[:400],
            source="structural:explicit_question",
        )

    return None


def _tool_error_in(text: str) -> bool:
    for pat in _TOOL_ERROR_PATTERNS:
        if pat.search(text):
            return True
    return False


# ── Layer 2: cheap-critic fast-path (heuristic) ─────────────────────────────

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
        return ReflectorDecision(decision="continue", reason="parse_error",
                                 source="parse_error")

    decision = (obj.get("decision") or "continue").lower()
    if decision not in _VALID:
        decision = "continue"
    return ReflectorDecision(
        decision=decision,
        reason=str(obj.get("reason") or "")[:280],
        question=obj.get("question"),
        replan_hint=obj.get("replan_hint"),
        source="llm-critic",
    )
