"""
PersonaRegistry — LRU cache of PersonaRuntime instances.

Keyed by (user_id, agent_id). Cap default 8, tunable via
FINAGENT_RUNTIME_CACHE_SIZE env var. When full, evicts least-recently-used
runtime and calls its close() before dropping the reference.

Not thread-safe. Caller is expected to run a single event loop per Python
process (today: one QProcess per invocation from the C++ side). If future
code runs async tasks or a thread pool sharing one registry, wrap
get_or_create/clear mutations in a lock.
"""

from __future__ import annotations

import logging
import os
from collections import OrderedDict
from typing import Any, Dict, Optional, Tuple

from finagent_core.persona_runtime import PersonaRuntime

logger = logging.getLogger(__name__)


class PersonaRegistry:
    """LRU cache of PersonaRuntime objects."""

    def __init__(self, cap: Optional[int] = None):
        if cap is None:
            raw = os.getenv("FINAGENT_RUNTIME_CACHE_SIZE", "8")
            try:
                cap = int(raw)
            except ValueError:
                logger.warning(
                    f"FINAGENT_RUNTIME_CACHE_SIZE={raw!r} not an int; using 8"
                )
                cap = 8
        if cap < 1:
            logger.warning(f"PersonaRegistry cap {cap} < 1; clamping to 1")
            cap = 1
        self.cap = cap
        self._items: "OrderedDict[Tuple[str, str], PersonaRuntime]" = OrderedDict()

    def get_or_create(self,
                      user_id: str,
                      agent_id: str,
                      config: Dict[str, Any],
                      api_keys: Dict[str, str]) -> PersonaRuntime:
        key = (user_id, agent_id)
        if key in self._items:
            self._items.move_to_end(key)
            return self._items[key]

        runtime = PersonaRuntime.build(
            user_id=user_id,
            agent_id=agent_id,
            config=config,
            api_keys=api_keys,
        )
        self._items[key] = runtime

        while len(self._items) > self.cap:
            evicted_key, evicted_rt = self._items.popitem(last=False)
            try:
                evicted_rt.close()
            except Exception as e:
                logger.warning(f"Evicting {evicted_key}: close() raised {e}")

        return runtime

    def clear(self) -> None:
        while self._items:
            _, rt = self._items.popitem(last=False)
            try:
                rt.close()
            except Exception as e:
                logger.debug(f"clear(): close() raised {e}")

    def __len__(self) -> int:
        return len(self._items)

    def __contains__(self, key: Tuple[str, str]) -> bool:
        return key in self._items
