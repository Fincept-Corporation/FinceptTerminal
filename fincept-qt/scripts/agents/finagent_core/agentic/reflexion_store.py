"""
ReflexionStore — persistent record of every reflector verdict.

Reflexion (Shinn 2023, arXiv:2303.11366) compounds task quality by feeding past
verbal critiques back into the next attempt. Here we persist per-step decisions
keyed by query so a new task with a similar query can be told "last time you
worked on something like this, you needed to replan because X" before planning.

We deliberately keep only the high-signal fields the planner can use:
  decision (continue/replan/question/done), reason, replan_hint, question.

Heavy critique text isn't stored — step results live in agent_tasks.steps_json.
"""
from __future__ import annotations

import json
import sqlite3
import uuid
from datetime import datetime
from pathlib import Path
from typing import Any, Dict, List, Optional


_DEFAULT_DB = str(Path(__file__).parent.parent.parent.parent / "agent_tasks.db")


_CREATE_TABLE = """
CREATE TABLE IF NOT EXISTS agent_reflections (
    id          TEXT PRIMARY KEY,
    task_id     TEXT NOT NULL,
    query       TEXT NOT NULL,
    step_idx    INTEGER NOT NULL,
    decision    TEXT NOT NULL,
    reason      TEXT,
    replan_hint TEXT,
    question    TEXT,
    created_at  TEXT NOT NULL
)
"""

_CREATE_INDEX = "CREATE INDEX IF NOT EXISTS idx_agent_reflections_task ON agent_reflections(task_id)"

_CREATE_FTS = """
CREATE VIRTUAL TABLE IF NOT EXISTS agent_reflections_fts USING fts5(
    query, reason, replan_hint,
    content='agent_reflections', content_rowid='rowid'
)
"""

_CREATE_TRIGGERS = [
    """
    CREATE TRIGGER IF NOT EXISTS agent_reflections_ai AFTER INSERT ON agent_reflections BEGIN
      INSERT INTO agent_reflections_fts(rowid, query, reason, replan_hint)
      VALUES (new.rowid, new.query, COALESCE(new.reason,''), COALESCE(new.replan_hint,''));
    END;
    """,
    """
    CREATE TRIGGER IF NOT EXISTS agent_reflections_ad AFTER DELETE ON agent_reflections BEGIN
      INSERT INTO agent_reflections_fts(agent_reflections_fts, rowid, query, reason, replan_hint)
      VALUES('delete', old.rowid, old.query, COALESCE(old.reason,''), COALESCE(old.replan_hint,''));
    END;
    """,
]


class ReflexionStore:
    """SQLite-backed Reflexion store with FTS5 retrieval over query+reason."""

    def __init__(self, db_path: str = _DEFAULT_DB):
        self.db_path = db_path
        self._has_fts = False
        self._ensure_schema()

    def _conn(self) -> sqlite3.Connection:
        conn = sqlite3.connect(self.db_path)
        conn.row_factory = sqlite3.Row
        return conn

    def _ensure_schema(self) -> None:
        with self._conn() as conn:
            conn.execute(_CREATE_TABLE)
            conn.execute(_CREATE_INDEX)
            try:
                conn.execute(_CREATE_FTS)
                for t in _CREATE_TRIGGERS:
                    conn.execute(t)
                conn.execute(
                    "INSERT INTO agent_reflections_fts(rowid, query, reason, replan_hint) "
                    "SELECT rowid, query, COALESCE(reason,''), COALESCE(replan_hint,'') "
                    "FROM agent_reflections "
                    "WHERE rowid NOT IN (SELECT rowid FROM agent_reflections_fts)"
                )
                self._has_fts = True
            except sqlite3.OperationalError:
                self._has_fts = False

    def save_reflection(
        self,
        task_id: str,
        query: str,
        step_idx: int,
        decision: str,
        reason: Optional[str],
        replan_hint: Optional[str] = None,
        question: Optional[str] = None,
    ) -> str:
        rid = str(uuid.uuid4())
        now = datetime.utcnow().isoformat()
        with self._conn() as conn:
            conn.execute(
                "INSERT INTO agent_reflections "
                "(id, task_id, query, step_idx, decision, reason, replan_hint, question, created_at) "
                "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)",
                (rid, task_id, query, step_idx, decision, reason, replan_hint, question, now),
            )
        return rid

    def list_for_task(self, task_id: str) -> List[Dict[str, Any]]:
        with self._conn() as conn:
            rows = conn.execute(
                "SELECT * FROM agent_reflections WHERE task_id = ? ORDER BY step_idx ASC",
                (task_id,),
            ).fetchall()
        return [dict(r) for r in rows]

    def search_lessons(
        self,
        query: str,
        k: int = 4,
        exclude_task_id: Optional[str] = None,
        only_interesting: bool = True,
    ) -> List[Dict[str, Any]]:
        """Top-K past reflections most similar to `query`.

        `only_interesting=True` filters to decisions that actually carry a
        lesson (replan / question / done) — `continue` verdicts are noise.
        """
        query = (query or "").strip()
        if not query:
            return []
        results: List[Dict[str, Any]] = []
        if self._has_fts:
            cleaned = self._fts_sanitize(query)
            if cleaned:
                try:
                    with self._conn() as conn:
                        rows = conn.execute(
                            "SELECT r.* FROM agent_reflections_fts f "
                            "JOIN agent_reflections r ON r.rowid = f.rowid "
                            "WHERE agent_reflections_fts MATCH ? "
                            "ORDER BY bm25(agent_reflections_fts) ASC LIMIT ?",
                            (cleaned, k * 4),
                        ).fetchall()
                    results = [dict(r) for r in rows]
                except sqlite3.OperationalError:
                    results = []
        if not results:
            like = f"%{query}%"
            with self._conn() as conn:
                rows = conn.execute(
                    "SELECT * FROM agent_reflections "
                    "WHERE query LIKE ? OR reason LIKE ? OR replan_hint LIKE ? "
                    "ORDER BY created_at DESC LIMIT ?",
                    (like, like, like, k * 4),
                ).fetchall()
            results = [dict(r) for r in rows]

        if only_interesting:
            results = [r for r in results if r["decision"] in ("replan", "question", "done")]
        if exclude_task_id:
            results = [r for r in results if r["task_id"] != exclude_task_id]
        return results[:k]

    @staticmethod
    def _fts_sanitize(query: str) -> str:
        out = "".join(c for c in query if c.isalnum() or c in (" ", "-", "_"))
        terms = [t for t in out.split() if len(t) >= 2]
        return " OR ".join(terms) if terms else ""
