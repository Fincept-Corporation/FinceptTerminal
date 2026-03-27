"""
cli.py — Entry point for Fincept rdagents integration.

Called by AIQuantLabService.cpp via PythonRunner:
  python agents/rdagents/cli.py <command> <json_params>

All stdout is JSON. All logs go to stderr.

Commands:
  check_status                — library availability + version
  get_capabilities            — full capability map
  start_factor_mining   <json> — start FactorRDLoop in background
  get_task_status       <id>   — progress of a running task
  get_discovered_factors <id>  — factors from completed task
  start_model_optimization <json> — start ModelRDLoop in background
  get_optimized_model   <id>   — best model config from completed task
  start_quant_research  <json> — start QuantRDLoop (factor + model)
  run_autonomous_research <json> — alias for start_quant_research
  stop_task             <id>   — cancel a running task
  list_tasks            [status] — list all tasks
  export_factor         <json> — export factor code (python or qlib format)
"""

from __future__ import annotations

import json
import logging
import sys
import os
from datetime import datetime
from pathlib import Path
from typing import Any

# Ensure the scripts/ directory is on sys.path for sibling imports
_HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(_HERE))
# Also add scripts/agents/ so deepagents imports work if needed
sys.path.insert(0, str(_HERE.parent))

logging.basicConfig(
    stream=sys.stderr,
    level=logging.INFO,
    format="%(asctime)s [rdagents] %(levelname)s %(message)s",
)
logger = logging.getLogger(__name__)


def _out(obj: Any) -> None:
    """Print JSON to stdout and flush."""
    print(json.dumps(obj, default=str), flush=True)


def _err(msg: str, code: int = 1) -> None:
    _out({"success": False, "error": msg})
    sys.exit(code)


# ---------------------------------------------------------------------------
# Lazy imports — only load heavy rdagent deps when actually needed
# ---------------------------------------------------------------------------

def _load_modules():
    from config import apply_llm_config  # noqa: F401
    from task_manager import get_task_manager, get_executor, register_executor, stop_executor, TaskExecutor
    from loops import (
        build_factor_loop, build_model_loop, build_quant_loop,
        build_report_loop, loop_availability,
        extract_factors_from_result, extract_models_from_result,
        FACTOR_LOOP_AVAILABLE, MODEL_LOOP_AVAILABLE,
        QUANT_LOOP_AVAILABLE, REPORT_LOOP_AVAILABLE,
    )
    return locals()


# ---------------------------------------------------------------------------
# Commands
# ---------------------------------------------------------------------------

def cmd_check_status(_params: dict[str, Any]) -> dict[str, Any]:
    try:
        import importlib.metadata
        rdagent_version = importlib.metadata.version("rdagent")
    except Exception:
        rdagent_version = "unknown"

    try:
        from loops import loop_availability
        avail = loop_availability()
    except Exception as e:
        avail = {"error": str(e)}

    return {
        "success":        True,
        "rdagent_version": rdagent_version,
        "availability":   avail,
        "workspace_base": str(Path(__file__).parent / "workspaces"),
    }


def cmd_get_capabilities(_params: dict[str, Any]) -> dict[str, Any]:
    from loops import loop_availability
    avail = loop_availability()

    return {
        "success": True,
        "capabilities": {
            "factor_mining": {
                "available":    avail["factor_loop"],
                "description":  "Autonomous alpha factor discovery using FactorRDLoop",
                "loop":         "FactorRDLoop",
                "steps":        ["propose", "code", "run", "feedback"],
                "metrics":      ["IC", "ICIR", "Sharpe", "Annualized Return", "Max Drawdown"],
            },
            "model_optimization": {
                "available":    avail["model_loop"],
                "description":  "ML model hyperparameter optimization using ModelRDLoop",
                "loop":         "ModelRDLoop",
                "supported_models": ["lightgbm", "xgboost", "lstm", "gru", "transformer", "tcn"],
                "targets":      ["sharpe", "ic", "max_drawdown", "win_rate", "calmar_ratio"],
            },
            "quant_research": {
                "available":    avail["quant_loop"],
                "description":  "Combined factor + model research using QuantRDLoop",
                "loop":         "QuantRDLoop",
            },
            "factor_from_pdf": {
                "available":    avail["report_loop"],
                "description":  "Extract alpha factors from research PDFs via FactorReportLoop",
                "loop":         "FactorReportLoop",
            },
            "checkpointing":  True,
            "background_exec": True,
            "litellm_backend": True,
        },
    }


