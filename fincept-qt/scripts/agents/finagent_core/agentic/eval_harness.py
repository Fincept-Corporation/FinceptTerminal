"""
Eval harness — replay a curated set of agentic tasks and score the outputs.

Internal developer tool, not a runtime user feature. Berkeley's April 2026 audit
showed every public agent benchmark was reward-hackable; this harness is a thin
golden-set replay so we can measure drift on OUR domain (finance research)
without claiming generality.

Score axes per case:
  phrase_coverage   fraction of `expected_phrases` present in the final result
  budget_ok         did the run stay under cost / wall / step caps
  citation_count    crude tally of source markers ("[source:", "(SEC ", "10-K", etc.)
  success           did the run complete vs error / cancel / budget_stop

Inputs: a JSON file of cases. Outputs: a JSON report + a Markdown summary on stdout.

Usage:
  python -m finagent_core.agentic.eval_harness \\
      --cases path/to/golden_set.json \\
      --report path/to/report.json \\
      [--db /custom/agent_tasks.db]
"""
from __future__ import annotations

import argparse
import json
import re
import sys
import time
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Any, Dict, List, Optional


_DEFAULT_DB = str(Path(__file__).parent.parent.parent.parent / "agent_tasks.db")

# Heuristic citation markers — case-insensitive substring match. Sufficient for
# a smoke-grade quality bar; precise citation grading is a future iteration.
_CITATION_PATTERNS = [
    r"\[source[:\s]",
    r"\bSEC\b",
    r"\b10-?K\b",
    r"\b10-?Q\b",
    r"\b8-?K\b",
    r"\bMD&A\b",
    r"per\s+the\s+filing",
    r"according to\s+\w+",
]


@dataclass
class EvalCase:
    name: str
    query: str
    expected_phrases: List[str] = field(default_factory=list)
    max_steps: int = 20
    max_wall_s: int = 600
    max_cost_usd: float = 2.0
    expected_min_citations: int = 0
    config: Dict[str, Any] = field(default_factory=dict)

    @classmethod
    def from_dict(cls, d: Dict[str, Any]) -> "EvalCase":
        return cls(
            name=d["name"], query=d["query"],
            expected_phrases=list(d.get("expected_phrases", [])),
            max_steps=int(d.get("max_steps", 20)),
            max_wall_s=int(d.get("max_wall_s", 600)),
            max_cost_usd=float(d.get("max_cost_usd", 2.0)),
            expected_min_citations=int(d.get("expected_min_citations", 0)),
            config=dict(d.get("config", {})),
        )


@dataclass
class EvalResult:
    case_name: str
    success: bool
    phrase_coverage: float
    matched_phrases: List[str]
    missing_phrases: List[str]
    citation_count: int
    steps_used: int
    cost_used: float
    wall_s: int
    budget_ok: bool
    error: Optional[str] = None
    final_response_preview: str = ""


def _count_citations(text: str) -> int:
    n = 0
    for pat in _CITATION_PATTERNS:
        n += len(re.findall(pat, text or "", re.IGNORECASE))
    return n


def _phrase_coverage(text: str, expected: List[str]) -> tuple[float, List[str], List[str]]:
    if not expected:
        return 1.0, [], []
    hay = (text or "").lower()
    matched = [p for p in expected if p.lower() in hay]
    missing = [p for p in expected if p.lower() not in hay]
    return (len(matched) / len(expected)), matched, missing


