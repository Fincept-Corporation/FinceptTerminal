"""
Embedder Registry - Unified access to 18+ embedding providers

Provides lazy loading and standardized embedder creation for RAG.
"""

from typing import Dict, Any, Optional, List
import logging
import os

logger = logging.getLogger(__name__)


class EmbedderRegistry:
    """
    Central registry for all Agno embedders.
    Handles API key management and embedder instantiation.
    """

    # Embedder providers and their configurations
    EMBEDDER_CATALOG = {
        # OpenAI
        "openai": {
            "class": "agno.knowledge.embedder.openai.OpenAIEmbedder",
            "models": ["text-embedding-3-small", "text-embedding-3-large", "text-embedding-ada-002"],
            "api_key_env": "OPENAI_API_KEY",
            "default_model": "text-embedding-3-small",
            "dimensions": {"text-embedding-3-small": 1536, "text-embedding-3-large": 3072, "text-embedding-ada-002": 1536},
        },

        # Azure OpenAI
        "azure_openai": {
            "class": "agno.knowledge.embedder.azure_openai.AzureOpenAIEmbedder",
            "models": ["text-embedding-3-small", "text-embedding-3-large"],
            "api_key_env": "AZURE_OPENAI_API_KEY",
            "default_model": "text-embedding-3-small",
        },

        # Google
        "google": {
            "class": "agno.knowledge.embedder.google.GoogleEmbedder",
            "models": ["text-embedding-004", "textembedding-gecko@003"],
            "api_key_env": "GOOGLE_API_KEY",
            "default_model": "text-embedding-004",
        },

        # AWS Bedrock
        "aws_bedrock": {
            "class": "agno.knowledge.embedder.aws_bedrock.AWSBedrockEmbedder",
            "models": ["amazon.titan-embed-text-v1", "cohere.embed-english-v3"],
            "api_key_env": "AWS_ACCESS_KEY_ID",
            "default_model": "amazon.titan-embed-text-v1",
        },

        # Cohere
        "cohere": {
            "class": "agno.knowledge.embedder.cohere.CohereEmbedder",
            "models": ["embed-english-v3.0", "embed-multilingual-v3.0", "embed-english-light-v3.0"],
            "api_key_env": "COHERE_API_KEY",
            "default_model": "embed-english-v3.0",
        },

        # Mistral
        "mistral": {
            "class": "agno.knowledge.embedder.mistral.MistralEmbedder",
            "models": ["mistral-embed"],
            "api_key_env": "MISTRAL_API_KEY",
            "default_model": "mistral-embed",
        },

        # Voyage AI
        "voyageai": {
            "class": "agno.knowledge.embedder.voyageai.VoyageAIEmbedder",
            "models": ["voyage-large-2", "voyage-code-2", "voyage-2"],
            "api_key_env": "VOYAGE_API_KEY",
            "default_model": "voyage-large-2",
        },

        # Jina
        "jina": {
            "class": "agno.knowledge.embedder.jina.JinaEmbedder",
            "models": ["jina-embeddings-v3", "jina-embeddings-v2-base-en"],
            "api_key_env": "JINA_API_KEY",
            "default_model": "jina-embeddings-v3",
        },

        # Together AI
        "together": {
            "class": "agno.knowledge.embedder.together.TogetherEmbedder",
            "models": ["togethercomputer/m2-bert-80M-8k-retrieval"],
            "api_key_env": "TOGETHER_API_KEY",
            "default_model": "togethercomputer/m2-bert-80M-8k-retrieval",
        },

        # Fireworks
        "fireworks": {
            "class": "agno.knowledge.embedder.fireworks.FireworksEmbedder",
            "models": ["nomic-ai/nomic-embed-text-v1.5"],
            "api_key_env": "FIREWORKS_API_KEY",
            "default_model": "nomic-ai/nomic-embed-text-v1.5",
        },

        # Local Models
        "ollama": {
            "class": "agno.knowledge.embedder.ollama.OllamaEmbedder",
            "models": ["nomic-embed-text", "mxbai-embed-large", "all-minilm"],
            "api_key_env": None,
            "default_model": "nomic-embed-text",
            "base_url": "http://localhost:11434",
        },

        # HuggingFace
        "huggingface": {
            "class": "agno.knowledge.embedder.huggingface.HuggingFaceEmbedder",
            "models": [],
            "api_key_env": "HUGGINGFACE_API_KEY",
            "default_model": None,
        },

        # Sentence Transformers (Local)
        "sentence_transformer": {
            "class": "agno.knowledge.embedder.sentence_transformer.SentenceTransformerEmbedder",
            "models": ["all-MiniLM-L6-v2", "all-mpnet-base-v2", "paraphrase-multilingual-MiniLM-L12-v2"],
            "api_key_env": None,
            "default_model": "all-MiniLM-L6-v2",
        },

        # FastEmbed (Local, optimized)
        "fastembed": {
            "class": "agno.knowledge.embedder.fastembed.FastEmbedEmbedder",
            "models": ["BAAI/bge-small-en-v1.5", "BAAI/bge-base-en-v1.5", "sentence-transformers/all-MiniLM-L6-v2"],
            "api_key_env": None,
            "default_model": "BAAI/bge-small-en-v1.5",
        },

        # vLLM
        "vllm": {
            "class": "agno.knowledge.embedder.vllm.vLLMEmbedder",
            "models": [],
            "api_key_env": None,
            "default_model": None,
        },

        # Nebius
        "nebius": {
            "class": "agno.knowledge.embedder.nebius.NebiusEmbedder",
            "models": [],
            "api_key_env": "NEBIUS_API_KEY",
            "default_model": None,
        },

        # LangDB
        "langdb": {
            "class": "agno.knowledge.embedder.langdb.LangDBEmbedder",
            "models": [],
            "api_key_env": "LANGDB_API_KEY",
            "default_model": None,
        },
    }

    _loaded_embedders: Dict[str, Any] = {}

    @classmethod
    def create_embedder(
        cls,
        provider: str,
        model: Optional[str] = None,
        api_key: Optional[str] = None,
        api_keys: Optional[Dict[str, str]] = None,
        **kwargs
    ) -> Any:
        """
        Create an embedder instance.

        Args:
            provider: Embedder provider (e.g., 'openai', 'cohere')
            model: Specific model (uses default if not provided)
            api_key: Direct API key
            api_keys: Dict of API keys
            **kwargs: Additional arguments

        Returns:
            Embedder instance

        Raises:
            ValueError: If provider not found
        """
        provider_lower = provider.lower()

        if provider_lower not in cls.EMBEDDER_CATALOG:
            raise ValueError(f"Unknown embedder: {provider}. Available: {list(cls.EMBEDDER_CATALOG.keys())}")

        config = cls.EMBEDDER_CATALOG[provider_lower]

        # Resolve model
        final_model = model or config.get("default_model")

        # Resolve API key
        final_api_key = api_key
        if not final_api_key and api_keys:
            env_key = config.get("api_key_env")
            if env_key:
                final_api_key = api_keys.get(env_key)

        if not final_api_key and config.get("api_key_env"):
            final_api_key = os.environ.get(config["api_key_env"])

        try:
            # Dynamic import
            import importlib
            class_path = config["class"]
            module_path, class_name = class_path.rsplit(".", 1)
            module = importlib.import_module(module_path)
            embedder_class = getattr(module, class_name)

            # Build constructor arguments
            embedder_kwargs = {}

            if final_model:
                embedder_kwargs["model"] = final_model
            if final_api_key:
                embedder_kwargs["api_key"] = final_api_key
            if config.get("base_url"):
                embedder_kwargs["base_url"] = kwargs.pop("base_url", config["base_url"])

            embedder_kwargs.update(kwargs)

            embedder_instance = embedder_class(**embedder_kwargs)
            logger.debug(f"Created embedder: {provider}/{final_model}")

            return embedder_instance

        except ImportError as e:
            raise ImportError(f"Failed to import embedder '{provider}': {e}")
        except Exception as e:
            raise RuntimeError(f"Failed to create embedder '{provider}': {e}")

    @classmethod
    def list_providers(cls) -> List[str]:
        """List all available embedder providers"""
        return list(cls.EMBEDDER_CATALOG.keys())

    @classmethod
    def list_models(cls, provider: str) -> List[str]:
        """List models for an embedder provider"""
        provider_lower = provider.lower()
        if provider_lower not in cls.EMBEDDER_CATALOG:
            return []
        return cls.EMBEDDER_CATALOG[provider_lower].get("models", [])

    @classmethod
    def get_provider_info(cls, provider: str) -> Optional[Dict[str, Any]]:
        """Get information about an embedder provider"""
        provider_lower = provider.lower()
        if provider_lower not in cls.EMBEDDER_CATALOG:
            return None

        config = cls.EMBEDDER_CATALOG[provider_lower]
        return {
            "name": provider_lower,
            "models": config.get("models", []),
            "default_model": config.get("default_model"),
            "api_key_env": config.get("api_key_env"),
            "local": config.get("api_key_env") is None,
            "dimensions": config.get("dimensions", {}),
        }

    @classmethod
    def get_default_model(cls, provider: str) -> Optional[str]:
        """Get default model for an embedder"""
        provider_lower = provider.lower()
        if provider_lower not in cls.EMBEDDER_CATALOG:
            return None
        return cls.EMBEDDER_CATALOG[provider_lower].get("default_model")

    @classmethod
    def is_local_embedder(cls, provider: str) -> bool:
        """Check if embedder runs locally"""
        provider_lower = provider.lower()
        if provider_lower not in cls.EMBEDDER_CATALOG:
            return False
        return cls.EMBEDDER_CATALOG[provider_lower].get("api_key_env") is None

    @classmethod
    def get_local_embedders(cls) -> List[str]:
        """Get list of local embedders"""
        return [
            name for name, config in cls.EMBEDDER_CATALOG.items()
            if config.get("api_key_env") is None
        ]

    @classmethod
    def get_dimensions(cls, provider: str, model: Optional[str] = None) -> Optional[int]:
        """Get embedding dimensions for a model"""
        provider_lower = provider.lower()
        if provider_lower not in cls.EMBEDDER_CATALOG:
            return None

        config = cls.EMBEDDER_CATALOG[provider_lower]
        dimensions = config.get("dimensions", {})

        if model and model in dimensions:
            return dimensions[model]

        default_model = config.get("default_model")
        if default_model and default_model in dimensions:
            return dimensions[default_model]

        return None
