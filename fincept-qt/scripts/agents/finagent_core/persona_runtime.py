"""
PersonaRuntime — owns one persona's stateful resources.

One instance per (user_id, agent_id). Holds the Agno Agent plus every
state-carrying module (memory, storage, knowledge, agentic memory,
guardrails, tracing, evaluation, compression, hooks).

PersonaRegistry creates these, caches them, and closes them on eviction.
"""

from __future__ import annotations

import logging
import os
from typing import Any, Dict, List, Optional

from finagent_core import resources
from finagent_core.run_errors import AgentFeatureUnavailable

logger = logging.getLogger(__name__)

# agno 2.x keeps sessions and memories in a `db` object (agno.db.*). The 1.x
# classes this module used (agno.memory.AgentMemory, agno.memory.db.sqlite,
# agno.storage.*) no longer exist, so memory and storage were silently off.


def _sqlite_db(feature: str, db_path: str, **tables: str) -> Any:
    """agno.db.sqlite.SqliteDb at db_path, or AgentFeatureUnavailable saying
    what is missing (SqliteDb needs SQLAlchemy)."""
    from finagent_core.registries.availability import class_unavailable_reason
    reason = class_unavailable_reason("agno.db.sqlite.SqliteDb")
    if reason:
        raise AgentFeatureUnavailable(f"Agent {feature} unavailable: agno's SQLite store {reason}")
    from agno.db.sqlite import SqliteDb
    try:
        os.makedirs(os.path.dirname(os.path.abspath(db_path)), exist_ok=True)
        return SqliteDb(db_file=db_path, **tables)
    except Exception as e:
        raise AgentFeatureUnavailable(f"Agent {feature} unavailable: cannot open {db_path}: {e}") from e


def _build_memory_backend(db_path: str, table_name: str = "agent_memory",
                          create_user_memories: bool = False,
                          create_session_summary: bool = False) -> Any:
    """agno 2.x memory backend: a MemoryManager with its own SQLite db at
    db_path (memories stay in the persona's memory file, apart from sessions).

    The create_* switches are Agent options in agno 2.x; CoreAgent._create_agent
    applies them from the memory config. Raises AgentFeatureUnavailable."""
    db = _sqlite_db("memory", db_path, memory_table=table_name or "agent_memory")
    from agno.memory import MemoryManager
    return MemoryManager(db=db)


def _build_storage_backend(db_path: str, table_name: str = "agent_sessions",
                           storage_type: str = "sqlite", db_url: Optional[str] = None) -> Any:
    """agno 2.x session store (Agent `db`). Raises AgentFeatureUnavailable."""
    storage_type = (storage_type or "sqlite").lower()
    table = table_name or "agent_sessions"
    if storage_type == "sqlite":
        return _sqlite_db("storage", db_path, session_table=table)
    if storage_type == "postgres":
        if not db_url:
            raise AgentFeatureUnavailable(
                "Agent storage unavailable: storage type 'postgres' needs a database URL (db_url)")
        import importlib.util
        if not (importlib.util.find_spec("psycopg") or importlib.util.find_spec("psycopg2")):
            raise AgentFeatureUnavailable(
                "Agent storage unavailable: storage type 'postgres' needs the Python package "
                "'psycopg' (pip install psycopg-binary), which is not installed")
        try:
            from agno.db.postgres import PostgresDb
            return PostgresDb(db_url=db_url, session_table=table)
        except Exception as e:
            raise AgentFeatureUnavailable(f"Agent storage unavailable: {e}") from e
    raise AgentFeatureUnavailable(
        f"Agent storage unavailable: unknown storage type '{storage_type}' (use sqlite or postgres)")


def _build_knowledge_module(knowledge_config: Dict[str, Any],
                            user_id: str, agent_id: str,
                            api_keys: Dict[str, str]) -> Any:
    """Build KnowledgeModule with per-persona default path. Raises
    AgentFeatureUnavailable when the config cannot be turned into one."""
    from finagent_core.modules import KnowledgeModule
    cfg = dict(knowledge_config)
    cfg.setdefault("path", resources.knowledge_dir(user_id, agent_id))
    cfg["api_keys"] = api_keys
    try:
        return KnowledgeModule.from_config(cfg)
    except AgentFeatureUnavailable:
        raise
    except Exception as e:
        raise AgentFeatureUnavailable(f"Knowledge base unavailable: {e}") from e


def _persona_path(user_id: str, agent_id: str, configured: Any, default: str) -> str:
    """A configured db path, made absolute under the persona directory when it
    is relative (the CREATE tab suggests bare names like "agent_memory.db",
    which resolved against the scripts directory — read-only on installs)."""
    p = configured.strip() if isinstance(configured, str) else ""
    if not p:
        return default
    p = os.path.expanduser(p)
    if os.path.isabs(p):
        return p
    return os.path.join(resources.persona_dir(user_id, agent_id), p)


