"""
Session Module - Conversation state persistence

Provides:
- Session creation and management
- Chat history persistence
- Session resume across restarts
- Session summaries
"""

from typing import Dict, Any, Optional, List
import logging
import json
from pathlib import Path

logger = logging.getLogger(__name__)


class SessionModule:
    """
    Manage conversation sessions with persistence.

    Supports:
    - Create/resume sessions
    - Store chat history
    - Session summaries
    - SQLite persistence
    """

    def __init__(
        self,
        storage_path: Optional[str] = None,
        auto_save: bool = True,
        **kwargs
    ):
        """
        Initialize session module.

        Args:
            storage_path: Path to SQLite database for persistence
            auto_save: Auto-save after each message
            **kwargs: Additional configuration
        """
        self.storage_path = storage_path or "./sessions.db"
        self.auto_save = auto_save
        self.config = kwargs

        self._sessions: Dict[str, Any] = {}
        self._current_session_id: Optional[str] = None

    def create_session(
        self,
        session_id: Optional[str] = None,
        metadata: Optional[Dict[str, Any]] = None
    ) -> str:
        """
        Create a new session.

        Args:
            session_id: Optional custom session ID
            metadata: Session metadata (user_id, tab_name, etc.)

        Returns:
            Session ID
        """
        import uuid

        if session_id is None:
            session_id = str(uuid.uuid4())

        try:
            from agno.session import AgentSession

            session = AgentSession(
                session_id=session_id,
                metadata=metadata or {}
            )
            self._sessions[session_id] = {
                "agno_session": session,
                "metadata": metadata or {},
                "messages": [],
                "created_at": self._get_timestamp(),
            }

        except ImportError:
            # Fallback to simple dict-based session
            self._sessions[session_id] = {
                "agno_session": None,
                "metadata": metadata or {},
                "messages": [],
                "created_at": self._get_timestamp(),
            }

        self._current_session_id = session_id
        logger.debug(f"Created session: {session_id}")

        return session_id

    def get_session(self, session_id: str) -> Optional[Dict[str, Any]]:
        """Get session by ID"""
        return self._sessions.get(session_id)

    def get_agno_session(self, session_id: str) -> Optional[Any]:
        """Get the Agno AgentSession object"""
        session = self._sessions.get(session_id)
        if session:
            return session.get("agno_session")
        return None

    def add_message(
        self,
        session_id: str,
        role: str,
        content: str,
        **metadata
    ) -> None:
        """
        Add a message to session.

        Args:
            session_id: Session ID
            role: Message role ('user', 'assistant', 'system')
            content: Message content
            **metadata: Additional message metadata
        """
        if session_id not in self._sessions:
            self.create_session(session_id)

        message = {
            "role": role,
            "content": content,
            "timestamp": self._get_timestamp(),
            **metadata
        }

        self._sessions[session_id]["messages"].append(message)

        if self.auto_save:
            self._save_session(session_id)

    def get_messages(
        self,
        session_id: str,
        limit: Optional[int] = None
    ) -> List[Dict[str, Any]]:
        """
        Get messages from session.

        Args:
            session_id: Session ID
            limit: Maximum messages to return

        Returns:
            List of messages
        """
        session = self._sessions.get(session_id)
        if not session:
            return []

        messages = session.get("messages", [])

        if limit:
            return messages[-limit:]
        return messages

    def get_chat_history(
        self,
        session_id: str,
        format: str = "list"
    ) -> Any:
        """
        Get formatted chat history.

        Args:
            session_id: Session ID
            format: Output format ('list', 'string', 'agno')

        Returns:
            Chat history in requested format
        """
        messages = self.get_messages(session_id)

        if format == "string":
            lines = []
            for msg in messages:
                lines.append(f"{msg['role'].upper()}: {msg['content']}")
            return "\n\n".join(lines)

        elif format == "agno":
            # Return in Agno message format
            agno_messages = []
            for msg in messages:
                agno_messages.append({
                    "role": msg["role"],
                    "content": msg["content"]
                })
            return agno_messages

        return messages

    def get_summary(
        self,
        session_id: str,
        model: Optional[Any] = None
    ) -> Optional[str]:
        """
        Get session summary.

        Args:
            session_id: Session ID
            model: LLM model for generating summary

        Returns:
            Session summary text
        """
        session = self._sessions.get(session_id)
        if not session:
            return None

        # Try Agno SessionSummaryManager
        if model:
            try:
                from agno.session import SessionSummaryManager

                manager = SessionSummaryManager(model=model)
                messages = self.get_chat_history(session_id, format="agno")
                summary = manager.create_session_summary(messages)
                return summary

            except ImportError:
                pass
            except Exception as e:
                logger.warning(f"Failed to create summary: {e}")

        # Fallback: simple summary
        messages = self.get_messages(session_id)
        return f"Session with {len(messages)} messages"

    def clear_session(self, session_id: str) -> None:
        """Clear all messages in a session"""
        if session_id in self._sessions:
            self._sessions[session_id]["messages"] = []
            if self.auto_save:
                self._save_session(session_id)

    def delete_session(self, session_id: str) -> None:
        """Delete a session entirely"""
        if session_id in self._sessions:
            del self._sessions[session_id]
            self._delete_from_storage(session_id)

    def list_sessions(self) -> List[Dict[str, Any]]:
        """List all sessions with metadata"""
        sessions = []
        for sid, data in self._sessions.items():
            sessions.append({
                "session_id": sid,
                "metadata": data.get("metadata", {}),
                "message_count": len(data.get("messages", [])),
                "created_at": data.get("created_at"),
            })
        return sessions

    def save_all(self) -> None:
        """Save all sessions to storage"""
        for session_id in self._sessions:
            self._save_session(session_id)

    def load_session(self, session_id: str) -> Optional[Dict[str, Any]]:
        """Load session from storage"""
        try:
            db_path = Path(self.storage_path)
            if not db_path.exists():
                return None

            import sqlite3
            conn = sqlite3.connect(str(db_path))
            cursor = conn.cursor()

            cursor.execute(
                "SELECT data FROM sessions WHERE session_id = ?",
                (session_id,)
            )
            row = cursor.fetchone()
            conn.close()

            if row:
                data = json.loads(row[0])
                self._sessions[session_id] = data
                return data

        except Exception as e:
            logger.error(f"Failed to load session {session_id}: {e}")

        return None

    def _save_session(self, session_id: str) -> None:
        """Save session to SQLite"""
        if session_id not in self._sessions:
            return

        try:
            db_path = Path(self.storage_path)
            db_path.parent.mkdir(parents=True, exist_ok=True)

            import sqlite3
            conn = sqlite3.connect(str(db_path))
            cursor = conn.cursor()

            # Create table if not exists
            cursor.execute("""
                CREATE TABLE IF NOT EXISTS sessions (
                    session_id TEXT PRIMARY KEY,
                    data TEXT,
                    updated_at TEXT
                )
            """)

            # Serialize session (exclude non-serializable Agno objects)
            session_data = self._sessions[session_id].copy()
            session_data["agno_session"] = None  # Don't serialize Agno object

            cursor.execute(
                """INSERT OR REPLACE INTO sessions (session_id, data, updated_at)
                   VALUES (?, ?, ?)""",
                (session_id, json.dumps(session_data), self._get_timestamp())
            )

            conn.commit()
            conn.close()

        except Exception as e:
            logger.error(f"Failed to save session {session_id}: {e}")

    def _delete_from_storage(self, session_id: str) -> None:
        """Delete session from storage"""
        try:
            db_path = Path(self.storage_path)
            if not db_path.exists():
                return

            import sqlite3
            conn = sqlite3.connect(str(db_path))
            cursor = conn.cursor()
            cursor.execute("DELETE FROM sessions WHERE session_id = ?", (session_id,))
            conn.commit()
            conn.close()

        except Exception as e:
            logger.error(f"Failed to delete session {session_id}: {e}")

    def _get_timestamp(self) -> str:
        """Get current timestamp"""
        from datetime import datetime
        return datetime.utcnow().isoformat()

    @classmethod
    def from_config(cls, config: Dict[str, Any]) -> "SessionModule":
        """Create SessionModule from config"""
        return cls(
            storage_path=config.get("storage_path"),
            auto_save=config.get("auto_save", True),
            **config.get("options", {})
        )


