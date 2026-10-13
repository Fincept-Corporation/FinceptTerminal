"""
TaskState - Persistent task state for autonomous multi-step agent operations.

Enables true agentic behavior by:
1. Saving task progress to SQLite after every step
2. Resuming a failed/interrupted task from the last checkpoint
3. Letting the agent pick up mid-task without re-running completed steps

This is what makes the agent truly autonomous — if it's running a 5-step
analysis and step 3 fails due to a network error, it can be resumed from
step 3 rather than starting over.

Storage format (SQLite):
  Table: agent_tasks
    id          TEXT PRIMARY KEY
    query       TEXT
    config_json TEXT   -- agent config snapshot (policy only: no API keys, no
                          per-run bridge tokens — see agentic/runner.py)
    steps_json  TEXT   -- list of completed steps with results
    status      TEXT   -- pending | running | completed | failed | paused
    current_step INT
    result      TEXT   -- final result when status=completed
    error       TEXT   -- error message when status=failed
    created_at  TEXT
    updated_at  TEXT
"""

import json
import sqlite3
import uuid
from datetime import datetime
from typing import Any, Dict, List, Optional

from finagent_core.paths import agent_tasks_db_path


# ─── Schema ──────────────────────────────────────────────────────────────────

_CREATE_TABLE = """
CREATE TABLE IF NOT EXISTS agent_tasks (
    id           TEXT PRIMARY KEY,
    query        TEXT NOT NULL,
    config_json  TEXT NOT NULL,
    plan_json    TEXT,
    steps_json   TEXT NOT NULL DEFAULT '[]',
    status       TEXT NOT NULL DEFAULT 'pending',
    current_step INTEGER NOT NULL DEFAULT 0,
    result       TEXT,
    error        TEXT,
    created_at   TEXT NOT NULL,
    updated_at   TEXT NOT NULL
)
"""


# ─── TaskStateManager ────────────────────────────────────────────────────────

