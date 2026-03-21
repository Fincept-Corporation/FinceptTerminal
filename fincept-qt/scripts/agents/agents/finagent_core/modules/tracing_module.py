"""
Tracing Module - Debugging and monitoring for financial agents

Provides:
- Full execution traces
- Performance monitoring
- Audit logging for compliance
- Database export of traces
"""

from typing import Dict, Any, Optional, List, Callable
from dataclasses import dataclass, field
from datetime import datetime
from contextlib import contextmanager
import logging
import json
import time
import uuid

logger = logging.getLogger(__name__)


@dataclass
class Span:
    """A single span in the trace."""
    span_id: str
    name: str
    start_time: float
    end_time: Optional[float] = None
    parent_id: Optional[str] = None
    attributes: Dict[str, Any] = field(default_factory=dict)
    events: List[Dict[str, Any]] = field(default_factory=list)
    status: str = "ok"
    error: Optional[str] = None

    @property
    def duration_ms(self) -> float:
        """Duration in milliseconds."""
        if self.end_time:
            return (self.end_time - self.start_time) * 1000
        return 0.0

    def add_event(self, name: str, attributes: Dict[str, Any] = None) -> None:
        """Add an event to the span."""
        self.events.append({
            "name": name,
            "timestamp": time.time(),
            "attributes": attributes or {}
        })

    def set_attribute(self, key: str, value: Any) -> None:
        """Set a span attribute."""
        self.attributes[key] = value

    def set_error(self, error: str) -> None:
        """Set error status."""
        self.status = "error"
        self.error = error

    def end(self) -> None:
        """End the span."""
        self.end_time = time.time()

    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary."""
        return {
            "span_id": self.span_id,
            "name": self.name,
            "start_time": self.start_time,
            "end_time": self.end_time,
            "duration_ms": self.duration_ms,
            "parent_id": self.parent_id,
            "attributes": self.attributes,
            "events": self.events,
            "status": self.status,
            "error": self.error
        }


@dataclass
class Trace:
    """A complete trace of an agent run."""
    trace_id: str
    name: str
    start_time: float
    end_time: Optional[float] = None
    spans: List[Span] = field(default_factory=list)
    metadata: Dict[str, Any] = field(default_factory=dict)

    @property
    def duration_ms(self) -> float:
        """Total duration in milliseconds."""
        if self.end_time:
            return (self.end_time - self.start_time) * 1000
        return 0.0

    def add_span(self, span: Span) -> None:
        """Add a span to the trace."""
        self.spans.append(span)

    def end(self) -> None:
        """End the trace."""
        self.end_time = time.time()

    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary."""
        return {
            "trace_id": self.trace_id,
            "name": self.name,
            "start_time": self.start_time,
            "end_time": self.end_time,
            "duration_ms": self.duration_ms,
            "spans": [s.to_dict() for s in self.spans],
            "metadata": self.metadata
        }

    def get_summary(self) -> Dict[str, Any]:
        """Get trace summary."""
        span_durations = {s.name: s.duration_ms for s in self.spans}
        error_spans = [s for s in self.spans if s.status == "error"]

        return {
            "trace_id": self.trace_id,
            "name": self.name,
            "total_duration_ms": self.duration_ms,
            "span_count": len(self.spans),
            "span_durations": span_durations,
            "has_errors": len(error_spans) > 0,
            "error_count": len(error_spans),
            "errors": [{"span": s.name, "error": s.error} for s in error_spans]
        }


