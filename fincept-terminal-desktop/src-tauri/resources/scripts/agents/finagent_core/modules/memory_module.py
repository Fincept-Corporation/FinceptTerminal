"""
Memory Module - Conversation history and user memory management

Provides:
- Conversation history management
- User memory storage
- Memory optimization strategies
- Session persistence
"""

from typing import Dict, Any, Optional, List
import logging

logger = logging.getLogger(__name__)


class MemoryModule:
    """
    Unified memory management for agents.

    Supports:
    - Agno MemoryManager for conversation history
    - User memory for persistent user preferences
    - Multiple optimization strategies
    """

    # Memory optimization strategies
    OPTIMIZATION_STRATEGIES = {
        "summarize": "SummarizeStrategy",
        "sliding_window": "SlidingWindowStrategy",
        "token_limit": "TokenLimitStrategy",
    }

    def __init__(
        self,
        db_path: Optional[str] = None,
        user_id: Optional[str] = None,
        enable_user_memory: bool = False,
        optimization_strategy: Optional[str] = None,
        **kwargs
    ):
        """
        Initialize memory module.

        Args:
            db_path: Path to SQLite database for persistence
            user_id: User ID for user-specific memory
            enable_user_memory: Enable persistent user memory
            optimization_strategy: Memory optimization strategy
            **kwargs: Additional configuration
        """
        self.db_path = db_path
        self.user_id = user_id
        self.enable_user_memory = enable_user_memory
        self.optimization_strategy = optimization_strategy
        self.config = kwargs

        self._memory_manager = None
        self._user_memory = None
        self._initialized = False

    def initialize(self) -> None:
        """Initialize memory systems"""
        if self._initialized:
            return

        try:
            # Initialize MemoryManager
            from agno.memory import MemoryManager

            manager_kwargs = {}

            # Add optimization strategy if specified
            if self.optimization_strategy:
                strategy = self._create_optimization_strategy()
                if strategy:
                    manager_kwargs["optimization_strategy"] = strategy

            self._memory_manager = MemoryManager(**manager_kwargs)

            # Initialize UserMemory if enabled
            if self.enable_user_memory and self.user_id:
                try:
                    from agno.memory import UserMemory
                    self._user_memory = UserMemory(
                        user_id=self.user_id,
                        db_path=self.db_path
                    )
                except ImportError:
                    logger.warning("UserMemory not available in this Agno version")

            self._initialized = True
            logger.debug("Memory module initialized")

        except ImportError as e:
            logger.warning(f"Memory module initialization failed: {e}")
        except Exception as e:
            logger.error(f"Memory module error: {e}")

    def _create_optimization_strategy(self) -> Optional[Any]:
        """Create memory optimization strategy"""
        if not self.optimization_strategy:
            return None

        try:
            from agno.memory import (
                MemoryOptimizationStrategyFactory,
                MemoryOptimizationStrategyType
            )

            strategy_map = {
                "summarize": MemoryOptimizationStrategyType.SUMMARIZE,
            }

            strategy_type = strategy_map.get(self.optimization_strategy.lower())
            if strategy_type:
                return MemoryOptimizationStrategyFactory.create(strategy_type)

        except ImportError:
            logger.warning("Memory optimization strategies not available")
        except Exception as e:
            logger.warning(f"Failed to create optimization strategy: {e}")

        return None

    def get_memory_manager(self) -> Optional[Any]:
        """Get the Agno MemoryManager instance"""
        self.initialize()
        return self._memory_manager

    def get_user_memory(self) -> Optional[Any]:
        """Get the UserMemory instance"""
        self.initialize()
        return self._user_memory

    def add_message(
        self,
        role: str,
        content: str,
        session_id: Optional[str] = None,
        **metadata
    ) -> None:
        """
        Add a message to memory.

        Args:
            role: Message role ('user', 'assistant', 'system')
            content: Message content
            session_id: Optional session ID
            **metadata: Additional metadata
        """
        self.initialize()

        if self._memory_manager:
            try:
                self._memory_manager.add_message(
                    role=role,
                    content=content,
                    session_id=session_id,
                    **metadata
                )
            except Exception as e:
                logger.error(f"Failed to add message to memory: {e}")

    def get_messages(
        self,
        session_id: Optional[str] = None,
        limit: Optional[int] = None
    ) -> List[Dict[str, Any]]:
        """
        Get messages from memory.

        Args:
            session_id: Optional session ID filter
            limit: Maximum number of messages

        Returns:
            List of message dictionaries
        """
        self.initialize()

        if self._memory_manager:
            try:
                return self._memory_manager.get_messages(
                    session_id=session_id,
                    limit=limit
                )
            except Exception as e:
                logger.error(f"Failed to get messages: {e}")

        return []

    def clear_session(self, session_id: str) -> None:
        """Clear messages for a session"""
        self.initialize()

        if self._memory_manager:
            try:
                self._memory_manager.clear_session(session_id)
            except Exception as e:
                logger.error(f"Failed to clear session: {e}")

    def store_user_preference(
        self,
        key: str,
        value: Any,
        category: Optional[str] = None
    ) -> None:
        """
        Store a user preference.

        Args:
            key: Preference key
            value: Preference value
            category: Optional category
        """
        if self._user_memory:
            try:
                self._user_memory.store(key=key, value=value, category=category)
            except Exception as e:
                logger.error(f"Failed to store user preference: {e}")

    def get_user_preference(
        self,
        key: str,
        default: Any = None
    ) -> Any:
        """
        Get a user preference.

        Args:
            key: Preference key
            default: Default value if not found

        Returns:
            Preference value or default
        """
        if self._user_memory:
            try:
                return self._user_memory.get(key=key, default=default)
            except Exception as e:
                logger.error(f"Failed to get user preference: {e}")

        return default

    def to_agent_config(self) -> Dict[str, Any]:
        """
        Convert memory module to agent configuration.

        Returns:
            Dict to pass to Agent constructor
        """
        self.initialize()

        config = {}

        if self._memory_manager:
            config["memory"] = self._memory_manager

        if self._user_memory:
            config["user_memory"] = self._user_memory

        return config

    @classmethod
    def from_config(cls, config: Dict[str, Any]) -> "MemoryModule":
        """
        Create MemoryModule from configuration dict.

        Args:
            config: Memory configuration

        Returns:
            MemoryModule instance
        """
        return cls(
            db_path=config.get("db_path"),
            user_id=config.get("user_id"),
            enable_user_memory=config.get("enable_user_memory", False),
            optimization_strategy=config.get("optimization_strategy"),
            **config.get("options", {})
        )

    @classmethod
    def list_strategies(cls) -> List[str]:
        """List available optimization strategies"""
        return list(cls.OPTIMIZATION_STRATEGIES.keys())
