"""
Long-lived agentic daemon — alternative to per-task QProcess spawning.

Reads JSONL commands on stdin, dispatches each to AgenticRunner, emits events
on stdout. One Python process owns the lifecycle of every task it's running,
which lets us:

  • Preserve KV-cache reuse across consecutive LLM calls (Manus principle).
  • Run multiple tasks concurrently (asyncio.create_task per task).
  • Auto-resume `running` tasks on daemon startup after a crash.
  • Save the Python import / Agno init cost (~3-5s) on every task.

Wire format (JSONL on stdin, one op per line):

  {"op": "start_task",        "id": "<corr>", "query": "...",       "config": {...}}
  {"op": "resume_task",       "id": "<corr>", "task_id": "..."}
  {"op": "reply_to_question", "id": "<corr>", "task_id": "...",     "answer": "..."}
  {"op": "pause_task",        "id": "<corr>", "task_id": "..."}
  {"op": "cancel_task",       "id": "<corr>", "task_id": "..."}
  {"op": "shutdown"}

Each emits `AGENTIC_EVENT: <json>` lines (same protocol as the per-call
streaming mode, so the C++ reader can stay identical). `id` is echoed back in
every event so the caller can correlate. A `daemon_ready` event lands on
startup once auto-resume is complete.

This module is invoked as:  python -m finagent_core.agentic.daemon
The C++ AgentService.start_daemon() launches one process per app session.
"""
from __future__ import annotations

import asyncio
import json
import os
import sys
import traceback
from typing import Any, Dict, Optional

from finagent_core.agentic.events import emit, EVENT_PREFIX
from finagent_core.agentic.runner import AgenticRunner
from finagent_core.task_state import TaskStateManager, _DEFAULT_DB


def _api_keys_from_env() -> Dict[str, str]:
    """Pull provider keys from env vars the C++ host already injects on spawn."""
    return {
        "ANTHROPIC_API_KEY": os.environ.get("ANTHROPIC_API_KEY", ""),
        "ANTHROPIC_AUTH_TOKEN": os.environ.get("ANTHROPIC_AUTH_TOKEN", ""),
        "OPENAI_API_KEY": os.environ.get("OPENAI_API_KEY", ""),
        "GOOGLE_API_KEY": os.environ.get("GOOGLE_API_KEY", ""),
        "GROQ_API_KEY": os.environ.get("GROQ_API_KEY", ""),
        "MOONSHOT_API_KEY": os.environ.get("MOONSHOT_API_KEY", ""),
        "FINCEPT_API_KEY": os.environ.get("FINCEPT_API_KEY", ""),
    }


