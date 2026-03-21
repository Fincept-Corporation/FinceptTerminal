"""
Knowledge Base for RD-Agent
Stores and manages research knowledge, hypotheses, experiments, and results
"""

import json
import sqlite3
from typing import List, Dict, Any, Optional
from dataclasses import dataclass, asdict
from datetime import datetime
from pathlib import Path


@dataclass
class KnowledgeEntry:
    """Knowledge entry in the database"""
    id: str
    type: str  # 'hypothesis', 'experiment', 'result', 'insight', 'factor', 'model'
    title: str
    content: str
    metadata: Dict[str, Any]
    tags: List[str]
    confidence: float
    created_at: str
    updated_at: str
    parent_id: Optional[str] = None  # Link to related entries
    status: str = 'active'  # 'active', 'archived', 'deprecated'

    def to_dict(self) -> Dict[str, Any]:
        return asdict(self)


class KnowledgeBase:
    """
    Knowledge management system for quantitative research
    Stores hypotheses, experiments, insights, and learned patterns
    """

    def __init__(self, db_path: Optional[str] = None):
        self.db_path = db_path or ":memory:"  # In-memory by default
        self.conn: Optional[sqlite3.Connection] = None
        self._initialize_db()

    def _initialize_db(self):
        """Initialize SQLite database"""
        self.conn = sqlite3.connect(self.db_path, check_same_thread=False)
        self.conn.row_factory = sqlite3.Row

        cursor = self.conn.cursor()

        # Knowledge entries table
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS knowledge (
                id TEXT PRIMARY KEY,
                type TEXT NOT NULL,
                title TEXT NOT NULL,
                content TEXT NOT NULL,
                metadata TEXT,
                tags TEXT,
                confidence REAL,
                created_at TEXT NOT NULL,
                updated_at TEXT NOT NULL,
                parent_id TEXT,
                status TEXT DEFAULT 'active'
            )
        ''')

        # Relationships table (for linking related knowledge)
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS relationships (
                source_id TEXT NOT NULL,
                target_id TEXT NOT NULL,
                relationship_type TEXT NOT NULL,
                strength REAL DEFAULT 1.0,
                created_at TEXT NOT NULL,
                PRIMARY KEY (source_id, target_id, relationship_type)
            )
        ''')

        # Performance metrics table
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS performance (
                id TEXT PRIMARY KEY,
                knowledge_id TEXT NOT NULL,
                metric_name TEXT NOT NULL,
                metric_value REAL NOT NULL,
                timestamp TEXT NOT NULL,
                FOREIGN KEY (knowledge_id) REFERENCES knowledge(id)
            )
        ''')

        # Indices for faster querying
        cursor.execute('CREATE INDEX IF NOT EXISTS idx_type ON knowledge(type)')
        cursor.execute('CREATE INDEX IF NOT EXISTS idx_status ON knowledge(status)')
        cursor.execute('CREATE INDEX IF NOT EXISTS idx_created ON knowledge(created_at)')
        cursor.execute('CREATE INDEX IF NOT EXISTS idx_tags ON knowledge(tags)')

        self.conn.commit()

    def add_entry(self, entry: KnowledgeEntry) -> bool:
        """Add knowledge entry to database"""
        try:
            cursor = self.conn.cursor()
            cursor.execute('''
                INSERT INTO knowledge (
                    id, type, title, content, metadata, tags, confidence,
                    created_at, updated_at, parent_id, status
                ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            ''', (
                entry.id,
                entry.type,
                entry.title,
                entry.content,
                json.dumps(entry.metadata),
                json.dumps(entry.tags),
                entry.confidence,
                entry.created_at,
                entry.updated_at,
                entry.parent_id,
                entry.status
            ))
            self.conn.commit()
            return True
        except Exception as e:
            print(f"Error adding entry: {e}")
            return False

    def get_entry(self, entry_id: str) -> Optional[KnowledgeEntry]:
        """Retrieve knowledge entry by ID"""
        cursor = self.conn.cursor()
        cursor.execute('SELECT * FROM knowledge WHERE id = ?', (entry_id,))
        row = cursor.fetchone()

        if row:
            return KnowledgeEntry(
                id=row['id'],
                type=row['type'],
                title=row['title'],
                content=row['content'],
                metadata=json.loads(row['metadata']) if row['metadata'] else {},
                tags=json.loads(row['tags']) if row['tags'] else [],
                confidence=row['confidence'],
                created_at=row['created_at'],
                updated_at=row['updated_at'],
                parent_id=row['parent_id'],
                status=row['status']
            )
        return None

    def search(self, query: str, entry_type: Optional[str] = None, limit: int = 100) -> List[KnowledgeEntry]:
        """Search knowledge base"""
        cursor = self.conn.cursor()

        if entry_type:
            sql = '''
                SELECT * FROM knowledge
                WHERE (title LIKE ? OR content LIKE ?) AND type = ? AND status = 'active'
                ORDER BY created_at DESC LIMIT ?
            '''
            cursor.execute(sql, (f'%{query}%', f'%{query}%', entry_type, limit))
        else:
            sql = '''
                SELECT * FROM knowledge
                WHERE (title LIKE ? OR content LIKE ?) AND status = 'active'
                ORDER BY created_at DESC LIMIT ?
            '''
            cursor.execute(sql, (f'%{query}%', f'%{query}%', limit))

        results = []
        for row in cursor.fetchall():
            results.append(KnowledgeEntry(
                id=row['id'],
                type=row['type'],
                title=row['title'],
                content=row['content'],
                metadata=json.loads(row['metadata']) if row['metadata'] else {},
                tags=json.loads(row['tags']) if row['tags'] else [],
                confidence=row['confidence'],
                created_at=row['created_at'],
                updated_at=row['updated_at'],
                parent_id=row['parent_id'],
                status=row['status']
            ))

        return results

    def get_by_type(self, entry_type: str, limit: int = 100) -> List[KnowledgeEntry]:
        """Get all entries of a specific type"""
        cursor = self.conn.cursor()
        cursor.execute(
            'SELECT * FROM knowledge WHERE type = ? AND status = \'active\' ORDER BY created_at DESC LIMIT ?',
            (entry_type, limit)
        )

        results = []
        for row in cursor.fetchall():
            results.append(KnowledgeEntry(
                id=row['id'],
                type=row['type'],
                title=row['title'],
                content=row['content'],
                metadata=json.loads(row['metadata']) if row['metadata'] else {},
                tags=json.loads(row['tags']) if row['tags'] else [],
                confidence=row['confidence'],
                created_at=row['created_at'],
                updated_at=row['updated_at'],
                parent_id=row['parent_id'],
                status=row['status']
            ))

        return results

    def get_by_tags(self, tags: List[str], match_all: bool = False) -> List[KnowledgeEntry]:
        """Get entries matching tags"""
        cursor = self.conn.cursor()
        cursor.execute('SELECT * FROM knowledge WHERE status = \'active\'')

        results = []
        for row in cursor.fetchall():
            entry_tags = json.loads(row['tags']) if row['tags'] else []

            if match_all:
                if all(tag in entry_tags for tag in tags):
                    results.append(self._row_to_entry(row))
            else:
                if any(tag in entry_tags for tag in tags):
                    results.append(self._row_to_entry(row))

        return results

    def _row_to_entry(self, row) -> KnowledgeEntry:
        """Convert database row to KnowledgeEntry"""
        return KnowledgeEntry(
            id=row['id'],
            type=row['type'],
            title=row['title'],
            content=row['content'],
            metadata=json.loads(row['metadata']) if row['metadata'] else {},
            tags=json.loads(row['tags']) if row['tags'] else [],
            confidence=row['confidence'],
            created_at=row['created_at'],
            updated_at=row['updated_at'],
            parent_id=row['parent_id'],
            status=row['status']
        )

    def add_relationship(self, source_id: str, target_id: str, rel_type: str, strength: float = 1.0) -> bool:
        """Add relationship between knowledge entries"""
        try:
            cursor = self.conn.cursor()
            cursor.execute('''
                INSERT OR REPLACE INTO relationships (source_id, target_id, relationship_type, strength, created_at)
                VALUES (?, ?, ?, ?, ?)
            ''', (source_id, target_id, rel_type, strength, datetime.now().isoformat()))
            self.conn.commit()
            return True
        except Exception as e:
            print(f"Error adding relationship: {e}")
            return False

    def get_related(self, entry_id: str, rel_type: Optional[str] = None) -> List[KnowledgeEntry]:
        """Get related knowledge entries"""
        cursor = self.conn.cursor()

        if rel_type:
            cursor.execute('''
                SELECT k.* FROM knowledge k
                INNER JOIN relationships r ON k.id = r.target_id
                WHERE r.source_id = ? AND r.relationship_type = ? AND k.status = 'active'
                ORDER BY r.strength DESC
            ''', (entry_id, rel_type))
        else:
            cursor.execute('''
                SELECT k.* FROM knowledge k
                INNER JOIN relationships r ON k.id = r.target_id
                WHERE r.source_id = ? AND k.status = 'active'
                ORDER BY r.strength DESC
            ''', (entry_id,))

        results = []
        for row in cursor.fetchall():
            results.append(self._row_to_entry(row))

        return results

    def update_entry(self, entry_id: str, updates: Dict[str, Any]) -> bool:
        """Update knowledge entry"""
        try:
            updates['updated_at'] = datetime.now().isoformat()

            if 'metadata' in updates:
                updates['metadata'] = json.dumps(updates['metadata'])
            if 'tags' in updates:
                updates['tags'] = json.dumps(updates['tags'])

            set_clause = ', '.join([f"{k} = ?" for k in updates.keys()])
            values = list(updates.values()) + [entry_id]

            cursor = self.conn.cursor()
            cursor.execute(f'UPDATE knowledge SET {set_clause} WHERE id = ?', values)
            self.conn.commit()
            return True
        except Exception as e:
            print(f"Error updating entry: {e}")
            return False

    def archive_entry(self, entry_id: str) -> bool:
        """Archive a knowledge entry"""
        return self.update_entry(entry_id, {'status': 'archived'})

    def add_performance_metric(self, knowledge_id: str, metric_name: str, metric_value: float) -> bool:
        """Add performance metric for knowledge entry"""
        try:
            cursor = self.conn.cursor()
            cursor.execute('''
                INSERT INTO performance (id, knowledge_id, metric_name, metric_value, timestamp)
                VALUES (?, ?, ?, ?, ?)
            ''', (
                f"{knowledge_id}-{metric_name}-{datetime.now().timestamp()}",
                knowledge_id,
                metric_name,
                metric_value,
                datetime.now().isoformat()
            ))
            self.conn.commit()
            return True
        except Exception as e:
            print(f"Error adding performance metric: {e}")
            return False

    def get_performance_history(self, knowledge_id: str, metric_name: Optional[str] = None) -> List[Dict[str, Any]]:
        """Get performance history for knowledge entry"""
        cursor = self.conn.cursor()

        if metric_name:
            cursor.execute('''
                SELECT * FROM performance
                WHERE knowledge_id = ? AND metric_name = ?
                ORDER BY timestamp DESC
            ''', (knowledge_id, metric_name))
        else:
            cursor.execute('''
                SELECT * FROM performance
                WHERE knowledge_id = ?
                ORDER BY timestamp DESC
            ''', (knowledge_id,))

        results = []
        for row in cursor.fetchall():
            results.append({
                'id': row['id'],
                'knowledge_id': row['knowledge_id'],
                'metric_name': row['metric_name'],
                'metric_value': row['metric_value'],
                'timestamp': row['timestamp']
            })

        return results

    def get_statistics(self) -> Dict[str, Any]:
        """Get knowledge base statistics"""
        cursor = self.conn.cursor()

        # Total entries by type
        cursor.execute('''
            SELECT type, COUNT(*) as count
            FROM knowledge
            WHERE status = 'active'
            GROUP BY type
        ''')
        by_type = {row['type']: row['count'] for row in cursor.fetchall()}

        # Total entries
        cursor.execute('SELECT COUNT(*) as total FROM knowledge WHERE status = \'active\'')
        total = cursor.fetchone()['total']

        # Average confidence
        cursor.execute('SELECT AVG(confidence) as avg_conf FROM knowledge WHERE status = \'active\'')
        avg_confidence = cursor.fetchone()['avg_conf'] or 0

        # Total relationships
        cursor.execute('SELECT COUNT(*) as total FROM relationships')
        total_relationships = cursor.fetchone()['total']

        return {
            'total_entries': total,
            'by_type': by_type,
            'avg_confidence': round(avg_confidence, 3),
            'total_relationships': total_relationships
        }

    def export_to_json(self, filepath: Optional[str] = None, entry_type: Optional[str] = None) -> str:
        """Export knowledge base to JSON"""
        if entry_type:
            entries = self.get_by_type(entry_type, limit=10000)
        else:
            cursor = self.conn.cursor()
            cursor.execute('SELECT * FROM knowledge WHERE status = \'active\'')
            entries = [self._row_to_entry(row) for row in cursor.fetchall()]

        data = {
            'exported_at': datetime.now().isoformat(),
            'total_entries': len(entries),
            'entries': [e.to_dict() for e in entries],
            'statistics': self.get_statistics()
        }

        json_str = json.dumps(data, indent=2)

        if filepath:
            with open(filepath, 'w') as f:
                f.write(json_str)

        return json_str

    def close(self):
        """Close database connection"""
        if self.conn:
            self.conn.close()


# CLI interface for testing
if __name__ == '__main__':
    import sys

    if len(sys.argv) > 1:
        command = sys.argv[1]

        kb = KnowledgeBase()

        if command == 'add':
            # Example: add hypothesis entry
            entry = KnowledgeEntry(
                id=f"KB-{datetime.now().strftime('%Y%m%d%H%M%S')}",
                type='hypothesis',
                title='Momentum strategy in crypto',
                content='20-day momentum effect observed in BTC/USD',
                metadata={'market': 'crypto', 'timeframe': '20d'},
                tags=['momentum', 'crypto', 'strategy'],
                confidence=0.75,
                created_at=datetime.now().isoformat(),
                updated_at=datetime.now().isoformat()
            )
            kb.add_entry(entry)
            print(json.dumps({'success': True, 'entry_id': entry.id}))

        elif command == 'stats':
            stats = kb.get_statistics()
            print(json.dumps({'success': True, 'statistics': stats}, indent=2))

        elif command == 'search':
            query = sys.argv[2] if len(sys.argv) > 2 else ''
            results = kb.search(query)
            print(json.dumps({
                'success': True,
                'count': len(results),
                'results': [r.to_dict() for r in results]
            }, indent=2))

        kb.close()

    else:
        print(json.dumps({
            'success': False,
            'error': 'No command provided',
            'usage': 'python knowledge_base.py [add|stats|search] [args]'
        }))
