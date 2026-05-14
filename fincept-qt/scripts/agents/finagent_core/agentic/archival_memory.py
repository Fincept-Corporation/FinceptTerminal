"""
ArchivalMemoryStore — cross-task persistent fact store (Letta tier 3).

Each entry is a piece of long-lived knowledge: a user preference, a prior
conclusion, a portfolio fact. Entries are searchable via FTS5 and injected
into the planner prompt when a new task's query overlaps semantically.

Note on the Letta tier model:
- TIER 1 (working)   → in-prompt; handled by CoreAgent context window
- TIER 2 (recall)    → searchable conversation history; Agno's built-in
                        `_agentic_memory` module already provides this via
                        core_agent.store_memory / recall_memories
- TIER 3 (archival)  → cold storage queryable on demand — THIS CLASS

Schema:
  agent_memory_archival(id, user_id, type, content, metadata_json,
                        created_at, last_accessed_at)
  + FTS5 mirror agent_memory_archival_fts(content) when available

The agent-callable MCP-tool surface (archival_memory_save / _search) is left
for a follow-up; for now the runner does best-effort plan-time injection of
relevant memories, mirroring how skill_library is consumed.
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
CREATE TABLE IF NOT EXISTS agent_memory_archival (
    id               TEXT PRIMARY KEY,
    user_id          TEXT,
    type             TEXT NOT NULL DEFAULT 'fact',
    content          TEXT NOT NULL,
    metadata_json    TEXT,
    created_at       TEXT NOT NULL,
    last_accessed_at TEXT
)
"""

_CREATE_FTS = """
CREATE VIRTUAL TABLE IF NOT EXISTS agent_memory_archival_fts USING fts5(
    content,
    content='agent_memory_archival', content_rowid='rowid'
)
"""

_CREATE_TRIGGERS = [
    """
    CREATE TRIGGER IF NOT EXISTS agent_memory_archival_ai AFTER INSERT ON agent_memory_archival BEGIN
      INSERT INTO agent_memory_archival_fts(rowid, content) VALUES (new.rowid, new.content);
    END;
    """,
    """
    CREATE TRIGGER IF NOT EXISTS agent_memory_archival_ad AFTER DELETE ON agent_memory_archival BEGIN
      INSERT INTO agent_memory_archival_fts(agent_memory_archival_fts, rowid, content)
      VALUES('delete', old.rowid, old.content);
    END;
    """,
    """
    CREATE TRIGGER IF NOT EXISTS agent_memory_archival_au AFTER UPDATE ON agent_memory_archival BEGIN
      INSERT INTO agent_memory_archival_fts(agent_memory_archival_fts, rowid, content)
      VALUES('delete', old.rowid, old.content);
      INSERT INTO agent_memory_archival_fts(rowid, content) VALUES (new.rowid, new.content);
    END;
    """,
]


