"""
Database Manager - Handles SQLite/PostgreSQL for agent persistence

This module manages database connections for agent sessions, memory,
and knowledge storage.
"""

import os
from typing import Optional
from pathlib import Path
from finagent_core.utils.logger import get_logger

logger = get_logger(__name__)


class DatabaseManager:
    """
    Manages database connections for Agno agents

    Supports:
    - SQLite (development and production)
    - PostgreSQL (production)
    - InMemoryDb (testing)
    """

    def __init__(self, base_path: Optional[Path] = None):
        """
        Initialize database manager

        Args:
            base_path: Base path for database files
        """
        self.base_path = base_path or Path(__file__).parent.parent
        self._db = None

    def get_database(self, db_type: str = "sqlite", **kwargs):
        """
        Get database instance

        Args:
            db_type: Database type (sqlite, postgres, memory)
            **kwargs: Database-specific parameters

        Returns:
            Agno database instance
        """
        if self._db:
            return self._db

        db_type = db_type.lower()

        if db_type == "sqlite":
            self._db = self._create_sqlite_db(**kwargs)
        elif db_type == "postgres" or db_type == "postgresql":
            self._db = self._create_postgres_db(**kwargs)
        elif db_type == "memory":
            self._db = self._create_memory_db()
        else:
            raise ValueError(f"Unknown database type: {db_type}")

        logger.info(f"Database initialized: {db_type}")
        return self._db

    def _create_sqlite_db(self, db_file: Optional[str] = None, **kwargs):
        """Create SQLite database"""
        try:
            from agno.storage.agent.sqlite import SqliteAgentStorage

            if not db_file:
                # Default database location
                db_path = self.base_path / "database" / "agents.db"
                db_path.parent.mkdir(parents=True, exist_ok=True)
                db_file = str(db_path)

            logger.info(f"Creating SQLite database at: {db_file}")
            return SqliteAgentStorage(table_name="agents", db_file=db_file)

        except ImportError:
            logger.error("Agno SQLite support not available. Install: pip install agno")
            raise

    def _create_postgres_db(self, url: Optional[str] = None, **kwargs):
        """Create PostgreSQL database"""
        try:
            from agno.storage.agent.postgres import PostgresAgentStorage

            if not url:
                # Try to get from environment
                url = os.getenv("DATABASE_URL") or os.getenv("POSTGRES_URL")

            if not url:
                raise ValueError(
                    "PostgreSQL URL required. Set DATABASE_URL environment variable "
                    "or pass url parameter"
                )

            logger.info(f"Creating PostgreSQL database connection")
            return PostgresAgentStorage(table_name="agents", db_url=url)

        except ImportError:
            logger.error("Agno PostgreSQL support not installed. Install: pip install 'agno[postgres]'")
            raise

    def _create_memory_db(self):
        """Create in-memory database (for testing)"""
        try:
            # Agno doesn't have InMemoryDb, use SQLite in-memory instead
            from agno.storage.agent.sqlite import SqliteAgentStorage

            logger.info("Creating in-memory database")
            return SqliteAgentStorage(table_name="agents", db_file=":memory:")

        except ImportError:
            logger.error("Agno SQLite support not available")
            raise

    def close(self):
        """Close database connection"""
        if self._db:
            logger.info("Closing database connection")
            # Agno databases don't have explicit close, but clear reference
            self._db = None
