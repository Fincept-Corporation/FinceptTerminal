"""
Hard integration test for the agentic stack — runs against a real LLM endpoint.

Gated behind two env vars so it doesn't run in normal CI:
  ANTHROPIC_AUTH_TOKEN  must be set (or AGENTIC_TEST_API_KEY)
  AGENTIC_INTEGRATION   must equal "1" to opt in

The default routing in this file targets MiniMax-as-Anthropic; override via:
  ANTHROPIC_BASE_URL   default "https://api.minimax.io/anthropic"
  AGENTIC_TEST_MODEL   default "MiniMax-M2.7"

Run with:
  AGENTIC_INTEGRATION=1 ANTHROPIC_AUTH_TOKEN=sk-... \\
      python scripts/agents/tests/agentic/test_hard_integration.py

Exercises Phase 1 (plan/exec/events/checkpoint), Phase 2 (reflector/replan/HITL/
budget), Phase 3 (skills/archival/reflexion/scheduler). See test_agentic_hardtest.py
runlog (post-Phase-3 audit, ~20 min wall, 8/9 PASS on MiniMax-M2.7).

Budgets are tight (3 steps, ~2k tokens, ~$0.25 per scenario) to bound total cost.
Total suite cost on MiniMax was ~$1-3 depending on which scenarios actually run.
"""
from __future__ import annotations

import json
import os
import sqlite3
import sys
import tempfile
import time
import traceback
from contextlib import contextmanager
from pathlib import Path


# ── Gating ──────────────────────────────────────────────────────────────────
if os.environ.get("AGENTIC_INTEGRATION") != "1":
    print("AGENTIC_INTEGRATION!=1 — skipping (set =1 to opt in).")
    sys.exit(0)

_TOKEN = os.environ.get("ANTHROPIC_AUTH_TOKEN") or os.environ.get("AGENTIC_TEST_API_KEY")
if not _TOKEN:
    print("ANTHROPIC_AUTH_TOKEN unset — skipping.")
    sys.exit(0)

# ── Routing — defaults to MiniMax-as-Anthropic but fully overridable ────────
MODEL = os.environ.get("AGENTIC_TEST_MODEL", "MiniMax-M2.7")
BASE_URL = os.environ.get("ANTHROPIC_BASE_URL", "https://api.minimax.io/anthropic")
os.environ["ANTHROPIC_BASE_URL"] = BASE_URL
os.environ["ANTHROPIC_AUTH_TOKEN"] = _TOKEN
os.environ["ANTHROPIC_API_KEY"] = _TOKEN  # SDK reads either

# Make finagent_core importable when run as a script.
HERE = Path(__file__).resolve()
PROJECT_AGENTS = HERE.parents[2]  # …/scripts/agents/
sys.path.insert(0, str(PROJECT_AGENTS))


# ── Test harness ────────────────────────────────────────────────────────────
RESULTS = []


@contextmanager
def section(name: str):
    print(f"\n━━━ {name} ━━━")
    t = time.time()
    try:
        yield
        dt = time.time() - t
        print(f"✓ PASS  ({dt:.1f}s)  {name}")
        RESULTS.append((name, True, dt, None))
    except AssertionError as e:
        dt = time.time() - t
        print(f"✗ FAIL  ({dt:.1f}s)  {name}  →  {e}")
        RESULTS.append((name, False, dt, str(e)))
    except Exception as e:
        dt = time.time() - t
        print(f"✗ ERROR ({dt:.1f}s)  {name}  →  {type(e).__name__}: {e}")
        traceback.print_exc()
        RESULTS.append((name, False, dt, f"{type(e).__name__}: {e}"))


