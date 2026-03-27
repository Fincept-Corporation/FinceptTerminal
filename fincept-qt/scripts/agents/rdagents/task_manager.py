"""
task_manager.py — Background execution engine + TaskManager for Fincept rdagents.

Responsibilities:
  - TaskManager: thread-safe in-memory task registry (status, progress, results)
  - TaskExecutor: runs LoopBase.run() on a background asyncio event loop thread
  - Progress callbacks: hook into LoopBase step completions to update TaskManager
  - Workspace management: each task gets scripts/agents/rdagents/workspaces/{task_id}/
"""

from __future__ import annotations

import asyncio
import json
import logging
import os
import threading
from datetime import datetime
from pathlib import Path
from typing import Any, Callable

logger = logging.getLogger(__name__)

# Base workspace directory
_WORKSPACE_BASE = Path(__file__).parent / "workspaces"


# ---------------------------------------------------------------------------
# TaskManager — thread-safe task registry
# ---------------------------------------------------------------------------

class TaskManager:
    """Thread-safe registry of all running/completed rdagent tasks."""

    def __init__(self) -> None:
        self._tasks: dict[str, dict[str, Any]] = {}
        self._lock = threading.Lock()

    def create(self, task_id: str, task_type: str, config: dict[str, Any]) -> None:
        workspace = str(_WORKSPACE_BASE / task_id)
        with self._lock:
            self._tasks[task_id] = {
                "id":                  task_id,
                "type":                task_type,
                "status":              "created",
                "progress":            0,
                "config":              config,
                "workspace":           workspace,
                "created_at":          datetime.now().isoformat(),
                "started_at":          None,
                "completed_at":        None,
                "error":               None,
                "iterations_completed": 0,
                "current_step":        None,
                "factors_generated":   0,
                "factors_tested":      0,
                "best_ic":             None,
                "best_sharpe":         None,
                "cost_spent":          0.0,
                "result":              None,
                "checkpoint_path":     None,
            }

    def update(self, task_id: str, updates: dict[str, Any]) -> None:
        with self._lock:
            if task_id in self._tasks:
                self._tasks[task_id].update(updates)

    def get(self, task_id: str) -> dict[str, Any] | None:
        with self._lock:
            task = self._tasks.get(task_id)
            return dict(task) if task else None

    def list_all(self, status: str | None = None) -> list[dict[str, Any]]:
        with self._lock:
            tasks = list(self._tasks.values())
        if status:
            tasks = [t for t in tasks if t.get("status") == status]
        return [
            {k: t[k] for k in ("id", "type", "status", "progress",
                                "created_at", "iterations_completed")}
            for t in tasks
        ]

    def set_result(self, task_id: str, result: Any) -> None:
        self.update(task_id, {"result": result})

    def get_result(self, task_id: str) -> Any:
        task = self.get(task_id)
        return task["result"] if task else None


# ---------------------------------------------------------------------------
# TaskExecutor — runs LoopBase on a background asyncio thread
# ---------------------------------------------------------------------------

