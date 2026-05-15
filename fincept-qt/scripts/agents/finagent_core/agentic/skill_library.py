"""
SkillLibrary — persistent store of reusable task recipes (Voyager-style).

Each "skill" is a natural-language recipe distilled from a successfully
completed task — name, description, and ordered steps. At plan time the
runner does a top-K semantic search and injects matching skills into the
planner prompt so common patterns get reused instead of re-decomposed.

Storage: same SQLite file as agent_tasks. FTS5 if available (fast, ranked);
falls back to LIKE-based search if the build lacks FTS5.

Skill recipe format (stored as JSON in `recipe_json`):
{
  "version": 1,
  "preconditions": "string (optional)",
  "steps": [
    {"name": "...", "query": "..."}  // step descriptions, no tool calls
  ],
  "expected_outputs": "string (optional)"
}
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
CREATE TABLE IF NOT EXISTS agent_skills (
    id            TEXT PRIMARY KEY,
    name          TEXT NOT NULL,
    description   TEXT NOT NULL,
    recipe_json   TEXT NOT NULL,
    success_count INTEGER NOT NULL DEFAULT 1,
    last_used_at  TEXT,
    created_at    TEXT NOT NULL
)
"""

# FTS5 mirror over name+description. content_rowid lets the FTS table act as
# an external index without duplicating storage.
_CREATE_FTS = """
CREATE VIRTUAL TABLE IF NOT EXISTS agent_skills_fts USING fts5(
    name, description,
    content='agent_skills', content_rowid='rowid'
)
"""

# Triggers keep FTS in sync with the base table. Idempotent CREATE.
_CREATE_TRIGGERS = [
    """
    CREATE TRIGGER IF NOT EXISTS agent_skills_ai AFTER INSERT ON agent_skills BEGIN
      INSERT INTO agent_skills_fts(rowid, name, description)
      VALUES (new.rowid, new.name, new.description);
    END;
    """,
    """
    CREATE TRIGGER IF NOT EXISTS agent_skills_ad AFTER DELETE ON agent_skills BEGIN
      INSERT INTO agent_skills_fts(agent_skills_fts, rowid, name, description)
      VALUES('delete', old.rowid, old.name, old.description);
    END;
    """,
    """
    CREATE TRIGGER IF NOT EXISTS agent_skills_au AFTER UPDATE ON agent_skills BEGIN
      INSERT INTO agent_skills_fts(agent_skills_fts, rowid, name, description)
      VALUES('delete', old.rowid, old.name, old.description);
      INSERT INTO agent_skills_fts(rowid, name, description)
      VALUES (new.rowid, new.name, new.description);
    END;
    """,
]


class SkillLibrary:
    """SQLite-backed skill store with FTS5 retrieval (LIKE fallback)."""

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
            try:
                conn.execute(_CREATE_FTS)
                for trig in _CREATE_TRIGGERS:
                    conn.execute(trig)
                # Backfill FTS for any rows inserted before triggers existed.
                conn.execute(
                    "INSERT INTO agent_skills_fts(rowid, name, description) "
                    "SELECT rowid, name, description FROM agent_skills "
                    "WHERE rowid NOT IN (SELECT rowid FROM agent_skills_fts)"
                )
                self._has_fts = True
            except sqlite3.OperationalError:
                # SQLite build without FTS5. Fall back to LIKE-based search.
                self._has_fts = False

    # ── CRUD ─────────────────────────────────────────────────────────────────

    def add(self, name: str, description: str, recipe: Dict[str, Any]) -> str:
        """Add a new skill. Returns its id."""
        skill_id = str(uuid.uuid4())
        now = datetime.utcnow().isoformat()
        with self._conn() as conn:
            conn.execute(
                "INSERT INTO agent_skills (id, name, description, recipe_json, "
                "success_count, created_at) VALUES (?, ?, ?, ?, 1, ?)",
                (skill_id, name.strip(), description.strip(),
                 json.dumps(recipe), now),
            )
        return skill_id

    def get(self, skill_id: str) -> Optional[Dict[str, Any]]:
        with self._conn() as conn:
            row = conn.execute(
                "SELECT * FROM agent_skills WHERE id = ?", (skill_id,)
            ).fetchone()
        return self._row_to_dict(row) if row else None

    def list(self, limit: int = 50) -> List[Dict[str, Any]]:
        with self._conn() as conn:
            rows = conn.execute(
                "SELECT * FROM agent_skills ORDER BY success_count DESC, "
                "created_at DESC LIMIT ?", (limit,)
            ).fetchall()
        return [self._row_to_dict(r) for r in rows]

    def delete(self, skill_id: str) -> None:
        with self._conn() as conn:
            conn.execute("DELETE FROM agent_skills WHERE id = ?", (skill_id,))

    def record_use(self, skill_id: str) -> None:
        """Increment success_count + bump last_used_at."""
        now = datetime.utcnow().isoformat()
        with self._conn() as conn:
            conn.execute(
                "UPDATE agent_skills SET success_count = success_count + 1, "
                "last_used_at = ? WHERE id = ?", (now, skill_id),
            )

    # ── Retrieval ────────────────────────────────────────────────────────────

    def search(self, query: str, k: int = 5) -> List[Dict[str, Any]]:
        """Top-K skills matching the query."""
        query = (query or "").strip()
        if not query:
            return []

        if self._has_fts:
            # FTS5 MATCH; sanitize for the query mini-language (drop quotes
            # and operators that confuse the tokenizer).
            cleaned = self._fts_sanitize(query)
            if not cleaned:
                return self._fallback_search(query, k)
            try:
                with self._conn() as conn:
                    rows = conn.execute(
                        "SELECT s.* FROM agent_skills_fts f "
                        "JOIN agent_skills s ON s.rowid = f.rowid "
                        "WHERE agent_skills_fts MATCH ? "
                        "ORDER BY bm25(agent_skills_fts) ASC LIMIT ?",
                        (cleaned, k),
                    ).fetchall()
                return [self._row_to_dict(r) for r in rows]
            except sqlite3.OperationalError:
                return self._fallback_search(query, k)
        return self._fallback_search(query, k)

    def _fallback_search(self, query: str, k: int) -> List[Dict[str, Any]]:
        """LIKE-based fallback when FTS5 is unavailable."""
        like = f"%{query}%"
        with self._conn() as conn:
            rows = conn.execute(
                "SELECT * FROM agent_skills "
                "WHERE name LIKE ? OR description LIKE ? "
                "ORDER BY success_count DESC LIMIT ?", (like, like, k),
            ).fetchall()
        return [self._row_to_dict(r) for r in rows]

    @staticmethod
    def _fts_sanitize(query: str) -> str:
        # Keep alnum, space, hyphen. Strip everything else so the FTS5 parser
        # doesn't choke on stray punctuation.
        out_chars = []
        for ch in query:
            if ch.isalnum() or ch in (" ", "-", "_"):
                out_chars.append(ch)
        cleaned = "".join(out_chars).strip()
        # Match any term — use OR so partial overlaps still rank.
        terms = [t for t in cleaned.split() if len(t) >= 2]
        return " OR ".join(terms) if terms else ""

    # ── Row hydration ────────────────────────────────────────────────────────

    @staticmethod
    def _row_to_dict(row: sqlite3.Row) -> Dict[str, Any]:
        d = dict(row)
        recipe_raw = d.pop("recipe_json", None)
        d["recipe"] = json.loads(recipe_raw) if recipe_raw else {}
        return d
