"""
PersonaRuntime — owns one persona's stateful resources.

One instance per (user_id, agent_id). Holds the Agno Agent plus every
state-carrying module (memory, storage, knowledge, agentic memory,
guardrails, tracing, evaluation, compression, hooks).

PersonaRegistry creates these, caches them, and closes them on eviction.
"""

from __future__ import annotations

import logging
from typing import Any, Dict, Optional

from finagent_core import resources

logger = logging.getLogger(__name__)


def _build_memory_backend(db_path: str, table_name: str = "agent_memory",
                          create_user_memories: bool = True,
                          create_session_summary: bool = True) -> Optional[Any]:
    """Build an Agno memory backend at the given DB path."""
    try:
        from agno.memory import AgentMemory
        from agno.memory.db.sqlite import SqliteMemoryDb
        db = SqliteMemoryDb(table_name=table_name, db_file=db_path)
        return AgentMemory(
            db=db,
            create_user_memories=create_user_memories,
            create_session_summary=create_session_summary,
        )
    except ImportError as e:
        logger.warning(f"Memory backend unavailable: {e}")
        return None
    except Exception as e:
        logger.error(f"Memory backend build failed: {e}")
        return None


def _build_storage_backend(db_path: str, table_name: str = "agent_sessions") -> Optional[Any]:
    """Build an Agno SQLite storage backend at the given DB path."""
    try:
        from agno.storage.sqlite import SqliteStorage
        return SqliteStorage(table_name=table_name, db_file=db_path)
    except ImportError as e:
        logger.warning(f"Storage backend unavailable: {e}")
        return None
    except Exception as e:
        logger.error(f"Storage backend build failed: {e}")
        return None


def _build_knowledge_module(knowledge_config: Dict[str, Any],
                            user_id: str, agent_id: str,
                            api_keys: Dict[str, str]) -> Optional[Any]:
    """Build KnowledgeModule with per-persona default path."""
    try:
        from finagent_core.modules import KnowledgeModule
        cfg = dict(knowledge_config)
        cfg.setdefault("path", resources.knowledge_dir(user_id, agent_id))
        cfg["api_keys"] = api_keys
        return KnowledgeModule.from_config(cfg)
    except Exception as e:
        logger.warning(f"Knowledge module build failed: {e}")
        return None


def build_agno_agent(config: Dict[str, Any],
                     api_keys: Dict[str, str],
                     memory_backend: Optional[Any] = None,
                     storage_backend: Optional[Any] = None,
                     knowledge_agent_kwargs: Optional[Dict[str, Any]] = None) -> Any:
    """
    Build an Agno Agent from config, with pre-built state injected.

    Delegates the model/tools/instructions assembly to the existing
    CoreAgent._create_agent logic, but replaces the memory/storage/knowledge
    construction with the injected instances.
    """
    # Import lazily to avoid circular imports.
    from finagent_core.core_agent import CoreAgent

    # Use a throwaway CoreAgent just for its _create_agent assembly logic.
    # We patch its memory/storage/knowledge helpers to return our instances.
    tmp = CoreAgent(api_keys=api_keys)
    tmp._create_memory_backend = lambda _cfg: memory_backend
    tmp._create_storage = lambda _cfg: storage_backend
    if knowledge_agent_kwargs is not None:
        tmp._setup_knowledge = lambda _cfg: knowledge_agent_kwargs

    # Ensure CoreAgent actually wires the storage/knowledge we built. Its
    # _create_storage/_setup_knowledge gates on config keys — inject empty
    # dicts so the gates open and our monkey-patches are used.
    cfg = dict(config)
    if storage_backend is not None and "storage" not in cfg and not cfg.get("session_id"):
        cfg["storage"] = {}
    if knowledge_agent_kwargs is not None and "knowledge" not in cfg:
        cfg["knowledge"] = {}
    return tmp._create_agent(cfg)