class TaskStateManager:
    """
    Manages persistent task state for resumable agentic operations.
    All state is stored in SQLite so it survives process restarts.
    """

    def __init__(self, db_path: Optional[str] = None):
        # Per-profile agent data dir (finagent_core/paths.py) unless overridden.
        self.db_path = db_path or agent_tasks_db_path()
        self._ensure_table()

    def _conn(self) -> sqlite3.Connection:
        conn = sqlite3.connect(self.db_path)
        conn.row_factory = sqlite3.Row
        return conn

    def _ensure_table(self):
        with self._conn() as conn:
            conn.execute(_CREATE_TABLE)
            # Idempotent migrations for DBs predating columns.
            cols = {row[1] for row in conn.execute("PRAGMA table_info(agent_tasks)").fetchall()}
            if "plan_json" not in cols:
                conn.execute("ALTER TABLE agent_tasks ADD COLUMN plan_json TEXT")
            if "pending_question" not in cols:
                conn.execute("ALTER TABLE agent_tasks ADD COLUMN pending_question TEXT")
            if "pending_answer" not in cols:
                conn.execute("ALTER TABLE agent_tasks ADD COLUMN pending_answer TEXT")

    # ── CRUD ─────────────────────────────────────────────────────────────────

    def create_task(self, query: str, config: Dict[str, Any]) -> str:
        """Create a new task and return its ID."""
        task_id = str(uuid.uuid4())
        now = datetime.utcnow().isoformat()
        with self._conn() as conn:
            conn.execute(
                "INSERT INTO agent_tasks (id, query, config_json, steps_json, status, current_step, created_at, updated_at) "
                "VALUES (?, ?, ?, '[]', 'pending', 0, ?, ?)",
                (task_id, query, json.dumps(config), now, now)
            )
        return task_id

    def get_task(self, task_id: str) -> Optional[Dict[str, Any]]:
        """Get task by ID."""
        with self._conn() as conn:
            row = conn.execute("SELECT * FROM agent_tasks WHERE id = ?", (task_id,)).fetchone()
        if not row:
            return None
        return self._row_to_dict(row)

    def list_tasks(self, status: Optional[str] = None, limit: int = 50) -> List[Dict[str, Any]]:
        """List tasks, optionally filtered by status."""
        with self._conn() as conn:
            if status:
                rows = conn.execute(
                    "SELECT * FROM agent_tasks WHERE status = ? ORDER BY created_at DESC LIMIT ?",
                    (status, limit)
                ).fetchall()
            else:
                rows = conn.execute(
                    "SELECT * FROM agent_tasks ORDER BY created_at DESC LIMIT ?", (limit,)
                ).fetchall()
        return [self._row_to_dict(r) for r in rows]

    def update_status(self, task_id: str, status: str, error: Optional[str] = None):
        """Update task status."""
        now = datetime.utcnow().isoformat()
        with self._conn() as conn:
            conn.execute(
                "UPDATE agent_tasks SET status = ?, error = ?, updated_at = ? WHERE id = ?",
                (status, error, now, task_id)
            )

    def save_plan(self, task_id: str, plan: Dict[str, Any]):
        """Persist the structured ExecutionPlan dict for this task."""
        now = datetime.utcnow().isoformat()
        with self._conn() as conn:
            conn.execute(
                "UPDATE agent_tasks SET plan_json = ?, updated_at = ? WHERE id = ?",
                (json.dumps(plan), now, task_id)
            )

    def save_question(self, task_id: str, question: str):
        """Persist a reflector-emitted clarifying question; status flips to paused_for_input."""
        now = datetime.utcnow().isoformat()
        with self._conn() as conn:
            conn.execute(
                "UPDATE agent_tasks SET pending_question = ?, pending_answer = NULL, "
                "status = 'paused_for_input', updated_at = ? WHERE id = ?",
                (question, now, task_id)
            )

    def save_answer(self, task_id: str, answer: str):
        """Stage a user reply so the next resume threads it into context."""
        now = datetime.utcnow().isoformat()
        with self._conn() as conn:
            conn.execute(
                "UPDATE agent_tasks SET pending_answer = ?, updated_at = ? WHERE id = ?",
                (answer, now, task_id)
            )

    def consume_answer(self, task_id: str) -> Optional[str]:
        """Atomically read-and-clear the pending answer."""
        with self._conn() as conn:
            row = conn.execute(
                "SELECT pending_answer FROM agent_tasks WHERE id = ?", (task_id,)
            ).fetchone()
            if not row or not row[0]:
                return None
            answer = row[0]
            conn.execute(
                "UPDATE agent_tasks SET pending_answer = NULL, pending_question = NULL "
                "WHERE id = ?", (task_id,)
            )
            return answer

    def save_checkpoint(self, task_id: str, step_index: int, step_result: Dict[str, Any]):
        """
        Save a completed step as a checkpoint.
        The agent can be resumed from this point if it fails on a later step.
        """
        task = self.get_task(task_id)
        if not task:
            raise ValueError(f"Task {task_id} not found")

        steps = task["steps"]
        # Ensure we don't duplicate steps on retry
        if len(steps) <= step_index:
            steps.append(step_result)
        else:
            steps[step_index] = step_result

        now = datetime.utcnow().isoformat()
        with self._conn() as conn:
            conn.execute(
                "UPDATE agent_tasks SET steps_json = ?, current_step = ?, status = 'running', updated_at = ? WHERE id = ?",
                (json.dumps(steps), step_index + 1, now, task_id)
            )

    def complete_task(self, task_id: str, result: str):
        """Mark task as completed with final result."""
        now = datetime.utcnow().isoformat()
        with self._conn() as conn:
            conn.execute(
                "UPDATE agent_tasks SET status = 'completed', result = ?, updated_at = ? WHERE id = ?",
                (result, now, task_id)
            )

    def fail_task(self, task_id: str, error: str):
        """Mark task as failed — can be resumed later."""
        now = datetime.utcnow().isoformat()
        with self._conn() as conn:
            conn.execute(
                "UPDATE agent_tasks SET status = 'failed', error = ?, updated_at = ? WHERE id = ?",
                (error, now, task_id)
            )

    def delete_task(self, task_id: str):
        """Delete a task."""
        with self._conn() as conn:
            conn.execute("DELETE FROM agent_tasks WHERE id = ?", (task_id,))

    def recover_interrupted(self) -> int:
        """Settle tasks orphaned by a terminal shutdown or crash.

        Only valid when no runner process of the current session exists (the
        terminal calls it once at startup): a task still marked running /
        pending / pause_requested has nobody driving it and would otherwise sit
        in that state forever with RESUME disabled. Those become 'paused' (so
        RESUME continues from the last checkpoint); cancel_requested becomes
        'cancelled'. Returns the number of tasks changed.
        """
        now = datetime.utcnow().isoformat()
        with self._conn() as conn:
            paused = conn.execute(
                "UPDATE agent_tasks SET status = 'paused', updated_at = ? "
                "WHERE status IN ('running', 'pending', 'pause_requested')",
                (now,),
            ).rowcount
            cancelled = conn.execute(
                "UPDATE agent_tasks SET status = 'cancelled', updated_at = ? "
                "WHERE status = 'cancel_requested'",
                (now,),
            ).rowcount
        return int(paused or 0) + int(cancelled or 0)

    def _row_to_dict(self, row: sqlite3.Row) -> Dict[str, Any]:
        d = dict(row)
        d["config"] = json.loads(d.pop("config_json", "{}"))
        d["steps"] = json.loads(d.pop("steps_json", "[]"))
        plan_raw = d.pop("plan_json", None)
        d["plan"] = json.loads(plan_raw) if plan_raw else None
        # pending_question / pending_answer are nullable strings; keep them
        # as-is so the C++ layer can render the banner from get_task.
        d.setdefault("pending_question", None)
        d.setdefault("pending_answer", None)
        return d


