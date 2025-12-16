"""
Path Resolver - Uses Rust-provided base paths

This module provides path resolution utilities that work with the Tauri
Rust backend's python.rs path resolution system.
"""

import os
from pathlib import Path
from typing import Optional


class PathResolver:
    """Resolves paths for agents, configs, and resources"""

    def __init__(self, base_path: Optional[str] = None):
        """
        Initialize path resolver

        Args:
            base_path: Base path provided by Rust. If None, auto-detect.
        """
        if base_path:
            self.base_path = Path(base_path)
        else:
            # Auto-detect: go up from this file to agents directory
            self.base_path = Path(__file__).parent.parent.parent

        self.finagents_path = self.base_path / "finagents"
        self.configs_path = self.base_path / "configs"

    def get_config_path(self, config_name: str) -> Path:
        """Get path to a configuration file"""
        return self.configs_path / config_name

    def get_agent_config_path(self, agent_category: str) -> Path:
        """
        Get path to agent configuration JSON

        Args:
            agent_category: e.g., "GeopoliticsAgents", "EconomicAgents"
        """
        return self.base_path / agent_category / "configs" / "agent_definitions.json"

    def get_database_path(self, db_name: str = "agents.db") -> Path:
        """Get path to database file"""
        db_path = self.finagents_path / "database" / db_name
        db_path.parent.mkdir(parents=True, exist_ok=True)
        return db_path

    def get_logs_path(self) -> Path:
        """Get path to logs directory"""
        logs_path = self.finagents_path / "logs"
        logs_path.mkdir(parents=True, exist_ok=True)
        return logs_path

    @staticmethod
    def get_base_path_from_rust() -> Optional[str]:
        """
        Get base path from Rust via environment variable

        When called from Tauri, Rust should set AGENTS_BASE_PATH env var
        """
        return os.getenv("AGENTS_BASE_PATH")

    @classmethod
    def from_rust(cls) -> "PathResolver":
        """Create PathResolver using Rust-provided base path"""
        base_path = cls.get_base_path_from_rust()
        return cls(base_path=base_path)
