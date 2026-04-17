"""
PersonaRegistry — LRU cache of PersonaRuntime instances.

Keyed by (user_id, agent_id). Cap default 8, tunable via
FINAGENT_RUNTIME_CACHE_SIZE env var. When full, evicts least-recently-used
runtime and calls its close() before dropping the reference.
"""

from __future__ import annotations

import logging
import os
from collections import OrderedDict
from typing import Any, Dict, Tuple

from finagent_core.persona_runtime import PersonaRuntime

logger = logging.getLogger(__name__)


class PersonaRegistry:
    """LRU cache of PersonaRuntime objects."""

    def __init__(self, cap: int = None):
        if cap is None:
            try:
                cap = int(os.getenv("FINAGENT_RUNTIME_CACHE_SIZE", "8"))
            except ValueError:
                cap = 8
        self.cap = max(1, cap)
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