def run_case(case: EvalCase, api_keys: Dict[str, str], db_path: str) -> EvalResult:
    """Execute one case via AgenticRunner; return the scored result."""
    # Imported lazily so the harness file itself is importable without the
    # full finagent_core (and its agno dependency) being available.
    from finagent_core.agentic.runner import AgenticRunner

    cfg: Dict[str, Any] = {
        **case.config,
        "budget": {
            "max_tokens": int(case.config.get("budget", {}).get("max_tokens", 500_000)),
            "max_cost_usd": case.max_cost_usd,
            "max_wall_s": case.max_wall_s,
            "max_steps": case.max_steps,
        },
        # Eval mode: skip skill distillation so a noisy critic call doesn't
        # pollute the eval cost accounting. Reflector still runs.
        "agentic": {**case.config.get("agentic", {}), "distill_skills": False},
    }
    runner = AgenticRunner(api_keys=api_keys, db_path=db_path, emit=lambda k, p: None)
    started = time.time()
    try:
        result = runner.start_task(case.query, cfg)
        error = None if result.get("success") else result.get("error", "unknown")
        final = result.get("response", "")
        budget = result.get("budget") or {}
    except Exception as e:
        return EvalResult(
            case_name=case.name, success=False, phrase_coverage=0.0,
            matched_phrases=[], missing_phrases=case.expected_phrases,
            citation_count=0, steps_used=0, cost_used=0.0,
            wall_s=int(time.time() - started), budget_ok=False, error=str(e),
        )

    coverage, matched, missing = _phrase_coverage(final, case.expected_phrases)
    citations = _count_citations(final)
    return EvalResult(
        case_name=case.name,
        success=bool(result.get("success")) and citations >= case.expected_min_citations,
        phrase_coverage=coverage,
        matched_phrases=matched,
        missing_phrases=missing,
        citation_count=citations,
        steps_used=int(budget.get("steps_used", result.get("steps_completed", 0))),
        cost_used=float(budget.get("cost_used", 0.0)),
        wall_s=int(time.time() - started),
        budget_ok=("budget_stop" not in result),
        error=error,
        final_response_preview=(final or "")[:400],
    )


def run_suite(cases: List[EvalCase], api_keys: Dict[str, str], db_path: str) -> List[EvalResult]:
    return [run_case(c, api_keys, db_path) for c in cases]


def render_markdown(results: List[EvalResult]) -> str:
    """One-table summary suitable for printing or pasting into a PR.

    Includes a totals row aggregating cost / wall_s / steps across the suite —
    the suite-wide cost is the number most useful for tracking regressions.
    (Task #25.)
    """
    if not results:
        return "_(no results)_"
    lines = [
        "| case | ok | coverage | cite | steps | $cost | wall_s | error |",
        "| --- | :---: | ---: | ---: | ---: | ---: | ---: | --- |",
    ]
    total_cost = 0.0
    total_wall = 0
    total_steps = 0
    total_citations = 0
    for r in results:
        lines.append(
            f"| {r.case_name} | {'✓' if r.success else '✗'} | "
            f"{r.phrase_coverage:.0%} | {r.citation_count} | "
            f"{r.steps_used} | ${r.cost_used:.2f} | {r.wall_s} | "
            f"{(r.error or '')[:30]} |"
        )
        total_cost += r.cost_used
        total_wall += r.wall_s
        total_steps += r.steps_used
        total_citations += r.citation_count
    ok = sum(1 for r in results if r.success)
    lines.append(
        f"| **TOTAL** | **{ok}/{len(results)}** | — | **{total_citations}** | "
        f"**{total_steps}** | **${total_cost:.2f}** | **{total_wall}** | |"
    )
    lines.append(f"\n**{ok}/{len(results)} passed** · "
                 f"suite cost **${total_cost:.2f}** · wall **{total_wall}s** "
                 f"· {total_steps} steps · {total_citations} citations")
    return "\n".join(lines)


def main(argv: Optional[List[str]] = None) -> int:
    ap = argparse.ArgumentParser(description="Agentic-mode golden-set eval harness")
    ap.add_argument("--cases", required=True, help="JSON file of EvalCase definitions")
    ap.add_argument("--report", help="Path to write the JSON report")
    ap.add_argument("--db", default=_DEFAULT_DB, help="Path to agent_tasks.db (default: same as runner)")
    args = ap.parse_args(argv)

    with open(args.cases) as f:
        case_dicts = json.load(f)
    cases = [EvalCase.from_dict(d) for d in case_dicts]

    # API keys come from env vars at runtime so we don't print them in reports.
    import os
    api_keys = {
        "openai": os.environ.get("OPENAI_API_KEY", ""),
        "anthropic": os.environ.get("ANTHROPIC_API_KEY", ""),
        "google": os.environ.get("GOOGLE_API_KEY", ""),
        "fincept": os.environ.get("FINCEPT_API_KEY", ""),
    }

    results = run_suite(cases, api_keys, args.db)
    if args.report:
        with open(args.report, "w") as f:
            json.dump([asdict(r) for r in results], f, indent=2)
    print(render_markdown(results))
    return 0 if all(r.success for r in results) else 1


if __name__ == "__main__":
    sys.exit(main())
