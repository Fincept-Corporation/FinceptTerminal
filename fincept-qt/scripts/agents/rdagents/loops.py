"""
loops.py — Real FactorRDLoop / ModelRDLoop / QuantRDLoop construction for Fincept rdagents.

Written against rdagent 0.8.0, where every loop is constructed from a
PROP_SETTING object (`RDLoop.__init__(self, PROP_SETTING)`); the scenario,
hypothesis generator, coder, runner and summarizer are all built by the loop
from that setting. rdagent reads its settings (LLM model, workspace, log and
data folders) from environment variables and the current directory AT IMPORT
TIME, so cli.py chdirs into the task workspace and applies the LLM config
before importing this module for a run.

Responsibilities:
  - preflight(): what a run of a given kind needs that this machine lacks
    (checked before anything is constructed: constructing a Qlib scenario
    already shells out to conda / Docker)
  - build_factor_loop() / build_model_loop() / build_quant_loop()
  - load_loop_checkpoint(): resume a loop from a LoopBase.dump() pickle
  - extract_factors_from_result() / extract_models_from_result()
"""

from __future__ import annotations

import logging
import os
import shutil
import sys
from pathlib import Path
from typing import Any

logger = logging.getLogger(__name__)

# ---------------------------------------------------------------------------
# Availability flags — checked at import time
# ---------------------------------------------------------------------------

FACTOR_LOOP_AVAILABLE = False
MODEL_LOOP_AVAILABLE = False
QUANT_LOOP_AVAILABLE = False
_IMPORT_ERRORS: dict[str, str] = {}

try:
    from rdagent.app.qlib_rd_loop.factor import FactorRDLoop
    FACTOR_LOOP_AVAILABLE = True
except Exception as e:  # ImportError, or a settings error raised while importing
    logger.warning("FactorRDLoop not available: %s", e)
    _IMPORT_ERRORS["factor_mining"] = str(e)
    FactorRDLoop = None  # type: ignore

try:
    from rdagent.app.qlib_rd_loop.model import ModelRDLoop
    MODEL_LOOP_AVAILABLE = True
except Exception as e:
    logger.warning("ModelRDLoop not available: %s", e)
    _IMPORT_ERRORS["model_optimization"] = str(e)
    ModelRDLoop = None  # type: ignore

try:
    from rdagent.app.qlib_rd_loop.quant import QuantRDLoop
    QUANT_LOOP_AVAILABLE = True
except Exception as e:
    logger.debug("QuantRDLoop not available: %s", e)
    _IMPORT_ERRORS["quant_research"] = str(e)
    QuantRDLoop = None  # type: ignore


def _prop_setting(kind: str) -> Any:
    """The module-level PROP_SETTING rdagent's own CLI uses for this loop kind."""
    from rdagent.app.qlib_rd_loop import conf
    return {
        "factor_mining": conf.FACTOR_PROP_SETTING,
        "model_optimization": conf.MODEL_PROP_SETTING,
        "quant_research": conf.QUANT_PROP_SETTING,
    }[kind]


def loop_class(kind: str) -> Any:
    return {"factor_mining": FactorRDLoop, "model_optimization": ModelRDLoop,
            "quant_research": QuantRDLoop}[kind]


# ---------------------------------------------------------------------------
# Preflight — what a run needs that is not here
# ---------------------------------------------------------------------------

# Providers that run locally and need no API key (a base_url is enough).
_KEYLESS_PROVIDERS = {"ollama", "lmstudio", "lm_studio", "local", "vllm", "llamacpp"}


def _env_ci(name: str) -> str | None:
    """Case-insensitive env lookup (pydantic-settings matches env names case-insensitively)."""
    want = name.lower()
    for k, v in os.environ.items():
        if k.lower() == want:
            return v
    return None


def model_env_type() -> str:
    """Where rdagent runs the Qlib backtest: 'conda' (default) or 'docker' (MODEL_CoSTEER_ENV_TYPE)."""
    return (_env_ci("MODEL_CoSTEER_ENV_TYPE") or "conda").strip().lower()


def factor_data_folders() -> list[Path]:
    """The factor source-data folders rdagent's Qlib factor scenario reads (relative to cwd)."""
    data = _env_ci("FACTOR_CoSTEER_DATA_FOLDER") or "git_ignore_folder/factor_implementation_source_data"
    debug = (_env_ci("FACTOR_CoSTEER_DATA_FOLDER_DEBUG")
             or "git_ignore_folder/factor_implementation_source_data_debug")
    return [Path(data), Path(debug)]


def docker_status() -> tuple[bool, str]:
    """(reachable, detail) for the Docker daemon, via the docker SDK rdagent itself uses."""
    try:
        import docker  # type: ignore
    except Exception as e:
        return False, f"the 'docker' Python package is not importable ({e})"
    try:
        client = docker.from_env(timeout=5)
        client.ping()
        return True, "Docker daemon reachable"
    except Exception as e:
        cli = "docker CLI found" if shutil.which("docker") else "no docker CLI on PATH"
        return False, f"Docker daemon not reachable ({cli}): {str(e)[:160]}"