def cmd_start_factor_mining(params: dict[str, Any]) -> dict[str, Any]:
    from config import apply_llm_config
    from task_manager import get_task_manager, TaskExecutor, register_executor
    from loops import build_factor_loop, FACTOR_LOOP_AVAILABLE

    if not FACTOR_LOOP_AVAILABLE:
        return {"success": False, "error": "FactorRDLoop not available. Install rdagent with qlib extras."}

    task_description = params.get("task_description", "Discover alpha factors for equity trading")
    target_market    = params.get("target_market", "US")
    max_iterations   = int(params.get("max_iterations", 10))
    target_ic        = float(params.get("target_ic", 0.05))
    llm_config       = params.get("config", {})

    apply_llm_config(llm_config)

    task_id = f"factor_{datetime.now().strftime('%Y%m%d_%H%M%S')}"
    workspace = str(Path(__file__).parent / "workspaces" / task_id)

    tm = get_task_manager()
    tm.create(task_id, "factor_mining", {
        "task_description": task_description,
        "target_market":    target_market,
        "max_iterations":   max_iterations,
        "target_ic":        target_ic,
    })

    enable_mcp = bool(params.get("enable_mcp", False))
    mcp_port   = int(params.get("mcp_port", 18765))

    def _factory():
        return build_factor_loop(workspace, task_description, target_market, target_ic,
                                 enable_mcp=enable_mcp, mcp_port=mcp_port)

    executor = TaskExecutor(task_id, tm, _factory, loop_n=max_iterations)
    register_executor(task_id, executor)
    executor.start()

    return {
        "success":        True,
        "task_id":        task_id,
        "status":         "started",
        "message":        "FactorRDLoop started in background",
        "max_iterations": max_iterations,
        "target_market":  target_market,
        "target_ic":      target_ic,
        "estimated_time": f"{max_iterations * 5}-{max_iterations * 10} minutes",
    }


def cmd_start_model_optimization(params: dict[str, Any]) -> dict[str, Any]:
    from config import apply_llm_config
    from task_manager import get_task_manager, TaskExecutor, register_executor
    from loops import build_model_loop, MODEL_LOOP_AVAILABLE

    if not MODEL_LOOP_AVAILABLE:
        return {"success": False, "error": "ModelRDLoop not available. Install rdagent with qlib extras."}

    model_type           = params.get("model_type", "lightgbm")
    optimization_target  = params.get("optimization_target", "sharpe")
    max_iterations       = int(params.get("max_iterations", 10))
    llm_config           = params.get("config", {})

    apply_llm_config(llm_config)

    task_id = f"model_{model_type}_{datetime.now().strftime('%Y%m%d_%H%M%S')}"
    workspace = str(Path(__file__).parent / "workspaces" / task_id)

    tm = get_task_manager()
    tm.create(task_id, "model_optimization", {
        "model_type":          model_type,
        "optimization_target": optimization_target,
        "max_iterations":      max_iterations,
    })

    enable_mcp = bool(params.get("enable_mcp", False))
    mcp_port   = int(params.get("mcp_port", 18765))

    def _factory():
        return build_model_loop(workspace, model_type, optimization_target,
                                enable_mcp=enable_mcp, mcp_port=mcp_port)

    executor = TaskExecutor(task_id, tm, _factory, loop_n=max_iterations)
    register_executor(task_id, executor)
    executor.start()

    return {
        "success":             True,
        "task_id":             task_id,
        "status":              "started",
        "message":             "ModelRDLoop started in background",
        "model_type":          model_type,
        "optimization_target": optimization_target,
        "max_iterations":      max_iterations,
        "estimated_time":      f"{max_iterations * 3}-{max_iterations * 6} minutes",
    }