class SessionManager:
    """
    High-level session manager for the terminal.

    Manages sessions across tabs and provides easy access.
    """

    _instance: Optional["SessionManager"] = None

    def __init__(self, storage_path: Optional[str] = None):
        self._module = SessionModule(storage_path=storage_path)
        self._tab_sessions: Dict[str, str] = {}  # tab_id -> session_id

    @classmethod
    def get_instance(cls, storage_path: Optional[str] = None) -> "SessionManager":
        """Get singleton instance"""
        if cls._instance is None:
            cls._instance = cls(storage_path)
        return cls._instance

    def get_or_create_session(
        self,
        tab_id: str,
        user_id: Optional[str] = None
    ) -> str:
        """Get existing session for tab or create new one"""
        if tab_id in self._tab_sessions:
            return self._tab_sessions[tab_id]

        session_id = self._module.create_session(
            metadata={"tab_id": tab_id, "user_id": user_id}
        )
        self._tab_sessions[tab_id] = session_id
        return session_id

    def add_message(
        self,
        tab_id: str,
        role: str,
        content: str
    ) -> None:
        """Add message to tab's session"""
        session_id = self.get_or_create_session(tab_id)
        self._module.add_message(session_id, role, content)

    def get_history(self, tab_id: str, limit: Optional[int] = None) -> List[Dict]:
        """Get chat history for tab"""
        session_id = self._tab_sessions.get(tab_id)
        if not session_id:
            return []
        return self._module.get_messages(session_id, limit=limit)

    def clear_tab_session(self, tab_id: str) -> None:
        """Clear session for a tab"""
        session_id = self._tab_sessions.get(tab_id)
        if session_id:
            self._module.clear_session(session_id)

    def get_module(self) -> SessionModule:
        """Get underlying SessionModule"""
        return self._module