class TracingModule:
    """
    Tracing manager for financial agents.

    Features:
    - Hierarchical span tracking
    - Automatic performance metrics
    - Audit logging
    - Database export
    """

    def __init__(
        self,
        enable_db_export: bool = False,
        db_path: str = None,
        max_traces: int = 1000
    ):
        """
        Initialize tracing module.

        Args:
            enable_db_export: Export traces to database
            db_path: Path to SQLite database
            max_traces: Maximum traces to keep in memory
        """
        self.enable_db_export = enable_db_export
        self.db_path = db_path
        self.max_traces = max_traces

        self._traces: List[Trace] = []
        self._current_trace: Optional[Trace] = None
        self._span_stack: List[Span] = []
        self._exporters: List[Callable] = []

    def start_trace(
        self,
        name: str,
        metadata: Dict[str, Any] = None
    ) -> Trace:
        """
        Start a new trace.

        Args:
            name: Trace name
            metadata: Additional metadata

        Returns:
            New Trace object
        """
        trace = Trace(
            trace_id=str(uuid.uuid4()),
            name=name,
            start_time=time.time(),
            metadata=metadata or {}
        )

        self._current_trace = trace
        self._span_stack = []

        logger.debug(f"Started trace: {trace.trace_id} - {name}")

        return trace

    def end_trace(self) -> Optional[Trace]:
        """End current trace and export."""
        if not self._current_trace:
            return None

        self._current_trace.end()

        # Store trace
        self._traces.append(self._current_trace)

        # Trim if needed
        if len(self._traces) > self.max_traces:
            self._traces = self._traces[-self.max_traces:]

        # Export
        self._export_trace(self._current_trace)

        trace = self._current_trace
        self._current_trace = None
        self._span_stack = []

        logger.debug(f"Ended trace: {trace.trace_id} ({trace.duration_ms:.2f}ms)")

        return trace

    def start_span(
        self,
        name: str,
        attributes: Dict[str, Any] = None
    ) -> Span:
        """
        Start a new span.

        Args:
            name: Span name
            attributes: Span attributes

        Returns:
            New Span object
        """
        parent_id = self._span_stack[-1].span_id if self._span_stack else None

        span = Span(
            span_id=str(uuid.uuid4()),
            name=name,
            start_time=time.time(),
            parent_id=parent_id,
            attributes=attributes or {}
        )

        self._span_stack.append(span)

        if self._current_trace:
            self._current_trace.add_span(span)

        return span

    def end_span(self, error: str = None) -> Optional[Span]:
        """End current span."""
        if not self._span_stack:
            return None

        span = self._span_stack.pop()
        span.end()

        if error:
            span.set_error(error)

        return span

    @contextmanager
    def trace(self, name: str, metadata: Dict[str, Any] = None):
        """Context manager for tracing."""
        self.start_trace(name, metadata)
        try:
            yield self._current_trace
        except Exception as e:
            if self._current_trace:
                self._current_trace.metadata["error"] = str(e)
            raise
        finally:
            self.end_trace()

    @contextmanager
    def span(self, name: str, attributes: Dict[str, Any] = None):
        """Context manager for spans."""
        span = self.start_span(name, attributes)
        try:
            yield span
        except Exception as e:
            span.set_error(str(e))
            raise
        finally:
            self.end_span()

    def add_exporter(self, exporter: Callable[[Trace], None]) -> None:
        """Add a trace exporter."""
        self._exporters.append(exporter)

    def _export_trace(self, trace: Trace) -> None:
        """Export trace to all exporters."""
        for exporter in self._exporters:
            try:
                exporter(trace)
            except Exception as e:
                logger.error(f"Exporter failed: {e}")

        if self.enable_db_export and self.db_path:
            self._export_to_db(trace)

    def _export_to_db(self, trace: Trace) -> None:
        """Export trace to SQLite database."""
        try:
            import sqlite3

            conn = sqlite3.connect(self.db_path)
            cursor = conn.cursor()

            # Create tables if needed
            cursor.execute("""
                CREATE TABLE IF NOT EXISTS traces (
                    trace_id TEXT PRIMARY KEY,
                    name TEXT,
                    start_time REAL,
                    end_time REAL,
                    duration_ms REAL,
                    metadata TEXT,
                    created_at TEXT
                )
            """)

            cursor.execute("""
                CREATE TABLE IF NOT EXISTS spans (
                    span_id TEXT PRIMARY KEY,
                    trace_id TEXT,
                    name TEXT,
                    start_time REAL,
                    end_time REAL,
                    duration_ms REAL,
                    parent_id TEXT,
                    attributes TEXT,
                    events TEXT,
                    status TEXT,
                    error TEXT,
                    FOREIGN KEY (trace_id) REFERENCES traces(trace_id)
                )
            """)

            # Insert trace
            cursor.execute("""
                INSERT OR REPLACE INTO traces
                (trace_id, name, start_time, end_time, duration_ms, metadata, created_at)
                VALUES (?, ?, ?, ?, ?, ?, ?)
            """, (
                trace.trace_id,
                trace.name,
                trace.start_time,
                trace.end_time,
                trace.duration_ms,
                json.dumps(trace.metadata),
                datetime.now().isoformat()
            ))

            # Insert spans
            for span in trace.spans:
                cursor.execute("""
                    INSERT OR REPLACE INTO spans
                    (span_id, trace_id, name, start_time, end_time, duration_ms,
                     parent_id, attributes, events, status, error)
                    VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                """, (
                    span.span_id,
                    trace.trace_id,
                    span.name,
                    span.start_time,
                    span.end_time,
                    span.duration_ms,
                    span.parent_id,
                    json.dumps(span.attributes),
                    json.dumps(span.events),
                    span.status,
                    span.error
                ))

            conn.commit()
            conn.close()

        except Exception as e:
            logger.error(f"Failed to export trace to DB: {e}")

    def get_traces(self, limit: int = 100) -> List[Dict[str, Any]]:
        """Get recent traces."""
        return [t.to_dict() for t in self._traces[-limit:]]

    def get_trace(self, trace_id: str) -> Optional[Dict[str, Any]]:
        """Get specific trace by ID."""
        for trace in self._traces:
            if trace.trace_id == trace_id:
                return trace.to_dict()
        return None

    def get_performance_summary(self) -> Dict[str, Any]:
        """Get performance summary across all traces."""
        if not self._traces:
            return {"message": "No traces recorded"}

        durations = [t.duration_ms for t in self._traces if t.end_time]

        span_durations = {}
        for trace in self._traces:
            for span in trace.spans:
                if span.name not in span_durations:
                    span_durations[span.name] = []
                span_durations[span.name].append(span.duration_ms)

        span_averages = {
            name: sum(d) / len(d)
            for name, d in span_durations.items()
            if d
        }

        return {
            "total_traces": len(self._traces),
            "average_duration_ms": sum(durations) / len(durations) if durations else 0,
            "min_duration_ms": min(durations) if durations else 0,
            "max_duration_ms": max(durations) if durations else 0,
            "span_averages": span_averages
        }

    def setup_agno_tracing(self) -> None:
        """Setup Agno's built-in tracing."""
        try:
            from agno.tracing import setup_tracing, DatabaseSpanExporter

            if self.enable_db_export and self.db_path:
                exporter = DatabaseSpanExporter(db_path=self.db_path)
                setup_tracing(exporter=exporter)
            else:
                setup_tracing()

            logger.info("Agno tracing enabled")

        except ImportError:
            logger.warning("Agno tracing not available")

    @classmethod
    def from_config(cls, config: Dict[str, Any]) -> "TracingModule":
        """Create from configuration."""
        return cls(
            enable_db_export=config.get("enable_db_export", False),
            db_path=config.get("db_path"),
            max_traces=config.get("max_traces", 1000)
        )