def build_agno_agent(config: Dict[str, Any],
                     api_keys: Dict[str, str],
                     memory_backend: Optional[Any] = None,
                     storage_backend: Optional[Any] = None,
                     knowledge_agent_kwargs: Optional[Dict[str, Any]] = None,
                     warnings: Optional[List[str]] = None) -> Any:
    """
    Build an Agno Agent from config, with pre-built state injected.

    Delegates the model/tools/instructions assembly to the existing
    CoreAgent._create_agent logic, but replaces the memory/storage/knowledge
    construction with the injected instances. Notes about requested features
    that are not available (e.g. a tool whose package is missing) are appended
    to `warnings` when a list is given.
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
    agent = tmp._create_agent(cfg)
    if warnings is not None:
        warnings.extend(tmp.build_warnings)
    return agent


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
        # Requested features that are not in effect for this persona (missing
        # tool packages, a substituted vector store, skipped knowledge sources).
        # main.py returns them with the run so the user is told.
        self.warnings: List[str] = []

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
        """Assemble all state and the Agno agent for this persona.

        A feature the config turns on (memory, storage, knowledge) that cannot
        be built raises AgentFeatureUnavailable naming what is missing — it used
        to be logged and dropped, so the agent answered without it and nobody
        knew."""
        resources.ensure_persona_dir(user_id, agent_id)
        warnings: List[str] = []

        memory_cfg = config.get("memory", {})
        memory_enabled = memory_cfg if isinstance(memory_cfg, bool) else memory_cfg.get("enabled", False)

        memory_backend = None
        if memory_enabled:
            mc = memory_cfg if isinstance(memory_cfg, dict) else {}
            memory_backend = _build_memory_backend(
                db_path=_persona_path(user_id, agent_id, mc.get("db_path"),
                                      resources.memory_db(user_id, agent_id)),
                table_name=mc.get("table_name") or "agent_memory",
                create_user_memories=bool(mc.get("create_user_memories", False)),
                create_session_summary=bool(mc.get("create_session_summary", False)),
            )

        storage_cfg = config.get("storage", {})
        storage_backend = None
        if memory_enabled or config.get("session_id") or storage_cfg:
            sc = storage_cfg if isinstance(storage_cfg, dict) else {}
            storage_backend = _build_storage_backend(
                db_path=_persona_path(user_id, agent_id, sc.get("db_path"),
                                      resources.sessions_db(user_id, agent_id)),
                table_name=sc.get("table_name") or "agent_sessions",
                storage_type=sc.get("type") or "sqlite",
                db_url=sc.get("db_url"),
            )

        knowledge_module = None
        knowledge_agent_kwargs = None
        knowledge_cfg = config.get("knowledge")
        if isinstance(knowledge_cfg, dict) and knowledge_cfg:
            knowledge_module = _build_knowledge_module(knowledge_cfg, user_id, agent_id, api_keys)
            knowledge_agent_kwargs = knowledge_module.to_agent_config()
            warnings.extend(getattr(knowledge_module, "warnings", []) or [])

        agno_agent = build_agno_agent(
            config=config,
            api_keys=api_keys,
            memory_backend=memory_backend,
            storage_backend=storage_backend,
            knowledge_agent_kwargs=knowledge_agent_kwargs,
            warnings=warnings,
        )

        agentic_memory = None
        if memory_enabled:
            from finagent_core.modules import AgenticMemoryModule
            agentic_memory = AgenticMemoryModule.from_config(
                {"user_id": user_id, "agent_id": agent_id,
                 "db_path": resources.agentic_memory_db(user_id, agent_id)}
            )

        runtime = cls(
            user_id=user_id,
            agent_id=agent_id,
            agno_agent=agno_agent,
            memory_backend=memory_backend,
            storage_backend=storage_backend,
            knowledge_module=knowledge_module,
            agentic_memory=agentic_memory,
        )
        runtime.warnings = warnings
        for w in warnings:
            logger.warning(f"[{agent_id}] {w}")
        return runtime

    def run(self, query: str, session_id: Optional[str] = None, stream: bool = False) -> Any:
        run_kwargs = {"input": query}
        if session_id:
            run_kwargs["session_id"] = session_id
        # agno 2.x scopes memories and sessions by user_id; without it every
        # user of a persona shared one anonymous bucket.
        if self.user_id and (self.memory_backend is not None or self.storage_backend is not None):
            run_kwargs["user_id"] = self.user_id
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
            # agno MemoryManager has no close(); its SqliteDb does.
            if not any(callable(getattr(obj, n, None)) for n in ("close", "shutdown", "dispose")):
                obj = getattr(obj, "db", None)
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
