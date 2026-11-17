"""
Knowledge Module - RAG (Retrieval Augmented Generation) system

Provides:
- Document sources (URLs, files) indexed into a vector store
- Vector embedding and storage
- Semantic search
- Knowledge base management

Written against the agno 2.x Knowledge API: sources are added with
Knowledge.insert(url=... | path=... | text_content=..., reader=...), which reads,
chunks, embeds and stores them in the vector DB. (The 1.x-era code read the
documents into a Python list that never reached the vector DB, imported reader
classes that no longer exist, passed `model=` to embedders that take `id=`, and
defaulted to LanceDB, which is not installed — so the option never worked.)

Anything that keeps the knowledge base from being built raises
AgentFeatureUnavailable with a message naming what is missing; a source that
fails while others succeed is reported in `warnings`.
"""

from typing import Dict, Any, Optional, List, Union
from pathlib import Path
import logging
import re

from finagent_core.run_errors import AgentFeatureUnavailable

logger = logging.getLogger(__name__)

# Vector store used when the config names none. Built in (SQLite + numpy), so it
# works on every install — see registries/local_vectordb.py.
DEFAULT_VECTORDB = "local"

# Embedders tried, in order, when the config names none: the first whose API key
# is configured wins; otherwise "openai" (and its missing key is reported).
_KEYED_EMBEDDERS = ("openai", "google", "cohere", "mistral", "voyageai", "jina",
                    "together", "fireworks")


class _AgnoErrorCapture(logging.Handler):
    """Collects ERROR records agno logs while it reads/embeds a source. agno's
    Knowledge.insert logs a failed source and returns normally, so this is the
    only place the reason is available."""

    def __init__(self):
        super().__init__(level=logging.ERROR)
        self.messages: List[str] = []

    def emit(self, record: logging.LogRecord) -> None:
        try:
            self.messages.append(record.getMessage())
        except Exception:
            pass


