"""
Config Loader - Load tab-specific agent configurations

Loads JSON configs for different terminal tabs and validates them.
"""

import json
from pathlib import Path
from typing import Dict, Any, Optional


class ConfigLoader:
    """Load and manage tab-specific agent configurations"""

    def __init__(self, config_dir: Optional[Path] = None):
        """
        Initialize config loader

        Args:
            config_dir: Directory containing config files (default: ./configs/)
        """
        if config_dir is None:
            config_dir = Path(__file__).parent / "configs"

        self.config_dir = Path(config_dir)
        self.config_dir.mkdir(exist_ok=True)

    def load_config(self, tab_name: str) -> Dict[str, Any]:
        """
        Load configuration for a specific tab

        Args:
            tab_name: Name of the tab (e.g., 'portfolio', 'geopolitics')

        Returns:
            Configuration dictionary

        Example:
            >>> loader = ConfigLoader()
            >>> config = loader.load_config('portfolio')
        """
        config_file = self.config_dir / f"{tab_name}_config.json"

        if not config_file.exists():
            raise FileNotFoundError(f"Config not found: {config_file}")

        with open(config_file, 'r', encoding='utf-8') as f:
            config = json.load(f)

        return self._validate_config(config, tab_name)

    def save_config(self, tab_name: str, config: Dict[str, Any]) -> None:
        """
        Save configuration for a tab

        Args:
            tab_name: Tab name
            config: Configuration dictionary
        """
        config_file = self.config_dir / f"{tab_name}_config.json"

        with open(config_file, 'w', encoding='utf-8') as f:
            json.dump(config, f, indent=2)

    def list_configs(self) -> list[str]:
        """List all available tab configurations"""
        configs = []
        for file in self.config_dir.glob("*_config.json"):
            tab_name = file.stem.replace("_config", "")
            configs.append(tab_name)
        return configs

    def _validate_config(self, config: Dict[str, Any], tab_name: str) -> Dict[str, Any]:
        """
        Validate configuration structure

        Required fields:
        - model: {provider, model_id, temperature?, max_tokens?}
        - name: Agent name
        - instructions: System instructions

        Optional:
        - tools: List of tool names
        - memory: Enable memory (bool)
        - knowledge: Knowledge base settings
        - output_format: 'text', 'json', 'markdown'
        """
        # Set defaults
        config.setdefault("name", f"{tab_name.title()} Agent")
        config.setdefault("instructions", "You are a helpful AI assistant.")
        config.setdefault("tools", [])
        config.setdefault("memory", False)
        config.setdefault("output_format", "markdown")

        # Validate model config
        if "model" not in config:
            raise ValueError(f"Config for {tab_name} missing 'model' field")

        model = config["model"]
        if "provider" not in model:
            raise ValueError(f"Config for {tab_name} missing 'model.provider'")
        if "model_id" not in model:
            raise ValueError(f"Config for {tab_name} missing 'model.model_id'")

        # Set model defaults
        model.setdefault("temperature", 0.7)
        model.setdefault("max_tokens", 4096)

        return config