class ArchivalMemoryStore:
    """Cross-task durable fact store with FTS5 retrieval (LIKE fallback)."""

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
                for t in _CREATE_TRIGGERS:
                    conn.execute(t)
                conn.execute(
                    "INSERT INTO agent_memory_archival_fts(rowid, content) "
                    "SELECT rowid, content FROM agent_memory_archival "
                    "WHERE rowid NOT IN (SELECT rowid FROM agent_memory_archival_fts)"
                )
                self._has_fts = True
            except sqlite3.OperationalError:
                self._has_fts = False

    # ── CRUD ─────────────────────────────────────────────────────────────────

    def save(
        self,
        content: str,
        user_id: Optional[str] = None,
        memory_type: str = "fact",
        metadata: Optional[Dict[str, Any]] = None,
    ) -> str:
        """Persist an entry. Returns its id."""
        mid = str(uuid.uuid4())
        now = datetime.utcnow().isoformat()
        with self._conn() as conn:
            conn.execute(
                "INSERT INTO agent_memory_archival "
                "(id, user_id, type, content, metadata_json, created_at) "
                "VALUES (?, ?, ?, ?, ?, ?)",
                (mid, user_id, memory_type, content,
                 json.dumps(metadata or {}), now),
            )
        return mid

    def get(self, mid: str) -> Optional[Dict[str, Any]]:
        with self._conn() as conn:
            row = conn.execute(
                "SELECT * FROM agent_memory_archival WHERE id = ?", (mid,)
            ).fetchone()
        return self._row_to_dict(row) if row else None

    def delete(self, mid: str) -> None:
        with self._conn() as conn:
            conn.execute("DELETE FROM agent_memory_archival WHERE id = ?", (mid,))

    def list(
        self,
        user_id: Optional[str] = None,
        memory_type: Optional[str] = None,
        limit: int = 50,
    ) -> List[Dict[str, Any]]:
        clauses, args = [], []
        if user_id is not None:
            clauses.append("user_id = ?"); args.append(user_id)
        if memory_type is not None:
            clauses.append("type = ?"); args.append(memory_type)
        where = ("WHERE " + " AND ".join(clauses)) if clauses else ""
        args.append(limit)
        with self._conn() as conn:
            rows = conn.execute(
                f"SELECT * FROM agent_memory_archival {where} "
                f"ORDER BY created_at DESC LIMIT ?", args,
            ).fetchall()
        return [self._row_to_dict(r) for r in rows]

    # ── Retrieval ────────────────────────────────────────────────────────────

    def search(
        self,
        query: str,
        k: int = 5,
        user_id: Optional[str] = None,
    ) -> List[Dict[str, Any]]:
        query = (query or "").strip()
        if not query:
            return []
        if self._has_fts:
            cleaned = self._fts_sanitize(query)
            if cleaned:
                try:
                    with self._conn() as conn:
                        if user_id is not None:
                            rows = conn.execute(
                                "SELECT m.* FROM agent_memory_archival_fts f "
                                "JOIN agent_memory_archival m ON m.rowid = f.rowid "
                                "WHERE agent_memory_archival_fts MATCH ? AND m.user_id = ? "
                                "ORDER BY bm25(agent_memory_archival_fts) ASC LIMIT ?",
                                (cleaned, user_id, k),
                            ).fetchall()
                        else:
                            rows = conn.execute(
                                "SELECT m.* FROM agent_memory_archival_fts f "
                                "JOIN agent_memory_archival m ON m.rowid = f.rowid "
                                "WHERE agent_memory_archival_fts MATCH ? "
                                "ORDER BY bm25(agent_memory_archival_fts) ASC LIMIT ?",
                                (cleaned, k),
                            ).fetchall()
                    self._mark_accessed([r["id"] for r in rows])
                    return [self._row_to_dict(r) for r in rows]
                except sqlite3.OperationalError:
                    pass
        return self._fallback_search(query, k, user_id)

    def _fallback_search(self, query: str, k: int, user_id: Optional[str]) -> List[Dict[str, Any]]:
        like = f"%{query}%"
        with self._conn() as conn:
            if user_id is not None:
                rows = conn.execute(
                    "SELECT * FROM agent_memory_archival "
                    "WHERE content LIKE ? AND user_id = ? "
                    "ORDER BY created_at DESC LIMIT ?", (like, user_id, k),
                ).fetchall()
            else:
                rows = conn.execute(
                    "SELECT * FROM agent_memory_archival "
                    "WHERE content LIKE ? ORDER BY created_at DESC LIMIT ?",
                    (like, k),
                ).fetchall()
        self._mark_accessed([r["id"] for r in rows])
        return [self._row_to_dict(r) for r in rows]

    def _mark_accessed(self, ids: List[str]) -> None:
        if not ids:
            return
        now = datetime.utcnow().isoformat()
        with self._conn() as conn:
            for mid in ids:
                conn.execute(
                    "UPDATE agent_memory_archival SET last_accessed_at = ? WHERE id = ?",
                    (now, mid),
                )

    # ── Helpers ──────────────────────────────────────────────────────────────

    @staticmethod
    def _fts_sanitize(query: str) -> str:
        out = "".join(c for c in query if c.isalnum() or c in (" ", "-", "_"))
        terms = [t for t in out.split() if len(t) >= 2]
        return " OR ".join(terms) if terms else ""

    @staticmethod
    def _row_to_dict(row: sqlite3.Row) -> Dict[str, Any]:
        d = dict(row)
        meta_raw = d.pop("metadata_json", None)
        d["metadata"] = json.loads(meta_raw) if meta_raw else {}
        return d
