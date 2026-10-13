"""
task_runner.py - run ONE RD-Agent loop to completion in the current process.

The terminal (AIQuantLabService) owns one child process per RD-Agent task and
runs `cli.py run_task --stdin` in it. This module is what that process does:
build nothing, register nothing, just drive the loop synchronously and report.

stdout protocol - one JSON object per line, nothing else on stdout:
  {"event": "progress", "task_id", "iterations_completed", "max_iterations",
   "progress": 0..1, "current_step", "best_ic", "best_sharpe"}       after every iteration
  {"event": "result", "success": true, "task_id", "factors": [...], "models": [...],
   "best_ic", "best_sharpe", "iterations_completed", "workspace"}     once, at the end
  {"event": "error", "task_id", "error"}                              on failure

A checkpoint (LoopBase.dump) is written to <workspace>/checkpoint.pkl after
every iteration so a stopped (killed) task can be resumed from the last one.

This replaces the old TaskManager/TaskExecutor design, which ran the loop on a
daemon thread of a one-shot process: the process printed "started" and exited,
killing the thread, so no task ever ran, and status/stop calls in later
processes inspected an empty in-memory registry.
"""

from __future__ import annotations

import asyncio
import inspect
import json
import logging
from pathlib import Path
from typing import Any, Callable, Dict, List

from loops import extract_factors_from_result, extract_models_from_result, save_checkpoint

logger = logging.getLogger(__name__)

Emit = Callable[[Dict[str, Any]], None]


def run_loop_sync(task_id: str, rd_loop: Any, loop_n: int, workspace: Path, emit: Emit) -> Dict[str, Any]:
    """Drive `rd_loop` for `loop_n` iterations, emitting progress; returns the result event."""
    workspace.mkdir(parents=True, exist_ok=True)
    checkpoint_path = str(workspace / "checkpoint.pkl")
    state: Dict[str, Any] = {"completed": 0, "best_ic": None, "best_sharpe": None}

    def after_iteration() -> None:
        state["completed"] += 1
        ic, sharpe = _latest_metrics(rd_loop)
        if ic is not None:
            state["best_ic"] = ic if state["best_ic"] is None else max(state["best_ic"], ic)
        if sharpe is not None:
            state["best_sharpe"] = sharpe if state["best_sharpe"] is None else max(state["best_sharpe"], sharpe)
        save_checkpoint(rd_loop, checkpoint_path)
        emit({
            "event": "progress",
            "task_id": task_id,
            "iterations_completed": state["completed"],
            "max_iterations": loop_n,
            "progress": min(0.99, state["completed"] / max(loop_n, 1)),
            "current_step": f"iteration {state['completed']}/{loop_n} done",
            "best_ic": state["best_ic"],
            "best_sharpe": state["best_sharpe"],
        })

    # Hook the loop's per-iteration entry point so progress is reported as it
    # happens (rdagent's LoopBase exposes execute_loop; sync or async).
    original = getattr(rd_loop, "execute_loop", None)
    if original is not None:
        if inspect.iscoroutinefunction(original):
            async def hooked(*args: Any, **kwargs: Any) -> Any:
                result = await original(*args, **kwargs)
                after_iteration()
                return result
        else:
            def hooked(*args: Any, **kwargs: Any) -> Any:  # type: ignore[misc]
                result = original(*args, **kwargs)
                after_iteration()
                return result
        rd_loop.execute_loop = hooked  # type: ignore[method-assign]

    run = rd_loop.run(loop_n=loop_n)
    if inspect.isawaitable(run):
        asyncio.run(_await(run))

    raw = _extract_results(rd_loop, workspace)
    factors = extract_factors_from_result(raw)
    models = extract_models_from_result(raw)
    best_ic = max((f["ic"] for f in factors if f.get("ic") is not None), default=state["best_ic"])
    best_sharpe = max((f["sharpe"] for f in factors if f.get("sharpe") is not None), default=state["best_sharpe"])
    return {
        "event": "result",
        "success": True,
        "task_id": task_id,
        "factors": factors,
        "models": models,
        "best_ic": best_ic,
        "best_sharpe": best_sharpe,
        "iterations_completed": state["completed"],
        "workspace": str(workspace),
    }


async def _await(awaitable: Any) -> Any:
    return await awaitable


def _latest_metrics(rd_loop: Any):
    """(ic, sharpe) of the newest trace entry, best effort."""
    try:
        hist = getattr(getattr(rd_loop, "trace", None), "hist", None) or []
        if hist:
            feedback = getattr(hist[-1], "feedback", None)
            if feedback is not None:
                return getattr(feedback, "ic", None), getattr(feedback, "sharpe", None)
    except Exception:
        pass
    return None, None


def _extract_results(rd_loop: Any, workspace: Path) -> Dict[str, Any]:
    """Factors/models from the loop trace; falls back to generated files in the workspace."""
    results: Dict[str, Any] = {"workspace": str(workspace), "factors": [], "models": []}
    try:
        hist = getattr(getattr(rd_loop, "trace", None), "hist", None) or []
        for i, entry in enumerate(hist):
            exp = getattr(entry, "experiment", None)
            feedback = getattr(entry, "feedback", None)
            if not exp or not feedback:
                continue
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
                results["factors"].append({
                    "factor_id": f"factor_{i}_{j}",
                    "name": getattr(task_obj, "factor_name", getattr(task_obj, "model_type", f"Factor_{i}_{j}")),
                    "description": getattr(task_obj, "factor_description", getattr(task_obj, "description", "")),
                    "code": code,
                    "iteration": i,
                    "ic": getattr(feedback, "ic", None),
                    "sharpe": getattr(feedback, "sharpe", None),
                })
    except Exception as exc:
        logger.warning("Could not extract factors from trace: %s", exc)

    if not results["factors"]:
        results["factors"] = _scan_workspace_for_factors(workspace)
    return results


def _scan_workspace_for_factors(workspace: Path) -> List[Dict[str, Any]]:
    """Generated factor .py files (with optional sibling .json metrics) in the workspace."""
    factors: List[Dict[str, Any]] = []
    for py_file in workspace.rglob("*.py"):
        try:
            code = py_file.read_text(encoding="utf-8")
            metrics_file = py_file.with_suffix(".json")
            metrics: Dict[str, Any] = {}
            if metrics_file.exists():
                metrics = json.loads(metrics_file.read_text(encoding="utf-8"))
            factors.append({
                "factor_id": py_file.stem,
                "name": py_file.stem.replace("_", " ").title(),
                "description": f"Generated factor from {py_file.name}",
                "code": code,
                "ic": metrics.get("ic"),
                "sharpe": metrics.get("sharpe"),
            })
        except Exception:
            continue
    return factors
