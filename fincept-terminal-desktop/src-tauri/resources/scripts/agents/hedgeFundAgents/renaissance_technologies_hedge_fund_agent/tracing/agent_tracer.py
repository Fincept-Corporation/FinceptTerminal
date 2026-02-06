"""
Agent Tracer

Specialized tracing for agent interactions:
- Agent request/response tracking
- Decision tracing
- Tool usage monitoring
- Inter-agent communication
"""

from typing import Optional, List, Dict, Any
from datetime import datetime
from contextlib import contextmanager
import uuid

from .base import Tracer, Span, SpanStatus, get_tracer


class AgentTracer:
    """
    Specialized tracer for agent activities.

    Tracks:
    1. Agent invocations and responses
    2. Tool calls and results
    3. Decision making processes
    4. Inter-agent communications
    """

    def __init__(self, tracer: Optional[Tracer] = None):
        self.tracer = tracer or get_tracer()

    @contextmanager
    def trace_agent_call(
        self,
        agent_id: str,
        task: str,
        input_data: Optional[Dict[str, Any]] = None,
    ):
        """
        Trace an agent invocation.

        Usage:
            with agent_tracer.trace_agent_call("signal_scientist", "analyze signal") as span:
                result = agent.run(task)
                span.set_attribute("result_summary", str(result)[:100])
        """
        with self.tracer.span(
            name=f"agent:{agent_id}",
            kind="server",
            attributes={
                "agent.id": agent_id,
                "agent.task": task,
                "agent.input_size": len(str(input_data)) if input_data else 0,
            }
        ) as span:
            span.agent_id = agent_id
            span.add_event("agent_start", {"task": task})

            yield span

            span.add_event("agent_complete", {})

    @contextmanager
    def trace_tool_call(
        self,
        agent_id: str,
        tool_name: str,
        tool_input: Optional[Dict[str, Any]] = None,
    ):
        """
        Trace a tool invocation by an agent.
        """
        with self.tracer.span(
            name=f"tool:{tool_name}",
            kind="client",
            attributes={
                "tool.name": tool_name,
                "tool.agent": agent_id,
                "tool.input_size": len(str(tool_input)) if tool_input else 0,
            }
        ) as span:
            span.add_event("tool_invoked", {"tool": tool_name})

            yield span

            span.add_event("tool_complete", {})

    @contextmanager
    def trace_decision(
        self,
        agent_id: str,
        decision_type: str,
        context: Optional[Dict[str, Any]] = None,
    ):
        """
        Trace a decision-making process.
        """
        with self.tracer.span(
            name=f"decision:{decision_type}",
            kind="internal",
            attributes={
                "decision.type": decision_type,
                "decision.agent": agent_id,
            }
        ) as span:
            if context:
                for key, value in context.items():
                    if isinstance(value, (str, int, float, bool)):
                        span.set_attribute(f"decision.context.{key}", value)

            span.add_event("decision_start", {"type": decision_type})

            yield span

            span.add_event("decision_complete", {})

    @contextmanager
    def trace_agent_communication(
        self,
        from_agent: str,
        to_agent: str,
        message_type: str,
    ):
        """
        Trace communication between agents.
        """
        with self.tracer.span(
            name=f"comm:{from_agent}->{to_agent}",
            kind="producer",
            attributes={
                "comm.from": from_agent,
                "comm.to": to_agent,
                "comm.type": message_type,
            }
        ) as span:
            span.add_event("message_sent", {
                "from": from_agent,
                "to": to_agent,
                "type": message_type,
            })

            yield span

            span.add_event("message_received", {})

    def record_agent_error(
        self,
        agent_id: str,
        error: Exception,
        context: Optional[Dict[str, Any]] = None,
    ):
        """Record an error from an agent"""
        span = self.tracer.current_span
        if span:
            span.add_event("agent_error", {
                "agent": agent_id,
                "error_type": type(error).__name__,
                "error_message": str(error),
                **(context or {}),
            })
            span.set_status(SpanStatus.ERROR, str(error))

    def record_agent_metric(
        self,
        agent_id: str,
        metric_name: str,
        value: float,
        unit: str = "",
    ):
        """Record a metric from an agent"""
        span = self.tracer.current_span
        if span:
            span.set_attribute(f"metric.{metric_name}", value)
            span.add_event("metric_recorded", {
                "agent": agent_id,
                "metric": metric_name,
                "value": value,
                "unit": unit,
            })

    def get_agent_traces(
        self,
        agent_id: str,
        limit: int = 100,
    ) -> List[Dict[str, Any]]:
        """Get recent traces for an agent"""
        all_traces = self.tracer.get_recent_traces(limit=limit * 2)

        # Filter to traces involving this agent
        agent_traces = []
        for trace_summary in all_traces:
            trace_id = trace_summary.get("trace_id")
            if trace_id:
                spans = self.tracer.get_trace(trace_id)
                for span in spans:
                    if span.get("agent_id") == agent_id:
                        agent_traces.append(trace_summary)
                        break

        return agent_traces[:limit]


# Global agent tracer instance
_agent_tracer: Optional[AgentTracer] = None


def get_agent_tracer() -> AgentTracer:
    """Get the global agent tracer"""
    global _agent_tracer
    if _agent_tracer is None:
        _agent_tracer = AgentTracer()
    return _agent_tracer