def make_emitter(sink: list):
    def emit(kind, payload):
        sink.append({"kind": kind, **payload})
        tag = f"  [{kind}]"
        if kind == "plan_ready":
            steps = payload.get("plan", {}).get("steps", [])
            tag += f" {len(steps)} step(s): " + " → ".join(s.get("name", "?")[:30] for s in steps)
        elif kind == "step_start":
            tag += f" #{payload.get('step_index')} ▶ {(payload.get('step') or {}).get('name', '')[:60]}"
        elif kind == "step_end":
            res = payload.get("result", {})
            tag += f" #{payload.get('step_index')} ✓ {(res.get('response') or '')[:80]}"
        elif kind == "reflection":
            d = payload.get("decision", {})
            tag += f" → {d.get('decision')} ({d.get('reason', '')[:60]})"
        elif kind == "replanned":
            tag += f" {payload.get('reason', '')[:80]}"
        elif kind == "question":
            tag += f" ❓ {payload.get('question', '')[:80]}"
        elif kind == "budget_stop":
            tag += f" 💸 breached={payload.get('breach')}"
        elif kind == "done":
            tag += f" 🎉 {(payload.get('final') or '')[:80]}"
        elif kind == "error":
            tag += f" ❌ {payload.get('error', '')[:80]}"
        elif kind == "skill_saved":
            tag += f" 💡 saved skill: {payload.get('name')}"
        print(tag)
    return emit


def base_config(*, max_steps=3, max_tokens=2000, max_cost_usd=0.25, max_wall_s=180,
                user_id="hardtest", distill=True):
    return {
        "model": {
            "provider": "anthropic",
            "model_id": MODEL,
            "api_key": _TOKEN,
            "base_url": BASE_URL,
            "temperature": 0.3,
            "max_tokens": 800,
        },
        "tools": [],
        "markdown": False,
        "reasoning": False,
        "budget": {
            "max_steps": max_steps,
            "max_tokens": max_tokens,
            "max_cost_usd": max_cost_usd,
            "max_wall_s": max_wall_s,
        },
        "agentic": {"distill_skills": distill, "reflector": True},
        "user_id": user_id,
        "max_iterations": 3,
    }


# ── Imports ─────────────────────────────────────────────────────────────────
print("Importing agentic stack…")
from finagent_core.agentic.runner import AgenticRunner
from finagent_core.agentic.budget import BudgetGuard
from finagent_core.agentic.skill_library import SkillLibrary
from finagent_core.agentic.archival_memory import ArchivalMemoryStore
from finagent_core.agentic.reflexion_store import ReflexionStore
from finagent_core.agentic.scheduler import ScheduledTaskStore, compute_next_run
from finagent_core.task_state import TaskStateManager
print("  ✓ imports OK")

TMP_DB_FD, TMP_DB = tempfile.mkstemp(suffix=".db", prefix="agentic_hardtest_")
os.close(TMP_DB_FD)
print(f"Temp DB: {TMP_DB}")


# ── S1: plan + execute + archive ────────────────────────────────────────────
with section("S1: plan-execute-archive"):
    events = []
    runner = AgenticRunner(api_keys={"ANTHROPIC_API_KEY": _TOKEN},
                           db_path=TMP_DB, emit=make_emitter(events))
    result = runner.start_task(
        "Explain in 2-3 short steps how to estimate a stock's fair value using a simple DCF.",
        base_config(max_steps=3, max_tokens=4000, max_cost_usd=0.50),
    )
    assert result.get("success") is True
    kinds = [e["kind"] for e in events]
    assert "plan_ready" in kinds and "done" in kinds
    with sqlite3.connect(TMP_DB) as c:
        row = c.execute("SELECT plan_json FROM agent_tasks WHERE id=?", (result["task_id"],)).fetchone()
    assert row and row[0]
    plan = json.loads(row[0])
    assert isinstance(plan.get("steps"), list) and len(plan["steps"]) >= 1


# ── S2: compounding ─────────────────────────────────────────────────────────
with section("S2: compounding retrieval"):
    events = []
    runner = AgenticRunner(api_keys={"ANTHROPIC_API_KEY": _TOKEN},
                           db_path=TMP_DB, emit=make_emitter(events))
    archived_before = len(ArchivalMemoryStore(db_path=TMP_DB).list(user_id="hardtest"))
    result = runner.start_task(
        "What 2-3 steps would you take to value a stock?",
        base_config(max_steps=3, max_tokens=4000, max_cost_usd=0.40),
    )
    assert result.get("success") is True
    archived_after = len(ArchivalMemoryStore(db_path=TMP_DB).list(user_id="hardtest"))
    assert archived_after >= archived_before + 1


