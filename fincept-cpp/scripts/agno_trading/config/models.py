"""
LLM Model Registry and Factory
===============================

Centralized registry of all supported LLM models with their capabilities
and factory functions to create Agno model instances.
"""

from typing import Dict, List, Any, Optional
from dataclasses import dataclass
from .settings import LLMProvider, LLMConfig


@dataclass
class ModelCapabilities:
    """Model capabilities and specifications"""
    max_tokens: int
    supports_streaming: bool
    supports_function_calling: bool
    supports_vision: bool = False
    supports_reasoning: bool = False
    context_window: int = 0
    cost_per_1k_input: float = 0.0
    cost_per_1k_output: float = 0.0


# Model Registry with capabilities
MODEL_REGISTRY: Dict[str, Dict[str, ModelCapabilities]] = {
    # OpenAI Models
    "openai": {
        "gpt-4": ModelCapabilities(
            max_tokens=8192,
            context_window=8192,
            supports_streaming=True,
            supports_function_calling=True,
            supports_vision=False,
            cost_per_1k_input=0.03,
            cost_per_1k_output=0.06
        ),
        "gpt-4-turbo": ModelCapabilities(
            max_tokens=4096,
            context_window=128000,
            supports_streaming=True,
            supports_function_calling=True,
            supports_vision=True,
            cost_per_1k_input=0.01,
            cost_per_1k_output=0.03
        ),
        "gpt-4o": ModelCapabilities(
            max_tokens=16384,
            context_window=128000,
            supports_streaming=True,
            supports_function_calling=True,
            supports_vision=True,
            cost_per_1k_input=0.005,
            cost_per_1k_output=0.015
        ),
        "gpt-4o-mini": ModelCapabilities(
            max_tokens=16384,
            context_window=128000,
            supports_streaming=True,
            supports_function_calling=True,
            supports_vision=True,
            cost_per_1k_input=0.00015,
            cost_per_1k_output=0.0006
        ),
        "o1": ModelCapabilities(
            max_tokens=100000,
            context_window=200000,
            supports_streaming=False,
            supports_function_calling=False,
            supports_reasoning=True,
            cost_per_1k_input=0.015,
            cost_per_1k_output=0.06
        ),
        "o1-mini": ModelCapabilities(
            max_tokens=65536,
            context_window=128000,
            supports_streaming=False,
            supports_function_calling=False,
            supports_reasoning=True,
            cost_per_1k_input=0.003,
            cost_per_1k_output=0.012
        ),
        "o4": ModelCapabilities(
            max_tokens=100000,
            context_window=200000,
            supports_streaming=False,
            supports_function_calling=True,
            supports_reasoning=True,
            cost_per_1k_input=0.01,
            cost_per_1k_output=0.04
        ),
    },
    # Anthropic Models
    "anthropic": {
        "claude-3-opus": ModelCapabilities(
            max_tokens=4096,
            context_window=200000,
            supports_streaming=True,
            supports_function_calling=True,
            supports_vision=True,
            cost_per_1k_input=0.015,
            cost_per_1k_output=0.075
        ),
        "claude-3-sonnet": ModelCapabilities(
            max_tokens=4096,
            context_window=200000,
            supports_streaming=True,
            supports_function_calling=True,
            supports_vision=True,
            cost_per_1k_input=0.003,
            cost_per_1k_output=0.015
        ),
        "claude-3-haiku": ModelCapabilities(
            max_tokens=4096,
            context_window=200000,
            supports_streaming=True,
            supports_function_calling=True,
            supports_vision=True,
            cost_per_1k_input=0.00025,
            cost_per_1k_output=0.00125
        ),
        "claude-sonnet-4": ModelCapabilities(
            max_tokens=8192,
            context_window=200000,
            supports_streaming=True,
            supports_function_calling=True,
            supports_vision=True,
            cost_per_1k_input=0.003,
            cost_per_1k_output=0.015
        ),
        "claude-opus-4": ModelCapabilities(
            max_tokens=8192,
            context_window=200000,
            supports_streaming=True,
            supports_function_calling=True,
            supports_vision=True,
            cost_per_1k_input=0.015,
            cost_per_1k_output=0.075
        ),
    },
    # Google Models
    "google": {
        "gemini-pro": ModelCapabilities(
            max_tokens=8192,
            context_window=32768,
            supports_streaming=True,
            supports_function_calling=True,
            supports_vision=False,
            cost_per_1k_input=0.0005,
            cost_per_1k_output=0.0015
        ),
        "gemini-pro-vision": ModelCapabilities(
            max_tokens=4096,
            context_window=16384,
            supports_streaming=True,
            supports_function_calling=True,
            supports_vision=True,
            cost_per_1k_input=0.00025,
            cost_per_1k_output=0.0005
        ),
        "gemini-1.5-pro": ModelCapabilities(
            max_tokens=8192,
            context_window=2000000,
            supports_streaming=True,
            supports_function_calling=True,
            supports_vision=True,
            cost_per_1k_input=0.00125,
            cost_per_1k_output=0.00375
        ),
        "gemini-1.5-flash": ModelCapabilities(
            max_tokens=8192,
            context_window=1000000,
            supports_streaming=True,
            supports_function_calling=True,
            supports_vision=True,
            cost_per_1k_input=0.000075,
            cost_per_1k_output=0.00030
        ),
        "gemini-2.0-flash": ModelCapabilities(
            max_tokens=8192,
            context_window=1000000,
            supports_streaming=True,
            supports_function_calling=True,
            supports_vision=True,
            cost_per_1k_input=0.000075,
            cost_per_1k_output=0.00030
        ),
    },
    # Groq Models
    "groq": {
        "llama-3.1-8b": ModelCapabilities(
            max_tokens=8000,
            context_window=128000,
            supports_streaming=True,
            supports_function_calling=True,
            cost_per_1k_input=0.00005,
            cost_per_1k_output=0.00008
        ),
        "llama-3.1-70b": ModelCapabilities(
            max_tokens=8000,
            context_window=128000,
            supports_streaming=True,
            supports_function_calling=True,
            cost_per_1k_input=0.00059,
            cost_per_1k_output=0.00079
        ),
        "mixtral-8x7b": ModelCapabilities(
            max_tokens=32768,
            context_window=32768,
            supports_streaming=True,
            supports_function_calling=True,
            cost_per_1k_input=0.00024,
            cost_per_1k_output=0.00024
        ),
    },
    # DeepSeek Models
    "deepseek": {
        "deepseek-chat": ModelCapabilities(
            max_tokens=4096,
            context_window=64000,
            supports_streaming=True,
            supports_function_calling=True,
            cost_per_1k_input=0.00014,
            cost_per_1k_output=0.00028
        ),
        "deepseek-coder": ModelCapabilities(
            max_tokens=4096,
            context_window=16000,
            supports_streaming=True,
            supports_function_calling=True,
            cost_per_1k_input=0.00014,
            cost_per_1k_output=0.00028
        ),
        "deepseek-reasoner": ModelCapabilities(
            max_tokens=8000,
            context_window=64000,
            supports_streaming=True,
            supports_function_calling=True,
            supports_reasoning=True,
            cost_per_1k_input=0.00055,
            cost_per_1k_output=0.0022
        ),
    },
    # xAI Grok Models
    "xai": {
        "grok-beta": ModelCapabilities(
            max_tokens=8192,
            context_window=131072,
            supports_streaming=True,
            supports_function_calling=True,
            cost_per_1k_input=0.005,
            cost_per_1k_output=0.015
        ),
        "grok-2": ModelCapabilities(
            max_tokens=8192,
            context_window=131072,
            supports_streaming=True,
            supports_function_calling=True,
            cost_per_1k_input=0.002,
            cost_per_1k_output=0.01
        ),
    },
    # Ollama Models (Local)
    "ollama": {
        "llama3.1": ModelCapabilities(
            max_tokens=8192,
            context_window=128000,
            supports_streaming=True,
            supports_function_calling=True,
            cost_per_1k_input=0.0,
            cost_per_1k_output=0.0
        ),
        "mistral": ModelCapabilities(
            max_tokens=8192,
            context_window=32768,
            supports_streaming=True,
            supports_function_calling=True,
            cost_per_1k_input=0.0,
            cost_per_1k_output=0.0
        ),
        "qwen2.5": ModelCapabilities(
            max_tokens=8192,
            context_window=128000,
            supports_streaming=True,
            supports_function_calling=True,
            cost_per_1k_input=0.0,
            cost_per_1k_output=0.0
        ),
    },
}


