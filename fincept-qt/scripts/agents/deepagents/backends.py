"""
backends.py — Backend factory for deepagents file/execution operations.

Single responsibility:
  - Select and configure the right BackendProtocol implementation
  - Provide a get_backend() factory used by agent.py

Backend modes:
  composite  (default) — CompositeBackend routing by path prefix (production)
  state                — StateBackend factory (ephemeral, in-state)
  filesystem           — FilesystemBackend (real disk, read/write)
  shell                — LocalShellBackend (disk + shell execute tool)

CompositeBackend routing:
  /memory/   → StoreBackend   (persistent cross-session via InMemoryStore)
  /scripts/  → FilesystemBackend(root_dir=scripts_dir)  (read our analytics scripts)
  /tmp/      → StateBackend   (ephemeral scratch space)
  default    → StateBackend   (everything else)
"""

from __future__ import annotations

import logging
from typing import Any

logger = logging.getLogger(__name__)


def get_backend(
    mode: str = "composite",
    scripts_dir: str | None = None,
    store: Any | None = None,
):
    from deepagents.backends.state import StateBackend
    from deepagents.backends.filesystem import FilesystemBackend
    from deepagents.backends.local_shell import LocalShellBackend
    from deepagents.backends.store import StoreBackend
    from deepagents.backends.composite import CompositeBackend

    mode = mode.lower().strip()

    if mode == "state":
        return StateBackend

    if mode == "filesystem":
        if not scripts_dir:
            raise ValueError("scripts_dir is required for filesystem backend")
        return FilesystemBackend(root_dir=scripts_dir)

    if mode == "shell":
        if not scripts_dir:
            raise ValueError("scripts_dir is required for shell backend")
        return LocalShellBackend(root_dir=scripts_dir)

    if mode == "composite":
        return _make_composite_factory(scripts_dir=scripts_dir, store=store)

    raise ValueError(
        f"Unknown backend mode '{mode}'. "
        "Supported: composite, state, filesystem, shell"
    )


def _make_composite_factory(
    scripts_dir: str | None,
    store: Any | None,
):
    from deepagents.backends.state import StateBackend
    from deepagents.backends.filesystem import FilesystemBackend
    from deepagents.backends.store import StoreBackend
    from deepagents.backends.composite import CompositeBackend

    def _factory(runtime):
        routes: dict[str, Any] = {}

        if store is not None:
            routes["/memory/"] = StoreBackend(
                runtime,
                namespace=lambda ctx: ("fincept", "memory"),
            )
        else:
            routes["/memory/"] = StateBackend(runtime)

        if scripts_dir:
            routes["/scripts/"] = FilesystemBackend(root_dir=scripts_dir)

        routes["/tmp/"] = StateBackend(runtime)

        return CompositeBackend(
            default=StateBackend(runtime),
            routes=routes,
        )

    return _factory


# =========================
# 🔥 Enhancements Added
# =========================

class BackendMonitor:
    """Logs execution time of backend operations."""

    def __init__(self, backend, name: str = "backend"):
        self._backend = backend
        self._name = name

    def __getattr__(self, attr_name):
        attr = getattr(self._backend, attr_name)

        if callable(attr):
            def wrapper(*args, **kwargs):
                import time
                start = time.time()
                try:
                    return attr(*args, **kwargs)
                finally:
                    duration = (time.time() - start) * 1000
                    logger.info(f"[{self._name}] {attr_name} took {duration:.2f} ms")
            return wrapper

        return attr


def wrap_with_monitor(backend, name: str = "backend"):
    """Wrap backend with monitoring."""
    return BackendMonitor(backend, name=name)


def validate_scripts_dir(scripts_dir: str | None):
    """Validate scripts directory."""
    import os

    if scripts_dir:
        if not os.path.isabs(scripts_dir):
            raise ValueError("scripts_dir must be an absolute path")
        if not os.path.exists(scripts_dir):
            raise FileNotFoundError(f"scripts_dir not found: {scripts_dir}")


def safe_get_backend(
    mode: str = "composite",
    scripts_dir: str | None = None,
    store: Any | None = None,
    enable_monitor: bool = True,
):
    """
    Safe wrapper over get_backend with:
    - validation
    - optional monitoring
    """

    validate_scripts_dir(scripts_dir)

    backend = get_backend(
        mode=mode,
        scripts_dir=scripts_dir,
        store=store,
    )

    # Only wrap actual backend instances (not factories)
    if enable_monitor and not callable(backend):
        return wrap_with_monitor(backend, name=mode)

    return backend    Args:
        mode        : "composite" | "state" | "filesystem" | "shell"
        scripts_dir : Absolute path to the scripts/ directory.
                      Required for "filesystem", "shell", and "composite" modes.
        store       : LangGraph BaseStore instance (required for StoreBackend in composite).
                      If None in composite mode, /memory/ route falls back to StateBackend.

    Returns:
        BackendProtocol instance  — for filesystem/shell modes (safe to pass directly)
        Callable[[ToolRuntime]]   — for state/composite modes (factory, required by library)
    """
    from deepagents.backends.state import StateBackend
    from deepagents.backends.filesystem import FilesystemBackend
    from deepagents.backends.local_shell import LocalShellBackend
    from deepagents.backends.store import StoreBackend
    from deepagents.backends.composite import CompositeBackend

    mode = mode.lower().strip()

    if mode == "state":
        # Factory callable — library calls this with ToolRuntime at invocation time
        return StateBackend

    if mode == "filesystem":
        if not scripts_dir:
            raise ValueError("scripts_dir is required for filesystem backend")
        return FilesystemBackend(root_dir=scripts_dir)

    if mode == "shell":
        if not scripts_dir:
            raise ValueError("scripts_dir is required for shell backend")
        return LocalShellBackend(root_dir=scripts_dir)

    if mode == "composite":
        return _make_composite_factory(scripts_dir=scripts_dir, store=store)

    raise ValueError(
        f"Unknown backend mode '{mode}'. "
        "Supported: composite, state, filesystem, shell"
    )


def _make_composite_factory(
    scripts_dir: str | None,
    store: Any | None,
):
    """
    Build a factory callable that creates a CompositeBackend per ToolRuntime.

    Routes:
      /memory/  → StoreBackend  (persistent, requires store)  or StateBackend fallback
      /scripts/ → FilesystemBackend(root_dir=scripts_dir)     (read analytics scripts)
      /tmp/     → StateBackend                                 (ephemeral scratch)
      default   → StateBackend                                 (everything else)
    """
    from deepagents.backends.state import StateBackend
    from deepagents.backends.filesystem import FilesystemBackend
    from deepagents.backends.store import StoreBackend
    from deepagents.backends.composite import CompositeBackend

    def _factory(runtime):  # runtime: ToolRuntime
        routes: dict[str, Any] = {}

        # /memory/ — persistent cross-session storage
        if store is not None:
            routes["/memory/"] = StoreBackend(
                runtime,
                namespace=lambda ctx: ("fincept", "memory"),
            )
        else:
            routes["/memory/"] = StateBackend(runtime)

        # /scripts/ — read-only access to analytics scripts
        if scripts_dir:
            routes["/scripts/"] = FilesystemBackend(root_dir=scripts_dir)

        # /tmp/ — ephemeral scratch
        routes["/tmp/"] = StateBackend(runtime)

        return CompositeBackend(
            default=StateBackend(runtime),
            routes=routes,
        )

    return _factory