def cmd_start_quant_research(params: dict[str, Any]) -> dict[str, Any]:
    from config import apply_llm_config
    from task_manager import get_task_manager, TaskExecutor, register_executor
    from loops import build_quant_loop, QUANT_LOOP_AVAILABLE

    if not QUANT_LOOP_AVAILABLE:
        return {"success": False, "error": "QuantRDLoop not available. Check rdagent installation."}

    research_goal  = params.get("research_goal", params.get("task_description", "Quantitative research"))
    target_market  = params.get("target_market", "US")
    max_iterations = int(params.get("iterations", params.get("max_iterations", 10)))
    llm_config     = params.get("config", {})

    apply_llm_config(llm_config)

    task_id = f"quant_{datetime.now().strftime('%Y%m%d_%H%M%S')}"
    workspace = str(Path(__file__).parent / "workspaces" / task_id)

    tm = get_task_manager()
    tm.create(task_id, "quant_research", {
        "research_goal": research_goal,
        "target_market": target_market,
        "max_iterations": max_iterations,
    })

    enable_mcp = bool(params.get("enable_mcp", False))
    mcp_port   = int(params.get("mcp_port", 18765))

    def _factory():
        return build_quant_loop(workspace, research_goal, target_market,
                                enable_mcp=enable_mcp, mcp_port=mcp_port)

    executor = TaskExecutor(task_id, tm, _factory, loop_n=max_iterations)
    register_executor(task_id, executor)
    executor.start()

    return {
        "success":        True,
        "task_id":        task_id,
        "status":         "started",
        "message":        "QuantRDLoop started in background",
        "research_goal":  research_goal,
        "max_iterations": max_iterations,
    }


def cmd_get_task_status(params: dict[str, Any]) -> dict[str, Any]:
    from task_manager import get_task_manager
    task_id = params.get("task_id", "")
    tm = get_task_manager()
    task = tm.get(task_id)
    if not task:
        return {"success": False, "error": f"Task {task_id!r} not found"}

    max_n = task.get("config", {}).get("max_iterations", 10)
    completed = task.get("iterations_completed", 0)

    elapsed = ""
    started = task.get("started_at")
    if started:
        try:
            delta = datetime.now() - datetime.fromisoformat(started)
            elapsed = f"{int(delta.total_seconds() / 60)} minutes"
        except Exception:
            pass

    return {
        "success":              True,
        "task_id":              task_id,
        "type":                 task.get("type"),
        "status":               task.get("status"),
        "progress":             task.get("progress", 0),
        "iterations_completed": completed,
        "max_iterations":       max_n,
        "current_step":         task.get("current_step"),
        "best_ic":              task.get("best_ic"),
        "best_sharpe":          task.get("best_sharpe"),
        "elapsed_time":         elapsed,
        "error":                task.get("error"),
    }


def cmd_get_discovered_factors(params: dict[str, Any]) -> dict[str, Any]:
    from task_manager import get_task_manager
    from loops import extract_factors_from_result

    task_id = params.get("task_id", "")
    tm = get_task_manager()
    task = tm.get(task_id)
    if not task:
        return {"success": False, "error": f"Task {task_id!r} not found"}

    result = tm.get_result(task_id) or {}
    factors = extract_factors_from_result(result)

    best_ic     = max((f["ic"]     for f in factors if f.get("ic")     is not None), default=None)
    best_sharpe = max((f["sharpe"] for f in factors if f.get("sharpe") is not None), default=None)

    return {
        "success":       True,
        "task_id":       task_id,
        "status":        task.get("status"),
        "total_factors": len(factors),
        "factors":       factors,
        "best_ic":       best_ic,
        "best_sharpe":   best_sharpe,
    }


def cmd_get_optimized_model(params: dict[str, Any]) -> dict[str, Any]:
    from task_manager import get_task_manager
    from loops import extract_models_from_result

    task_id = params.get("task_id", "")
    tm = get_task_manager()
    task = tm.get(task_id)
    if not task:
        return {"success": False, "error": f"Task {task_id!r} not found"}

    result = tm.get_result(task_id) or {}
    models = extract_models_from_result(result)

    return {
        "success":     True,
        "task_id":     task_id,
        "status":      task.get("status"),
        "model_type":  task.get("config", {}).get("model_type"),
        "total_models": len(models),
        "models":      models,
        "best_sharpe": task.get("best_sharpe"),
        "best_ic":     task.get("best_ic"),
    }


def cmd_stop_task(params: dict[str, Any]) -> dict[str, Any]:
    from task_manager import get_task_manager, stop_executor
    task_id = params.get("task_id", "")
    tm = get_task_manager()
    task = tm.get(task_id)
    if not task:
        return {"success": False, "error": f"Task {task_id!r} not found"}

    stopped = stop_executor(task_id)
    if stopped:
        tm.update(task_id, {"status": "stopped", "completed_at": datetime.now().isoformat()})
    return {"success": True, "task_id": task_id, "message": "Task stop requested"}


