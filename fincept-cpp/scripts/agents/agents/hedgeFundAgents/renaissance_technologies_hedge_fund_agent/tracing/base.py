"""
Base Tracing Infrastructure

Core tracing system for Renaissance Technologies multi-agent architecture.
Implements distributed tracing, span management, and observability.
"""

from typing import Optional, List, Dict, Any, Callable
from dataclasses import dataclass, field
from datetime import datetime
from enum import Enum
from contextlib import contextmanager
import uuid
import json
import threading

from ..config import get_config, TracingConfig


class TraceLevel(str, Enum):
    """Trace detail levels"""
    DEBUG = "debug"
    INFO = "info"
    WARNING = "warning"
    ERROR = "error"
    CRITICAL = "critical"


class SpanStatus(str, Enum):
    """Span completion status"""
    OK = "ok"
    ERROR = "error"
    CANCELLED = "cancelled"


@dataclass
class SpanContext:
    """Context propagated across span boundaries"""
    trace_id: str
    span_id: str
    parent_span_id: Optional[str] = None
    baggage: Dict[str, str] = field(default_factory=dict)

    def to_dict(self) -> Dict[str, Any]:
        return {
            "trace_id": self.trace_id,
            "span_id": self.span_id,
            "parent_span_id": self.parent_span_id,
            "baggage": self.baggage,
        }


@dataclass
class SpanEvent:
    """Event within a span"""
    name: str
    timestamp: str
    attributes: Dict[str, Any] = field(default_factory=dict)


@dataclass
class Span:
    """
    A span represents a single operation within a trace.

    Spans form a tree structure representing the call hierarchy
    of operations in the system.
    """
    name: str
    context: SpanContext
    start_time: datetime
    end_time: Optional[datetime] = None

    # Span metadata
    kind: str = "internal"  # internal, server, client, producer, consumer
    status: SpanStatus = SpanStatus.OK
    status_message: str = ""

    # Attributes and events
    attributes: Dict[str, Any] = field(default_factory=dict)
    events: List[SpanEvent] = field(default_factory=list)

    # Resource info
    service_name: str = "rentech_agents"
    agent_id: Optional[str] = None

    @property
    def duration_ms(self) -> Optional[float]:
        """Get span duration in milliseconds"""
        if self.end_time:
            return (self.end_time - self.start_time).total_seconds() * 1000
        return None

    def set_attribute(self, key: str, value: Any):
        """Set a span attribute"""
        self.attributes[key] = value

    def add_event(self, name: str, attributes: Optional[Dict[str, Any]] = None):
        """Add an event to the span"""
        self.events.append(SpanEvent(
            name=name,
            timestamp=datetime.utcnow().isoformat(),
            attributes=attributes or {},
        ))

    def set_status(self, status: SpanStatus, message: str = ""):
        """Set span status"""
        self.status = status
        self.status_message = message

    def end(self, status: Optional[SpanStatus] = None):
        """End the span"""
        self.end_time = datetime.utcnow()
        if status:
            self.status = status

    def to_dict(self) -> Dict[str, Any]:
        """Convert span to dictionary for export"""
        return {
            "name": self.name,
            "trace_id": self.context.trace_id,
            "span_id": self.context.span_id,
            "parent_span_id": self.context.parent_span_id,
            "start_time": self.start_time.isoformat(),
            "end_time": self.end_time.isoformat() if self.end_time else None,
            "duration_ms": self.duration_ms,
            "kind": self.kind,
            "status": self.status.value,
            "status_message": self.status_message,
            "attributes": self.attributes,
            "events": [{"name": e.name, "timestamp": e.timestamp, "attributes": e.attributes} for e in self.events],
            "service_name": self.service_name,
            "agent_id": self.agent_id,
        }


