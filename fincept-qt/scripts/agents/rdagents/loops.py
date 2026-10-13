"""
loops.py — Real FactorRDLoop and ModelRDLoop execution for Fincept rdagents.

Responsibilities:
  - build_factor_loop(): configure and return a FactorRDLoop instance
  - build_model_loop(): configure and return a ModelRDLoop instance
  - build_quant_loop(): configure and return a QuantRDLoop instance
  - extract_factors_from_result(): parse final results into serializable dicts
  - extract_models_from_result(): parse model results into serializable dicts

All loops use the LiteLLM backend — config.apply_llm_config() must be called
before any loop is built so env vars are set correctly.
"""

from __future__ import annotations

import logging
import os
from pathlib import Path
from typing import Any

logger = logging.getLogger(__name__)

# ---------------------------------------------------------------------------
# Availability flags — checked at import time
# ---------------------------------------------------------------------------

FACTOR_LOOP_AVAILABLE  = False
MODEL_LOOP_AVAILABLE   = False
QUANT_LOOP_AVAILABLE   = False

try:
    from rdagent.app.qlib_rd_loop.factor import FactorRDLoop
    FACTOR_LOOP_AVAILABLE = True
except ImportError as e:
    logger.warning("FactorRDLoop not available: %s", e)
    FactorRDLoop = None  # type: ignore

try:
    from rdagent.app.qlib_rd_loop.model import ModelRDLoop
    MODEL_LOOP_AVAILABLE = True
except ImportError as e:
    logger.warning("ModelRDLoop not available: %s", e)
    ModelRDLoop = None  # type: ignore

try:
    from rdagent.app.qlib_rd_loop.quant import QuantRDLoop
    QUANT_LOOP_AVAILABLE = True
except ImportError as e:
    logger.debug("QuantRDLoop not available: %s", e)
    QuantRDLoop = None  # type: ignore

# ---------------------------------------------------------------------------
# MCP injection helper (lazy import to avoid hard dependency)
# ---------------------------------------------------------------------------

def _try_inject_mcp(loop: Any, mcp_port: int) -> bool:
    """Start MCP server if needed and inject toolset into loop. Returns True on success."""
    try:
        from mcp_tools import start_mcp_server_process, inject_mcp_into_loop, MCP_AVAILABLE
        if not MCP_AVAILABLE:
            logger.warning("MCP not available — skipping tool injection")
            return False
        _proc, toolset = start_mcp_server_process(port=mcp_port)
        if toolset is None:
            logger.warning("MCP server failed to start — skipping tool injection")
            return False
        ok = inject_mcp_into_loop(loop, toolset)
        if ok:
            logger.info("MCP tools injected into loop (port=%d)", mcp_port)
        return ok
    except Exception as e:
        logger.warning("MCP injection failed (non-fatal): %s", e)
        return False


# ---------------------------------------------------------------------------
# Loop builders
# ---------------------------------------------------------------------------

def build_factor_loop(
    workspace_dir: str,
    task_description: str,
    target_market: str = "US",
    target_ic: float = 0.05,
    enable_mcp: bool = False,
    mcp_port: int = 18765,
) -> Any:
    """
    Build a configured FactorRDLoop instance.

    Args:
        workspace_dir:    Path where the loop stores code, checkpoints, logs.
        task_description: Natural language description of factors to discover.
        target_market:    'US', 'CN', or 'CRYPTO'.
        target_ic:        Target Information Coefficient to achieve.

    Returns:
        FactorRDLoop instance ready to call .run(loop_n=N)
    """
    if not FACTOR_LOOP_AVAILABLE:
        raise RuntimeError("FactorRDLoop is not available. Check rdagent installation.")

    os.makedirs(workspace_dir, exist_ok=True)

    # FactorRDLoop reads PROP_SETTING from env/config — set workspace
    os.environ["FACTOR_PROP_SETTING__scen__market"] = target_market
    os.environ["FACTOR_PROP_SETTING__hypothesis_gen__target_ic"] = str(target_ic)

    loop = FactorRDLoop(
        scen=_get_factor_scenario(task_description, target_market),
    )

    # Override workspace if the loop supports it
    if hasattr(loop, "set_workspace"):
        loop.set_workspace(workspace_dir)
    elif hasattr(loop, "workspace"):
        loop.workspace = workspace_dir

    if enable_mcp:
        _try_inject_mcp(loop, mcp_port)

    logger.info("Built FactorRDLoop for market=%s target_ic=%.3f mcp=%s",
                target_market, target_ic, enable_mcp)
    return loop


def build_model_loop(
    workspace_dir: str,
    model_type: str = "lightgbm",
    optimization_target: str = "sharpe",
    enable_mcp: bool = False,
    mcp_port: int = 18765,
) -> Any:
    """
    Build a configured ModelRDLoop instance.

    Args:
        workspace_dir:        Path where the loop stores code, checkpoints, logs.
        model_type:           Model to optimize: lightgbm, xgboost, lstm, transformer, etc.
        optimization_target:  Metric to optimize: sharpe, ic, max_drawdown, win_rate.

    Returns:
        ModelRDLoop instance ready to call .run(loop_n=N)
    """
    if not MODEL_LOOP_AVAILABLE:
        raise RuntimeError("ModelRDLoop is not available. Check rdagent installation.")

    os.makedirs(workspace_dir, exist_ok=True)

    os.environ["MODEL_PROP_SETTING__scen__model_type"] = model_type
    os.environ["MODEL_PROP_SETTING__scen__optimization_target"] = optimization_target

    loop = ModelRDLoop(
        scen=_get_model_scenario(model_type, optimization_target),
    )

    if hasattr(loop, "set_workspace"):
        loop.set_workspace(workspace_dir)
    elif hasattr(loop, "workspace"):
        loop.workspace = workspace_dir

    if enable_mcp:
        _try_inject_mcp(loop, mcp_port)

    logger.info("Built ModelRDLoop for model=%s target=%s mcp=%s",
                model_type, optimization_target, enable_mcp)
    return loop


