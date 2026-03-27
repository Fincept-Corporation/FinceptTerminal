"""
Fincept Terminal — RD-Agent Integration
Built on rdagent 0.8.0 (Microsoft's autonomous R&D agent framework)

Entry point: cli.py
"""

from config import apply_llm_config, clear_llm_config
from loops import (
    build_factor_loop,
    build_model_loop,
    build_quant_loop,
    build_report_loop,
    build_kaggle_loop,
    build_ds_loop,
    build_multitrace_loop,
    loop_availability,
    extract_factors_from_result,
    extract_models_from_result,
    FACTOR_LOOP_AVAILABLE,
    MODEL_LOOP_AVAILABLE,
    QUANT_LOOP_AVAILABLE,
    REPORT_LOOP_AVAILABLE,
    KAGGLE_LOOP_AVAILABLE,
    DS_LOOP_AVAILABLE,
)
from task_manager import (
    TaskManager,
    TaskExecutor,
    get_task_manager,
    get_executor,
    register_executor,
    stop_executor,
)
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
    "build_report_loop",
    "build_kaggle_loop",
    "build_ds_loop",
    "build_multitrace_loop",
    # Loop status
    "loop_availability",
    "FACTOR_LOOP_AVAILABLE",
    "MODEL_LOOP_AVAILABLE",
    "QUANT_LOOP_AVAILABLE",
    "REPORT_LOOP_AVAILABLE",
    "KAGGLE_LOOP_AVAILABLE",
    "DS_LOOP_AVAILABLE",
    # Result extraction
    "extract_factors_from_result",
    "extract_models_from_result",
    # Task management
    "TaskManager",
    "TaskExecutor",
    "get_task_manager",
    "get_executor",
    "register_executor",
    "stop_executor",
    # MCP tool server
    "MCP_AVAILABLE",
    "PAI_MCP_AVAILABLE",
    "get_mcp_toolset",
    "start_mcp_server_process",
    "stop_mcp_server_process",
    "inject_mcp_into_loop",
    "mcp_availability",
]