# ─── Resumable Task Runner ────────────────────────────────────────────────────

class ResumableTaskRunner:
    """
    Planning / step-execution / synthesis base for agentic tasks.

    Plans come from the shared ExecutionPlanner.generate_dynamic_plan, so the
    plan format is unified with the PLANNER tab. The task lifecycle itself —
    start, resume from the last checkpoint, pause/cancel polling, events,
    budgets — lives in finagent_core.agentic.runner.AgenticRunner, the only
    runner the terminal drives (main.py agentic_* actions).
    """

    def __init__(self, api_keys: Dict[str, str], db_path: Optional[str] = None):
        self.api_keys = api_keys
        self.state_mgr = TaskStateManager(db_path)

    def _plan_steps(self, query: str, config: Dict[str, Any]) -> Dict[str, Any]:
        """
        Generate a structured ExecutionPlan dict for the query via the shared
        ExecutionPlanner.generate_dynamic_plan entry point. Same plan format used
        by the PLANNER tab and the agentic-mode runner — single source of truth.
        """
        from finagent_core.execution_planner import generate_dynamic_plan

        result = generate_dynamic_plan(query, api_keys=self.api_keys, caller_config=config)
        if result.get("success") and result.get("plan", {}).get("steps"):
            return result["plan"]

        # Last-ditch single-step fallback so callers always have a plan to iterate.
        import time
        return {
            "id": f"trivial_{int(time.time())}",
            "name": "Trivial Plan",
            "description": query,
            "steps": [self._fallback_step(query)],
            "context": {},
            "status": "pending",
            "current_step_index": 0,
        }

    @staticmethod
    def _fallback_step(query: str) -> Dict[str, Any]:
        return {
            "id": "step_1",
            "name": query[:80],
            "step_type": "agent",
            "config": {"query": query},
            "dependencies": [],
            "status": "pending",
        }

    def _execute_step(
        self,
        step: Dict[str, Any],
        original_query: str,
        config: Dict[str, Any],
        step_index: int,
        prior_results: List[Any],
    ) -> Dict[str, Any]:
        """Execute a single PlanStep with full tool access and prior context."""
        from finagent_core.core_agent import CoreAgent

        # Build context from prior steps
        context_parts = []
        for j, r in enumerate(prior_results):
            if isinstance(r, dict) and r.get("response"):
                context_parts.append(f"Step {j+1} result: {r['response'][:500]}")
        context = "\n".join(context_parts)

        step_cfg = step.get("config") or {}
        step_desc = step.get("name") or step_cfg.get("query") or original_query
        step_query = step_cfg.get("query") or step_desc

        step_prompt = f"""Original goal: {original_query}

Prior steps completed:
{context if context else "(none)"}

Current step {step_index + 1}: {step_desc}
{step_query}

Execute this step thoroughly using available tools."""

        agent = CoreAgent(api_keys=self.api_keys)
        response = agent.run(step_prompt, config)
        content = agent.get_response_content(response)

        return {
            "step": step_index + 1,
            "id": step.get("id", f"step_{step_index + 1}"),
            "description": step_desc,
            "response": content,
        }

    def _synthesize(
        self,
        original_query: str,
        all_results: List[Dict[str, Any]],
        config: Dict[str, Any],
    ) -> str:
        """Synthesize all step results into a final coherent answer."""
        from finagent_core.core_agent import CoreAgent

        # Per-step budget: keep total synthesis prompt under 35k chars
        # (leaving room for system prompt, query, and synthesis instructions)
        MAX_SYNTHESIS_CHARS = 30_000
        per_step_budget = max(500, MAX_SYNTHESIS_CHARS // max(len(all_results), 1))

        step_summaries = []
        for i, r in enumerate(all_results):
            step_text = str(r.get("response", ""))
            if len(step_text) > per_step_budget:
                step_text = step_text[:per_step_budget] + "\n... [truncated for length]"
            step_summaries.append(
                f"**Step {r.get('step', i+1)}: {r.get('description', '')}**\n{step_text}"
            )

        steps_summary = "\n\n".join(step_summaries)

        synthesis_config = {
            **config,
            "instructions": "You are a financial analyst. Synthesize the research steps into a clear, comprehensive final answer.",
            "tools": [],  # Synthesis is pure reasoning
        }

        prompt = f"""The user asked: {original_query}

Here are the research steps completed:

{steps_summary}

Synthesize all of the above into a single comprehensive, well-structured final answer."""

        agent = CoreAgent(api_keys=self.api_keys)
        response = agent.run(prompt, synthesis_config)
        return agent.get_response_content(response)


# ─── Module-level helpers for main.py dispatch ───────────────────────────────

_state_mgr: Optional[TaskStateManager] = None


def get_state_manager(db_path: Optional[str] = None) -> TaskStateManager:
    global _state_mgr
    if _state_mgr is None:
        _state_mgr = TaskStateManager(db_path)
    return _state_mgr
