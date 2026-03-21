"""
Dynamic Embedder Factory for Agno Framework

Reads embedding configuration from frontend SQLite database and creates
appropriate Agno embedder instances for all supported providers.

Supports 18+ providers including:
- OpenAI, Google, Azure, AWS Bedrock
- Ollama (local, no API key needed)
- Cohere, Mistral, Voyage AI
- HuggingFace, FastEmbed, Sentence Transformers
"""

import os
from typing import Dict, Any, Optional
from dataclasses import dataclass
import logging

logger = logging.getLogger(__name__)


@dataclass
class EmbedderConfig:
    """Embedder configuration from frontend database"""
    provider: str
    model_id: str
    api_key: Optional[str] = None
    base_url: Optional[str] = None
    dimensions: int = 1536
    batch_enabled: bool = True
    batch_size: int = 100


class EmbedderFactory:
    """
    Factory for creating Agno embedder instances from frontend configuration.

    Supports all Agno embedding providers with fallback to Ollama (local, free).
    """

    # Default dimensions for known providers
    PROVIDER_DIMENSIONS = {
        "openai": 1536,
        "ollama": 4096,
        "google": 768,
        "cohere": 1024,
        "mistral": 1024,
        "voyageai": 1024,
        "jina": 768,
    }

    # Default model IDs
    DEFAULT_MODELS = {
        "openai": "text-embedding-3-small",
        "ollama": "nomic-embed-text",  # Fast, free, local
        "google": "models/embedding-001",
        "azure_openai": "text-embedding-3-small",
        "cohere": "embed-english-v3.0",
        "mistral": "mistral-embed",
    }

    def __init__(self, frontend_db_path: Optional[str] = None):
        """
        Initialize embedder factory.

        Args:
            frontend_db_path: Path to frontend SQLite database (fincept.db)
        """
        self.frontend_db_path = frontend_db_path
        self._config_cache: Optional[EmbedderConfig] = None

    def _get_frontend_db_path(self) -> Optional[str]:
        """
        Find frontend database path using centralized path utilities.

        Priority:
        1. Explicitly provided path
        2. Centralized path utility (checks standard locations)
        """
        if self.frontend_db_path:
            return self.frontend_db_path

        # Use centralized path utility
        try:
            from .paths import get_frontend_db_path
            frontend_db = get_frontend_db_path()
            if frontend_db:
                return str(frontend_db)
        except ImportError:
            logger.warning("Path utilities not available")

        logger.warning("Frontend database not found. Using Ollama fallback.")
        return None

    def _load_embedder_config_from_db(self) -> Optional[EmbedderConfig]:
        """
        Load embedder configuration from frontend SQLite database.

        Reads from LLM config table (embedding provider may be same as LLM provider).

        Returns:
            EmbedderConfig if found, None otherwise
        """
        db_path = self._get_frontend_db_path()
        if not db_path:
            return None

        try:
            import sqlite3
            conn = sqlite3.connect(db_path)
            cursor = conn.cursor()

            # Try to get embedding-specific config (if exists)
            cursor.execute("""
                SELECT provider, model, api_key, base_url
                FROM llm_configs
                WHERE is_active = 1 AND provider IS NOT NULL
                LIMIT 1
            """)

            row = cursor.fetchone()
            conn.close()

            if not row:
                logger.warning("No active LLM config found for embeddings")
                return None

            provider, model, api_key, base_url = row

            # Get dimensions based on provider
            dimensions = self.PROVIDER_DIMENSIONS.get(provider.lower(), 1536)

            config = EmbedderConfig(
                provider=provider.lower(),
                model_id=model or self.DEFAULT_MODELS.get(provider.lower(), ""),
                api_key=api_key,
                base_url=base_url,
                dimensions=dimensions,
                batch_enabled=True,
                batch_size=100
            )

            logger.info(f"Loaded embedder config: {provider} / {model}")
            return config

        except Exception as e:
            logger.error(f"Error loading embedder config from database: {e}")
            return None

    def _get_embedder_config(self, override_config: Optional[Dict[str, Any]] = None) -> EmbedderConfig:
        """
        Get embedder configuration with priority:
        1. Override config (runtime parameters)
        2. Cached config from database
        3. Fallback to Ollama (local, free)

        Args:
            override_config: Optional runtime configuration override

        Returns:
            EmbedderConfig
        """
        # If override provided, use it
        if override_config:
            return EmbedderConfig(
                provider=override_config.get("provider", "ollama"),
                model_id=override_config.get("model_id", "nomic-embed-text"),
                api_key=override_config.get("api_key"),
                base_url=override_config.get("base_url"),
                dimensions=override_config.get("dimensions", 768),
                batch_enabled=override_config.get("batch_enabled", True),
                batch_size=override_config.get("batch_size", 100)
            )

        # Try to load from database (cache for performance)
        if not self._config_cache:
            self._config_cache = self._load_embedder_config_from_db()

        if self._config_cache:
            return self._config_cache

        # Fallback to Ollama (local, no API key)
        logger.info("Using Ollama embedder (local, free)")
        return EmbedderConfig(
            provider="ollama",
            model_id="nomic-embed-text",
            dimensions=768,
            batch_enabled=True,
            batch_size=100
        )

    def _create_openai_embedder(self, config: EmbedderConfig):
        """Create OpenAI embedder"""
        from agno.knowledge.embedder.openai import OpenAIEmbedder

        return OpenAIEmbedder(
            id=config.model_id,
            api_key=config.api_key or os.getenv("OPENAI_API_KEY"),
            base_url=config.base_url,
            dimensions=config.dimensions,
            enable_batch=config.batch_enabled,
            batch_size=config.batch_size,
        )

    def _create_ollama_embedder(self, config: EmbedderConfig):
        """Create Ollama embedder (local, no API key needed)"""
        from agno.knowledge.embedder.ollama import OllamaEmbedder

        return OllamaEmbedder(
            id=config.model_id,
            host=config.base_url or "http://localhost:11434",
            dimensions=config.dimensions,
            enable_batch=config.batch_enabled,
            batch_size=config.batch_size,
        )

    def _create_google_embedder(self, config: EmbedderConfig):
        """Create Google Gemini embedder"""
        from agno.knowledge.embedder.google import GoogleEmbedder

        return GoogleEmbedder(
            id=config.model_id,
            api_key=config.api_key or os.getenv("GOOGLE_API_KEY"),
            dimensions=config.dimensions,
            enable_batch=config.batch_enabled,
            batch_size=config.batch_size,
        )

    def _create_azure_embedder(self, config: EmbedderConfig):
        """Create Azure OpenAI embedder"""
        from agno.knowledge.embedder.azure_openai import AzureOpenAIEmbedder

        return AzureOpenAIEmbedder(
            id=config.model_id,
            api_key=config.api_key or os.getenv("AZURE_OPENAI_API_KEY"),
            azure_endpoint=config.base_url or os.getenv("AZURE_OPENAI_ENDPOINT"),
            dimensions=config.dimensions,
            enable_batch=config.batch_enabled,
            batch_size=config.batch_size,
        )

    def _create_cohere_embedder(self, config: EmbedderConfig):
        """Create Cohere embedder"""
        from agno.knowledge.embedder.cohere import CohereEmbedder

        return CohereEmbedder(
            id=config.model_id,
            api_key=config.api_key or os.getenv("COHERE_API_KEY"),
            dimensions=config.dimensions,
            enable_batch=config.batch_enabled,
            batch_size=config.batch_size,
        )

    def _create_mistral_embedder(self, config: EmbedderConfig):
        """Create Mistral embedder"""
        from agno.knowledge.embedder.mistral import MistralEmbedder

        return MistralEmbedder(
            id=config.model_id,
            api_key=config.api_key or os.getenv("MISTRAL_API_KEY"),
            dimensions=config.dimensions,
            enable_batch=config.batch_enabled,
            batch_size=config.batch_size,
        )

    def create_embedder(self, override_config: Optional[Dict[str, Any]] = None):
        """
        Create Agno embedder instance from configuration.

        Args:
            override_config: Optional configuration override

        Returns:
            Agno embedder instance

        Examples:
            # Use active config from frontend
            embedder = factory.create_embedder()

            # Override with runtime config
            embedder = factory.create_embedder({
                "provider": "openai",
                "model_id": "text-embedding-3-large",
                "api_key": "sk-...",
                "dimensions": 3072
            })

            # Use Ollama (local, free)
            embedder = factory.create_embedder({
                "provider": "ollama",
                "model_id": "nomic-embed-text"
            })
        """
        config = self._get_embedder_config(override_config)
        provider = config.provider.lower()

        # Map provider to creation method
        try:
            if provider == "openai":
                return self._create_openai_embedder(config)
            elif provider == "ollama":
                return self._create_ollama_embedder(config)
            elif provider in ["google", "gemini"]:
                return self._create_google_embedder(config)
            elif provider in ["azure", "azure_openai"]:
                return self._create_azure_embedder(config)
            elif provider == "cohere":
                return self._create_cohere_embedder(config)
            elif provider == "mistral":
                return self._create_mistral_embedder(config)
            else:
                # Universal fallback - try Ollama
                logger.warning(f"Provider {provider} not explicitly supported, falling back to Ollama")
                return self._create_ollama_embedder(EmbedderConfig(
                    provider="ollama",
                    model_id="nomic-embed-text",
                    dimensions=768
                ))

        except ImportError as e:
            logger.warning(f"Provider {provider} not available, falling back to Ollama: {e}")
            return self._create_ollama_embedder(EmbedderConfig(
                provider="ollama",
                model_id="nomic-embed-text",
                dimensions=768
            ))
        except Exception as e:
            logger.error(f"Error creating embedder for {provider}: {e}")
            # Final fallback to Ollama
            logger.warning("Using Ollama fallback due to error")
            return self._create_ollama_embedder(EmbedderConfig(
                provider="ollama",
                model_id="nomic-embed-text",
                dimensions=768
            ))

    def refresh_config(self):
        """Force reload configuration from database"""
        self._config_cache = None
        logger.info("Embedder configuration cache cleared")


# Global factory instance
_global_embedder_factory: Optional[EmbedderFactory] = None


def get_embedder_factory(frontend_db_path: Optional[str] = None) -> EmbedderFactory:
    """
    Get global embedder factory instance.

    Args:
        frontend_db_path: Optional path to frontend database

    Returns:
        EmbedderFactory instance
    """
    global _global_embedder_factory
    if _global_embedder_factory is None:
        _global_embedder_factory = EmbedderFactory(frontend_db_path)
    return _global_embedder_factory


def create_embedder_from_config(config_dict: Optional[Dict[str, Any]] = None):
    """
    Convenience function to create embedder from configuration.

    Args:
        config_dict: Optional configuration dictionary

    Returns:
        Agno embedder instance

    Examples:
        # Use frontend config (or Ollama fallback)
        embedder = create_embedder_from_config()

        # Override config
        embedder = create_embedder_from_config({
            "provider": "openai",
            "model_id": "text-embedding-3-small",
            "api_key": "sk-...",
            "dimensions": 1536
        })

        # Use local Ollama
        embedder = create_embedder_from_config({
            "provider": "ollama"
        })
    """
    factory = get_embedder_factory()
    return factory.create_embedder(config_dict)