class Daemon:
    """Per-task asyncio scheduling around AgenticRunner."""

    def __init__(self, db_path: str = _DEFAULT_DB):
        self.db_path = db_path
        self.api_keys = _api_keys_from_env()
        # Map task_id → running asyncio.Task (so cancel can interrupt mid-step).
        # AgenticRunner itself is synchronous; we wrap each call in run_in_executor.
        self._tasks: Dict[str, asyncio.Task] = {}
        self._lock = asyncio.Lock()

    # ── Event emission with correlation id ───────────────────────────────────

    def _make_emit(self, corr_id: str):
        def _emit(kind: str, payload: Dict[str, Any]) -> None:
            payload = {**payload, "corr_id": corr_id}
            emit(kind, payload)
        return _emit

    # ── Op dispatch ──────────────────────────────────────────────────────────

    async def handle(self, op: Dict[str, Any]) -> None:
        kind = op.get("op")
        corr_id = op.get("id", "")
        try:
            if kind == "start_task":
                await self._start_task(corr_id, op["query"], op.get("config") or {})
            elif kind == "resume_task":
                await self._resume_task(corr_id, op["task_id"])
            elif kind == "reply_to_question":
                await self._reply_to_question(corr_id, op["task_id"], op.get("answer", ""))
            elif kind == "pause_task":
                self._signal_status(op["task_id"], "pause_requested")
                emit("control_ack", {"corr_id": corr_id, "task_id": op["task_id"], "signal": "pause_requested"})
            elif kind == "cancel_task":
                self._signal_status(op["task_id"], "cancel_requested")
                emit("control_ack", {"corr_id": corr_id, "task_id": op["task_id"], "signal": "cancel_requested"})
            elif kind == "shutdown":
                # Daemon caller will close stdin; the read loop exits on EOF.
                emit("shutdown_ack", {"corr_id": corr_id})
            else:
                emit("error", {"corr_id": corr_id, "error": f"unknown op: {kind!r}"})
        except Exception as e:
            emit("error", {"corr_id": corr_id, "error": f"{type(e).__name__}: {e}",
                           "trace": traceback.format_exc()[:1000]})

    # ── Task launchers — wrap synchronous AgenticRunner in a worker thread ──

    async def _start_task(self, corr_id: str, query: str, config: Dict[str, Any]) -> None:
        loop = asyncio.get_event_loop()
        runner = AgenticRunner(api_keys=self.api_keys, db_path=self.db_path,
                                emit=self._make_emit(corr_id))
        # Run synchronously in a thread so the daemon stays responsive to new ops.
        fut = loop.run_in_executor(None, runner.start_task, query, config)
        task = asyncio.create_task(fut)
        async with self._lock:
            # We don't know the task_id yet — patch it in after start.
            pass
        try:
            result = await task
            tid = result.get("task_id")
            if tid:
                async with self._lock:
                    self._tasks[tid] = task
        except Exception as e:
            emit("error", {"corr_id": corr_id, "error": str(e)})

    async def _resume_task(self, corr_id: str, task_id: str) -> None:
        loop = asyncio.get_event_loop()
        runner = AgenticRunner(api_keys=self.api_keys, db_path=self.db_path,
                                emit=self._make_emit(corr_id))
        fut = loop.run_in_executor(None, runner.resume_task, task_id)
        task = asyncio.create_task(fut)
        async with self._lock:
            self._tasks[task_id] = task
        try:
            await task
        except Exception as e:
            emit("error", {"corr_id": corr_id, "error": str(e)})
        finally:
            async with self._lock:
                self._tasks.pop(task_id, None)

    async def _reply_to_question(self, corr_id: str, task_id: str, answer: str) -> None:
        mgr = TaskStateManager(db_path=self.db_path)
        mgr.save_answer(task_id, answer)
        await self._resume_task(corr_id, task_id)

    def _signal_status(self, task_id: str, status: str) -> None:
        TaskStateManager(db_path=self.db_path).update_status(task_id, status)

    # ── Auto-resume on startup ───────────────────────────────────────────────

    async def auto_resume_on_boot(self) -> None:
        """Pick up any `running` tasks left over from a previous daemon crash.

        Conservative — we only resume `running` rows. `paused_for_input` rows
        wait for the user to reply; `failed`/`cancelled`/`completed` are
        terminal and require explicit operator action.
        """
        mgr = TaskStateManager(db_path=self.db_path)
        running = mgr.list_tasks(status="running", limit=20)
        if not running:
            return
        emit("daemon_auto_resume", {"count": len(running),
                                     "task_ids": [t["id"] for t in running]})
        for t in running:
            corr_id = f"resume_boot_{t['id'][:8]}"
            asyncio.create_task(self._resume_task(corr_id, t["id"]))

    # ── Read loop ────────────────────────────────────────────────────────────

    async def run(self) -> int:
        emit("daemon_ready", {"pid": os.getpid(), "db_path": self.db_path})
        await self.auto_resume_on_boot()

        reader = asyncio.StreamReader()
        protocol = asyncio.StreamReaderProtocol(reader)
        loop = asyncio.get_event_loop()
        await loop.connect_read_pipe(lambda: protocol, sys.stdin)

        while True:
            try:
                line = await reader.readline()
            except Exception as e:
                emit("error", {"error": f"stdin read failed: {e}"})
                break
            if not line:
                # EOF from parent → exit cleanly.
                break
            text = line.decode("utf-8", errors="replace").strip()
            if not text:
                continue
            try:
                op = json.loads(text)
            except json.JSONDecodeError as e:
                emit("error", {"error": f"bad JSON on stdin: {e}", "raw": text[:200]})
                continue
            # Don't await — let ops run concurrently. handle() catches its own errors.
            asyncio.create_task(self.handle(op))

        # Drain in-flight tasks before exit so we don't lose their final events.
        async with self._lock:
            inflight = list(self._tasks.values())
        if inflight:
            emit("daemon_draining", {"count": len(inflight)})
            await asyncio.gather(*inflight, return_exceptions=True)
        emit("daemon_exit", {})
        return 0


def main() -> int:
    return asyncio.run(Daemon().run())


if __name__ == "__main__":
    sys.exit(main())