def runtime_requirements() -> dict[str, Any]:
    """Machine-level facts every RD-Agent run depends on (no LLM config needed)."""
    docker_ok, docker_detail = docker_status()
    folders = factor_data_folders()
    return {
        "platform": sys.platform,
        "platform_supported": os.name != "nt",
        "conda": bool(shutil.which("conda")),
        "conda_default_env": os.environ.get("CONDA_DEFAULT_ENV") or None,
        "docker": docker_ok,
        "docker_detail": docker_detail,
        "model_env_type": model_env_type(),
        "factor_data_ready": all(p.exists() for p in folders),
        "factor_data_folder": str(folders[0].resolve()),
    }


def preflight(kind: str, llm_config: dict[str, Any] | None = None,
              req: dict[str, Any] | None = None) -> list[str]:
    """
    Everything a run of `kind` needs that is missing, as user-readable lines
    (empty list = nothing known to be missing). `llm_config` None skips the
    LLM check (check_status has no config).
    """
    missing: list[str] = []
    if not {"factor_mining": FACTOR_LOOP_AVAILABLE, "model_optimization": MODEL_LOOP_AVAILABLE,
            "quant_research": QUANT_LOOP_AVAILABLE}.get(kind, False):
        missing.append(f"rdagent's {kind} loop cannot be imported: "
                       f"{_IMPORT_ERRORS.get(kind, 'rdagent is not installed')}")
        return missing

    r = req if req is not None else runtime_requirements()
    if not r["platform_supported"]:
        missing.append("a Linux (or macOS) machine: rdagent 0.8.0 runs generated code through "
                       "rdagent.utils.env.LocalEnv, which builds PATH with ':' and /bin, /usr/bin "
                       "and cannot work on Windows (use WSL)")

    if llm_config is not None:
        provider = str(llm_config.get("llm_provider") or "").lower()
        if not provider:
            missing.append("an LLM: no provider is configured for AI Quant Lab (Settings > LLM Config)")
        elif not llm_config.get("llm_api_key") and not (
                provider in _KEYLESS_PROVIDERS and llm_config.get("llm_base_url")):
            missing.append(f"an API key for the LLM provider '{provider}' (Settings > LLM Config)")

    needs_factor_env = kind in ("factor_mining", "quant_research")
    if needs_factor_env:
        if not r["conda"]:
            missing.append("conda on PATH: the factor coder runs generated factor code in a conda "
                           "environment (rdagent CondaConf)")
        if not r["conda_default_env"]:
            missing.append("CONDA_DEFAULT_ENV: start the terminal from an activated conda environment "
                           "(rdagent's factor coder runs code in that env; without it construction "
                           "fails with 'conda_env_name: Input should be a valid string')")
        if not r["factor_data_ready"] and not r["docker"]:
            where = f" (not found at {r['factor_data_folder']})" if r.get("factor_data_folder") else ""
            missing.append(f"Docker, once, to build the Qlib image and generate the factor source data"
                           f"{where}; {r['docker_detail']}")

    if r["model_env_type"] == "docker":
        if not r["docker"]:
            missing.append(f"Docker for the Qlib backtest (MODEL_CoSTEER_ENV_TYPE=docker); {r['docker_detail']}")
    elif r["model_env_type"] == "conda":
        if not r["conda"] and not needs_factor_env:  # factor kinds already reported conda
            missing.append("conda on PATH: the Qlib backtest runs in the conda env 'rdagent4qlib' "
                           "(created on first use), or set MODEL_CoSTEER_ENV_TYPE=docker to use Docker")
    else:
        missing.append(f"a valid MODEL_CoSTEER_ENV_TYPE ('conda' or 'docker'), got {r['model_env_type']!r}")
    return missing


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

def _add_user_focus(loop: Any, lines: list[str]) -> bool:
    """
    Append the user's research focus to the scenario background every rdagent
    component prompts with. The Factor and Model scenarios keep it in
    `_background` (shared by hypothesis_gen, coder, runner, summarizer, trace).
    Returns False when the scenario has no such field (QlibQuantScenario builds
    its background on every call).
    """
    lines = [ln for ln in lines if ln]
    if not lines:
        return True
    scen = getattr(getattr(loop, "trace", None), "scen", None)
    if scen is None or not isinstance(getattr(scen, "_background", None), str):
        return False
    scen._background += "\n\nResearch focus requested by the user:\n" + "\n".join(lines)
    return True


