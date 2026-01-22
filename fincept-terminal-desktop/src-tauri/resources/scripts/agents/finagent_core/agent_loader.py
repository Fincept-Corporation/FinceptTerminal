"""
Dynamic Agent Loader - Load agents from configuration at runtime

Similar to ValueCell's _resolve_local_agent_class_sync pattern.
Enables runtime agent discovery and instantiation from JSON configs.

Performance optimizations:
- Uses targeted directory scanning instead of recursive rglob()
- Caches discovered agents to avoid repeated file I/O
- Limits discovery to known agent directories only
- Timeout protection for file operations
"""

import json
import importlib
import importlib.util
import os
import re
import time
from pathlib import Path
from typing import Dict, Any, Optional, List, Type, Set
from dataclasses import dataclass, field
import logging
from functools import lru_cache

logger = logging.getLogger(__name__)

# Known agent directories to scan (avoids expensive rglob)
KNOWN_AGENT_DIRS = [
    "TraderInvestorsAgent",
    "hedgeFundAgents",
    "EconomicAgents",
    "GeopoliticsAgents",
    "finagent_core",
]

# Maximum time allowed for discovery (seconds)
DISCOVERY_TIMEOUT = 5.0

# Cache TTL for discovered agents (seconds)
CACHE_TTL = 300  # 5 minutes


@dataclass
class AgentCard:
    """
    Agent metadata card (similar to ValueCell AgentCard)
    Defines agent capabilities and configuration.
    """
    id: str
    name: str
    description: str
    category: str
    version: str = "1.0.0"
    provider: str = "local"
    capabilities: List[str] = field(default_factory=list)
    config: Dict[str, Any] = field(default_factory=dict)
    module_path: Optional[str] = None
    class_name: Optional[str] = None

    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> "AgentCard":
        return cls(
            id=data.get("id", data.get("name", "unknown")),
            name=data.get("name", "Unknown Agent"),
            description=data.get("description", ""),
            category=data.get("category", "general"),
            version=data.get("version", "1.0.0"),
            provider=data.get("provider", "local"),
            capabilities=data.get("capabilities", []),
            config=data.get("config", {}),
            module_path=data.get("module_path"),
            class_name=data.get("class_name")
        )

    def to_dict(self) -> Dict[str, Any]:
        return {
            "id": self.id,
            "name": self.name,
            "description": self.description,
            "category": self.category,
            "version": self.version,
            "provider": self.provider,
            "capabilities": self.capabilities,
            "config": self.config,
            "module_path": self.module_path,
            "class_name": self.class_name
        }


class AgentRegistry:
    """
    Registry for dynamically loaded agents.
    Maintains a catalog of available agents and their configurations.
    """

    _instance = None

    def __new__(cls):
        if cls._instance is None:
            cls._instance = super().__new__(cls)
            cls._instance._agents: Dict[str, AgentCard] = {}
            cls._instance._loaded_classes: Dict[str, Type] = {}
            cls._instance._initialized = False
        return cls._instance

    def register(self, card: AgentCard) -> None:
        """Register an agent card"""
        self._agents[card.id] = card
        logger.info(f"Registered agent: {card.id} ({card.name})")

    def unregister(self, agent_id: str) -> bool:
        """Unregister an agent"""
        if agent_id in self._agents:
            del self._agents[agent_id]
            if agent_id in self._loaded_classes:
                del self._loaded_classes[agent_id]
            return True
        return False

    def get(self, agent_id: str) -> Optional[AgentCard]:
        """Get agent card by ID"""
        return self._agents.get(agent_id)

    def list_agents(self, category: Optional[str] = None) -> List[AgentCard]:
        """List all registered agents, optionally filtered by category"""
        agents = list(self._agents.values())
        if category:
            agents = [a for a in agents if a.category == category]
        return agents

    def list_categories(self) -> List[str]:
        """List all unique categories"""
        return list(set(a.category for a in self._agents.values()))

    def get_class(self, agent_id: str) -> Optional[Type]:
        """Get loaded class for agent"""
        return self._loaded_classes.get(agent_id)

    def set_class(self, agent_id: str, cls: Type) -> None:
        """Cache loaded class"""
        self._loaded_classes[agent_id] = cls

    def clear(self) -> None:
        """Clear all registrations"""
        self._agents.clear()
        self._loaded_classes.clear()