def cmd_list_tasks(params: dict[str, Any]) -> dict[str, Any]:
    from task_manager import get_task_manager
    status = params.get("status")
    tm = get_task_manager()
    tasks = tm.list_all(status)
    return {"success": True, "tasks": tasks, "total": len(tasks)}


def cmd_export_factor(params: dict[str, Any]) -> dict[str, Any]:
    from task_manager import get_task_manager
    from loops import extract_factors_from_result

    task_id   = params.get("task_id", "")
    factor_id = params.get("factor_id", "")
    fmt       = params.get("format", "python")

    tm = get_task_manager()
    result = tm.get_result(task_id) or {}
    factors = extract_factors_from_result(result)
    factor = next((f for f in factors if f["factor_id"] == factor_id), None)

    if not factor:
        return {"success": False, "error": f"Factor {factor_id!r} not found in task {task_id!r}"}

    code = factor.get("code", "")
    if fmt == "qlib":
        code = _to_qlib_format(factor)

    return {
        "success":   True,
        "factor_id": factor_id,
        "format":    fmt,
        "code":      code,
        "metadata": {
            "name":        factor.get("name"),
            "description": factor.get("description"),
            "ic":          factor.get("ic"),
            "sharpe":      factor.get("sharpe"),
        },
    }


def cmd_resume_task(params: dict[str, Any]) -> dict[str, Any]:
    """Task 12 — Resume a checkpointed task."""
    from config import apply_llm_config
    from task_manager import get_task_manager, TaskExecutor, register_executor
    from loops import build_factor_loop, build_model_loop, load_checkpoint

    task_id = params.get("task_id", "")
    llm_config = params.get("config", {})
    apply_llm_config(llm_config)

    tm = get_task_manager()
    task = tm.get(task_id)
    if not task:
        return {"success": False, "error": f"Task {task_id!r} not found"}

    checkpoint_path = task.get("checkpoint_path")
    if not checkpoint_path or not Path(checkpoint_path).exists():
        return {"success": False, "error": f"No checkpoint found for task {task_id!r}"}

    task_type      = task.get("type", "factor_mining")
    max_iterations = task.get("config", {}).get("max_iterations", 10)
    workspace      = task.get("workspace", str(Path(__file__).parent / "workspaces" / task_id))

    if task_type == "factor_mining":
        td = task.get("config", {}).get("task_description", "")
        market = task.get("config", {}).get("target_market", "US")
        target_ic = task.get("config", {}).get("target_ic", 0.05)
        def _factory():
            loop = build_factor_loop(workspace, td, market, target_ic)
            load_checkpoint(loop, checkpoint_path)
            return loop
    elif task_type == "model_optimization":
        mt = task.get("config", {}).get("model_type", "lightgbm")
        ot = task.get("config", {}).get("optimization_target", "sharpe")
        def _factory():
            loop = build_model_loop(workspace, mt, ot)
            load_checkpoint(loop, checkpoint_path)
            return loop
    else:
        return {"success": False, "error": f"Resume not supported for task type {task_type!r}"}

    tm.update(task_id, {"status": "created"})
    executor = TaskExecutor(task_id, tm, _factory, loop_n=max_iterations)
    register_executor(task_id, executor)
    executor.start()

    return {"success": True, "task_id": task_id, "message": "Task resumed from checkpoint"}


def cmd_extract_factors_from_pdf(params: dict[str, Any]) -> dict[str, Any]:
    """Task 13 — Extract factors from research PDFs using FactorReportLoop."""
    from config import apply_llm_config
    from task_manager import get_task_manager, TaskExecutor, register_executor
    from loops import build_report_loop, REPORT_LOOP_AVAILABLE

    if not REPORT_LOOP_AVAILABLE:
        return {"success": False, "error": "FactorReportLoop not available. Install rdagent report extras."}

    pdf_paths  = params.get("pdf_paths", [])
    llm_config = params.get("config", {})

    if not pdf_paths:
        return {"success": False, "error": "pdf_paths is required"}

    apply_llm_config(llm_config)

    task_id = f"pdf_{datetime.now().strftime('%Y%m%d_%H%M%S')}"
    workspace = str(Path(__file__).parent / "workspaces" / task_id)

    tm = get_task_manager()
    tm.create(task_id, "pdf_factor_extraction", {
        "pdf_paths":      pdf_paths,
        "max_iterations": len(pdf_paths),
    })

    def _factory():
        return build_report_loop(workspace, pdf_paths)

    executor = TaskExecutor(task_id, tm, _factory, loop_n=len(pdf_paths))
    register_executor(task_id, executor)
    executor.start()

    return {
        "success":  True,
        "task_id":  task_id,
        "status":   "started",
        "message":  f"FactorReportLoop started for {len(pdf_paths)} PDF(s)",
        "pdf_count": len(pdf_paths),
    }


