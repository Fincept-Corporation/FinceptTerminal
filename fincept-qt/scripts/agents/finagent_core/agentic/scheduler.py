"""
ScheduledTaskStore — durable schedule table + due-task ticker.

Intentionally avoids the cron grammar (which would pull in croniter or a hand-
rolled parser nobody wants to debug). The schedule language is a tight, finance-
appropriate subset:

  every <N>m          — every N minutes               (e.g. "every 5m")
  every <N>h          — every N hours                 (e.g. "every 2h")
  every <N>d          — every N days                  (e.g. "every 1d")
  hourly              — alias for "every 1h"
  daily HH:MM         — once a day at the given UTC time
  weekday HH:MM       — Mon-Fri at the given UTC time

Storage is one row per schedule. `next_run_at` is the single source of truth:
the tick reads schedules whose next_run_at <= now AND enabled=1, computes a
fresh next_run_at, and hands back the work list. The caller (AgentService) is
responsible for actually starting the agentic task; this module never spawns.
"""
from __future__ import annotations

import json
import re
import sqlite3
import uuid
from datetime import datetime, timedelta
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple


_DEFAULT_DB = str(Path(__file__).parent.parent.parent.parent / "agent_tasks.db")


_CREATE_TABLE = """
CREATE TABLE IF NOT EXISTS agent_scheduled_tasks (
    id            TEXT PRIMARY KEY,
    name          TEXT NOT NULL,
    query         TEXT NOT NULL,
    config_json   TEXT NOT NULL,
    schedule_expr TEXT NOT NULL,
    enabled       INTEGER NOT NULL DEFAULT 1,
    last_run_at   TEXT,
    next_run_at   TEXT NOT NULL,
    created_at    TEXT NOT NULL
)
"""

_CREATE_INDEX = (
    "CREATE INDEX IF NOT EXISTS idx_sched_due "
    "ON agent_scheduled_tasks(enabled, next_run_at)"
)


class ScheduledTaskStore:
    """SQLite-backed schedule store with a small DSL for run cadence."""

    def __init__(self, db_path: str = _DEFAULT_DB):
        self.db_path = db_path
        self._ensure_schema()

    def _conn(self) -> sqlite3.Connection:
        conn = sqlite3.connect(self.db_path)
        conn.row_factory = sqlite3.Row
        return conn

    def _ensure_schema(self) -> None:
        with self._conn() as conn:
            conn.execute(_CREATE_TABLE)
            conn.execute(_CREATE_INDEX)

    # ── CRUD ─────────────────────────────────────────────────────────────────

    def create(
        self,
        name: str,
        query: str,
        schedule_expr: str,
        config: Optional[Dict[str, Any]] = None,
        start_now: bool = False,
    ) -> str:
        """Register a new schedule. Returns id."""
        sid = str(uuid.uuid4())
        now = datetime.utcnow()
        # `start_now=True` means fire on the next tick rather than at the first
        # natural cadence boundary; useful for "schedule this and run me a
        # sample right away".
        next_run = now if start_now else compute_next_run(schedule_expr, now)
        with self._conn() as conn:
            conn.execute(
                "INSERT INTO agent_scheduled_tasks "
                "(id, name, query, config_json, schedule_expr, enabled, next_run_at, created_at) "
                "VALUES (?, ?, ?, ?, ?, 1, ?, ?)",
                (sid, name.strip(), query, json.dumps(config or {}),
                 schedule_expr.strip(), next_run.isoformat(), now.isoformat()),
            )
        return sid

    def list(self, enabled_only: bool = False, limit: int = 200) -> List[Dict[str, Any]]:
        with self._conn() as conn:
            if enabled_only:
                rows = conn.execute(
                    "SELECT * FROM agent_scheduled_tasks WHERE enabled = 1 "
                    "ORDER BY next_run_at ASC LIMIT ?", (limit,),
                ).fetchall()
            else:
                rows = conn.execute(
                    "SELECT * FROM agent_scheduled_tasks "
                    "ORDER BY created_at DESC LIMIT ?", (limit,),
                ).fetchall()
        return [self._row_to_dict(r) for r in rows]

    def get(self, sid: str) -> Optional[Dict[str, Any]]:
        with self._conn() as conn:
            row = conn.execute(
                "SELECT * FROM agent_scheduled_tasks WHERE id = ?", (sid,),
            ).fetchone()
        return self._row_to_dict(row) if row else None

    def delete(self, sid: str) -> None:
        with self._conn() as conn:
            conn.execute("DELETE FROM agent_scheduled_tasks WHERE id = ?", (sid,))

    def set_enabled(self, sid: str, enabled: bool) -> None:
        with self._conn() as conn:
            conn.execute(
                "UPDATE agent_scheduled_tasks SET enabled = ? WHERE id = ?",
                (1 if enabled else 0, sid),
            )

    # ── Tick: return due rows AND advance their next_run_at ──────────────────

    def take_due(self, now: Optional[datetime] = None) -> List[Dict[str, Any]]:
        """
        Atomically claim all due schedules and advance their next_run_at.

        Returns the list of due schedule rows so the caller can fire each as
        an agentic task. After this call the same schedule won't reappear
        until its next cadence boundary.
        """
        now = now or datetime.utcnow()
        with self._conn() as conn:
            rows = conn.execute(
                "SELECT * FROM agent_scheduled_tasks "
                "WHERE enabled = 1 AND next_run_at <= ?",
                (now.isoformat(),),
            ).fetchall()
            due: List[Dict[str, Any]] = [self._row_to_dict(r) for r in rows]
            for d in due:
                nxt = compute_next_run(d["schedule_expr"], now)
                conn.execute(
                    "UPDATE agent_scheduled_tasks "
                    "SET last_run_at = ?, next_run_at = ? WHERE id = ?",
                    (now.isoformat(), nxt.isoformat(), d["id"]),
                )
                d["last_run_at"] = now.isoformat()
                d["next_run_at"] = nxt.isoformat()
        return due

    # ── Row hydration ────────────────────────────────────────────────────────

    @staticmethod
    def _row_to_dict(row: sqlite3.Row) -> Dict[str, Any]:
        d = dict(row)
        cfg_raw = d.pop("config_json", None)
        d["config"] = json.loads(cfg_raw) if cfg_raw else {}
        d["enabled"] = bool(d.get("enabled", 0))
        return d