class TaskExecutor:
    """
    Runs a single rdagent LoopBase instance on a dedicated background thread
    with its own asyncio event loop.

    Progress is fed back into TaskManager via a step-completion callback
    that wraps the loop's internal step mechanism.
    """

    def __init__(
        self,
        task_id: str,
        task_manager: TaskManager,
        loop_factory: Callable[[], Any],  # returns a LoopBase instance
        loop_n: int = 10,
        on_complete: Callable[[str, Any], None] | None = None,
    ) -> None:
        self._task_id      = task_id
        self._tm           = task_manager
        self._loop_factory = loop_factory
        self._loop_n       = loop_n
        self._on_complete  = on_complete
        self._stop_event   = threading.Event()
        self._thread: threading.Thread | None = None
        self._aio_loop: asyncio.AbstractEventLoop | None = None

    def start(self) -> None:
        self._thread = threading.Thread(
            target=self._run_in_thread,
            name=f"rdagent-{self._task_id}",
            daemon=True,
        )
        self._thread.start()
        self._tm.update(self._task_id, {
            "status":     "running",
            "started_at": datetime.now().isoformat(),
        })

    def stop(self) -> None:
        self._stop_event.set()
        if self._aio_loop and self._aio_loop.is_running():
            self._aio_loop.call_soon_threadsafe(self._aio_loop.stop)

    def _run_in_thread(self) -> None:
        self._aio_loop = asyncio.new_event_loop()
        asyncio.set_event_loop(self._aio_loop)
        try:
            self._aio_loop.run_until_complete(self._execute())
        except Exception as exc:
            logger.error("Task %s failed: %s", self._task_id, exc)
            self._tm.update(self._task_id, {
                "status":       "failed",
                "error":        str(exc),
                "completed_at": datetime.now().isoformat(),
            })
        finally:
            self._aio_loop.close()

    async def _execute(self) -> None:
        task_id = self._task_id

        # Build workspace dir
        workspace = Path(self._tm.get(task_id)["workspace"])
        workspace.mkdir(parents=True, exist_ok=True)

        # Instantiate the loop (factory runs in async context)
        rd_loop = self._loop_factory()

        # Wrap the loop's step execution to intercept progress
        original_execute_loop = getattr(rd_loop, "execute_loop", None)

        async def _step_hook(*args: Any, **kwargs: Any) -> Any:
            if self._stop_event.is_set():
                return
            result = await original_execute_loop(*args, **kwargs)
            self._on_step_complete(rd_loop)
            return result

        if original_execute_loop:
            rd_loop.execute_loop = _step_hook  # type: ignore[method-assign]

        try:
            # Save checkpoint path for resume support
            checkpoint_path = str(workspace / "checkpoint.pkl")
            self._tm.update(task_id, {"checkpoint_path": checkpoint_path})

            # Run the loop
            await rd_loop.run(loop_n=self._loop_n)

            # Extract final results from workspace
            result = self._extract_results(rd_loop, workspace)
            self._tm.set_result(task_id, result)
            self._tm.update(task_id, {
                "status":       "completed",
                "progress":     100,
                "completed_at": datetime.now().isoformat(),
            })

            if self._on_complete:
                self._on_complete(task_id, result)

        except asyncio.CancelledError:
            self._tm.update(task_id, {
                "status":       "stopped",
                "completed_at": datetime.now().isoformat(),
            })
        except Exception as exc:
            logger.error("Loop execution failed for %s: %s", task_id, exc)
            self._tm.update(task_id, {
                "status":       "failed",
                "error":        str(exc),
                "completed_at": datetime.now().isoformat(),
            })

    def _on_step_complete(self, rd_loop: Any) -> None:
        """Called after each loop step — update progress in TaskManager."""
        task = self._tm.get(self._task_id)
        if not task:
            return

        max_n = self._loop_n
        completed = task.get("iterations_completed", 0) + 1
        progress = min(99, int((completed / max_n) * 100))

        updates: dict[str, Any] = {
            "iterations_completed": completed,
            "progress":             progress,
            "current_step":         "feedback",
        }

        # Try to extract live metrics from the loop's trace
        try:
            trace = getattr(rd_loop, "trace", None)
            if trace:
                hist = getattr(trace, "hist", [])
                if hist:
                    last = hist[-1]
                    feedback = getattr(last, "feedback", None)
                    if feedback:
                        ic = getattr(feedback, "ic", None)
                        sharpe = getattr(feedback, "sharpe", None)
                        if ic is not None:
                            best_ic = task.get("best_ic")
                            updates["best_ic"] = ic if best_ic is None else max(best_ic, ic)
                        if sharpe is not None:
                            best_sharpe = task.get("best_sharpe")
                            updates["best_sharpe"] = sharpe if best_sharpe is None else max(best_sharpe, sharpe)
        except Exception:
            pass  # Metrics extraction is best-effort

        self._tm.update(self._task_id, updates)

    def _extract_results(self, rd_loop: Any, workspace: Path) -> dict[str, Any]:
        """Extract final results from the loop workspace after completion."""
        results: dict[str, Any] = {
            "workspace": str(workspace),
            "factors":   [],
            "models":    [],
        }

        # Try to get factors from trace
        try:
            trace = getattr(rd_loop, "trace", None)
            if trace:
                hist = getattr(trace, "hist", [])
                for i, entry in enumerate(hist):
                    exp = getattr(entry, "experiment", None)
                    feedback = getattr(entry, "feedback", None)
                    if exp and feedback:
                        # Extract sub-tasks (factor/model implementations)
                        sub_tasks = getattr(exp, "sub_tasks", []) or []
                        sub_ws = getattr(exp, "sub_workspace_list", []) or []
                        for j, (task_obj, ws_obj) in enumerate(zip(sub_tasks, sub_ws)):
                            code = ""
                            try:
                                if ws_obj:
                                    code_dict = getattr(ws_obj, "code", {}) or {}
                                    code = next(iter(code_dict.values()), "")
                            except Exception:
                                pass

                            factor_entry = {
                                "factor_id":   f"factor_{i}_{j}",
                                "name":        getattr(task_obj, "factor_name",
                                               getattr(task_obj, "model_type", f"Factor_{i}_{j}")),
                                "description": getattr(task_obj, "factor_description",
                                               getattr(task_obj, "description", "")),
                                "code":        code,
                                "iteration":   i,
                                "ic":          getattr(feedback, "ic", None),
                                "sharpe":      getattr(feedback, "sharpe", None),
                            }
                            results["factors"].append(factor_entry)
        except Exception as exc:
            logger.warning("Could not extract factors from trace: %s", exc)

        # Fallback: scan workspace directory for generated Python files
        if not results["factors"]:
            results["factors"] = _scan_workspace_for_factors(workspace)

        return results


def _scan_workspace_for_factors(workspace: Path) -> list[dict[str, Any]]:
    """Scan workspace dir for generated factor .py files as fallback."""
    factors = []
    for py_file in workspace.rglob("*.py"):
        try:
            code = py_file.read_text(encoding="utf-8")
            metrics_file = py_file.with_suffix(".json")
            metrics: dict[str, Any] = {}
            if metrics_file.exists():
                metrics = json.loads(metrics_file.read_text())

            factors.append({
                "factor_id":   py_file.stem,
                "name":        py_file.stem.replace("_", " ").title(),
                "description": f"Generated factor from {py_file.name}",
                "code":        code,
                "ic":          metrics.get("ic"),
                "sharpe":      metrics.get("sharpe"),
            })
        except Exception:
            continue
    return factors


# ---------------------------------------------------------------------------
# Module-level shared instances
# ---------------------------------------------------------------------------

_task_manager = TaskManager()
_executors: dict[str, TaskExecutor] = {}


def get_task_manager() -> TaskManager:
    return _task_manager


def get_executor(task_id: str) -> TaskExecutor | None:
    return _executors.get(task_id)


def register_executor(task_id: str, executor: TaskExecutor) -> None:
    _executors[task_id] = executor


def stop_executor(task_id: str) -> bool:
    executor = _executors.get(task_id)
    if executor:
        executor.stop()
        return True
    return False
