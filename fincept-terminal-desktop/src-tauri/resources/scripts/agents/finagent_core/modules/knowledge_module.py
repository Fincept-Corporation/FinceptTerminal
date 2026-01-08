"""
Knowledge Module - RAG (Retrieval Augmented Generation) system

Provides:
- Document loading and chunking
- Vector embedding and storage
- Semantic search
- Knowledge base management
"""

from typing import Dict, Any, Optional, List, Union
from pathlib import Path
import logging

logger = logging.getLogger(__name__)


class KnowledgeModule:
    """
    RAG knowledge base management.

    Supports:
    - Multiple document types (PDF, text, web, etc.)
    - Various vector databases
    - Multiple embedding providers
    - Chunking strategies
    """

    # Supported document types
    DOCUMENT_TYPES = {
        "pdf": "agno.knowledge.reader.PDFReader",
        "text": "agno.knowledge.reader.TextReader",
        "csv": "agno.knowledge.reader.CSVReader",
        "json": "agno.knowledge.reader.JSONReader",
        "docx": "agno.knowledge.reader.DocxReader",
        "url": "agno.knowledge.reader.URLReader",
        "website": "agno.knowledge.reader.WebsiteReader",
        "arxiv": "agno.knowledge.reader.ArxivReader",
        "github": "agno.knowledge.reader.GithubReader",
    }

    # Chunking strategies
    CHUNKING_STRATEGIES = {
        "fixed": "Fixed size chunks",
        "semantic": "Semantic chunking based on content",
        "sentence": "Sentence-based chunking",
        "paragraph": "Paragraph-based chunking",
        "recursive": "Recursive character text splitter",
    }

    def __init__(
        self,
        name: str = "Knowledge Base",
        embedder_provider: str = "openai",
        embedder_model: Optional[str] = None,
        vectordb_type: str = "lancedb",
        vectordb_config: Optional[Dict[str, Any]] = None,
        chunk_size: int = 1000,
        chunk_overlap: int = 200,
        chunking_strategy: str = "recursive",
        api_keys: Optional[Dict[str, str]] = None,
        **kwargs
    ):
        """
        Initialize knowledge module.

        Args:
            name: Knowledge base name
            embedder_provider: Embedding provider ('openai', 'cohere', etc.)
            embedder_model: Specific embedding model
            vectordb_type: Vector database type
            vectordb_config: Vector database configuration
            chunk_size: Size of text chunks
            chunk_overlap: Overlap between chunks
            chunking_strategy: Chunking strategy to use
            api_keys: API keys for providers
            **kwargs: Additional configuration
        """
        self.name = name
        self.embedder_provider = embedder_provider
        self.embedder_model = embedder_model
        self.vectordb_type = vectordb_type
        self.vectordb_config = vectordb_config or {}
        self.chunk_size = chunk_size
        self.chunk_overlap = chunk_overlap
        self.chunking_strategy = chunking_strategy
        self.api_keys = api_keys or {}
        self.config = kwargs

        self._embedder = None
        self._vectordb = None
        self._knowledge = None
        self._documents: List[Any] = []

    def _create_embedder(self) -> Any:
        """Create embedder instance"""
        if self._embedder:
            return self._embedder

        from finagent_core.registries import EmbedderRegistry

        self._embedder = EmbedderRegistry.create_embedder(
            provider=self.embedder_provider,
            model=self.embedder_model,
            api_keys=self.api_keys
        )

        return self._embedder

    def _create_vectordb(self) -> Any:
        """Create vector database instance"""
        if self._vectordb:
            return self._vectordb

        from finagent_core.registries import VectorDBRegistry

        embedder = self._create_embedder()

        self._vectordb = VectorDBRegistry.create_vectordb(
            db_type=self.vectordb_type,
            embedder=embedder,
            **self.vectordb_config
        )

        return self._vectordb

    def add_documents(
        self,
        source: Union[str, Path, List[str]],
        doc_type: str = "text",
        **kwargs
    ) -> "KnowledgeModule":
        """
        Add documents to the knowledge base.

        Args:
            source: Document source (path, URL, or list)
            doc_type: Document type ('pdf', 'text', 'url', etc.)
            **kwargs: Additional reader arguments

        Returns:
            self for chaining
        """
        try:
            reader = self._get_reader(doc_type)
            if reader is None:
                logger.warning(f"No reader available for type: {doc_type}")
                return self

            # Handle different source types
            if isinstance(source, (str, Path)):
                sources = [str(source)]
            else:
                sources = [str(s) for s in source]

            for src in sources:
                try:
                    docs = reader.read(src, **kwargs)
                    if isinstance(docs, list):
                        self._documents.extend(docs)
                    else:
                        self._documents.append(docs)
                    logger.debug(f"Added document from: {src}")
                except Exception as e:
                    logger.error(f"Failed to read document {src}: {e}")

        except Exception as e:
            logger.error(f"Failed to add documents: {e}")

        return self

    def add_pdf(self, path: Union[str, Path], **kwargs) -> "KnowledgeModule":
        """Add PDF document"""
        return self.add_documents(path, doc_type="pdf", **kwargs)

    def add_text(self, path: Union[str, Path], **kwargs) -> "KnowledgeModule":
        """Add text document"""
        return self.add_documents(path, doc_type="text", **kwargs)

    def add_url(self, url: str, **kwargs) -> "KnowledgeModule":
        """Add web URL"""
        return self.add_documents(url, doc_type="url", **kwargs)

    def add_website(self, url: str, max_depth: int = 1, **kwargs) -> "KnowledgeModule":
        """Add entire website"""
        return self.add_documents(url, doc_type="website", max_depth=max_depth, **kwargs)

    def add_raw_text(self, text: str, metadata: Optional[Dict] = None) -> "KnowledgeModule":
        """
        Add raw text directly.

        Args:
            text: Text content
            metadata: Optional metadata

        Returns:
            self for chaining
        """
        try:
            from agno.knowledge.document import Document

            doc = Document(
                content=text,
                metadata=metadata or {}
            )
            self._documents.append(doc)

        except ImportError:
            # Fallback to simple dict
            self._documents.append({
                "content": text,
                "metadata": metadata or {}
            })

        return self

    def _get_reader(self, doc_type: str) -> Optional[Any]:
        """Get document reader for type"""
        if doc_type not in self.DOCUMENT_TYPES:
            return None

        try:
            reader_path = self.DOCUMENT_TYPES[doc_type]
            module_path, class_name = reader_path.rsplit(".", 1)

            import importlib
            module = importlib.import_module(module_path)
            reader_class = getattr(module, class_name)

            return reader_class()

        except ImportError as e:
            logger.warning(f"Reader for {doc_type} not available: {e}")
            return None

    def build(self) -> Any:
        """
        Build the Agno Knowledge base.

        Returns:
            Agno Knowledge instance
        """
        if self._knowledge:
            return self._knowledge

        try:
            from agno.knowledge import Knowledge

            vectordb = self._create_vectordb()

            # Agno Knowledge uses: name, vector_db, readers, max_results
            # Documents are added via readers, not directly
            self._knowledge = Knowledge(
                name=self.name,
                vector_db=vectordb,
            )

            logger.debug(f"Built knowledge base '{self.name}' with {len(self._documents)} documents")
            return self._knowledge

        except ImportError as e:
            logger.warning(f"Agno Knowledge not available: {e}")
            return self._create_fallback_knowledge()
        except Exception as e:
            raise RuntimeError(f"Failed to build knowledge base: {e}")

    def _create_fallback_knowledge(self) -> "SimpleKnowledge":
        """Create a simple fallback knowledge system"""
        return SimpleKnowledge(
            documents=self._documents,
            embedder=self._embedder
        )

    def search(
        self,
        query: str,
        limit: int = 5,
        **kwargs
    ) -> List[Dict[str, Any]]:
        """
        Search the knowledge base.

        Args:
            query: Search query
            limit: Maximum results
            **kwargs: Additional search parameters

        Returns:
            List of relevant documents
        """
        knowledge = self.build()

        try:
            results = knowledge.search(query=query, limit=limit, **kwargs)
            return results
        except Exception as e:
            logger.error(f"Search failed: {e}")
            return []

    def get_knowledge(self) -> Optional[Any]:
        """Get the built knowledge instance"""
        return self._knowledge

    def get_document_count(self) -> int:
        """Get number of documents"""
        return len(self._documents)

    def to_agent_config(self) -> Dict[str, Any]:
        """
        Convert knowledge module to agent configuration.

        Returns:
            Dict to pass to Agent constructor
        """
        knowledge = self.build()
        return {"knowledge": knowledge} if knowledge else {}

    @classmethod
    def from_config(cls, config: Dict[str, Any]) -> "KnowledgeModule":
        """
        Create KnowledgeModule from configuration.

        Args:
            config: Knowledge configuration

        Returns:
            KnowledgeModule instance
        """
        module = cls(
            name=config.get("name", "Knowledge Base"),
            embedder_provider=config.get("embedder", {}).get("provider", "openai"),
            embedder_model=config.get("embedder", {}).get("model"),
            vectordb_type=config.get("vectordb", {}).get("type", "lancedb"),
            vectordb_config=config.get("vectordb", {}),
            chunk_size=config.get("chunk_size", 1000),
            chunk_overlap=config.get("chunk_overlap", 200),
            chunking_strategy=config.get("chunking_strategy", "recursive"),
            api_keys=config.get("api_keys", {}),
        )

        # Add documents from config
        for doc_config in config.get("documents", []):
            module.add_documents(
                source=doc_config.get("source"),
                doc_type=doc_config.get("type", "text"),
                **doc_config.get("options", {})
            )

        return module

    @classmethod
    def list_document_types(cls) -> List[str]:
        """List supported document types"""
        return list(cls.DOCUMENT_TYPES.keys())

    @classmethod
    def list_chunking_strategies(cls) -> Dict[str, str]:
        """List available chunking strategies"""
        return cls.CHUNKING_STRATEGIES.copy()


