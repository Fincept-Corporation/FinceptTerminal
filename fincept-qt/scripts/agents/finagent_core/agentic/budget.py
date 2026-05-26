"""
BudgetGuard — layered ceilings for an agentic task.

Four independent caps; the first to breach aborts the task with a `budget_stop`
event. Caps come from the task config (`budget`) with sensible defaults.

  max_tokens     int   cumulative prompt + completion tokens (default 1_000_000)
  max_cost_usd   float dollar ceiling derived from provider pricing (default 5.0)
  max_wall_s     int   wall-clock seconds (default 3600 = 1 hr)
  max_steps      int   total step count (default 50)

Per-call tracking pulls token counts best-effort from Agno's RunResponse.metrics
and falls back to char-count / 4 estimation when unavailable. The cost table is
intentionally conservative — under-bills slightly so we never block at the
"$0.999999" boundary.
"""
from __future__ import annotations

import time
from dataclasses import dataclass, field
from typing import Any, Dict, Optional, Tuple


# Approximate USD per 1K tokens (input, output). Add models as needed.
# Keep conservative; this is a guard rail, not an invoice.
_PRICING_PER_1K: Dict[str, Tuple[float, float]] = {
    # OpenAI
    "gpt-4o": (0.0025, 0.010),
    "gpt-4o-mini": (0.00015, 0.0006),
    "gpt-4-turbo": (0.010, 0.030),
    "gpt-3.5-turbo": (0.0005, 0.0015),
    # Anthropic
    "claude-opus-4": (0.015, 0.075),
    "claude-sonnet-4": (0.003, 0.015),
    "claude-haiku-4": (0.0008, 0.004),
    # Gemini
    "gemini-2.0-flash": (0.00010, 0.00040),
    "gemini-1.5-pro": (0.00125, 0.005),
    # Fallback
    "default": (0.001, 0.003),
}


def _price_for(model_id: str) -> Tuple[float, float]:
    if not model_id:
        return _PRICING_PER_1K["default"]
    # Prefix match — pricing tables don't always reflect minor revisions.
    for key, price in _PRICING_PER_1K.items():
        if key == "default":
            continue
        if model_id.startswith(key):
            return price
    return _PRICING_PER_1K["default"]


def _estimate_tokens(text: str) -> int:
    """~4 chars per token is the OpenAI rule of thumb; close enough for budgets."""
    return max(1, len(text or "") // 4)


@dataclass
class BudgetGuard:
    max_tokens: int = 1_000_000
    max_cost_usd: float = 5.0
    max_wall_s: int = 3600
    max_steps: int = 50

    tokens_used: int = 0
    cost_used: float = 0.0
    steps_used: int = 0
    started_at: float = field(default_factory=time.time)
    _model_id: str = ""

    @classmethod
    def from_config(cls, config: Dict[str, Any]) -> "BudgetGuard":
        b = config.get("budget") or {}
        model = (config.get("model") or {}).get("model_id", "")
        guard = cls(
            max_tokens=int(b.get("max_tokens", 1_000_000)),
            max_cost_usd=float(b.get("max_cost_usd", 5.0)),
            max_wall_s=int(b.get("max_wall_s", 3600)),
            max_steps=int(b.get("max_steps", 50)),
        )
        guard._model_id = model
        return guard

    # ── Recording ────────────────────────────────────────────────────────────

    def record_llm_call(self, prompt_text: str, response_obj: Any) -> None:
        """Pull tokens from Agno's RunResponse.metrics if available; else estimate."""
        in_tok = out_tok = 0
        try:
            metrics = getattr(response_obj, "metrics", None) or {}
            if isinstance(metrics, dict):
                in_tok = int(metrics.get("input_tokens", 0) or 0)
                out_tok = int(metrics.get("output_tokens", 0) or 0)
            else:
                # Pydantic-style attrs.
                in_tok = int(getattr(metrics, "input_tokens", 0) or 0)
                out_tok = int(getattr(metrics, "output_tokens", 0) or 0)
        except Exception:
            pass

        if in_tok == 0 and out_tok == 0:
            # Best-effort estimation.
            content = ""
            try:
                content = getattr(response_obj, "content", "") or ""
            except Exception:
                content = ""
            in_tok = _estimate_tokens(prompt_text)
            out_tok = _estimate_tokens(str(content))

        self.tokens_used += in_tok + out_tok
        in_price, out_price = _price_for(self._model_id)
        self.cost_used += (in_tok / 1000.0) * in_price + (out_tok / 1000.0) * out_price

    def record_step(self) -> None:
        self.steps_used += 1

    # ── Checks ───────────────────────────────────────────────────────────────

    def wall_elapsed_s(self) -> int:
        return int(time.time() - self.started_at)

    def breach(self) -> Optional[str]:
        """Return the breached cap name, or None."""
        if self.tokens_used >= self.max_tokens:
            return "tokens"
        if self.cost_used >= self.max_cost_usd:
            return "cost"
        if self.wall_elapsed_s() >= self.max_wall_s:
            return "wall_clock"
        if self.steps_used >= self.max_steps:
            return "steps"
        return None

    # ── Reporting ────────────────────────────────────────────────────────────

    def snapshot(self) -> Dict[str, Any]:
        return {
            "tokens_used": self.tokens_used,
            "max_tokens": self.max_tokens,
            "cost_used": round(self.cost_used, 4),
            "max_cost_usd": self.max_cost_usd,
            "wall_s": self.wall_elapsed_s(),
            "max_wall_s": self.max_wall_s,
            "steps_used": self.steps_used,
            "max_steps": self.max_steps,
        }