class PersonaRuntime:
    """One persona's live state. One instance per (user_id, agent_id)."""

    def __init__(self,
                 user_id: str,
                 agent_id: str,
                 agno_agent: Any,
                 memory_backend: Optional[Any] = None,
                 storage_backend: Optional[Any] = None,
                 knowledge_module: Optional[Any] = None,
                 agentic_memory: Optional[Any] = None):
        self.user_id = user_id
        self.agent_id = agent_id
        self.agno_agent = agno_agent
        self.memory_backend = memory_backend
        self.storage_backend = storage_backend
        self.knowledge_module = knowledge_module
        self.agentic_memory = agentic_memory

        # Advanced modules — created on demand by setup_* methods.
        self.guardrails = None
        self.tracing = None
        self.evaluation = None
        self.compression = None
        self.hooks = None

        self._closed = False

    @classmethod
    def build(cls,
              user_id: str,
              agent_id: str,
              config: Dict[str, Any],
              api_keys: Dict[str, str]) -> "PersonaRuntime":
        """Assemble all state and the Agno agent for this persona."""
        resources.ensure_persona_dir(user_id, agent_id)

        memory_cfg = config.get("memory", {})
        memory_enabled = memory_cfg if isinstance(memory_cfg, bool) else memory_cfg.get("enabled", False)

        memory_backend = None
        if memory_enabled:
            mc = memory_cfg if isinstance(memory_cfg, dict) else {}
            memory_backend = _build_memory_backend(
                db_path=mc.get("db_path") or resources.memory_db(user_id, agent_id),
                table_name=mc.get("table_name", "agent_memory"),
                create_user_memories=mc.get("create_user_memories", True),
                create_session_summary=mc.get("create_session_summary", True),
            )
            if memory_backend is None:
                logger.error(
                    f"Memory backend build returned None for persona "
                    f"user_id={user_id!r} agent_id={agent_id!r}"
                )

        storage_cfg = config.get("storage", {})
        storage_backend = None
        if memory_enabled or config.get("session_id") or storage_cfg:
            sc = storage_cfg if isinstance(storage_cfg, dict) else {}
            storage_backend = _build_storage_backend(
                db_path=sc.get("db_path") or resources.sessions_db(user_id, agent_id),
                table_name=sc.get("table_name", "agent_sessions"),
            )
            if storage_backend is None:
                logger.error(
                    f"Storage backend build returned None for persona "
                    f"user_id={user_id!r} agent_id={agent_id!r}"
                )

        knowledge_module = None
        knowledge_agent_kwargs = None
        knowledge_cfg = config.get("knowledge")
        if isinstance(knowledge_cfg, dict) and knowledge_cfg:
            knowledge_module = _build_knowledge_module(knowledge_cfg, user_id, agent_id, api_keys)
            if knowledge_module is not None:
                knowledge_agent_kwargs = knowledge_module.to_agent_config()
            if knowledge_module is None:
                logger.error(
                    f"Knowledge module build returned None for persona "
                    f"user_id={user_id!r} agent_id={agent_id!r}"
                )

        agno_agent = build_agno_agent(
            config=config,
            api_keys=api_keys,
            memory_backend=memory_backend,
            storage_backend=storage_backend,
            knowledge_agent_kwargs=knowledge_agent_kwargs,
        )

        agentic_memory = None
        if memory_enabled:
            from finagent_core.modules import AgenticMemoryModule
            agentic_memory = AgenticMemoryModule.from_config(
                {"user_id": user_id, "agent_id": agent_id,
                 "db_path": resources.agentic_memory_db(user_id, agent_id)}
            )

        return cls(
            user_id=user_id,
            agent_id=agent_id,
            agno_agent=agno_agent,
            memory_backend=memory_backend,
            storage_backend=storage_backend,
            knowledge_module=knowledge_module,
            agentic_memory=agentic_memory,
        )

    def run(self, query: str, session_id: Optional[str] = None, stream: bool = False) -> Any:
        run_kwargs = {"input": query}
        if session_id:
            run_kwargs["session_id"] = session_id
        if stream:
            run_kwargs["stream"] = True
        return self.agno_agent.run(**run_kwargs)

    def close(self) -> None:
        """Close SQLite handles and drop references. Idempotent."""
        if self._closed:
            return
        self._closed = True
        for attr in ("memory_backend", "storage_backend", "agentic_memory"):
            obj = getattr(self, attr, None)
            if obj is None:
                continue
            for closer in ("close", "shutdown", "dispose"):
                fn = getattr(obj, closer, None)
                if callable(fn):
                    try:
                        fn()
                    except Exception as e:
                        logger.debug(f"{attr}.{closer}() ignored: {e}")
                    break
        self.agno_agent = None
