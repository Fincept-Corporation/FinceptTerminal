"""
Registries - Central registries for all Agno components

Provides lazy loading and unified access to:
- Tools (100+ available)
- Models (40+ providers)
- VectorDBs (20+ options)
- Embedders (15+ options)
"""

from finagent_core.registries.tools_registry import ToolsRegistry
from finagent_core.registries.models_registry import ModelsRegistry
from finagent_core.registries.vectordb_registry import VectorDBRegistry
from finagent_core.registries.embedder_registry import EmbedderRegistry

__all__ = [
    "ToolsRegistry",
    "ModelsRegistry",
    "VectorDBRegistry",
    "EmbedderRegistry"
]