class AuditLogger:
    """
    Audit logger for compliance.

    Logs all agent actions for regulatory compliance.
    """

    def __init__(self, db_path: str = None, log_file: str = None):
        """
        Initialize audit logger.

        Args:
            db_path: SQLite database path
            log_file: Log file path
        """
        self.db_path = db_path
        self.log_file = log_file
        self._init_db()

    def _init_db(self) -> None:
        """Initialize database tables."""
        if not self.db_path:
            return

        try:
            import sqlite3

            conn = sqlite3.connect(self.db_path)
            cursor = conn.cursor()

            cursor.execute("""
                CREATE TABLE IF NOT EXISTS audit_log (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    timestamp TEXT,
                    user_id TEXT,
                    agent_id TEXT,
                    action TEXT,
                    symbol TEXT,
                    details TEXT,
                    ip_address TEXT,
                    session_id TEXT
                )
            """)

            conn.commit()
            conn.close()

        except Exception as e:
            logger.error(f"Failed to initialize audit DB: {e}")

    def log(
        self,
        action: str,
        user_id: str = None,
        agent_id: str = None,
        symbol: str = None,
        details: Dict[str, Any] = None,
        ip_address: str = None,
        session_id: str = None
    ) -> None:
        """
        Log an audit event.

        Args:
            action: Action performed
            user_id: User ID
            agent_id: Agent ID
            symbol: Financial symbol involved
            details: Additional details
            ip_address: Client IP
            session_id: Session ID
        """
        timestamp = datetime.now().isoformat()
        details_json = json.dumps(details or {})

        # Log to file
        if self.log_file:
            try:
                with open(self.log_file, "a") as f:
                    f.write(f"{timestamp}|{action}|{user_id}|{agent_id}|{symbol}|{details_json}\n")
            except Exception as e:
                logger.error(f"Failed to write audit log: {e}")

        # Log to database
        if self.db_path:
            try:
                import sqlite3

                conn = sqlite3.connect(self.db_path)
                cursor = conn.cursor()

                cursor.execute("""
                    INSERT INTO audit_log
                    (timestamp, user_id, agent_id, action, symbol, details, ip_address, session_id)
                    VALUES (?, ?, ?, ?, ?, ?, ?, ?)
                """, (
                    timestamp,
                    user_id,
                    agent_id,
                    action,
                    symbol,
                    details_json,
                    ip_address,
                    session_id
                ))

                conn.commit()
                conn.close()

            except Exception as e:
                logger.error(f"Failed to log to audit DB: {e}")

    def get_logs(
        self,
        user_id: str = None,
        agent_id: str = None,
        action: str = None,
        symbol: str = None,
        limit: int = 100
    ) -> List[Dict[str, Any]]:
        """Query audit logs."""
        if not self.db_path:
            return []

        try:
            import sqlite3

            conn = sqlite3.connect(self.db_path)
            cursor = conn.cursor()

            query = "SELECT * FROM audit_log WHERE 1=1"
            params = []

            if user_id:
                query += " AND user_id = ?"
                params.append(user_id)
            if agent_id:
                query += " AND agent_id = ?"
                params.append(agent_id)
            if action:
                query += " AND action = ?"
                params.append(action)
            if symbol:
                query += " AND symbol = ?"
                params.append(symbol)

            query += f" ORDER BY timestamp DESC LIMIT {limit}"

            cursor.execute(query, params)
            rows = cursor.fetchall()
            columns = [desc[0] for desc in cursor.description]

            conn.close()

            return [dict(zip(columns, row)) for row in rows]

        except Exception as e:
            logger.error(f"Failed to query audit logs: {e}")
            return []
