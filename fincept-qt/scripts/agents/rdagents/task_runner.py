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
import math
from pathlib import Path
from typing import Any, Callable, Dict, List

from loops import extract_factors_from_result, extract_models_from_result, save_checkpoint

logger = logging.getLogger(__name__)

Emit = Callable[[Dict[str, Any]], None]


def run_loop_sync(task_id: str, rd_loop: Any, loop_n: int, workspace: Path, emit: Emit,
                  extra: Dict[str, Any] | None = None) -> Dict[str, Any]:
    """Drive `rd_loop` for `loop_n` iterations, emitting progress; returns the result event.

    `extra` is merged into the result event (e.g. ignored_params)."""
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

    _hook_iteration_end(rd_loop, after_iteration)

    run = rd_loop.run(loop_n=loop_n)
    if inspect.isawaitable(run):
        asyncio.run(_await(run))

    raw = _extract_results(rd_loop, workspace)
    factors = extract_factors_from_result(raw)
    models = extract_models_from_result(raw)
    scored = factors + models
    best_ic = max((f["ic"] for f in scored if f.get("ic") is not None), default=state["best_ic"])
    best_sharpe = max((f["sharpe"] for f in scored if f.get("sharpe") is not None), default=state["best_sharpe"])
    result = {
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
    if extra:
        result.update(extra)
    return result


def _hook_iteration_end(rd_loop: Any, callback: Callable[[], None]) -> None:
    """
    Call `callback` once per finished iteration.

    rdagent 0.8.0's LoopBase runs steps (direct_exp_gen, coding, running,
    feedback) and dumps itself after each finished step as
    <session>/<loop>/<step>_<name>; the dump of the last step marks the end of
    an iteration (with step_idx already advanced, so a checkpoint written then
    resumes at the next iteration). The hook wraps `dump` on the loop's CLASS:
    an instance attribute holding a closure would make every one of those
    pickle dumps fail ("Can't pickle local object"). This process runs one
    loop, so patching its class is contained.
    """
    cls = type(rd_loop)
    steps = list(getattr(cls, "steps", []) or [])
    last_step = steps[-1] if steps else "feedback"
    original = cls.dump
    if getattr(original, "_fincept_hooked", False):
        return

    def dump(self: Any, path: Any) -> None:
        original(self, path)
        if self is rd_loop and Path(path).name.endswith("_" + last_step):
            try:
                callback()
            except Exception as exc:  # progress reporting must never break the loop
                logger.warning("progress hook failed: %s", exc)

    dump._fincept_hooked = True  # type: ignore[attr-defined]
    cls.dump = dump  # type: ignore[method-assign]


async def _await(awaitable: Any) -> Any:
    return await awaitable


def _hist_entries(rd_loop: Any) -> List[tuple]:
    """(experiment, feedback) pairs from the loop trace (rdagent 0.8.0 stores tuples)."""
    hist = getattr(getattr(rd_loop, "trace", None), "hist", None) or []
    out = []
    for entry in hist:
        if isinstance(entry, (tuple, list)) and len(entry) >= 2:
            out.append((entry[0], entry[1]))
        else:  # older rdagent: objects with attributes
            out.append((getattr(entry, "experiment", None), getattr(entry, "feedback", None)))
    return out


# Qlib backtest metric names (rdagent.scenarios.qlib.developer.feedback IMPORTANT_METRICS + IR).
_METRIC_KEYS = {
    "ic": "IC",
    "sharpe": "1day.excess_return_with_cost.information_ratio",
    "annualized_return": "1day.excess_return_with_cost.annualized_return",
    "max_drawdown": "1day.excess_return_with_cost.max_drawdown",
}


def _exp_metrics(exp: Any) -> Dict[str, Any]:
    """Backtest metrics of one experiment (exp.result is a pandas Series indexed by metric name)."""
    out: Dict[str, Any] = {k: None for k in _METRIC_KEYS}
    res = getattr(exp, "result", None)
    if res is None:
        return out
    for key, name in _METRIC_KEYS.items():
        try:
            v = res.get(name) if hasattr(res, "get") else None
            if v is not None:
                v = float(v)
                out[key] = v if math.isfinite(v) else None
        except Exception:
            pass
    return out


def _latest_metrics(rd_loop: Any):
    """(ic, sharpe) of the newest trace entry, best effort."""
    try:
        entries = _hist_entries(rd_loop)
        if entries:
            m = _exp_metrics(entries[-1][0])
            return m["ic"], m["sharpe"]
    except Exception:
        pass
    return None, None


def _workspace_code(ws_obj: Any) -> str:
    """Main source file of a sub-workspace (FBWorkspace.file_dict in 0.8.0)."""
    try:
        files = getattr(ws_obj, "file_dict", None) or getattr(ws_obj, "code_dict", None) or {}
        for name, code in files.items():
            if str(name).endswith(".py"):
                return code
        return next(iter(files.values()), "")
    except Exception:
        return ""


def _extract_results(rd_loop: Any, workspace: Path) -> Dict[str, Any]:
    """Factors/models from the loop trace; falls back to generated files in the workspace."""
    results: Dict[str, Any] = {"workspace": str(workspace), "factors": [], "models": []}
    try:
        for i, (exp, feedback) in enumerate(_hist_entries(rd_loop)):
            if exp is None:
                continue
            metrics = _exp_metrics(exp)
            accepted = bool(getattr(feedback, "decision", False)) if feedback is not None else None
            sub_tasks = getattr(exp, "sub_tasks", []) or []
            sub_ws = getattr(exp, "sub_workspace_list", []) or []
            for j, task_obj in enumerate(sub_tasks):
                ws_obj = sub_ws[j] if j < len(sub_ws) else None
                is_model = hasattr(task_obj, "model_type") and not hasattr(task_obj, "factor_name")
                name = (getattr(task_obj, "factor_name", None) or getattr(task_obj, "name", None)
                        or f"{'Model' if is_model else 'Factor'}_{i}_{j}")
                entry = {
                    "name": name,
                    "description": str(getattr(task_obj, "description", "") or ""),
                    "code": _workspace_code(ws_obj) if ws_obj is not None else "",
                    "iteration": i,
                    "accepted": accepted,
                    **metrics,
                }
                if is_model:
                    entry["model_id"] = f"model_{i}_{j}"
                    entry["model_type"] = getattr(task_obj, "model_type", None)
                    results["models"].append(entry)
                else:
                    entry["factor_id"] = f"factor_{i}_{j}"
                    results["factors"].append(entry)
    except Exception as exc:
        logger.warning("Could not extract results from trace: %s", exc)
    # No fallback scan of the workspace: since the run's cwd is the workspace,
    # it also holds rdagent's templates and every failed attempt, and listing
    # those .py files as "discovered factors" would be a wrong result.
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