class KnowledgeModule:
    """
    RAG knowledge base management.

    Supports:
    - Multiple document types (PDF, text, web, etc.)
    - Various vector databases (built-in local store by default)
    - Multiple embedding providers
    """

    # Supported document types -> agno 2.x reader class. None = let agno pick
    # the reader from the URL / file extension.
    DOCUMENT_TYPES = {
        "url": None,
        "pdf": "agno.knowledge.reader.pdf_reader.PDFReader",
        "text": "agno.knowledge.reader.text_reader.TextReader",
        "markdown": "agno.knowledge.reader.markdown_reader.MarkdownReader",
        "csv": "agno.knowledge.reader.csv_reader.CSVReader",
        "json": "agno.knowledge.reader.json_reader.JSONReader",
        "docx": "agno.knowledge.reader.docx_reader.DocxReader",
        "website": "agno.knowledge.reader.website_reader.WebsiteReader",
        "arxiv": "agno.knowledge.reader.arxiv_reader.ArxivReader",
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
        embedder_provider: Optional[str] = None,
        embedder_model: Optional[str] = None,
        vectordb_type: str = DEFAULT_VECTORDB,
        vectordb_config: Optional[Dict[str, Any]] = None,
        chunk_size: int = 1000,
        chunk_overlap: int = 200,
        chunking_strategy: str = "recursive",
        api_keys: Optional[Dict[str, str]] = None,
        path: Optional[str] = None,
        **kwargs
    ):
        """
        Initialize knowledge module.

        Args:
            name: Knowledge base name
            embedder_provider: Embedding provider ('openai', 'ollama', ...)
            embedder_model: Specific embedding model
            vectordb_type: Vector database type ('local' = built-in store)
            vectordb_config: Vector database configuration
            chunk_size: Size of text chunks
            chunk_overlap: Overlap between chunks
            chunking_strategy: Chunking strategy to use
            api_keys: API keys for providers
            path: Directory for on-disk vector stores (per persona)
            **kwargs: Additional configuration
        """
        self.name = name
        self.api_keys = api_keys or {}
        if not embedder_provider:
            from finagent_core.registries import EmbedderRegistry
            embedder_provider = next(
                (p for p in _KEYED_EMBEDDERS if EmbedderRegistry.resolve_api_key(p, self.api_keys)),
                "openai",
            )
        self.embedder_provider = embedder_provider
        self.embedder_model = embedder_model
        self.vectordb_type = (vectordb_type or DEFAULT_VECTORDB).lower()
        self.vectordb_config = vectordb_config or {}
        self.chunk_size = chunk_size
        self.chunk_overlap = chunk_overlap
        self.chunking_strategy = chunking_strategy
        self.path = path
        self.config = kwargs
        self.warnings: List[str] = []

        self._embedder = None
        self._vectordb = None
        self._knowledge = None
        # (source, doc_type, reader options) — indexed by build()
        self._sources: List[Dict[str, Any]] = []
        self._texts: List[Dict[str, Any]] = []

    def _fail(self, reason: str) -> AgentFeatureUnavailable:
        return AgentFeatureUnavailable(f"Knowledge base unavailable: {reason}")

    def _create_embedder(self) -> Any:
        """Create embedder instance (raises with the reason it cannot be built)."""
        if self._embedder:
            return self._embedder

        from finagent_core.registries import EmbedderRegistry

        problem = EmbedderRegistry.missing_requirement(self.embedder_provider, self.api_keys)
        if problem:
            hint = "" if self.embedder_provider == "ollama" else \
                " Add the key in Settings, or choose the 'ollama' embedder (runs locally)."
            raise self._fail(problem + "." + hint)
        try:
            self._embedder = EmbedderRegistry.create_embedder(
                provider=self.embedder_provider,
                model=self.embedder_model,
                api_keys=self.api_keys,
            )
        except Exception as e:
            raise self._fail(str(e)) from e
        return self._embedder

    def _table_name(self) -> str:
        return re.sub(r"[^a-z0-9_]+", "_", (self.name or "knowledge").lower()).strip("_") or "knowledge"

    def _create_vectordb(self) -> Any:
        """Create vector database instance (raises with the reason it cannot)."""
        if self._vectordb:
            return self._vectordb

        from finagent_core.registries import VectorDBRegistry

        db_type = self.vectordb_type
        reason = VectorDBRegistry.unavailable_reason(db_type)
        if reason and db_type == "lancedb":
            # LanceDB is the CREATE tab's default choice, and it is not part of
            # the terminal's requirements. Use the built-in store instead of
            # failing every agent that kept the default — and say so.
            self.warnings.append(
                f"Vector DB 'lancedb' is not available ({reason}); "
                f"the knowledge base uses the built-in local store instead.")
            db_type = DEFAULT_VECTORDB
            reason = VectorDBRegistry.unavailable_reason(db_type)
        if reason:
            raise self._fail(f"vector DB '{db_type}' {reason}")

        embedder = self._create_embedder()
        db_kwargs = VectorDBRegistry.placement_kwargs(db_type, self.path, self._table_name())
        db_kwargs.update(self.vectordb_config)
        if db_type in ("local", "chroma", "lancedb", "qdrant") and not self.path and not any(
                k in db_kwargs for k in ("path", "uri", "url", "db_file", "host")):
            raise self._fail(f"vector DB '{db_type}' needs a directory (path)")
        try:
            self._vectordb = VectorDBRegistry.create_vectordb(
                db_type=db_type, embedder=embedder, **db_kwargs)
        except Exception as e:
            raise self._fail(str(e)) from e
        self.vectordb_type = db_type
        return self._vectordb

    def add_documents(
        self,
        source: Union[str, Path, List[str]],
        doc_type: str = "text",
        **kwargs
    ) -> "KnowledgeModule":
        """
        Queue document sources for indexing (done by build()).

        Args:
            source: Document source (path, URL, or list)
            doc_type: Document type ('pdf', 'text', 'url', etc.)
            **kwargs: Additional reader arguments

        Returns:
            self for chaining
        """
        doc_type = (doc_type or "text").lower()
        if doc_type not in self.DOCUMENT_TYPES:
            raise self._fail(f"unsupported document type '{doc_type}' "
                             f"(supported: {', '.join(self.DOCUMENT_TYPES)})")
        sources = [str(source)] if isinstance(source, (str, Path)) else [str(s) for s in source or []]
        for src in sources:
            if src.strip():
                self._sources.append({"source": src.strip(), "type": doc_type, "options": dict(kwargs)})
        self._knowledge = None  # rebuild with the new sources
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
        if text and text.strip():
            self._texts.append({"text": text, "metadata": metadata or {}})
            self._knowledge = None
        return self

    def _make_reader(self, doc_type: str, options: Dict[str, Any]) -> Optional[Any]:
        """Reader instance for a type (None = agno chooses). Raises if the
        reader's package is not installed."""
        reader_path = self.DOCUMENT_TYPES.get(doc_type)
        if not reader_path:
            return None
        from finagent_core.registries.availability import class_unavailable_reason, load_class
        reason = class_unavailable_reason(reader_path)
        if reason:
            raise self._fail(f"the '{doc_type}' reader {reason}")
        reader_class = load_class(reader_path)
        try:
            return reader_class(**options) if options else reader_class()
        except TypeError:
            return reader_class()

    def _insert_source(self, knowledge: Any, item: Dict[str, Any]) -> None:
        src, doc_type = item["source"], item["type"]
        reader = self._make_reader(doc_type, item.get("options") or {})
        is_url = src.lower().startswith(("http://", "https://"))
        if not is_url and not Path(src).expanduser().exists():
            raise FileNotFoundError(f"file not found: {src}")
        kwargs: Dict[str, Any] = {"name": src, "skip_if_exists": True, "upsert": False}
        if is_url:
            kwargs["url"] = src
        else:
            kwargs["path"] = str(Path(src).expanduser())
        if reader is not None:
            kwargs["reader"] = reader
        knowledge.insert(**kwargs)

    def build(self) -> Any:
        """
        Build the Agno Knowledge base and index every queued source.

        Returns:
            Agno Knowledge instance

        Raises:
            AgentFeatureUnavailable naming what is missing (embedder key,
            package, unreadable sources).
        """
        if self._knowledge:
            return self._knowledge

        try:
            from agno.knowledge.knowledge import Knowledge
        except ImportError as e:
            raise self._fail(f"agno Knowledge is not available ({e})") from e

        vectordb = self._create_vectordb()
        knowledge = Knowledge(name=self.name, vector_db=vectordb)

        capture = _AgnoErrorCapture()
        agno_logger = logging.getLogger("agno")
        agno_logger.addHandler(capture)
        failures: List[str] = []
        try:
            for item in self._sources:
                before = len(capture.messages)
                try:
                    self._insert_source(knowledge, item)
                except AgentFeatureUnavailable:
                    raise
                except Exception as e:
                    failures.append(f"{item['source']}: {e}")
                    continue
                if len(capture.messages) > before:
                    failures.append(f"{item['source']}: {capture.messages[before]}")
            for t in self._texts:
                try:
                    knowledge.insert(text_content=t["text"], metadata=t["metadata"] or None,
                                     skip_if_exists=True, upsert=False)
                except Exception as e:
                    failures.append(f"text: {e}")
        finally:
            agno_logger.removeHandler(capture)

        stored = vectordb.count() if hasattr(vectordb, "count") else None
        wanted = len(self._sources) + len(self._texts)
        if wanted and stored == 0:
            detail = "; ".join(failures[:3]) or "no text could be read from them"
            raise self._fail(f"none of the {wanted} source(s) could be read and embedded ({detail})")
        for f in failures:
            self.warnings.append(f"Knowledge source skipped — {f}")

        logger.debug(f"Built knowledge base '{self.name}' ({stored} chunks)")
        self._knowledge = knowledge
        return knowledge

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
            **kwargs: Additional search parameters (filters)

        Returns:
            List of {content, name, meta_data, score}
        """
        knowledge = self.build()
        docs = knowledge.search(query=query, max_results=limit, filters=kwargs.get("filters"))
        out = []
        for d in docs or []:
            out.append({
                "content": getattr(d, "content", str(d)),
                "name": getattr(d, "name", None),
                "meta_data": getattr(d, "meta_data", None) or {},
                "score": getattr(d, "reranking_score", None),
            })
        return out

    def get_knowledge(self) -> Optional[Any]:
        """Get the built knowledge instance"""
        return self._knowledge

    def get_document_count(self) -> int:
        """Number of queued sources (URLs/files/texts)"""
        return len(self._sources) + len(self._texts)

    def to_agent_config(self) -> Dict[str, Any]:
        """
        Convert knowledge module to agent configuration.

        Returns:
            Dict to pass to Agent constructor (raises if it cannot be built)
        """
        return {"knowledge": self.build()}

    @classmethod
    def from_config(cls, config: Dict[str, Any]) -> "KnowledgeModule":
        """
        Create KnowledgeModule from configuration.

        Args:
            config: {name, embedder{provider, model}, vectordb{type, ...},
                     documents[{source, type, options}], path, api_keys}

        Returns:
            KnowledgeModule instance
        """
        vectordb = dict(config.get("vectordb") or {})
        # `type` selects the backend; passed through it became an unexpected
        # constructor kwarg (VectorDBRegistry forwards every remaining key).
        vectordb_type = vectordb.pop("type", None) or DEFAULT_VECTORDB
        embedder = config.get("embedder") or {}
        module = cls(
            name=config.get("name", "Knowledge Base"),
            embedder_provider=embedder.get("provider"),
            embedder_model=embedder.get("model") or embedder.get("id"),
            vectordb_type=vectordb_type,
            vectordb_config=vectordb,
            chunk_size=config.get("chunk_size", 1000),
            chunk_overlap=config.get("chunk_overlap", 200),
            chunking_strategy=config.get("chunking_strategy", "recursive"),
            api_keys=config.get("api_keys", {}),
            path=str(config["path"]) if config.get("path") else None,
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
    Simple keyword-matching knowledge store (kept for direct users; the agent
    builder no longer falls back to it silently).
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
            .with_vectordb("local", path="./data/vectors")
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
        """Set vector database (a `path` entry is the store's directory)"""
        self._module.vectordb_type = db_type
        path = config.pop("path", None)
        if path:
            self._module.path = str(path)
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
