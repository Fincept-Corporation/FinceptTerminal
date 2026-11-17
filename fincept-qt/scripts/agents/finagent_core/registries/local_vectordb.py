"""
LocalVectorDb — a small agno VectorDb kept in one SQLite file.

The knowledge base needs a vector store that works on every install. agno's
local stores (LanceDB, Chroma, Qdrant local mode, Milvus Lite) each need a
package that the terminal's requirements do not install, so the Knowledge
option could never be built. This store needs only the standard library's
sqlite3 and numpy: vectors are kept as float32 blobs and searched by cosine
similarity in memory — fine for the per-agent knowledge bases the CREATE tab
builds (a few documents, a few thousand chunks).

One file per persona (the caller passes the persona's knowledge directory),
one table per knowledge base name.
"""

import json
import logging
import os
import re
import sqlite3
from contextlib import contextmanager
from hashlib import md5
from typing import Any, Dict, Iterator, List, Optional

import numpy as np

from agno.knowledge.document import Document
from agno.vectordb.base import VectorDb

logger = logging.getLogger(__name__)

_SAFE_TABLE_RE = re.compile(r"[^A-Za-z0-9_]")


class LocalVectorDb(VectorDb):
    """SQLite + numpy cosine-similarity vector store (agno VectorDb interface)."""

    def __init__(
        self,
        path: Optional[str] = None,
        table_name: str = "knowledge",
        embedder: Optional[Any] = None,
        db_file: Optional[str] = None,
        name: Optional[str] = None,
        description: Optional[str] = None,
        id: Optional[str] = None,
        similarity_threshold: Optional[float] = None,
    ):
        if embedder is None:
            raise ValueError("LocalVectorDb needs an embedder")
        super().__init__(id=id, name=name or table_name, description=description,
                         similarity_threshold=similarity_threshold)
        if not db_file:
            if not path:
                raise ValueError("LocalVectorDb needs a directory (path) or a db_file")
            db_file = os.path.join(str(path), "knowledge_vectors.db")
        self.db_file = str(db_file)
        self.table_name = _SAFE_TABLE_RE.sub("_", table_name or "knowledge") or "knowledge"
        self.embedder = embedder

    # ── connection / schema ──────────────────────────────────────────────────

    @contextmanager
    def _conn(self) -> Iterator[sqlite3.Connection]:
        """Commit on success, roll back on error, and always close (an open
        handle keeps the file locked on Windows)."""
        os.makedirs(os.path.dirname(os.path.abspath(self.db_file)), exist_ok=True)
        conn = sqlite3.connect(self.db_file)
        try:
            with conn:
                yield conn
        finally:
            conn.close()

    def create(self) -> None:
        with self._conn() as c:
            c.execute(
                f"CREATE TABLE IF NOT EXISTS {self.table_name} ("
                "id TEXT PRIMARY KEY, name TEXT, content TEXT NOT NULL, meta_data TEXT, "
                "content_id TEXT, content_hash TEXT, dim INTEGER NOT NULL, embedding BLOB NOT NULL)"
            )
            c.execute(f"CREATE INDEX IF NOT EXISTS {self.table_name}_hash ON {self.table_name}(content_hash)")

    async def async_create(self) -> None:
        self.create()

    def exists(self) -> bool:
        if not os.path.exists(self.db_file):
            return False
        with self._conn() as c:
            row = c.execute("SELECT 1 FROM sqlite_master WHERE type='table' AND name=?",
                            (self.table_name,)).fetchone()
        return row is not None

    async def async_exists(self) -> bool:
        return self.exists()

    def _scalar_exists(self, column: str, value: Any) -> bool:
        if not self.exists():
            return False
        with self._conn() as c:
            row = c.execute(f"SELECT 1 FROM {self.table_name} WHERE {column}=? LIMIT 1", (value,)).fetchone()
        return row is not None

    def name_exists(self, name: str) -> bool:
        return self._scalar_exists("name", name)

    async def async_name_exists(self, name: str) -> bool:
        return self.name_exists(name)

    def id_exists(self, id: str) -> bool:
        return self._scalar_exists("id", id)

    def content_hash_exists(self, content_hash: str) -> bool:
        return self._scalar_exists("content_hash", content_hash)

    def count(self) -> int:
        if not self.exists():
            return 0
        with self._conn() as c:
            return int(c.execute(f"SELECT COUNT(*) FROM {self.table_name}").fetchone()[0])

    # ── writes ───────────────────────────────────────────────────────────────

    def _rows(self, content_hash: str, documents: List[Document],
              filters: Optional[Dict[str, Any]]) -> List[tuple]:
        rows = []
        for doc in documents:
            if not doc.content:
                continue
            doc.embed(embedder=self.embedder)
            vec = np.asarray(doc.embedding or [], dtype=np.float32)
            if vec.size == 0:
                raise RuntimeError(
                    f"The embedder returned no vector for '{doc.name or 'document'}' "
                    f"(check the embedding model and its API key)")
            meta = dict(doc.meta_data or {})
            if filters:
                meta.update(filters)
            meta["content_hash"] = content_hash
            base_id = doc.id or md5(doc.content.encode("utf-8", "replace")).hexdigest()
            doc_id = md5(f"{base_id}_{content_hash}".encode()).hexdigest()
            rows.append((doc_id, doc.name, doc.content, json.dumps(meta, default=str),
                         doc.content_id, content_hash, int(vec.size), vec.tobytes()))
        return rows

    def insert(self, content_hash: str, documents: List[Document], filters: Optional[Dict[str, Any]] = None) -> None:
        self.create()
        rows = self._rows(content_hash, documents, filters)
        with self._conn() as c:
            c.executemany(f"INSERT OR REPLACE INTO {self.table_name} VALUES (?,?,?,?,?,?,?,?)", rows)

    async def async_insert(self, content_hash: str, documents: List[Document],
                           filters: Optional[Dict[str, Any]] = None) -> None:
        self.insert(content_hash, documents, filters)

    def upsert_available(self) -> bool:
        return True

    def upsert(self, content_hash: str, documents: List[Document], filters: Optional[Dict[str, Any]] = None) -> None:
        self.create()
        rows = self._rows(content_hash, documents, filters)
        with self._conn() as c:
            c.execute(f"DELETE FROM {self.table_name} WHERE content_hash=?", (content_hash,))
            c.executemany(f"INSERT OR REPLACE INTO {self.table_name} VALUES (?,?,?,?,?,?,?,?)", rows)

    async def async_upsert(self, content_hash: str, documents: List[Document],
                           filters: Optional[Dict[str, Any]] = None) -> None:
        self.upsert(content_hash, documents, filters)

    # ── search ───────────────────────────────────────────────────────────────

    @staticmethod
    def _matches(meta: Dict[str, Any], filters: Optional[Any]) -> bool:
        if not filters or not isinstance(filters, dict):
            return True
        return all(meta.get(k) == v for k, v in filters.items())

    def search(self, query: str, limit: int = 5, filters: Optional[Any] = None) -> List[Document]:
        if not self.exists():
            return []
        if filters is not None and not isinstance(filters, dict):
            logger.warning("LocalVectorDb supports dict filters only; ignoring %r", filters)
            filters = None
        q = np.asarray(self.embedder.get_embedding(query) or [], dtype=np.float32)
        if q.size == 0:
            raise RuntimeError("The embedder returned no vector for the search query")
        q_norm = float(np.linalg.norm(q)) or 1.0
        with self._conn() as c:
            rows = c.execute(
                f"SELECT id, name, content, meta_data, content_id, dim, embedding "
                f"FROM {self.table_name} WHERE dim=?", (int(q.size),)).fetchall()
        scored = []
        for rid, name, content, meta_raw, content_id, dim, blob in rows:
            meta = json.loads(meta_raw) if meta_raw else {}
            if not self._matches(meta, filters):
                continue
            v = np.frombuffer(blob, dtype=np.float32)
            score = float(np.dot(q, v) / (q_norm * (float(np.linalg.norm(v)) or 1.0)))
            if self.similarity_threshold is not None and score < self.similarity_threshold:
                continue
            scored.append((score, rid, name, content, meta, content_id))
        scored.sort(key=lambda t: t[0], reverse=True)
        return [
            Document(id=rid, name=name, content=content, meta_data=meta, content_id=content_id,
                     embedder=self.embedder, reranking_score=score)
            for score, rid, name, content, meta, content_id in scored[: max(int(limit or 5), 1)]
        ]

    async def async_search(self, query: str, limit: int = 5, filters: Optional[Any] = None) -> List[Document]:
        return self.search(query, limit, filters)

    def get_supported_search_types(self) -> List[str]:
        return ["vector"]

    # ── deletes ──────────────────────────────────────────────────────────────

    def drop(self) -> None:
        if os.path.exists(self.db_file):
            with self._conn() as c:
                c.execute(f"DROP TABLE IF EXISTS {self.table_name}")

    async def async_drop(self) -> None:
        self.drop()

    def _delete_where(self, clause: str, args: tuple) -> bool:
        if not self.exists():
            return False
        with self._conn() as c:
            return c.execute(f"DELETE FROM {self.table_name} WHERE {clause}", args).rowcount > 0

    def delete(self) -> bool:
        if not self.exists():
            return False
        with self._conn() as c:
            c.execute(f"DELETE FROM {self.table_name}")
        return True

    def delete_by_id(self, id: str) -> bool:
        return self._delete_where("id=?", (id,))

    def delete_by_name(self, name: str) -> bool:
        return self._delete_where("name=?", (name,))

    def delete_by_content_id(self, content_id: str) -> bool:
        return self._delete_where("content_id=?", (content_id,))

    def delete_by_metadata(self, metadata: Dict[str, Any]) -> bool:
        if not self.exists() or not metadata:
            return False
        with self._conn() as c:
            ids = [rid for rid, raw in c.execute(f"SELECT id, meta_data FROM {self.table_name}")
                   if self._matches(json.loads(raw) if raw else {}, metadata)]
            for rid in ids:
                c.execute(f"DELETE FROM {self.table_name} WHERE id=?", (rid,))
        return bool(ids)

    def update_metadata(self, content_id: str, metadata: Dict[str, Any]) -> None:
        if not self.exists():
            return
        with self._conn() as c:
            for rid, raw in c.execute(f"SELECT id, meta_data FROM {self.table_name} WHERE content_id=?",
                                      (content_id,)).fetchall():
                meta = json.loads(raw) if raw else {}
                meta.update(metadata or {})
                c.execute(f"UPDATE {self.table_name} SET meta_data=? WHERE id=?",
                          (json.dumps(meta, default=str), rid))