class SimpleKnowledge:
    """
    Simple fallback knowledge system when Agno Knowledge is not available.
    Uses basic text matching for search.
    """

    def __init__(self, documents: List[Any], embedder: Optional[Any] = None):
        self.documents = documents
        self.embedder = embedder

    def search(self, query: str, limit: int = 5, **kwargs) -> List[Dict[str, Any]]:
        """Simple text-based search"""
        query_lower = query.lower()
        results = []

        for doc in self.documents:
            content = doc.get("content", "") if isinstance(doc, dict) else str(doc)
            if query_lower in content.lower():
                results.append({
                    "content": content[:500],
                    "score": content.lower().count(query_lower) / len(content)
                })

        # Sort by score and limit
        results.sort(key=lambda x: x["score"], reverse=True)
        return results[:limit]


class KnowledgeBuilder:
    """
    Fluent builder for creating knowledge bases.

    Example:
        kb = (KnowledgeBuilder("Finance KB")
            .with_embedder("openai", "text-embedding-3-small")
            .with_vectordb("lancedb", path="./data/vectors")
            .add_pdf("reports/annual.pdf")
            .add_url("https://example.com/docs")
            .build())
    """

    def __init__(self, name: str):
        """Initialize knowledge builder"""
        self._module = KnowledgeModule(name=name)

    def with_embedder(
        self,
        provider: str,
        model: Optional[str] = None
    ) -> "KnowledgeBuilder":
        """Set embedder"""
        self._module.embedder_provider = provider
        self._module.embedder_model = model
        return self

    def with_vectordb(
        self,
        db_type: str,
        **config
    ) -> "KnowledgeBuilder":
        """Set vector database"""
        self._module.vectordb_type = db_type
        self._module.vectordb_config = config
        return self

    def with_chunking(
        self,
        chunk_size: int = 1000,
        chunk_overlap: int = 200,
        strategy: str = "recursive"
    ) -> "KnowledgeBuilder":
        """Set chunking parameters"""
        self._module.chunk_size = chunk_size
        self._module.chunk_overlap = chunk_overlap
        self._module.chunking_strategy = strategy
        return self

    def with_api_keys(self, api_keys: Dict[str, str]) -> "KnowledgeBuilder":
        """Set API keys"""
        self._module.api_keys = api_keys
        return self

    def add_pdf(self, path: Union[str, Path]) -> "KnowledgeBuilder":
        """Add PDF document"""
        self._module.add_pdf(path)
        return self

    def add_text(self, path: Union[str, Path]) -> "KnowledgeBuilder":
        """Add text document"""
        self._module.add_text(path)
        return self

    def add_url(self, url: str) -> "KnowledgeBuilder":
        """Add web URL"""
        self._module.add_url(url)
        return self

    def add_raw_text(self, text: str, metadata: Optional[Dict] = None) -> "KnowledgeBuilder":
        """Add raw text"""
        self._module.add_raw_text(text, metadata)
        return self

    def build(self) -> Any:
        """Build and return the knowledge base"""
        return self._module.build()

    def get_module(self) -> KnowledgeModule:
        """Get the underlying KnowledgeModule"""
        return self._module