class Tracer:
    """
    Core tracing system for observability.

    Provides:
    1. Distributed tracing across agents and workflows
    2. Span management and propagation
    3. Attribute and event recording
    4. Export to various backends
    """

    def __init__(self, config: Optional[TracingConfig] = None):
        self.config = config or get_config().tracing

        # Storage
        self._traces: Dict[str, List[Span]] = {}
        self._active_spans: Dict[str, Span] = {}  # Thread-local active spans

        # Threading
        self._lock = threading.Lock()
        self._local = threading.local()

        # Export callbacks
        self._exporters: List[Callable[[Span], None]] = []

        if self.config.export_to_console:
            self._exporters.append(self._console_exporter)

    @property
    def current_span(self) -> Optional[Span]:
        """Get the current active span for this thread"""
        return getattr(self._local, 'current_span', None)

    def start_trace(
        self,
        name: str,
        attributes: Optional[Dict[str, Any]] = None,
    ) -> Span:
        """
        Start a new trace (root span).
        """
        trace_id = uuid.uuid4().hex
        span_id = uuid.uuid4().hex[:16]

        context = SpanContext(
            trace_id=trace_id,
            span_id=span_id,
        )

        span = Span(
            name=name,
            context=context,
            start_time=datetime.utcnow(),
            service_name=self.config.service_name,
            attributes=attributes or {},
        )

        with self._lock:
            self._traces[trace_id] = [span]

        self._local.current_span = span
        return span

    def start_span(
        self,
        name: str,
        parent: Optional[Span] = None,
        kind: str = "internal",
        attributes: Optional[Dict[str, Any]] = None,
    ) -> Span:
        """
        Start a new span within an existing trace.
        """
        parent_span = parent or self.current_span

        if parent_span is None:
            # No parent, start a new trace
            return self.start_trace(name, attributes)

        span_id = uuid.uuid4().hex[:16]

        context = SpanContext(
            trace_id=parent_span.context.trace_id,
            span_id=span_id,
            parent_span_id=parent_span.context.span_id,
            baggage=parent_span.context.baggage.copy(),
        )

        span = Span(
            name=name,
            context=context,
            start_time=datetime.utcnow(),
            kind=kind,
            service_name=self.config.service_name,
            attributes=attributes or {},
        )

        with self._lock:
            if context.trace_id in self._traces:
                self._traces[context.trace_id].append(span)

        self._local.current_span = span
        return span

    def end_span(self, span: Span, status: Optional[SpanStatus] = None):
        """End a span and export it"""
        span.end(status)

        # Export to all exporters
        for exporter in self._exporters:
            try:
                exporter(span)
            except Exception as e:
                print(f"Export error: {e}")

        # Restore parent span as current
        if span.context.parent_span_id:
            parent = self._find_span(
                span.context.trace_id,
                span.context.parent_span_id
            )
            self._local.current_span = parent
        else:
            self._local.current_span = None

    def _find_span(self, trace_id: str, span_id: str) -> Optional[Span]:
        """Find a span by ID"""
        with self._lock:
            spans = self._traces.get(trace_id, [])
            for span in spans:
                if span.context.span_id == span_id:
                    return span
        return None

    @contextmanager
    def span(
        self,
        name: str,
        kind: str = "internal",
        attributes: Optional[Dict[str, Any]] = None,
    ):
        """
        Context manager for creating spans.

        Usage:
            with tracer.span("my_operation") as span:
                span.set_attribute("key", "value")
                # do work
        """
        span = self.start_span(name, kind=kind, attributes=attributes)
        try:
            yield span
            self.end_span(span, SpanStatus.OK)
        except Exception as e:
            span.set_status(SpanStatus.ERROR, str(e))
            span.add_event("exception", {"message": str(e)})
            self.end_span(span, SpanStatus.ERROR)
            raise

    def record_exception(self, exception: Exception, span: Optional[Span] = None):
        """Record an exception in the current or specified span"""
        target_span = span or self.current_span
        if target_span:
            target_span.add_event("exception", {
                "type": type(exception).__name__,
                "message": str(exception),
            })
            target_span.set_status(SpanStatus.ERROR, str(exception))

    def add_exporter(self, exporter: Callable[[Span], None]):
        """Add a span exporter"""
        self._exporters.append(exporter)

    def _console_exporter(self, span: Span):
        """Export span to console"""
        indent = "  " if span.context.parent_span_id else ""
        status_icon = "OK" if span.status == SpanStatus.OK else "ERR"
        duration = f"{span.duration_ms:.2f}ms" if span.duration_ms else "ongoing"

        print(f"{indent}[{status_icon}] {span.name} ({duration})")
        if span.status == SpanStatus.ERROR:
            print(f"{indent}  Error: {span.status_message}")

    def get_trace(self, trace_id: str) -> List[Dict[str, Any]]:
        """Get all spans for a trace"""
        with self._lock:
            spans = self._traces.get(trace_id, [])
            return [s.to_dict() for s in spans]

    def get_trace_summary(self, trace_id: str) -> Dict[str, Any]:
        """Get summary of a trace"""
        with self._lock:
            spans = self._traces.get(trace_id, [])

        if not spans:
            return {"error": "Trace not found"}

        root_span = spans[0]
        total_duration = root_span.duration_ms

        error_spans = [s for s in spans if s.status == SpanStatus.ERROR]

        return {
            "trace_id": trace_id,
            "root_name": root_span.name,
            "total_spans": len(spans),
            "total_duration_ms": total_duration,
            "error_count": len(error_spans),
            "status": "error" if error_spans else "ok",
        }

    def get_recent_traces(self, limit: int = 100) -> List[Dict[str, Any]]:
        """Get recent traces"""
        with self._lock:
            traces = list(self._traces.keys())[-limit:]
            return [self.get_trace_summary(t) for t in traces]


# Global tracer instance
_tracer: Optional[Tracer] = None


def get_tracer() -> Tracer:
    """Get the global tracer instance"""
    global _tracer
    if _tracer is None:
        _tracer = Tracer()
    return _tracer


def reset_tracer():
    """Reset the tracer (for testing)"""
    global _tracer
    _tracer = None