def cmd_start_kaggle_task(params: dict[str, Any]) -> dict[str, Any]:
    """Task 14 — Run KaggleRDLoop for a competition."""
    from config import apply_llm_config
    from task_manager import get_task_manager, TaskExecutor, register_executor
    from loops import build_kaggle_loop, KAGGLE_LOOP_AVAILABLE

    if not KAGGLE_LOOP_AVAILABLE:
        return {"success": False, "error": "KaggleRDLoop not available. Install rdagent with kaggle extras."}

    competition    = params.get("competition", "")
    auto_submit    = bool(params.get("auto_submit", False))
    max_iterations = int(params.get("max_iterations", 10))
    llm_config     = params.get("config", {})

    if not competition:
        return {"success": False, "error": "competition name is required"}

    apply_llm_config(llm_config)

    task_id = f"kaggle_{competition}_{datetime.now().strftime('%Y%m%d_%H%M%S')}"
    workspace = str(Path(__file__).parent / "workspaces" / task_id)

    tm = get_task_manager()
    tm.create(task_id, "kaggle", {
        "competition":    competition,
        "auto_submit":    auto_submit,
        "max_iterations": max_iterations,
    })

    def _factory():
        return build_kaggle_loop(workspace, competition, auto_submit)

    executor = TaskExecutor(task_id, tm, _factory, loop_n=max_iterations)
    register_executor(task_id, executor)
    executor.start()

    return {
        "success":        True,
        "task_id":        task_id,
        "status":         "started",
        "competition":    competition,
        "max_iterations": max_iterations,
        "auto_submit":    auto_submit,
    }


def cmd_start_ds_task(params: dict[str, Any]) -> dict[str, Any]:
    """Task 14 — Run DataScienceRDLoop for a general DS task."""
    from config import apply_llm_config
    from task_manager import get_task_manager, TaskExecutor, register_executor
    from loops import build_ds_loop, DS_LOOP_AVAILABLE

    if not DS_LOOP_AVAILABLE:
        return {"success": False, "error": "DataScienceRDLoop not available. Check rdagent installation."}

    task_description = params.get("task_description", "")
    dataset_path     = params.get("dataset_path")
    trace_scheduler  = params.get("trace_scheduler", "round_robin")
    parallel_traces  = int(params.get("parallel_traces", 1))
    max_iterations   = int(params.get("max_iterations", 10))
    llm_config       = params.get("config", {})

    if not task_description:
        return {"success": False, "error": "task_description is required"}

    apply_llm_config(llm_config)

    task_id = f"ds_{datetime.now().strftime('%Y%m%d_%H%M%S')}"
    workspace = str(Path(__file__).parent / "workspaces" / task_id)

    tm = get_task_manager()
    tm.create(task_id, "data_science", {
        "task_description": task_description,
        "trace_scheduler":  trace_scheduler,
        "parallel_traces":  parallel_traces,
        "max_iterations":   max_iterations,
    })

    def _factory():
        return build_ds_loop(workspace, task_description, dataset_path, trace_scheduler, parallel_traces)

    executor = TaskExecutor(task_id, tm, _factory, loop_n=max_iterations)
    register_executor(task_id, executor)
    executor.start()

    return {
        "success":          True,
        "task_id":          task_id,
        "status":           "started",
        "trace_scheduler":  trace_scheduler,
        "parallel_traces":  parallel_traces,
        "max_iterations":   max_iterations,
    }