def get_model(provider: str, model_name: str) -> Optional[ModelCapabilities]:
    """Get model capabilities"""
    return MODEL_REGISTRY.get(provider, {}).get(model_name)


def list_available_models(provider: Optional[str] = None) -> Dict[str, List[str]]:
    """List all available models, optionally filtered by provider"""
    if provider:
        return {provider: list(MODEL_REGISTRY.get(provider, {}).keys())}
    return {prov: list(models.keys()) for prov, models in MODEL_REGISTRY.items()}


def get_best_model_for_task(
    task: str,
    budget: str = "medium",
    requires_reasoning: bool = False
) -> tuple[str, str]:
    """
    Recommend best model for a specific task

    Args:
        task: Type of task (trading, analysis, risk_management, etc.)
        budget: Budget level (low, medium, high)
        requires_reasoning: Whether reasoning capabilities are needed

    Returns:
        Tuple of (provider, model_name)
    """

    if requires_reasoning:
        if budget == "high":
            return ("openai", "o4")
        elif budget == "medium":
            return ("openai", "o1-mini")
        else:
            return ("deepseek", "deepseek-reasoner")

    if task == "trading":
        # Fast, accurate, function calling
        if budget == "high":
            return ("anthropic", "claude-sonnet-4")
        elif budget == "medium":
            return ("openai", "gpt-4o-mini")
        else:
            return ("groq", "llama-3.1-70b")

    elif task == "analysis":
        # Deep understanding, large context
        if budget == "high":
            return ("google", "gemini-1.5-pro")
        elif budget == "medium":
            return ("anthropic", "claude-3-sonnet")
        else:
            return ("groq", "mixtral-8x7b")

    elif task == "risk_management":
        # Conservative, accurate
        if budget == "high":
            return ("openai", "gpt-4")
        elif budget == "medium":
            return ("anthropic", "claude-3-sonnet")
        else:
            return ("deepseek", "deepseek-chat")

    # Default
    return ("openai", "gpt-4o-mini")


def validate_model_config(llm_config: LLMConfig) -> tuple[bool, Optional[str]]:
    """
    Validate LLM configuration

    Returns:
        Tuple of (is_valid, error_message)
    """
    provider = llm_config.provider.value
    model = llm_config.model

    # Check if model exists
    if provider not in MODEL_REGISTRY:
        return False, f"Provider '{provider}' not found in registry"

    if model not in MODEL_REGISTRY[provider]:
        available = ", ".join(MODEL_REGISTRY[provider].keys())
        return False, f"Model '{model}' not found for provider '{provider}'. Available: {available}"

    capabilities = MODEL_REGISTRY[provider][model]

    # Check max_tokens
    if llm_config.max_tokens and llm_config.max_tokens > capabilities.max_tokens:
        return False, f"max_tokens ({llm_config.max_tokens}) exceeds model limit ({capabilities.max_tokens})"

    # Check streaming support
    if llm_config.stream and not capabilities.supports_streaming:
        return False, f"Model '{model}' does not support streaming"

    # Check reasoning
    if llm_config.reasoning_effort and not capabilities.supports_reasoning:
        return False, f"Model '{model}' does not support reasoning"

    return True, None
