"""
Model Factory for Renaissance Technologies System

Creates model instances based on configuration.
Supports multiple providers.
"""

from typing import Any, Optional
from ..config import get_config, ModelProvider, ModelConfig


def create_model(config: Optional[ModelConfig] = None) -> Any:
    """
    Create a model instance based on configuration.

    Supports:
    - OpenAI (gpt-4o, gpt-4o-mini, etc.)
    - Anthropic (claude-3-opus, claude-3-sonnet, etc.)
    - Groq (llama-3.3-70b-versatile, etc.)
    - Ollama (local models)
    - Zhipu AI

    Returns:
        Model instance compatible with Agno Agent
    """
    config = config or get_config().models

    if config.provider == ModelProvider.ZHIPUAI:
        return create_zhipuai_model(config)
    elif config.provider == ModelProvider.OPENAI:
        return create_openai_model(config)
    elif config.provider == ModelProvider.ANTHROPIC:
        return create_anthropic_model(config)
    elif config.provider == ModelProvider.GROQ:
        return create_groq_model(config)
    elif config.provider == ModelProvider.OLLAMA:
        return create_ollama_model(config)
    else:
        raise ValueError(f"Unsupported provider: {config.provider}")


def create_zhipuai_model(config: ModelConfig) -> Any:
    """
    Create a Zhipu AI model instance.

    Uses OpenAILike for OpenAI-compatible API endpoints.
    """
    try:
        # Use OpenAILike for better compatibility with OpenAI-compatible endpoints
        from agno.models.openai.like import OpenAILike

        return OpenAILike(
            id=config.model_id,
            name="ZhipuAI",
            provider="ZhipuAI",
            api_key=config.zhipuai_api_key,
            base_url=config.zhipuai_base_url,
        )
    except ImportError:
        try:
            # Fallback to OpenAIChat if OpenAILike not available
            from agno.models.openai import OpenAIChat

            return OpenAIChat(
                id=config.model_id,
                api_key=config.zhipuai_api_key,
                base_url=config.zhipuai_base_url,
                temperature=config.temperature,
                max_tokens=config.max_tokens,
            )
        except ImportError:
            # Final fallback: Return config dict for manual initialization
            return {
                "provider": "zhipuai",
                "model_id": config.model_id,
                "api_key": config.zhipuai_api_key,
                "base_url": config.zhipuai_base_url,
                "temperature": config.temperature,
                "max_tokens": config.max_tokens,
            }


def create_openai_model(config: ModelConfig) -> Any:
    """Create an OpenAI model instance."""
    try:
        from agno.models.openai import OpenAIChat

        return OpenAIChat(
            id=config.model_id,
            temperature=config.temperature,
            max_tokens=config.max_tokens,
        )
    except ImportError:
        return {
            "provider": "openai",
            "model_id": config.model_id,
            "temperature": config.temperature,
            "max_tokens": config.max_tokens,
        }


def create_anthropic_model(config: ModelConfig) -> Any:
    """
    Create an Anthropic model instance.

    Supports custom base_url for Anthropic-compatible endpoints.
    """
    try:
        from agno.models.anthropic import Claude
        import anthropic

        # Check if we have a custom base URL
        base_url = getattr(config, 'anthropic_base_url', None)
        api_key = getattr(config, 'anthropic_api_key', None)

        if base_url and api_key:
            # Create custom Anthropic client with custom base_url
            custom_client = anthropic.Anthropic(
                api_key=api_key,
                base_url=base_url,
            )
            return Claude(
                id=config.model_id,
                client=custom_client,
                temperature=config.temperature,
                max_tokens=config.max_tokens,
            )
        else:
            return Claude(
                id=config.model_id,
                temperature=config.temperature,
                max_tokens=config.max_tokens,
            )
    except ImportError:
        return {
            "provider": "anthropic",
            "model_id": config.model_id,
            "temperature": config.temperature,
            "max_tokens": config.max_tokens,
        }


def create_groq_model(config: ModelConfig) -> Any:
    """Create a Groq model instance."""
    try:
        from agno.models.groq import Groq

        return Groq(
            id=config.model_id,
            temperature=config.temperature,
            max_tokens=config.max_tokens,
        )
    except ImportError:
        return {
            "provider": "groq",
            "model_id": config.model_id,
            "temperature": config.temperature,
            "max_tokens": config.max_tokens,
        }


def create_ollama_model(config: ModelConfig) -> Any:
    """Create an Ollama model instance."""
    try:
        from agno.models.ollama import Ollama

        return Ollama(
            id=config.model_id,
            temperature=config.temperature,
        )
    except ImportError:
        return {
            "provider": "ollama",
            "model_id": config.model_id,
            "temperature": config.temperature,
        }


def get_model_info() -> dict:
    """Get information about the currently configured model."""
    config = get_config().models

    return {
        "provider": config.provider.value,
        "model_id": config.model_id,
        "reasoning_model_id": config.reasoning_model_id,
        "memory_model_id": config.memory_model_id,
        "temperature": config.temperature,
        "max_tokens": config.max_tokens,
    }


__all__ = [
    "create_model",
    "create_zhipuai_model",
    "create_openai_model",
    "create_anthropic_model",
    "create_groq_model",
    "create_ollama_model",
    "get_model_info",
]
