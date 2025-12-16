"""
LLM Provider Manager - Supports all major LLM providers

This module provides unified access to multiple LLM providers with
automatic API key management, fallback handling, and model selection.
"""

import os
from typing import Dict, Any, Optional, List
from finagent_core.utils.logger import get_logger

logger = get_logger(__name__)


class LLMProviderManager:
    """
    Manages all LLM providers with API key handling and model selection

    Supported Providers:
    - OpenAI (GPT-4, GPT-3.5, GPT-4 Turbo, etc.)
    - Anthropic (Claude 3 Opus, Sonnet, Haiku)
    - Google (Gemini Pro, Gemini 1.5 Pro)
    - Ollama (Local models: Llama2, Mistral, etc.)
    - Groq (Fast inference)
    - Cohere
    - Together AI
    """

    # Available models per provider
    PROVIDER_MODELS = {
        "openai": [
            "gpt-4",
            "gpt-4-turbo",
            "gpt-4-turbo-preview",
            "gpt-3.5-turbo",
            "gpt-3.5-turbo-16k",
        ],
        "anthropic": [
            "claude-3-opus-20240229",
            "claude-3-sonnet-20240229",
            "claude-3-haiku-20240307",
            "claude-3-5-sonnet-20241022",
        ],
        "google": [
            "gemini-pro",
            "gemini-1.5-pro-latest",
            "gemini-1.5-flash",
        ],
        "ollama": [
            "llama2",
            "llama2:13b",
            "llama2:70b",
            "mistral",
            "mixtral",
            "codellama",
            "phi",
        ],
        "groq": [
            "mixtral-8x7b-32768",
            "llama2-70b-4096",
            "gemma-7b-it",
        ],
        "cohere": [
            "command",
            "command-light",
            "command-r",
            "command-r-plus",
        ],
    }

    @classmethod
    def get_model(
        cls,
        provider: str,
        model_id: str,
        api_key: Optional[str] = None,
        temperature: float = 0.7,
        max_tokens: int = 4096,
        **kwargs
    ):
        """
        Get a configured LLM model

        Args:
            provider: Provider name (openai, anthropic, google, ollama, etc.)
            model_id: Model identifier
            api_key: Optional API key (will use env var if not provided)
            temperature: Sampling temperature
            max_tokens: Maximum tokens
            **kwargs: Additional provider-specific parameters

        Returns:
            Configured Agno model instance
        """
        provider = provider.lower()

        # Validate provider
        if provider not in cls.PROVIDER_MODELS:
            raise ValueError(
                f"Unknown provider: {provider}. "
                f"Supported: {list(cls.PROVIDER_MODELS.keys())}"
            )

        # Validate model
        if model_id not in cls.PROVIDER_MODELS[provider]:
            logger.warning(
                f"Model {model_id} not in known models for {provider}. "
                f"Attempting to use anyway..."
            )

        # Get API key from parameter or environment
        if not api_key:
            api_key = cls._get_api_key_from_env(provider)

        # Create model based on provider
        if provider == "openai":
            return cls._create_openai_model(model_id, api_key, temperature, max_tokens, **kwargs)
        elif provider == "anthropic":
            return cls._create_anthropic_model(model_id, api_key, temperature, max_tokens, **kwargs)
        elif provider == "google":
            return cls._create_google_model(model_id, api_key, temperature, max_tokens, **kwargs)
        elif provider == "ollama":
            return cls._create_ollama_model(model_id, temperature, max_tokens, **kwargs)
        elif provider == "groq":
            return cls._create_groq_model(model_id, api_key, temperature, max_tokens, **kwargs)
        elif provider == "cohere":
            return cls._create_cohere_model(model_id, api_key, temperature, max_tokens, **kwargs)
        else:
            raise ValueError(f"Provider {provider} not yet implemented")

    @staticmethod
    def _get_api_key_from_env(provider: str) -> Optional[str]:
        """Get API key from environment variables"""
        env_var_map = {
            "openai": "OPENAI_API_KEY",
            "anthropic": "ANTHROPIC_API_KEY",
            "google": "GOOGLE_API_KEY",
            "groq": "GROQ_API_KEY",
            "cohere": "COHERE_API_KEY",
            "ollama": None,  # Ollama doesn't need API key (local)
        }

        env_var = env_var_map.get(provider)
        if env_var:
            api_key = os.getenv(env_var)
            if not api_key:
                logger.warning(f"No API key found in environment for {provider} ({env_var})")
            return api_key
        return None

    @staticmethod
    def _create_openai_model(model_id: str, api_key: Optional[str], temperature: float, max_tokens: int, **kwargs):
        """Create OpenAI model"""
        try:
            from agno.models.openai import OpenAIChat

            model = OpenAIChat(
                id=model_id,
                api_key=api_key,
                temperature=temperature,
                max_tokens=max_tokens,
                **kwargs
            )
            logger.info(f"Created OpenAI model: {model_id}")
            return model
        except ImportError:
            logger.error("Agno OpenAI support not installed. Install: pip install agno[openai]")
            raise

    @staticmethod
    def _create_anthropic_model(model_id: str, api_key: Optional[str], temperature: float, max_tokens: int, **kwargs):
        """Create Anthropic Claude model"""
        try:
            from agno.models.anthropic import Claude

            model = Claude(
                id=model_id,
                api_key=api_key,
                temperature=temperature,
                max_tokens=max_tokens,
                **kwargs
            )
            logger.info(f"Created Anthropic model: {model_id}")
            return model
        except ImportError:
            logger.error("Agno Anthropic support not installed. Install: pip install agno[anthropic]")
            raise

    @staticmethod
    def _create_google_model(model_id: str, api_key: Optional[str], temperature: float, max_tokens: int, **kwargs):
        """Create Google Gemini model"""
        try:
            from agno.models.google import Gemini

            model = Gemini(
                id=model_id,
                api_key=api_key,
                temperature=temperature,
                max_tokens=max_tokens,
                **kwargs
            )
            logger.info(f"Created Google model: {model_id}")
            return model
        except ImportError:
            logger.error("Agno Google support not installed. Install: pip install agno[google]")
            raise

    @staticmethod
    def _create_ollama_model(model_id: str, temperature: float, max_tokens: int, **kwargs):
        """Create Ollama local model"""
        try:
            from agno.models.ollama import Ollama

            # Ollama doesn't need API key (local)
            model = Ollama(
                id=model_id,
                temperature=temperature,
                max_tokens=max_tokens,
                **kwargs
            )
            logger.info(f"Created Ollama model: {model_id}")
            return model
        except ImportError:
            logger.error("Agno Ollama support not installed. Install: pip install agno[ollama]")
            raise

    @staticmethod
    def _create_groq_model(model_id: str, api_key: Optional[str], temperature: float, max_tokens: int, **kwargs):
        """Create Groq model"""
        try:
            from agno.models.groq import Groq

            model = Groq(
                id=model_id,
                api_key=api_key,
                temperature=temperature,
                max_tokens=max_tokens,
                **kwargs
            )
            logger.info(f"Created Groq model: {model_id}")
            return model
        except ImportError:
            logger.error("Agno Groq support not installed. Install: pip install agno[groq]")
            raise

    @staticmethod
    def _create_cohere_model(model_id: str, api_key: Optional[str], temperature: float, max_tokens: int, **kwargs):
        """Create Cohere model"""
        try:
            from agno.models.cohere import CohereChat

            model = CohereChat(
                id=model_id,
                api_key=api_key,
                temperature=temperature,
                max_tokens=max_tokens,
                **kwargs
            )
            logger.info(f"Created Cohere model: {model_id}")
            return model
        except ImportError:
            logger.error("Agno Cohere support not installed. Install: pip install agno[cohere]")
            raise

    @classmethod
    def list_providers(cls) -> List[str]:
        """List all supported providers"""
        return list(cls.PROVIDER_MODELS.keys())

    @classmethod
    def list_models(cls, provider: str) -> List[str]:
        """List all models for a provider"""
        provider = provider.lower()
        if provider not in cls.PROVIDER_MODELS:
            raise ValueError(f"Unknown provider: {provider}")
        return cls.PROVIDER_MODELS[provider]

    @classmethod
    def get_provider_info(cls) -> Dict[str, Any]:
        """Get information about all providers and their models"""
        return {
            "providers": {
                provider: {
                    "models": models,
                    "requires_api_key": provider != "ollama",
                    "env_var": cls._get_env_var_name(provider)
                }
                for provider, models in cls.PROVIDER_MODELS.items()
            }
        }

    @staticmethod
    def _get_env_var_name(provider: str) -> Optional[str]:
        """Get environment variable name for provider"""
        env_var_map = {
            "openai": "OPENAI_API_KEY",
            "anthropic": "ANTHROPIC_API_KEY",
            "google": "GOOGLE_API_KEY",
            "groq": "GROQ_API_KEY",
            "cohere": "COHERE_API_KEY",
            "ollama": None,
        }
        return env_var_map.get(provider)