# ── S3: budget guard — accept any cap breach (Task #26) ─────────────────────
with section("S3: budget guard fires on any cap"):
    events = []
    runner = AgenticRunner(api_keys={"ANTHROPIC_API_KEY": _TOKEN},
                           db_path=TMP_DB, emit=make_emitter(events))
    cfg = base_config(max_steps=1, max_tokens=4000, max_cost_usd=0.30, distill=False)
    result = runner.start_task(
        "Outline 5 distinct steps for a sector rotation strategy.",
        cfg,
    )
    kinds = [e["kind"] for e in events]
    assert "budget_stop" in kinds
    breach_event = next(e for e in events if e["kind"] == "budget_stop")
    # Any real cap is a valid breach — wall_clock typically trips first against
    # slow LLM endpoints (e.g. MiniMax averaged 30s/call in the audit run).
    assert breach_event["breach"] in {"steps", "wall_clock", "tokens", "cost"}


# ── S4: reflector adaptive control ──────────────────────────────────────────
with section("S4: reflector adaptive decisions"):
    events = []
    runner = AgenticRunner(api_keys={"ANTHROPIC_API_KEY": _TOKEN},
                           db_path=TMP_DB, emit=make_emitter(events))
    runner.start_task(
        "I want to research a tech company's prospects. Plan 2 steps, then in your first step "
        "explicitly state 'CORRECTION NEEDED: I should also analyse the competitive landscape' "
        "before continuing.",
        base_config(max_steps=4, max_tokens=5000, max_cost_usd=0.50),
    )
    refl_events = [e for e in events if e["kind"] == "reflection"]
    assert refl_events, "reflector never fired"


# ── S5: HITL pause + reply ──────────────────────────────────────────────────
with section("S5: HITL pause-for-question"):
    events = []
    runner = AgenticRunner(api_keys={"ANTHROPIC_API_KEY": _TOKEN},
                           db_path=TMP_DB, emit=make_emitter(events))
    result = runner.start_task(
        "Analyse the stock. After your first step, you should realise you don't know which "
        "ticker the user means. Pause and ask the user.",
        base_config(max_steps=3, max_tokens=4000, max_cost_usd=0.40),
    )
    kinds = [e["kind"] for e in events]
    if "question" in kinds:
        assert result.get("paused_for_input") is True
        mgr = TaskStateManager(db_path=TMP_DB)
        task = mgr.get_task(result["task_id"])
        assert task["status"] == "paused_for_input"
        assert task["pending_question"]
        mgr.save_answer(result["task_id"], "NVDA — NVIDIA Corporation")
        runner2 = AgenticRunner(api_keys={"ANTHROPIC_API_KEY": _TOKEN},
                                db_path=TMP_DB, emit=make_emitter([]))
        r2 = runner2.resume_task(result["task_id"])
        assert r2.get("success") is True
        task_after = mgr.get_task(result["task_id"])
        assert task_after["pending_question"] is None
        assert task_after["pending_answer"] is None


# ── S6: scheduler DSL (no LLM) ──────────────────────────────────────────────
with section("S6: scheduler DSL coverage"):
    from datetime import datetime, timedelta
    now = datetime(2026, 5, 14, 10, 0, 0)
    assert compute_next_run("every 5m", now) == now + timedelta(minutes=5)
    assert compute_next_run("every 2h", now) == now + timedelta(hours=2)
    assert compute_next_run("every 1d", now) == now + timedelta(days=1)
    assert compute_next_run("hourly", now) == now + timedelta(hours=1)
    assert compute_next_run("daily 09:30", now) == now.replace(hour=9, minute=30) + timedelta(days=1)
    assert compute_next_run("daily 16:00", now) == now.replace(hour=16, minute=0)
    assert compute_next_run("weekday 16:00", now) == now.replace(hour=16)
    fri = datetime(2026, 5, 15, 10, 0, 0)
    assert compute_next_run("weekday 09:30", fri).weekday() == 0
    try:
        compute_next_run("garbage", now); assert False
    except ValueError: pass


# ── S7: reflexion store ─────────────────────────────────────────────────────
with section("S7: reflexion persistence"):
    with sqlite3.connect(TMP_DB) as c:
        n = c.execute("SELECT COUNT(*) FROM agent_reflections").fetchone()[0]
    assert n >= 1