def build_factor_loop(
    task_description: str,
    target_market: str = "",
    target_ic: float | None = None,
    enable_mcp: bool = False,
    mcp_port: int = 18765,
) -> tuple[Any, list[str]]:
    """
    Build FactorRDLoop(FACTOR_PROP_SETTING). Returns (loop, ignored_params).

    The task description is added to the scenario background. rdagent's Qlib
    factor scenario always uses its China A-share (csi300) dataset and has no
    IC target, so target_market and target_ic cannot be applied.
    """
    if not FACTOR_LOOP_AVAILABLE:
        raise RuntimeError("FactorRDLoop is not available. Check rdagent installation.")
    loop = FactorRDLoop(_prop_setting("factor_mining"))
    ignored = []
    if not _add_user_focus(loop, [task_description]):
        ignored.append("task_description")
    if target_market:
        ignored.append(f"target_market={target_market} (rdagent's Qlib scenario uses its China A-share csi300 data)")
    if target_ic is not None:
        ignored.append(f"target_ic={target_ic} (rdagent has no IC target; it reports the IC it reaches)")
    if enable_mcp:
        _try_inject_mcp(loop, mcp_port)
    logger.info("Built FactorRDLoop mcp=%s", enable_mcp)
    return loop, ignored


def build_model_loop(
    model_type: str = "",
    optimization_target: str = "",
    enable_mcp: bool = False,
    mcp_port: int = 18765,
) -> tuple[Any, list[str]]:
    """Build ModelRDLoop(MODEL_PROP_SETTING). Returns (loop, ignored_params)."""
    if not MODEL_LOOP_AVAILABLE:
        raise RuntimeError("ModelRDLoop is not available. Check rdagent installation.")
    loop = ModelRDLoop(_prop_setting("model_optimization"))
    focus = []
    if model_type:
        focus.append(f"Preferred model family: {model_type}.")
    if optimization_target:
        focus.append(f"Optimise primarily for: {optimization_target}.")
    ignored = [] if _add_user_focus(loop, focus) else ["model_type", "optimization_target"]
    if enable_mcp:
        _try_inject_mcp(loop, mcp_port)
    logger.info("Built ModelRDLoop model=%s target=%s mcp=%s", model_type, optimization_target, enable_mcp)
    return loop, ignored


def build_quant_loop(
    research_goal: str = "",
    target_market: str = "",
    enable_mcp: bool = False,
    mcp_port: int = 18765,
) -> tuple[Any, list[str]]:
    """Build QuantRDLoop(QUANT_PROP_SETTING). Returns (loop, ignored_params)."""
    if not QUANT_LOOP_AVAILABLE:
        raise RuntimeError("QuantRDLoop is not available. Check rdagent installation.")
    loop = QuantRDLoop(_prop_setting("quant_research"))
    ignored = []
    if research_goal and not _add_user_focus(loop, [research_goal]):
        ignored.append("research_goal (rdagent's quant scenario builds its background itself)")
    if target_market:
        ignored.append(f"target_market={target_market} (rdagent's Qlib scenario uses its China A-share csi300 data)")
    if enable_mcp:
        _try_inject_mcp(loop, mcp_port)
    logger.info("Built QuantRDLoop mcp=%s", enable_mcp)
    return loop, ignored


# ---------------------------------------------------------------------------
# Checkpoint helpers
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


def load_loop_checkpoint(kind: str, checkpoint_path: str) -> Any:
    """
    The loop restored from a LoopBase.dump() pickle. In rdagent 0.8.0 `load` is
    a classmethod that RETURNS the restored loop (it does not mutate an
    instance), so no fresh loop is constructed for a resume.
    """
    cls = loop_class(kind)
    if cls is None:
        raise RuntimeError(f"The RD-Agent {kind} loop is not available.")
    if not Path(checkpoint_path).exists():
        raise FileNotFoundError(f"Checkpoint not found: {checkpoint_path}")
    loop = cls.load(checkpoint_path, checkout=False)
    logger.info("Checkpoint loaded: %s", checkpoint_path)
    return loop


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
            "accepted":    f.get("accepted"),
            "performance_metrics": {
                "ic":     f.get("ic"),
                "sharpe": f.get("sharpe"),
                "annualized_return": f.get("annualized_return"),
                "max_drawdown": f.get("max_drawdown"),
            },
        })
    return clean


def extract_models_from_result(result: dict[str, Any]) -> list[dict[str, Any]]:
    """Convert raw loop result dict into clean serializable model list."""
    raw = result.get("models", [])
    clean = []
    for m in raw:
        clean.append({
            "model_id":    m.get("model_id", m.get("name", "unknown")),
            "name":        m.get("name", "Unknown Model"),
            "model_type":  m.get("model_type"),
            "description": m.get("description", ""),
            "code":        m.get("code", ""),
            "sharpe":      m.get("sharpe"),
            "ic":          m.get("ic"),
            "iteration":   m.get("iteration", 0),
            "accepted":    m.get("accepted"),
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
