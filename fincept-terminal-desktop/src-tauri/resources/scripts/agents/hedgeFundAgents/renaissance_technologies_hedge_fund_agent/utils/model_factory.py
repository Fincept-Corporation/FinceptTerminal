"""
Dynamic Model Factory for Agno Framework

Reads LLM configuration from frontend SQLite database and creates
appropriate Agno model instances for all supported providers.

Supports 40+ providers including:
- OpenAI, Anthropic, Google, Mistral, Cohere, Meta
- Cloud providers: AWS Bedrock, Azure, Vertex AI
- Fast inference: Groq, Together, Fireworks
- Local: Ollama, llama.cpp, LM Studio
- Custom OpenAI-compatible endpoints
"""

import os
import sqlite3
from typing import Dict, Any, Optional, List
from dataclasses import dataclass
import logging

logger = logging.getLogger(__name__)


@dataclass
class ModelConfig:
    """Model configuration from frontend database"""
    provider: str
    model_id: str
    api_key: Optional[str] = None
    base_url: Optional[str] = None
    temperature: float = 0.7
    max_tokens: int = 4096


class ModelFactory:
    """
    Factory for creating Agno model instances from frontend configuration.

    Supports all Agno providers with fallback handling and custom base URLs.
    """

    # Default base URLs for known providers
    PROVIDER_BASE_URLS = {
        "openai": "https://api.openai.com/v1",
        "anthropic": "https://api.anthropic.com",
        "google": "https://generativelanguage.googleapis.com",
        "groq": "https://api.groq.com/openai/v1",
        "together": "https://api.together.xyz/v1",
        "fireworks": "https://api.fireworks.ai/inference/v1",
        "deepseek": "https://api.deepseek.com",
        "mistral": "https://api.mistral.ai/v1",
        "cohere": "https://api.cohere.ai",
        "perplexity": "https://api.perplexity.ai",
        "openrouter": "https://openrouter.ai/api/v1",
        "ollama": "http://localhost:11434",
        "x-ai": "https://api.x.ai/v1",
    }

    def __init__(self, frontend_db_path: Optional[str] = None):
        """
        Initialize model factory.

        Args:
            frontend_db_path: Path to frontend SQLite database (fincept.db)
        """
        self.frontend_db_path = frontend_db_path
        self._config_cache: Optional[ModelConfig] = None

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

        logger.warning("Frontend database not found. Using fallback configuration.")
        return None

    def _load_llm_config_from_db(self) -> Optional[ModelConfig]:
        """
        Load active LLM configuration from frontend SQLite database.

        Returns:
            ModelConfig if found, None otherwise
        """
        db_path = self._get_frontend_db_path()
        if not db_path:
            return None

        try:
            conn = sqlite3.connect(db_path)
            cursor = conn.cursor()

            # Get active LLM config
            cursor.execute("""
                SELECT provider, api_key, base_url, model
                FROM llm_configs
                WHERE is_active = 1
                LIMIT 1
            """)

            row = cursor.fetchone()
            if not row:
                logger.warning("No active LLM config found in database")
                conn.close()
                return None

            provider, api_key, base_url, model = row

            # Get global settings
            cursor.execute("""
                SELECT temperature, max_tokens
                FROM llm_global_settings
                LIMIT 1
            """)

            settings_row = cursor.fetchone()
            temperature = settings_row[0] if settings_row else 0.7
            max_tokens = settings_row[1] if settings_row else 4096

            conn.close()

            config = ModelConfig(
                provider=provider,
                model_id=model,
                api_key=api_key,
                base_url=base_url,
                temperature=temperature,
                max_tokens=max_tokens
            )

            logger.info(f"Loaded config: {provider} / {model}")
            return config

        except Exception as e:
            logger.error(f"Error loading config from database: {e}")
            return None

    def _get_model_config(self, override_config: Optional[Dict[str, Any]] = None) -> ModelConfig:
        """
        Get model configuration with priority:
        1. Override config (runtime parameters)
        2. Cached config from database
        3. Fallback to default

        Args:
            override_config: Optional runtime configuration override

        Returns:
            ModelConfig
        """
        # If override provided, use it
        if override_config:
            return ModelConfig(
                provider=override_config.get("provider", "openai"),
                model_id=override_config.get("model_id", "gpt-4o"),
                api_key=override_config.get("api_key"),
                base_url=override_config.get("base_url"),
                temperature=override_config.get("temperature", 0.7),
                max_tokens=override_config.get("max_tokens", 4096)
            )

        # Try to load from database (cache for performance)
        if not self._config_cache:
            self._config_cache = self._load_llm_config_from_db()

        if self._config_cache:
            return self._config_cache

        # Fallback to default
        logger.warning("Using fallback OpenAI GPT-4o configuration")
        return ModelConfig(
            provider="openai",
            model_id="gpt-4o",
            temperature=0.7,
            max_tokens=4096
        )

    def _create_openai_model(self, config: ModelConfig):
        """Create OpenAI model"""
        from agno.models.openai import OpenAI

        return OpenAI(
            id=config.model_id,
            api_key=config.api_key or os.getenv("OPENAI_API_KEY"),
            temperature=config.temperature,
            max_tokens=config.max_tokens
        )

    def _create_anthropic_model(self, config: ModelConfig):
        """Create Anthropic Claude model"""
        from agno.models.anthropic import Claude

        kwargs = {
            "id": config.model_id,
            "temperature": config.temperature,
            "max_tokens": config.max_tokens
        }

        # Handle custom base URL
        if config.base_url:
            import anthropic
            custom_client = anthropic.Anthropic(
                api_key=config.api_key or os.getenv("ANTHROPIC_API_KEY", ""),
                base_url=config.base_url
            )
            kwargs["client"] = custom_client
        else:
            kwargs["api_key"] = config.api_key or os.getenv("ANTHROPIC_API_KEY")

        return Claude(**kwargs)

    def _create_google_model(self, config: ModelConfig):
        """Create Google Gemini model"""
        from agno.models.google import Gemini

        return Gemini(
            id=config.model_id,
            api_key=config.api_key or os.getenv("GOOGLE_API_KEY"),
            temperature=config.temperature,
            max_tokens=config.max_tokens
        )

    def _create_groq_model(self, config: ModelConfig):
        """Create Groq model"""
        from agno.models.groq import Groq

        return Groq(
            id=config.model_id,
            api_key=config.api_key or os.getenv("GROQ_API_KEY"),
            temperature=config.temperature,
            max_tokens=config.max_tokens
        )

    def _create_ollama_model(self, config: ModelConfig):
        """Create Ollama local model"""
        from agno.models.ollama import Ollama

        return Ollama(
            id=config.model_id,
            host=config.base_url or self.PROVIDER_BASE_URLS["ollama"],
            temperature=config.temperature,
            max_tokens=config.max_tokens
        )

    def _create_openai_like_model(self, config: ModelConfig):
        """
        Create OpenAI-compatible model for any provider.

        This is the universal fallback for:
        - Custom providers
        - Providers not natively supported
        - OpenAI-compatible endpoints
        """
        from agno.models.openai.like import OpenAILike

        base_url = config.base_url or self.PROVIDER_BASE_URLS.get(
            config.provider.lower(),
            "http://localhost:8000/v1"
        )

        return OpenAILike(
            id=config.model_id,
            api_key=config.api_key or os.getenv(f"{config.provider.upper()}_API_KEY", ""),
            base_url=base_url,
            temperature=config.temperature,
            max_tokens=config.max_tokens
        )

    def create_model(self, override_config: Optional[Dict[str, Any]] = None):
        """
        Create Agno model instance from configuration.

        Args:
            override_config: Optional configuration override

        Returns:
            Agno model instance

        Examples:
            # Use active config from frontend
            model = factory.create_model()

            # Override with runtime config
            model = factory.create_model({
                "provider": "groq",
                "model_id": "llama-3-70b-versatile",
                "api_key": "gsk_...",
                "temperature": 0.3
            })
        """
        config = self._get_model_config(override_config)
        provider = config.provider.lower()

        # Map provider to creation method
        try:
            if provider == "openai":
                return self._create_openai_model(config)
            elif provider == "anthropic":
                return self._create_anthropic_model(config)
            elif provider in ["google", "gemini"]:
                return self._create_google_model(config)
            elif provider == "groq":
                return self._create_groq_model(config)
            elif provider == "ollama":
                return self._create_ollama_model(config)
            else:
                # Universal fallback for all other providers
                logger.info(f"Using OpenAI-compatible interface for provider: {provider}")
                return self._create_openai_like_model(config)

        except ImportError as e:
            logger.warning(f"Provider {provider} not available, using OpenAI-like fallback: {e}")
            return self._create_openai_like_model(config)
        except Exception as e:
            logger.error(f"Error creating model for {provider}: {e}")
            # Final fallback
            logger.warning("Using OpenAI-like fallback due to error")
            return self._create_openai_like_model(config)

    def refresh_config(self):
        """Force reload configuration from database"""
        self._config_cache = None
        logger.info("Configuration cache cleared")


