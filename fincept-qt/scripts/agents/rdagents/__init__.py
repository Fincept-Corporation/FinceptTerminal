"""
Fincept Terminal — RD-Agent Integration
Built on rdagent 0.8.0 (Microsoft's autonomous R&D agent framework)

Entry point: cli.py (driven by AIQuantLabService). Long tasks run one loop per
child process via task_runner.run_loop_sync.
"""

from config import apply_llm_config, clear_llm_config
from loops import (
    build_factor_loop,
    build_model_loop,
    build_quant_loop,
    loop_availability,
    extract_factors_from_result,
    extract_models_from_result,
    save_checkpoint,
    load_checkpoint,
    FACTOR_LOOP_AVAILABLE,
    MODEL_LOOP_AVAILABLE,
    QUANT_LOOP_AVAILABLE,
)
from task_runner import run_loop_sync
from mcp_tools import (
    MCP_AVAILABLE,
    PAI_MCP_AVAILABLE,
    get_mcp_toolset,
    start_mcp_server_process,
    stop_mcp_server_process,
    inject_mcp_into_loop,
    mcp_availability,
)

__all__ = [
    # Config
    "apply_llm_config",
    "clear_llm_config",
    # Loop builders
    "build_factor_loop",
    "build_model_loop",
    "build_quant_loop",
    # Loop status
    "loop_availability",
    "FACTOR_LOOP_AVAILABLE",
    "MODEL_LOOP_AVAILABLE",
    "QUANT_LOOP_AVAILABLE",
    # Result extraction / checkpoints
    "extract_factors_from_result",
    "extract_models_from_result",
    "save_checkpoint",
    "load_checkpoint",
    # Task execution
    "run_loop_sync",
    # MCP tool server
    "MCP_AVAILABLE",
    "PAI_MCP_AVAILABLE",
    "get_mcp_toolset",
    "start_mcp_server_process",
    "stop_mcp_server_process",
    "inject_mcp_into_loop",
    "mcp_availability",
]