# ── Schedule DSL parser ──────────────────────────────────────────────────────

_EVERY_RE = re.compile(r"^every\s+(\d+)\s*([mhd])$", re.IGNORECASE)
_DAILY_RE = re.compile(r"^daily\s+(\d{1,2}):(\d{2})$", re.IGNORECASE)
_WEEKDAY_RE = re.compile(r"^weekday\s+(\d{1,2}):(\d{2})$", re.IGNORECASE)


def compute_next_run(expr: str, after: datetime) -> datetime:
    """Compute the next firing time strictly *after* `after` for `expr`.

    Raises ValueError for unparseable expressions. Always returns a UTC-naive
    datetime to match how we serialise everywhere else.
    """
    expr = (expr or "").strip().lower()
    if expr == "hourly":
        return after + timedelta(hours=1)

    m = _EVERY_RE.match(expr)
    if m:
        n, unit = int(m.group(1)), m.group(2)
        if n <= 0:
            raise ValueError(f"invalid period: {expr}")
        if unit == "m":
            return after + timedelta(minutes=n)
        if unit == "h":
            return after + timedelta(hours=n)
        if unit == "d":
            return after + timedelta(days=n)

    m = _DAILY_RE.match(expr)
    if m:
        h, mn = int(m.group(1)), int(m.group(2))
        return _next_at_time(after, h, mn, weekdays_only=False)

    m = _WEEKDAY_RE.match(expr)
    if m:
        h, mn = int(m.group(1)), int(m.group(2))
        return _next_at_time(after, h, mn, weekdays_only=True)

    raise ValueError(f"unrecognised schedule expression: {expr!r}")


def _next_at_time(after: datetime, hour: int, minute: int, weekdays_only: bool) -> datetime:
    if not (0 <= hour < 24 and 0 <= minute < 60):
        raise ValueError(f"invalid time {hour}:{minute}")
    candidate = after.replace(hour=hour, minute=minute, second=0, microsecond=0)
    if candidate <= after:
        candidate += timedelta(days=1)
    if weekdays_only:
        while candidate.weekday() >= 5:  # Saturday=5, Sunday=6
            candidate += timedelta(days=1)
    return candidate