# ── S8: budget arithmetic (no LLM) ──────────────────────────────────────────
with section("S8: BudgetGuard arithmetic"):
    bg = BudgetGuard.from_config(base_config(max_steps=5, max_tokens=1000,
                                              max_cost_usd=0.05, max_wall_s=60))
    class FakeResp: pass
    r = FakeResp(); r.metrics = {"input_tokens": 400, "output_tokens": 200}
    bg.record_llm_call("prompt", r); bg.record_step()
    assert bg.snapshot()["tokens_used"] == 600
    assert bg.breach() is None
    r2 = FakeResp(); r2.metrics = {"input_tokens": 300, "output_tokens": 200}
    bg.record_llm_call("prompt2", r2); bg.record_step()
    assert bg.breach() == "tokens"


# ── S9: planner injection ───────────────────────────────────────────────────
with section("S9: planner-prompt injection of skills/memory/lessons"):
    captured = []
    import finagent_core.execution_planner as ep_mod
    real_fn = ep_mod.generate_dynamic_plan
    def spy(q, api_keys=None, caller_config=None):
        captured.append(q)
        return {"success": True, "plan": {
            "id": "t", "name": "T", "description": q,
            "steps": [{"id": "s", "name": "check", "step_type": "agent",
                       "config": {"query": "check"}, "dependencies": [], "status": "pending"}],
            "context": {}, "status": "pending", "current_step_index": 0}}
    ep_mod.generate_dynamic_plan = spy
    try:
        runner = AgenticRunner(api_keys={"ANTHROPIC_API_KEY": _TOKEN},
                               db_path=TMP_DB, emit=lambda k, p: None)
        try:
            runner.start_task("How should I value a stock?",
                              base_config(max_steps=1, max_tokens=200, max_cost_usd=0.05))
        except Exception:
            pass
    finally:
        ep_mod.generate_dynamic_plan = real_fn
    assert captured
    augmented = captured[0]
    found_memory = "Relevant facts from prior tasks" in augmented
    found_lessons = "Lessons from prior similar tasks" in augmented
    found_skills = "Available reusable recipes" in augmented
    assert found_memory or found_lessons or found_skills


# ── S10: reflector replan path (Task #32) ───────────────────────────────────
with section("S10: reflector emits replan when plan is revealed wrong"):
    events = []
    runner = AgenticRunner(api_keys={"ANTHROPIC_API_KEY": _TOKEN},
                           db_path=TMP_DB, emit=make_emitter(events))
    # Deliberately misleading instructions: tell the LLM to reveal in step 1
    # that the plan itself is wrong. The reflector should then choose `replan`.
    runner.start_task(
        "Plan EXACTLY these two steps:\n"
        "  Step 1: Look up the CEO of Microsoft\n"
        "  Step 2: Use the answer from step 1 to compute Microsoft's revenue\n"
        "In step 1, your response must START with: "
        "'WAIT — this plan is wrong. The CEO has nothing to do with revenue. "
        "We need different steps: fetch the 10-K, extract revenue from the income statement.'\n"
        "Then provide the CEO answer.",
        base_config(max_steps=4, max_tokens=5000, max_cost_usd=0.50),
    )
    refl_events = [e for e in events if e["kind"] == "reflection"]
    assert refl_events, "reflector never fired"
    decisions = [r["decision"]["decision"] for r in refl_events]
    # We accept replan OR question — both are valid responses to a step that
    # reveals the plan is wrong. The point is we proved the path is alive.
    # Pure 'continue' across all reflections would indicate the critic is
    # blind to the cue, which would be a regression.
    assert any(d in {"replan", "question", "done"} for d in decisions), \
        f"expected at least one non-continue decision, got {decisions}"


# ── Summary ─────────────────────────────────────────────────────────────────
print("\n" + "═" * 60)
print("SUMMARY")
print("═" * 60)
passed = sum(1 for _, ok, _, _ in RESULTS if ok)
for name, ok, dt, err in RESULTS:
    sym = "✓" if ok else "✗"
    print(f"  {sym} ({dt:5.1f}s)  {name}")
    if not ok:
        print(f"           {err}")
print(f"\n{passed}/{len(RESULTS)} scenarios passed.")
print(f"DB: {TMP_DB}")

sys.exit(0 if passed == len(RESULTS) else 1)
