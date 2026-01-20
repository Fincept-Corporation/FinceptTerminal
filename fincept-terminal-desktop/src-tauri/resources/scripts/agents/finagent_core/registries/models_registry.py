"""
Models Registry - Unified access to 40+ LLM providers

Provides lazy loading and standardized model creation.
"""

from typing import Dict, Any, Optional, List
import logging
import os

logger = logging.getLogger(__name__)


class ModelsRegistry:
    """
    Central registry for all Agno model providers.
    Handles API key management and model instantiation.
    """

    # Model providers and their configurations
    MODEL_CATALOG = {
        # OpenAI & Compatible
        "openai": {
            "class": "agno.models.openai.OpenAIChat",
            "models": ["gpt-4o", "gpt-4o-mini", "gpt-4-turbo", "gpt-4", "gpt-3.5-turbo", "o1", "o1-mini", "o1-preview"],
            "api_key_env": "OPENAI_API_KEY",
            "default_model": "gpt-4o-mini",
        },
        "azure": {
            "class": "agno.models.azure.AzureOpenAI",
            "models": ["gpt-4o", "gpt-4-turbo", "gpt-4", "gpt-35-turbo"],
            "api_key_env": "AZURE_OPENAI_API_KEY",
            "default_model": "gpt-4o",
        },

        # Anthropic
        "anthropic": {
            "class": "agno.models.anthropic.Claude",
            "models": ["claude-sonnet-4-5-20250514", "claude-3-5-sonnet-20241022", "claude-3-opus-20240229", "claude-3-sonnet-20240229", "claude-3-haiku-20240307"],
            "api_key_env": "ANTHROPIC_API_KEY",
            "default_model": "claude-sonnet-4-5-20250514",
        },

        # Google
        "google": {
            "class": "agno.models.google.Gemini",
            "models": ["gemini-2.0-flash-exp", "gemini-1.5-pro", "gemini-1.5-flash", "gemini-1.0-pro"],
            "api_key_env": "GOOGLE_API_KEY",
            "default_model": "gemini-2.0-flash-exp",
        },
        "vertexai": {
            "class": "agno.models.vertexai.VertexAI",
            "models": ["gemini-1.5-pro", "gemini-1.5-flash"],
            "api_key_env": "GOOGLE_APPLICATION_CREDENTIALS",
            "default_model": "gemini-1.5-pro",
        },

        # Meta / Llama
        "meta": {
            "class": "agno.models.meta.MetaLlama",
            "models": ["llama-3.3-70b", "llama-3.1-405b", "llama-3.1-70b", "llama-3.1-8b"],
            "api_key_env": "META_API_KEY",
            "default_model": "llama-3.3-70b",
        },

        # Groq (Fast inference)
        "groq": {
            "class": "agno.models.groq.Groq",
            "models": ["llama-3.3-70b-versatile", "llama-3.1-70b-versatile", "mixtral-8x7b-32768", "gemma2-9b-it"],
            "api_key_env": "GROQ_API_KEY",
            "default_model": "llama-3.3-70b-versatile",
        },

        # Local Models
        "ollama": {
            "class": "agno.models.ollama.Ollama",
            "models": ["llama3.3", "llama3.2", "mistral", "mixtral", "codellama", "phi3", "gemma2", "qwen2.5"],
            "api_key_env": None,  # No API key needed
            "default_model": "llama3.3",
            "base_url": "http://localhost:11434",
        },
        "lmstudio": {
            "class": "agno.models.lmstudio.LMStudio",
            "models": [],  # Dynamic based on loaded models
            "api_key_env": None,
            "default_model": None,
            "base_url": "http://localhost:1234",
        },
        "llama_cpp": {
            "class": "agno.models.llama_cpp.LlamaCpp",
            "models": [],
            "api_key_env": None,
            "default_model": None,
        },
        "vllm": {
            "class": "agno.models.vllm.vLLM",
            "models": [],
            "api_key_env": None,
            "default_model": None,
        },

        # DeepSeek
        "deepseek": {
            "class": "agno.models.deepseek.DeepSeek",
            "models": ["deepseek-chat", "deepseek-coder", "deepseek-reasoner"],
            "api_key_env": "DEEPSEEK_API_KEY",
            "default_model": "deepseek-chat",
        },

        # Mistral
        "mistral": {
            "class": "agno.models.mistral.Mistral",
            "models": ["mistral-large-latest", "mistral-medium-latest", "mistral-small-latest", "codestral-latest"],
            "api_key_env": "MISTRAL_API_KEY",
            "default_model": "mistral-large-latest",
        },

        # Cohere
        "cohere": {
            "class": "agno.models.cohere.Cohere",
            "models": ["command-r-plus", "command-r", "command-light"],
            "api_key_env": "COHERE_API_KEY",
            "default_model": "command-r-plus",
        },

        # xAI (Grok)
        "xai": {
            "class": "agno.models.xai.xAI",
            "models": ["grok-2", "grok-2-mini", "grok-beta"],
            "api_key_env": "XAI_API_KEY",
            "default_model": "grok-2",
        },

        # Perplexity
        "perplexity": {
            "class": "agno.models.perplexity.Perplexity",
            "models": ["llama-3.1-sonar-large-128k-online", "llama-3.1-sonar-small-128k-online"],
            "api_key_env": "PERPLEXITY_API_KEY",
            "default_model": "llama-3.1-sonar-large-128k-online",
        },

        # Together AI
        "together": {
            "class": "agno.models.together.Together",
            "models": ["meta-llama/Llama-3.3-70B-Instruct-Turbo", "mistralai/Mixtral-8x22B-Instruct-v0.1"],
            "api_key_env": "TOGETHER_API_KEY",
            "default_model": "meta-llama/Llama-3.3-70B-Instruct-Turbo",
        },

        # Fireworks
        "fireworks": {
            "class": "agno.models.fireworks.Fireworks",
            "models": ["accounts/fireworks/models/llama-v3p1-405b-instruct"],
            "api_key_env": "FIREWORKS_API_KEY",
            "default_model": "accounts/fireworks/models/llama-v3p1-405b-instruct",
        },

        # Nvidia
        "nvidia": {
            "class": "agno.models.nvidia.Nvidia",
            "models": ["meta/llama-3.1-405b-instruct", "nvidia/nemotron-4-340b-instruct"],
            "api_key_env": "NVIDIA_API_KEY",
            "default_model": "meta/llama-3.1-405b-instruct",
        },

        # AWS Bedrock
        "aws": {
            "class": "agno.models.aws.AwsBedrock",
            "models": ["anthropic.claude-3-5-sonnet-20241022-v2:0", "amazon.titan-text-express-v1"],
            "api_key_env": "AWS_ACCESS_KEY_ID",
            "default_model": "anthropic.claude-3-5-sonnet-20241022-v2:0",
        },

        # IBM
        "ibm": {
            "class": "agno.models.ibm.IBM",
            "models": ["ibm/granite-13b-chat-v2"],
            "api_key_env": "IBM_API_KEY",
            "default_model": "ibm/granite-13b-chat-v2",
        },

        # Cerebras
        "cerebras": {
            "class": "agno.models.cerebras.Cerebras",
            "models": ["llama3.1-70b", "llama3.1-8b"],
            "api_key_env": "CEREBRAS_API_KEY",
            "default_model": "llama3.1-70b",
        },

        # SambaNova
        "sambanova": {
            "class": "agno.models.sambanova.SambaNova",
            "models": ["Meta-Llama-3.1-405B-Instruct"],
            "api_key_env": "SAMBANOVA_API_KEY",
            "default_model": "Meta-Llama-3.1-405B-Instruct",
        },

        # HuggingFace
        "huggingface": {
            "class": "agno.models.huggingface.HuggingFace",
            "models": [],
            "api_key_env": "HUGGINGFACE_API_KEY",
            "default_model": None,
        },

        # OpenRouter (Multi-provider)
        "openrouter": {
            "class": "agno.models.openrouter.OpenRouter",
            "models": ["anthropic/claude-3.5-sonnet", "openai/gpt-4o", "google/gemini-pro"],
            "api_key_env": "OPENROUTER_API_KEY",
            "default_model": "anthropic/claude-3.5-sonnet",
        },

        # Portkey (Multi-provider)
        "portkey": {
            "class": "agno.models.portkey.Portkey",
            "models": [],
            "api_key_env": "PORTKEY_API_KEY",
            "default_model": None,
        },

        # DeepInfra
        "deepinfra": {
            "class": "agno.models.deepinfra.DeepInfra",
            "models": ["meta-llama/Llama-3.3-70B-Instruct"],
            "api_key_env": "DEEPINFRA_API_KEY",
            "default_model": "meta-llama/Llama-3.3-70B-Instruct",
        },

        # Nebius
        "nebius": {
            "class": "agno.models.nebius.Nebius",
            "models": [],
            "api_key_env": "NEBIUS_API_KEY",
            "default_model": None,
        },

        # SiliconFlow
        "siliconflow": {
            "class": "agno.models.siliconflow.SiliconFlow",
            "models": [],
            "api_key_env": "SILICONFLOW_API_KEY",
            "default_model": None,
        },

        # Vercel AI SDK
        "vercel": {
            "class": "agno.models.vercel.Vercel",
            "models": [],
            "api_key_env": "VERCEL_API_KEY",
            "default_model": None,
        },

        # DashScope (Alibaba)
        "dashscope": {
            "class": "agno.models.dashscope.DashScope",
            "models": ["qwen-max", "qwen-plus", "qwen-turbo"],
            "api_key_env": "DASHSCOPE_API_KEY",
            "default_model": "qwen-max",
        },

        # InternLM
        "internlm": {
            "class": "agno.models.internlm.InternLM",
            "models": [],
            "api_key_env": "INTERNLM_API_KEY",
            "default_model": None,
        },
    }

    _loaded_models: Dict[str, Any] = {}

    @classmethod
    def create_model(
        cls,
        provider: str,
        model_id: Optional[str] = None,
        api_key: Optional[str] = None,
        api_keys: Optional[Dict[str, str]] = None,
        **kwargs
    ) -> Any:
        """
        Create a model instance.

        Args:
            provider: Provider name (e.g., 'openai', 'anthropic')
            model_id: Specific model ID (uses default if not provided)
            api_key: Direct API key
            api_keys: Dict of API keys
            **kwargs: Additional arguments (temperature, max_tokens, etc.)

        Returns:
            Model instance

        Raises:
            ValueError: If provider not found or API key missing
        """
        provider_lower = provider.lower()

        if provider_lower not in cls.MODEL_CATALOG:
            raise ValueError(f"Unknown provider: {provider}. Available: {list(cls.MODEL_CATALOG.keys())}")

        config = cls.MODEL_CATALOG[provider_lower]

        # Resolve model ID
        final_model_id = model_id or config.get("default_model")
        if not final_model_id and config.get("models"):
            final_model_id = config["models"][0]

        # Resolve API key
        final_api_key = api_key
        if not final_api_key and api_keys:
            env_key = config.get("api_key_env")
            if env_key:
                final_api_key = api_keys.get(env_key)

        # Try environment variable as fallback
        if not final_api_key and config.get("api_key_env"):
            final_api_key = os.environ.get(config["api_key_env"])

        # Check if API key is required
        if config.get("api_key_env") and not final_api_key:
            logger.warning(f"No API key found for {provider}. Expected: {config['api_key_env']}")

        try:
            # Dynamic import
            import importlib
            class_path = config["class"]
            module_path, class_name = class_path.rsplit(".", 1)
            module = importlib.import_module(module_path)
            model_class = getattr(module, class_name)

            # Build constructor arguments
            # Agno models use 'id' for model identifier
            model_kwargs = {}
            if final_model_id:
                model_kwargs["id"] = final_model_id

            if final_api_key:
                model_kwargs["api_key"] = final_api_key

            # Add base_url for local models
            if config.get("base_url"):
                model_kwargs["base_url"] = kwargs.pop("base_url", config["base_url"])

            # Add optional parameters (Agno uses these names)
            if "temperature" in kwargs:
                model_kwargs["temperature"] = kwargs.pop("temperature")
            if "max_tokens" in kwargs:
                model_kwargs["max_tokens"] = kwargs.pop("max_tokens")
            if "max_completion_tokens" in kwargs:
                model_kwargs["max_completion_tokens"] = kwargs.pop("max_completion_tokens")

            # Add remaining kwargs (filter out None values)
            for k, v in kwargs.items():
                if v is not None:
                    model_kwargs[k] = v

            model_instance = model_class(**model_kwargs)
            logger.debug(f"Created model: {provider}/{final_model_id}")

            return model_instance

        except ImportError as e:
            raise ImportError(f"Failed to import model provider '{provider}': {e}")
        except Exception as e:
            raise RuntimeError(f"Failed to create model '{provider}/{final_model_id}': {e}")

    @classmethod
    def list_providers(cls) -> List[str]:
        """List all available providers"""
        return list(cls.MODEL_CATALOG.keys())

    @classmethod
    def list_models(cls, provider: str) -> List[str]:
        """List models for a provider"""
        provider_lower = provider.lower()
        if provider_lower not in cls.MODEL_CATALOG:
            return []
        return cls.MODEL_CATALOG[provider_lower].get("models", [])

    @classmethod
    def get_provider_info(cls, provider: str) -> Optional[Dict[str, Any]]:
        """Get information about a provider"""
        provider_lower = provider.lower()
        if provider_lower not in cls.MODEL_CATALOG:
            return None

        config = cls.MODEL_CATALOG[provider_lower]
        return {
            "name": provider_lower,
            "models": config.get("models", []),
            "default_model": config.get("default_model"),
            "api_key_env": config.get("api_key_env"),
            "local": config.get("api_key_env") is None,
        }

    @classmethod
    def get_default_model(cls, provider: str) -> Optional[str]:
        """Get default model for a provider"""
        provider_lower = provider.lower()
        if provider_lower not in cls.MODEL_CATALOG:
            return None
        return cls.MODEL_CATALOG[provider_lower].get("default_model")

    @classmethod
    def is_local_provider(cls, provider: str) -> bool:
        """Check if provider runs locally (no API key needed)"""
        provider_lower = provider.lower()
        if provider_lower not in cls.MODEL_CATALOG:
            return False
        return cls.MODEL_CATALOG[provider_lower].get("api_key_env") is None

    @classmethod
    def get_required_api_key(cls, provider: str) -> Optional[str]:
        """Get the required API key environment variable name"""
        provider_lower = provider.lower()
        if provider_lower not in cls.MODEL_CATALOG:
            return None
        return cls.MODEL_CATALOG[provider_lower].get("api_key_env")