def cmd_start_multitrace_research(params: dict[str, Any]) -> dict[str, Any]:
    """Task 15 — Multi-trace MCTS parallel exploration."""
    from config import apply_llm_config
    from task_manager import get_task_manager, TaskExecutor, register_executor
    from loops import build_multitrace_loop, DS_LOOP_AVAILABLE

    if not DS_LOOP_AVAILABLE:
        return {"success": False, "error": "DataScienceRDLoop (multi-trace) not available."}

    task_description = params.get("task_description", params.get("research_goal", ""))
    trace_scheduler  = params.get("trace_scheduler", "mcts")
    parallel_traces  = int(params.get("parallel_traces", 3))
    max_iterations   = int(params.get("max_iterations", 10))
    dataset_path     = params.get("dataset_path")
    llm_config       = params.get("config", {})

    if not task_description:
        return {"success": False, "error": "task_description is required"}

    apply_llm_config(llm_config)

    task_id = f"mcts_{datetime.now().strftime('%Y%m%d_%H%M%S')}"
    workspace = str(Path(__file__).parent / "workspaces" / task_id)

    tm = get_task_manager()
    tm.create(task_id, "multitrace_mcts", {
        "task_description": task_description,
        "trace_scheduler":  trace_scheduler,
        "parallel_traces":  parallel_traces,
        "max_iterations":   max_iterations,
    })

    def _factory():
        return build_multitrace_loop(workspace, task_description, trace_scheduler, parallel_traces, dataset_path)

    executor = TaskExecutor(task_id, tm, _factory, loop_n=max_iterations)
    register_executor(task_id, executor)
    executor.start()

    return {
        "success":         True,
        "task_id":         task_id,
        "status":          "started",
        "trace_scheduler": trace_scheduler,
        "parallel_traces": parallel_traces,
        "max_iterations":  max_iterations,
        "message":         f"Multi-trace {trace_scheduler.upper()} exploration started with {parallel_traces} parallel traces",
    }


def cmd_start_mcp_server(params: dict[str, Any]) -> dict[str, Any]:
    """Task 20 — Start the Fincept MCP tool server for RD-Agent loop use."""
    try:
        from mcp_tools import start_mcp_server_process, MCP_AVAILABLE, mcp_availability
    except ImportError as e:
        return {"success": False, "error": f"mcp_tools not available: {e}"}

    if not MCP_AVAILABLE:
        return {
            "success": False,
            "error":   "MCP not available. Install: pip install mcp[cli] pydantic-ai[mcp] yfinance",
            "install": "pip install 'mcp[cli]' 'pydantic-ai[mcp]' yfinance requests",
        }

    port = int(params.get("port", 18765))
    proc, toolset = start_mcp_server_process(port=port)
    avail = mcp_availability()

    return {
        "success":  toolset is not None,
        "url":      f"http://127.0.0.1:{avail['running_ports'][0] if avail['running_ports'] else port}/mcp",
        "port":     avail["running_ports"][0] if avail["running_ports"] else port,
        "pid":      proc.pid if proc else None,
        "tools":    avail["tools"],
        "message":  "Fincept MCP server started" if toolset else "MCP server failed to start",
        "already_running": proc is None and toolset is not None,
    }


def cmd_stop_mcp_server(params: dict[str, Any]) -> dict[str, Any]:
    """Stop the Fincept MCP server on the given port."""
    try:
        from mcp_tools import stop_mcp_server_process
    except ImportError as e:
        return {"success": False, "error": str(e)}
    port = int(params.get("port", 18765))
    stopped = stop_mcp_server_process(port)
    return {"success": True, "stopped": stopped, "port": port}


def cmd_mcp_status(_params: dict[str, Any]) -> dict[str, Any]:
    """Return MCP availability and running server info."""
    try:
        from mcp_tools import mcp_availability
        avail = mcp_availability()
        return {"success": True, **avail}
    except ImportError:
        return {
            "success":              False,
            "mcp_server_available": False,
            "pydantic_ai_mcp":      False,
            "error": "mcp_tools module not importable",
        }


