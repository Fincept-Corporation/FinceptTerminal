"""
Knowledge Base Infrastructure

Provides the core knowledge management system for Renaissance Technologies agents.
Uses Agno's Knowledge and VectorDB capabilities for semantic search.
"""

from typing import Optional, List, Dict, Any, Union
from dataclasses import dataclass, field
from enum import Enum
from datetime import datetime
import json
import os

# Agno imports - conditional
try:
    from agno.knowledge.base import Knowledge
    from agno.vectordb.lancedb import LanceDb
    from agno.document import Document
    AGNO_KNOWLEDGE_AVAILABLE = True
except ImportError:
    AGNO_KNOWLEDGE_AVAILABLE = False
    Knowledge = object  # type: ignore
    LanceDb = None  # type: ignore
    Document = None  # type: ignore

from ..config import get_config, RenTechConfig
from ..utils.embedder_factory import create_embedder_from_config


class KnowledgeCategory(str, Enum):
    """Categories of knowledge in the system"""
    MARKET_DATA = "market_data"
    RESEARCH = "research"
    STRATEGY = "strategy"
    TRADE_HISTORY = "trade_history"
    RISK_MODELS = "risk_models"
    EXECUTION = "execution"
    COMPLIANCE = "compliance"


@dataclass
class KnowledgeDocument:
    """
    A document in the knowledge base.

    Wraps content with metadata for proper indexing and retrieval.
    """
    id: str
    category: KnowledgeCategory
    title: str
    content: str
    metadata: Dict[str, Any] = field(default_factory=dict)
    created_at: str = field(default_factory=lambda: datetime.utcnow().isoformat())
    updated_at: str = field(default_factory=lambda: datetime.utcnow().isoformat())
    tags: List[str] = field(default_factory=list)

    # Relevance scoring
    importance: float = 0.5  # 0-1, higher = more important
    freshness_weight: float = 0.5  # How much freshness matters for this doc

    def to_agno_document(self) -> Document:
        """Convert to Agno Document format"""
        return Document(
            id=self.id,
            name=self.title,
            content=self.content,
            meta_data={
                "category": self.category.value,
                "created_at": self.created_at,
                "updated_at": self.updated_at,
                "tags": self.tags,
                "importance": self.importance,
                **self.metadata,
            },
        )

    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary"""
        return {
            "id": self.id,
            "category": self.category.value,
            "title": self.title,
            "content": self.content,
            "metadata": self.metadata,
            "created_at": self.created_at,
            "updated_at": self.updated_at,
            "tags": self.tags,
            "importance": self.importance,
            "freshness_weight": self.freshness_weight,
        }


class RenTechKnowledge:
    """
    Renaissance Technologies Knowledge Base.

    Unified knowledge management system that provides:
    - Semantic search across all knowledge categories
    - Category-specific filtering
    - Importance and freshness weighting
    - Easy integration with Agno agents

    Uses LanceDB for vector storage (local, no external dependencies).
    """

    def __init__(
        self,
        config: Optional[RenTechConfig] = None,
        db_path: Optional[str] = None,
        embedder_config: Optional[Dict[str, Any]] = None,
    ):
        """
        Initialize Renaissance Technologies Knowledge Base.

        Args:
            config: Optional RenTech configuration
            db_path: Optional path to vector database
            embedder_config: Optional embedder configuration override
                             If None, uses frontend config or falls back to Ollama
        """
        self.config = config or get_config()
        self.db_path = db_path or self.config.database.vector_db_path

        # Initialize embedder dynamically from frontend config
        # Falls back to Ollama (local, free) if no config found
        self.embedder = create_embedder_from_config(embedder_config)

        # Initialize vector databases for each category
        self._vector_dbs: Dict[KnowledgeCategory, LanceDb] = {}
        self._knowledge_stores: Dict[KnowledgeCategory, Knowledge] = {}

        # Document cache
        self._documents: Dict[str, KnowledgeDocument] = {}

    def _get_vector_db(self, category: KnowledgeCategory) -> LanceDb:
        """Get or create vector DB for a category"""
        if category not in self._vector_dbs:
            table_name = f"rentech_{category.value}"
            self._vector_dbs[category] = LanceDb(
                table_name=table_name,
                uri=self.db_path,
                embedder=self.embedder,
            )
        return self._vector_dbs[category]

    def _get_knowledge(self, category: KnowledgeCategory) -> Knowledge:
        """Get or create Knowledge instance for a category"""
        if category not in self._knowledge_stores:
            self._knowledge_stores[category] = Knowledge(
                vector_db=self._get_vector_db(category),
            )
        return self._knowledge_stores[category]

    def add_document(self, doc: KnowledgeDocument) -> str:
        """
        Add a document to the knowledge base.

        Args:
            doc: The document to add

        Returns:
            Document ID
        """
        # Store in cache
        self._documents[doc.id] = doc

        # Add to vector DB
        knowledge = self._get_knowledge(doc.category)
        agno_doc = doc.to_agno_document()

        # Load document into knowledge base
        knowledge.load_documents([agno_doc])

        return doc.id

    def add_documents(self, docs: List[KnowledgeDocument]) -> List[str]:
        """Add multiple documents"""
        ids = []
        for doc in docs:
            doc_id = self.add_document(doc)
            ids.append(doc_id)
        return ids

    def search(
        self,
        query: str,
        category: Optional[KnowledgeCategory] = None,
        limit: int = 10,
        min_relevance: float = 0.0,
    ) -> List[Dict[str, Any]]:
        """
        Search the knowledge base.

        Args:
            query: Search query
            category: Optional category filter
            limit: Maximum results to return
            min_relevance: Minimum relevance score (0-1)

        Returns:
            List of matching documents with scores
        """
        results = []

        # Search specific category or all
        categories = [category] if category else list(KnowledgeCategory)

        for cat in categories:
            if cat in self._knowledge_stores:
                knowledge = self._knowledge_stores[cat]
                try:
                    search_results = knowledge.search(query, num_documents=limit)
                    for result in search_results:
                        results.append({
                            "id": result.id if hasattr(result, 'id') else None,
                            "content": result.content if hasattr(result, 'content') else str(result),
                            "category": cat.value,
                            "metadata": result.meta_data if hasattr(result, 'meta_data') else {},
                        })
                except Exception as e:
                    # Knowledge base might be empty
                    pass

        return results[:limit]

    def get_document(self, doc_id: str) -> Optional[KnowledgeDocument]:
        """Get a document by ID"""
        return self._documents.get(doc_id)

    def get_documents_by_category(
        self,
        category: KnowledgeCategory,
    ) -> List[KnowledgeDocument]:
        """Get all documents in a category"""
        return [
            doc for doc in self._documents.values()
            if doc.category == category
        ]

    def get_documents_by_tag(self, tag: str) -> List[KnowledgeDocument]:
        """Get all documents with a specific tag"""
        return [
            doc for doc in self._documents.values()
            if tag in doc.tags
        ]

    def update_document(
        self,
        doc_id: str,
        content: Optional[str] = None,
        metadata: Optional[Dict[str, Any]] = None,
        tags: Optional[List[str]] = None,
    ) -> bool:
        """Update an existing document"""
        if doc_id not in self._documents:
            return False

        doc = self._documents[doc_id]
        doc.updated_at = datetime.utcnow().isoformat()

        if content:
            doc.content = content
        if metadata:
            doc.metadata.update(metadata)
        if tags:
            doc.tags = tags

        # Re-index
        knowledge = self._get_knowledge(doc.category)
        knowledge.load_documents([doc.to_agno_document()])

        return True

    def delete_document(self, doc_id: str) -> bool:
        """Delete a document"""
        if doc_id not in self._documents:
            return False

        del self._documents[doc_id]
        return True

    def get_context_for_agent(
        self,
        query: str,
        agent_role: str,
        max_tokens: int = 2000,
    ) -> str:
        """
        Get relevant knowledge context for an agent.

        Args:
            query: The query/task the agent is working on
            agent_role: The agent's role (for filtering)
            max_tokens: Maximum context tokens

        Returns:
            Formatted context string
        """
        # Determine relevant categories based on agent role
        role_categories = {
            "signal_scientist": [KnowledgeCategory.MARKET_DATA, KnowledgeCategory.RESEARCH],
            "data_scientist": [KnowledgeCategory.MARKET_DATA],
            "quant_researcher": [KnowledgeCategory.RESEARCH, KnowledgeCategory.STRATEGY],
            "research_lead": [KnowledgeCategory.RESEARCH, KnowledgeCategory.STRATEGY],
            "execution_trader": [KnowledgeCategory.EXECUTION, KnowledgeCategory.MARKET_DATA],
            "market_maker": [KnowledgeCategory.EXECUTION, KnowledgeCategory.MARKET_DATA],
            "risk_quant": [KnowledgeCategory.RISK_MODELS, KnowledgeCategory.MARKET_DATA],
            "compliance_officer": [KnowledgeCategory.COMPLIANCE, KnowledgeCategory.TRADE_HISTORY],
            "portfolio_manager": [KnowledgeCategory.STRATEGY, KnowledgeCategory.TRADE_HISTORY],
            "investment_committee_chair": list(KnowledgeCategory),  # Access to all
        }

        categories = role_categories.get(agent_role, list(KnowledgeCategory))

        # Search across relevant categories
        all_results = []
        for cat in categories:
            results = self.search(query, category=cat, limit=5)
            all_results.extend(results)

        if not all_results:
            return ""

        # Format context
        context_parts = ["## Relevant Knowledge\n"]
        for result in all_results[:10]:  # Limit total results
            context_parts.append(f"### {result.get('category', 'Unknown')}")
            context_parts.append(result.get('content', '')[:500])  # Truncate long content
            context_parts.append("")

        return "\n".join(context_parts)

    def get_statistics(self) -> Dict[str, Any]:
        """Get knowledge base statistics"""
        stats = {
            "total_documents": len(self._documents),
            "by_category": {},
        }

        for cat in KnowledgeCategory:
            count = len([d for d in self._documents.values() if d.category == cat])
            stats["by_category"][cat.value] = count

        return stats


# =============================================================================
# GLOBAL KNOWLEDGE BASE INSTANCE
# =============================================================================

_knowledge_base: Optional[RenTechKnowledge] = None


def get_knowledge_base() -> RenTechKnowledge:
    """Get the global knowledge base instance"""
    global _knowledge_base
    if _knowledge_base is None:
        _knowledge_base = RenTechKnowledge()
    return _knowledge_base


def reset_knowledge_base():
    """Reset the global knowledge base"""
    global _knowledge_base
    _knowledge_base = None


# =============================================================================
# KNOWLEDGE LOADER UTILITIES
# =============================================================================

def load_knowledge_from_dict(data: Dict[str, Any]) -> KnowledgeDocument:
    """Load a knowledge document from a dictionary"""
    return KnowledgeDocument(
        id=data["id"],
        category=KnowledgeCategory(data["category"]),
        title=data["title"],
        content=data["content"],
        metadata=data.get("metadata", {}),
        tags=data.get("tags", []),
        importance=data.get("importance", 0.5),
    )


def load_knowledge_from_json(json_path: str) -> List[KnowledgeDocument]:
    """Load knowledge documents from a JSON file"""
    with open(json_path, 'r') as f:
        data = json.load(f)

    docs = []
    for item in data:
        doc = load_knowledge_from_dict(item)
        docs.append(doc)

    return docs