class AgentLoader:
    """
    Dynamic agent loader - resolves and instantiates agents at runtime.

    Features:
    - Load agents from JSON config files
    - Discover agents from directory structure (optimized - no rglob)
    - Dynamic module loading
    - Agent instantiation with config injection
    - Caching for performance
    """

    def __init__(self, config_dir: Optional[Path] = None, agents_dir: Optional[Path] = None):
        self.config_dir = config_dir or Path(__file__).parent / "configs"
        self.agents_dir = agents_dir or Path(__file__).parent.parent
        self.registry = AgentRegistry()
        self._discovery_cache: Optional[List[AgentCard]] = None
        self._cache_timestamp: float = 0

        # Ensure directories exist
        if self.config_dir.exists():
            pass  # Don't create if not exists - avoid permission errors
        else:
            try:
                self.config_dir.mkdir(exist_ok=True)
            except Exception:
                pass  # Ignore if can't create

    def discover_agents(self) -> List[AgentCard]:
        """
        Discover all available agents from config files and directory structure.
        Returns list of discovered agent cards.

        Optimizations:
        - Uses caching to avoid repeated discovery
        - Scans only known agent directories (not recursive rglob)
        - Has timeout protection
        - Skips __pycache__ and other non-agent directories
        """
        # Check cache first
        if self._discovery_cache is not None:
            if time.time() - self._cache_timestamp < CACHE_TTL:
                logger.debug(f"Returning {len(self._discovery_cache)} cached agents")
                return self._discovery_cache

        start_time = time.time()
        discovered = []
        discovered_ids: Set[str] = set()

        try:
            # 1. Load from JSON config files in config directory (fast)
            discovered.extend(self._load_config_files(discovered_ids))

            # Check timeout
            if time.time() - start_time > DISCOVERY_TIMEOUT:
                logger.warning("Discovery timeout after loading config files")
                return self._finalize_discovery(discovered)

            # 2. Load from known agent directories (targeted, not recursive)
            discovered.extend(self._load_from_known_dirs(discovered_ids, start_time))

        except Exception as e:
            logger.error(f"Discovery error: {e}")

        return self._finalize_discovery(discovered)

    def _finalize_discovery(self, discovered: List[AgentCard]) -> List[AgentCard]:
        """Finalize discovery and update cache"""
        # Register all discovered agents
        for card in discovered:
            self.registry.register(card)

        # Update cache
        self._discovery_cache = discovered
        self._cache_timestamp = time.time()

        logger.info(f"Discovered {len(discovered)} agents")
        return discovered

    def _load_config_files(self, discovered_ids: Set[str]) -> List[AgentCard]:
        """Load agents from JSON config files"""
        discovered = []

        if not self.config_dir.exists():
            return discovered

        try:
            # Use os.listdir for faster directory listing
            for filename in os.listdir(self.config_dir):
                if not filename.endswith('_agent.json'):
                    continue

                config_file = self.config_dir / filename
                try:
                    card = self._load_agent_config(config_file)
                    if card and card.id not in discovered_ids:
                        discovered.append(card)
                        discovered_ids.add(card.id)
                except Exception as e:
                    logger.debug(f"Failed to load config {filename}: {e}")
        except Exception as e:
            logger.debug(f"Error listing config dir: {e}")

        return discovered

    def _load_from_known_dirs(self, discovered_ids: Set[str], start_time: float) -> List[AgentCard]:
        """Load agents from known agent directories (targeted scanning)"""
        discovered = []

        for dir_name in KNOWN_AGENT_DIRS:
            # Check timeout
            if time.time() - start_time > DISCOVERY_TIMEOUT:
                logger.warning(f"Discovery timeout at directory: {dir_name}")
                break

            agent_dir = self.agents_dir / dir_name
            if not agent_dir.exists() or not agent_dir.is_dir():
                continue

            # Load from configs subdirectory if exists (JSON configs)
            configs_dir = agent_dir / "configs"
            if configs_dir.exists():
                discovered.extend(self._load_json_configs_from_dir(configs_dir, dir_name, discovered_ids))

            # Scan only top-level Python files (no recursion)
            discovered.extend(self._scan_directory_shallow(agent_dir, dir_name, discovered_ids))

        return discovered

    def _load_json_configs_from_dir(self, configs_dir: Path, category: str, discovered_ids: Set[str]) -> List[AgentCard]:
        """Load agent definitions from JSON config files in a directory"""
        discovered = []

        try:
            for filename in os.listdir(configs_dir):
                if not filename.endswith('.json'):
                    continue

                config_file = configs_dir / filename
                try:
                    with open(config_file, 'r', encoding='utf-8') as f:
                        data = json.load(f)

                    # Handle both single agent and multi-agent config files
                    agents_data = data.get('agents', [data] if 'id' in data or 'name' in data else [])

                    for agent_data in agents_data:
                        if not agent_data:
                            continue
                        agent_data['category'] = agent_data.get('category', category)
                        card = AgentCard.from_dict(agent_data)
                        if card.id not in discovered_ids:
                            discovered.append(card)
                            discovered_ids.add(card.id)
                except Exception as e:
                    logger.debug(f"Failed to load {filename}: {e}")
        except Exception as e:
            logger.debug(f"Error reading configs dir {configs_dir}: {e}")

        return discovered

    def _scan_directory_shallow(self, agent_dir: Path, category: str, discovered_ids: Set[str]) -> List[AgentCard]:
        """Scan directory for agent Python files (shallow, no recursion)"""
        discovered = []

        try:
            for filename in os.listdir(agent_dir):
                # Only look for *_agent.py files
                if not filename.endswith('_agent.py'):
                    continue

                # Skip CLI scripts
                if '_cli' in filename:
                    continue

                agent_file = agent_dir / filename
                if not agent_file.is_file():
                    continue

                try:
                    card = self._discover_from_file(agent_file, category)
                    if card and card.id not in discovered_ids:
                        discovered.append(card)
                        discovered_ids.add(card.id)
                except Exception as e:
                    logger.debug(f"Could not discover {filename}: {e}")
        except Exception as e:
            logger.debug(f"Error scanning {agent_dir}: {e}")

        return discovered

    def _load_agent_config(self, config_file: Path) -> Optional[AgentCard]:
        """Load agent card from JSON config file"""
        try:
            with open(config_file, 'r', encoding='utf-8') as f:
                data = json.load(f)
            return AgentCard.from_dict(data)
        except Exception as e:
            logger.debug(f"Error loading config {config_file}: {e}")
            return None

    def _discover_from_file(self, agent_file: Path, category: str = "general") -> Optional[AgentCard]:
        """Auto-discover agent from Python file structure"""
        # Extract agent name from filename
        name = agent_file.stem.replace("_agent", "").replace("_", " ").title()
        agent_id = agent_file.stem

        # Find class name in file (with size limit to avoid reading huge files)
        class_name = self._find_agent_class_fast(agent_file)
        if not class_name:
            return None

        return AgentCard(
            id=agent_id,
            name=f"{name} Agent",
            description=f"Auto-discovered agent from {agent_file.name}",
            category=category,
            module_path=str(agent_file),
            class_name=class_name
        )

    def _find_agent_class_fast(self, agent_file: Path, max_bytes: int = 8192) -> Optional[str]:
        """
        Find the main agent class in a Python file.
        Only reads first max_bytes to avoid slow I/O on large files.
        """
        try:
            # Read only the beginning of the file (class definitions are usually at top)
            with open(agent_file, 'r', encoding='utf-8') as f:
                content = f.read(max_bytes)

            # Look for class definitions that end with Agent
            matches = re.findall(r'class\s+(\w+Agent)\s*[:\(]', content)
            if matches:
                return matches[0]
        except Exception:
            pass
        return None

    def load_agent_class(self, agent_id: str) -> Optional[Type]:
        """
        Dynamically load agent class.
        Similar to ValueCell's _resolve_local_agent_class_sync.
        """
        # Check cache first
        cached = self.registry.get_class(agent_id)
        if cached:
            return cached

        # Get agent card
        card = self.registry.get(agent_id)
        if not card or not card.module_path or not card.class_name:
            logger.warning(f"Agent {agent_id} not found or missing module info")
            return None

        try:
            # Load module dynamically
            module_path = Path(card.module_path)
            spec = importlib.util.spec_from_file_location(
                f"agent_{agent_id}",
                module_path
            )
            if spec and spec.loader:
                module = importlib.util.module_from_spec(spec)
                spec.loader.exec_module(module)

                # Get class from module
                agent_class = getattr(module, card.class_name, None)
                if agent_class:
                    self.registry.set_class(agent_id, agent_class)
                    logger.info(f"Loaded agent class: {card.class_name}")
                    return agent_class
        except Exception as e:
            logger.error(f"Failed to load agent class {agent_id}: {e}")

        return None

    def create_agent(
        self,
        agent_id: str,
        api_keys: Optional[Dict[str, str]] = None,
        override_config: Optional[Dict[str, Any]] = None
    ) -> Any:
        """
        Create agent instance from registered agent.

        Args:
            agent_id: ID of the registered agent
            api_keys: API keys to inject
            override_config: Config overrides

        Returns:
            Instantiated agent
        """
        card = self.registry.get(agent_id)
        if not card:
            raise ValueError(f"Agent not found: {agent_id}")

        # Merge configs
        config = {**card.config}
        if override_config:
            config.update(override_config)
        if api_keys:
            config["api_keys"] = api_keys

        # Try to load and instantiate class
        agent_class = self.load_agent_class(agent_id)
        if agent_class:
            return agent_class(**config) if config else agent_class()

        # Fallback to CoreAgent with config
        from finagent_core.core_agent import CoreAgent
        agent = CoreAgent(api_keys=api_keys)
        return agent

    def create_from_config(
        self,
        config: Dict[str, Any],
        api_keys: Optional[Dict[str, str]] = None
    ) -> Any:
        """
        Create agent directly from config dict (no registration needed).

        Args:
            config: Agent configuration
            api_keys: API keys

        Returns:
            Instantiated agent
        """
        from finagent_core.core_agent import CoreAgent

        agent = CoreAgent(
            api_keys=api_keys,
            user_id=config.get("user_id")
        )
        return agent

    def save_agent_config(self, card: AgentCard) -> Path:
        """Save agent card to config file"""
        config_file = self.config_dir / f"{card.id}_agent.json"
        with open(config_file, 'w', encoding='utf-8') as f:
            json.dump(card.to_dict(), f, indent=2)
        return config_file


# Singleton instance
_loader: Optional[AgentLoader] = None


def get_loader() -> AgentLoader:
    """Get singleton AgentLoader instance"""
    global _loader
    if _loader is None:
        _loader = AgentLoader()
    return _loader


def discover_agents() -> List[Dict[str, Any]]:
    """Discover and return all available agents as dicts"""
    loader = get_loader()
    agents = loader.discover_agents()
    return [a.to_dict() for a in agents]


def create_agent(agent_id: str, api_keys: Dict[str, str] = None, config: Dict[str, Any] = None) -> Any:
    """Create agent instance"""
    loader = get_loader()
    return loader.create_agent(agent_id, api_keys, config)


def list_agents(category: str = None) -> List[Dict[str, Any]]:
    """List registered agents"""
    loader = get_loader()
    agents = loader.registry.list_agents(category)
    return [a.to_dict() for a in agents]


def list_categories() -> List[str]:
    """List all agent categories"""
    loader = get_loader()
    return loader.registry.list_categories()