def cmd_start_ui(params: dict[str, Any]) -> dict[str, Any]:
    """Task 19 — Launch rdagent's built-in Streamlit log viewer as a background subprocess."""
    import subprocess
    import socket

    port = int(params.get("port", 19999))

    # Check if port is free; if not, increment
    for attempt in range(10):
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            if s.connect_ex(("127.0.0.1", port)) != 0:
                break  # port is free
            port += 1
    else:
        return {"success": False, "error": "No free port found in range 19999-20009"}

    try:
        proc = subprocess.Popen(
            ["python", "-m", "rdagent.app.streamlit", "--server.port", str(port),
             "--server.headless", "true"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            start_new_session=True,
        )
        url = f"http://localhost:{port}"
        logger.info("RD-Agent Streamlit UI started at %s (pid=%s)", url, proc.pid)
        return {
            "success": True,
            "url":     url,
            "port":    port,
            "pid":     proc.pid,
            "message": f"Log viewer started at {url}",
        }
    except FileNotFoundError:
        # Fallback: try `rdagent ui` CLI entry point
        try:
            proc = subprocess.Popen(
                ["rdagent", "ui", "--port", str(port)],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                start_new_session=True,
            )
            url = f"http://localhost:{port}"
            return {"success": True, "url": url, "port": port, "pid": proc.pid,
                    "message": f"Log viewer started at {url}"}
        except FileNotFoundError:
            return {"success": False, "error": "rdagent UI not available. Install rdagent and streamlit."}
    except Exception as e:
        return {"success": False, "error": str(e)}


def _to_qlib_format(factor: dict[str, Any]) -> str:
    name = (factor.get("name") or "CustomFactor").replace(" ", "")
    return (
        f'from qlib.data.dataset.handler import DataHandlerLP\n\n'
        f'class {name}Handler(DataHandlerLP):\n'
        f'    """\n'
        f'    {factor.get("description", "")}\n'
        f'    IC: {factor.get("ic", "N/A")}\n'
        f'    Sharpe: {factor.get("sharpe", "N/A")}\n'
        f'    """\n\n'
        f'{factor.get("code", "")}\n'
    )


# ---------------------------------------------------------------------------
# Command dispatch
# ---------------------------------------------------------------------------

_COMMANDS = {
    # Status / capabilities
    "check_status":                  cmd_check_status,
    "get_capabilities":              cmd_get_capabilities,
    # Factor mining
    "start_factor_mining":           cmd_start_factor_mining,
    # Model optimization
    "start_model_optimization":      cmd_start_model_optimization,
    # Quant research (factor + model combined)
    "start_quant_research":          cmd_start_quant_research,
    "run_autonomous_research":       cmd_start_quant_research,       # alias
    # PDF factor extraction (Task 13)
    "extract_factors_from_pdf":      cmd_extract_factors_from_pdf,
    # Kaggle / DataScience (Task 14)
    "start_kaggle_task":             cmd_start_kaggle_task,
    "start_ds_task":                 cmd_start_ds_task,
    # Multi-trace MCTS (Task 15)
    "start_multitrace_research":     cmd_start_multitrace_research,
    # Task lifecycle
    "get_task_status":               cmd_get_task_status,
    "get_factor_mining_status":      cmd_get_task_status,            # alias
    "get_model_optimization_status": cmd_get_task_status,            # alias
    "get_discovered_factors":        cmd_get_discovered_factors,
    "get_optimized_model":           cmd_get_optimized_model,
    "stop_task":                     cmd_stop_task,
    "list_tasks":                    cmd_list_tasks,
    # Checkpoint resume (Task 12)
    "resume_task":                   cmd_resume_task,
    # Export
    "export_factor":                 cmd_export_factor,
    # Streamlit UI log viewer (Task 19)
    "start_ui":                      cmd_start_ui,
    # MCP tool server (Task 20)
    "start_mcp_server":              cmd_start_mcp_server,
    "stop_mcp_server":               cmd_stop_mcp_server,
    "mcp_status":                    cmd_mcp_status,
}


def main() -> None:
    if len(sys.argv) < 2:
        _err("No command specified. Usage: cli.py <command> [json_params]")

    command = sys.argv[1]
    handler = _COMMANDS.get(command)
    if not handler:
        _err(f"Unknown command: {command!r}. Available: {list(_COMMANDS)}")

    # Parse params — positional arg 2 or empty dict
    params: dict[str, Any] = {}
    if len(sys.argv) >= 3:
        try:
            params = json.loads(sys.argv[2])
        except json.JSONDecodeError as e:
            _err(f"Invalid JSON params: {e}")

    # For commands that take a bare task_id as second arg
    if not params and len(sys.argv) >= 3:
        params = {"task_id": sys.argv[2]}

    try:
        result = handler(params)
        _out(result)
    except Exception as exc:
        logger.exception("Command %r failed", command)
        _err(str(exc))


if __name__ == "__main__":
    main()