def build_quant_loop(
    workspace_dir: str,
    task_description: str,
    target_market: str = "US",
    enable_mcp: bool = False,
    mcp_port: int = 18765,
) -> Any:
    """Build a QuantRDLoop (combined factor + model discovery)."""
    if not QUANT_LOOP_AVAILABLE:
        raise RuntimeError("QuantRDLoop is not available. Check rdagent installation.")

    os.makedirs(workspace_dir, exist_ok=True)
    loop = QuantRDLoop()
    if hasattr(loop, "workspace"):
        loop.workspace = workspace_dir

    if enable_mcp:
        _try_inject_mcp(loop, mcp_port)

    logger.info("Built QuantRDLoop market=%s mcp=%s", target_market, enable_mcp)
    return loop


# ---------------------------------------------------------------------------
# Checkpoint helpers (Task 12)
# ---------------------------------------------------------------------------

def save_checkpoint(loop: Any, checkpoint_path: str) -> bool:
    """Persist loop state to disk via LoopBase.dump()."""
    try:
        if hasattr(loop, "dump"):
            loop.dump(checkpoint_path)
            logger.info("Checkpoint saved: %s", checkpoint_path)
            return True
    except Exception as exc:
        logger.warning("Checkpoint save failed: %s", exc)
    return False


def load_checkpoint(loop: Any, checkpoint_path: str) -> bool:
    """Restore loop state from disk via LoopBase.load()."""
    try:
        if hasattr(loop, "load") and Path(checkpoint_path).exists():
            loop.load(checkpoint_path)
            logger.info("Checkpoint loaded: %s", checkpoint_path)
            return True
    except Exception as exc:
        logger.warning("Checkpoint load failed: %s", exc)
    return False


# ---------------------------------------------------------------------------
# Scenario helpers — try real scenario classes, fall back gracefully
# ---------------------------------------------------------------------------

def _get_factor_scenario(task_description: str, market: str) -> Any | None:
    try:
        from rdagent.scenarios.qlib.experiment.factor_experiment import QlibFactorScenario
        scen = QlibFactorScenario()
        if hasattr(scen, "background"):
            scen.background = task_description
        return scen
    except Exception as e:
        logger.debug("Could not build QlibFactorScenario: %s", e)
        return None


def _get_model_scenario(model_type: str, target: str) -> Any | None:
    try:
        from rdagent.scenarios.qlib.experiment.model_experiment import QlibModelScenario
        scen = QlibModelScenario()
        return scen
    except Exception as e:
        logger.debug("Could not build QlibModelScenario: %s", e)
        return None


# ---------------------------------------------------------------------------
# Result extraction helpers
# ---------------------------------------------------------------------------

def extract_factors_from_result(result: dict[str, Any]) -> list[dict[str, Any]]:
    """
    Convert raw loop result dict into clean serializable factor list.
    Filters out entries with no code or no name.
    """
    raw = result.get("factors", [])
    clean = []
    for f in raw:
        if not f.get("name") and not f.get("factor_id"):
            continue
        clean.append({
            "factor_id":   f.get("factor_id", f.get("name", "unknown")),
            "name":        f.get("name", f.get("factor_id", "Unknown Factor")),
            "description": f.get("description", ""),
            "code":        f.get("code", ""),
            "ic":          f.get("ic"),
            "sharpe":      f.get("sharpe"),
            "iteration":   f.get("iteration", 0),
            "performance_metrics": {
                "ic":     f.get("ic"),
                "sharpe": f.get("sharpe"),
            },
        })
    return clean


def extract_models_from_result(result: dict[str, Any]) -> list[dict[str, Any]]:
    """Convert raw loop result dict into clean serializable model list."""
    raw = result.get("models", []) or result.get("factors", [])
    clean = []
    for m in raw:
        clean.append({
            "model_id":    m.get("factor_id", m.get("name", "unknown")),
            "name":        m.get("name", "Unknown Model"),
            "description": m.get("description", ""),
            "code":        m.get("code", ""),
            "sharpe":      m.get("sharpe"),
            "ic":          m.get("ic"),
        })
    return clean


# ---------------------------------------------------------------------------
# Status summary
# ---------------------------------------------------------------------------

def loop_availability() -> dict[str, Any]:
    avail: dict[str, Any] = {
        "factor_loop":  FACTOR_LOOP_AVAILABLE,
        "model_loop":   MODEL_LOOP_AVAILABLE,
        "quant_loop":   QUANT_LOOP_AVAILABLE,
    }
    try:
        from mcp_tools import mcp_availability
        avail["mcp"] = mcp_availability()
    except Exception:
        avail["mcp"] = {"mcp_server_available": False}
    return avail
