"""
VectorDB Registry - Unified access to 20+ vector databases

Provides lazy loading and standardized vectordb creation for RAG.
"""

from typing import Dict, Any, Optional, List
import logging

logger = logging.getLogger(__name__)


class VectorDBRegistry:
    """
    Central registry for all Agno vector databases.
    Handles configuration and instantiation of vector stores.
    """

    # VectorDB providers and their configurations
    VECTORDB_CATALOG = {
        # PostgreSQL-based
        "pgvector": {
            "class": "agno.vectordb.pgvector.PgVector",
            "config_keys": ["table_name", "db_url", "schema"],
            "description": "PostgreSQL with pgvector extension",
        },

        # Cloud Vector DBs
        "pinecone": {
            "class": "agno.vectordb.pineconedb.Pinecone",
            "config_keys": ["api_key", "index_name", "namespace", "environment"],
            "api_key_env": "PINECONE_API_KEY",
            "description": "Pinecone managed vector database",
        },
        "qdrant": {
            "class": "agno.vectordb.qdrant.Qdrant",
            "config_keys": ["collection_name", "url", "api_key", "host", "port"],
            "api_key_env": "QDRANT_API_KEY",
            "description": "Qdrant vector database",
        },
        "weaviate": {
            "class": "agno.vectordb.weaviate.Weaviate",
            "config_keys": ["class_name", "url", "api_key"],
            "api_key_env": "WEAVIATE_API_KEY",
            "description": "Weaviate vector database",
        },
        "milvus": {
            "class": "agno.vectordb.milvus.Milvus",
            "config_keys": ["collection_name", "uri", "token"],
            "description": "Milvus vector database",
        },

        # Lightweight / Local
        "chroma": {
            "class": "agno.vectordb.chroma.Chroma",
            "config_keys": ["collection_name", "path", "host", "port"],
            "description": "ChromaDB - lightweight local vector store",
        },
        "lancedb": {
            "class": "agno.vectordb.lancedb.LanceDB",
            "config_keys": ["table_name", "uri", "db_path"],
            "description": "LanceDB - serverless vector database",
        },

        # Redis-based
        "redis": {
            "class": "agno.vectordb.redis.Redis",
            "config_keys": ["index_name", "url", "host", "port"],
            "description": "Redis with vector search",
        },
        "upstash": {
            "class": "agno.vectordb.upstashdb.Upstash",
            "config_keys": ["index_name", "url", "token"],
            "api_key_env": "UPSTASH_VECTOR_REST_TOKEN",
            "description": "Upstash serverless Redis vector",
        },

        # Database-integrated
        "mongodb": {
            "class": "agno.vectordb.mongodb.MongoDB",
            "config_keys": ["collection_name", "connection_string", "database_name", "index_name"],
            "description": "MongoDB Atlas vector search",
        },
        "cassandra": {
            "class": "agno.vectordb.cassandra.Cassandra",
            "config_keys": ["table_name", "keyspace", "contact_points"],
            "description": "Apache Cassandra with vector support",
        },
        "couchbase": {
            "class": "agno.vectordb.couchbase.Couchbase",
            "config_keys": ["bucket_name", "scope_name", "collection_name", "connection_string"],
            "description": "Couchbase vector search",
        },
        "clickhouse": {
            "class": "agno.vectordb.clickhouse.ClickHouse",
            "config_keys": ["table_name", "host", "port", "database"],
            "description": "ClickHouse with vector support",
        },
        "singlestore": {
            "class": "agno.vectordb.singlestore.SingleStore",
            "config_keys": ["table_name", "host", "port", "database"],
            "description": "SingleStore vector database",
        },
        "surrealdb": {
            "class": "agno.vectordb.surrealdb.SurrealDB",
            "config_keys": ["table_name", "url", "namespace", "database"],
            "description": "SurrealDB vector search",
        },

        # Framework integrations
        "langchain": {
            "class": "agno.vectordb.langchaindb.LangchainDB",
            "config_keys": ["vectorstore"],
            "description": "LangChain vectorstore wrapper",
        },
        "llamaindex": {
            "class": "agno.vectordb.llamaindex.LlamaIndex",
            "config_keys": ["index"],
            "description": "LlamaIndex vector index wrapper",
        },

        # Graph-based
        "lightrag": {
            "class": "agno.vectordb.lightrag.LightRAG",
            "config_keys": ["working_dir"],
            "description": "LightRAG graph-based retrieval",
        },
    }

    _loaded_dbs: Dict[str, Any] = {}

    @classmethod
    def create_vectordb(
        cls,
        db_type: str,
        embedder: Any = None,
        **config
    ) -> Any:
        """
        Create a vector database instance.

        Args:
            db_type: Type of vectordb (e.g., 'pgvector', 'pinecone')
            embedder: Embedder instance for creating vectors
            **config: Database-specific configuration

        Returns:
            VectorDB instance

        Raises:
            ValueError: If db_type not found
        """
        db_type_lower = db_type.lower()

        if db_type_lower not in cls.VECTORDB_CATALOG:
            raise ValueError(f"Unknown vectordb: {db_type}. Available: {list(cls.VECTORDB_CATALOG.keys())}")

        db_config = cls.VECTORDB_CATALOG[db_type_lower]

        try:
            # Dynamic import
            import importlib
            class_path = db_config["class"]
            module_path, class_name = class_path.rsplit(".", 1)
            module = importlib.import_module(module_path)
            db_class = getattr(module, class_name)

            # Build constructor arguments
            db_kwargs = {}

            # Add embedder if provided
            if embedder:
                db_kwargs["embedder"] = embedder

            # Add configuration
            for key in db_config.get("config_keys", []):
                if key in config:
                    db_kwargs[key] = config[key]

            # Add any extra config
            for key, value in config.items():
                if key not in db_kwargs:
                    db_kwargs[key] = value

            db_instance = db_class(**db_kwargs)
            logger.debug(f"Created vectordb: {db_type}")

            return db_instance

        except ImportError as e:
            raise ImportError(f"Failed to import vectordb '{db_type}': {e}")
        except Exception as e:
            raise RuntimeError(f"Failed to create vectordb '{db_type}': {e}")

    @classmethod
    def list_vectordbs(cls) -> List[str]:
        """List all available vector databases"""
        return list(cls.VECTORDB_CATALOG.keys())

    @classmethod
    def get_vectordb_info(cls, db_type: str) -> Optional[Dict[str, Any]]:
        """Get information about a vector database"""
        db_type_lower = db_type.lower()
        if db_type_lower not in cls.VECTORDB_CATALOG:
            return None

        config = cls.VECTORDB_CATALOG[db_type_lower]
        return {
            "name": db_type_lower,
            "description": config.get("description", ""),
            "config_keys": config.get("config_keys", []),
            "api_key_env": config.get("api_key_env"),
        }

    @classmethod
    def get_required_config(cls, db_type: str) -> List[str]:
        """Get required configuration keys for a vectordb"""
        db_type_lower = db_type.lower()
        if db_type_lower not in cls.VECTORDB_CATALOG:
            return []
        return cls.VECTORDB_CATALOG[db_type_lower].get("config_keys", [])

    @classmethod
    def is_cloud_db(cls, db_type: str) -> bool:
        """Check if vectordb requires API key (cloud service)"""
        db_type_lower = db_type.lower()
        if db_type_lower not in cls.VECTORDB_CATALOG:
            return False
        return cls.VECTORDB_CATALOG[db_type_lower].get("api_key_env") is not None

    @classmethod
    def get_local_vectordbs(cls) -> List[str]:
        """Get list of local/self-hosted vectordbs"""
        return [
            name for name, config in cls.VECTORDB_CATALOG.items()
            if config.get("api_key_env") is None
        ]

    @classmethod
    def get_cloud_vectordbs(cls) -> List[str]:
        """Get list of cloud vectordbs"""
        return [
            name for name, config in cls.VECTORDB_CATALOG.items()
            if config.get("api_key_env") is not None
        ]