# Global factory instance
_global_factory: Optional[ModelFactory] = None


def get_model_factory(frontend_db_path: Optional[str] = None) -> ModelFactory:
    """
    Get global model factory instance.

    Args:
        frontend_db_path: Optional path to frontend database

    Returns:
        ModelFactory instance
    """
    global _global_factory
    if _global_factory is None:
        _global_factory = ModelFactory(frontend_db_path)
    return _global_factory


def create_model_from_config(config_dict: Optional[Dict[str, Any]] = None):
    """
    Convenience function to create model from configuration.

    Args:
        config_dict: Optional configuration dictionary

    Returns:
        Agno model instance

    Examples:
        # Use frontend config
        model = create_model_from_config()

        # Override config
        model = create_model_from_config({
            "provider": "anthropic",
            "model_id": "claude-3-5-sonnet-20241022",
            "api_key": "sk-ant-...",
            "temperature": 0.5
        })
    """
    factory = get_model_factory()
    return factory.create_model(config_dict)


def get_default_model_settings() -> Dict[str, Any]:
    """
    Get default model settings from active frontend configuration.

    Returns model settings that should be used as defaults throughout
    the system (schemas, presets, etc.)

    Returns:
        Dict with provider, model_id, temperature, max_tokens

    Examples:
        # Use in schema defaults
        @dataclass
        class ModelSettings:
            provider: str = field(default_factory=lambda: get_default_model_settings()["provider"])
            model_id: str = field(default_factory=lambda: get_default_model_settings()["model_id"])
    """
    factory = get_model_factory()
    config = factory._get_model_config()

    return {
        "provider": config.provider,
        "model_id": config.model_id,
        "temperature": config.temperature,
        "max_tokens": config.max_tokens,
        "api_key": config.api_key,
        "base_url": config.base_url,
    }
