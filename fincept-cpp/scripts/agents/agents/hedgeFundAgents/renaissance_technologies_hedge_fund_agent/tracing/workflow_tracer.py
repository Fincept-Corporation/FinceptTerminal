"""
Workflow Tracer

Specialized tracing for workflow execution:
- Workflow step tracking
- Pipeline monitoring
- Step dependencies
- Performance analysis
"""

from typing import Optional, List, Dict, Any
from datetime import datetime
from contextlib import contextmanager
import uuid

from .base import Tracer, Span, SpanStatus, get_tracer


class WorkflowTracer:
    """
    Specialized tracer for workflow execution.

    Tracks:
    1. Workflow start/end
    2. Individual step execution
    3. Step dependencies and transitions
    4. Workflow performance metrics
    """

    def __init__(self, tracer: Optional[Tracer] = None):
        self.tracer = tracer or get_tracer()

        # Active workflows
        self._active_workflows: Dict[str, Span] = {}

    @contextmanager
    def trace_workflow(
        self,
        workflow_id: str,
        workflow_name: str,
        workflow_type: str,
        context: Optional[Dict[str, Any]] = None,
    ):
        """
        Trace a complete workflow execution.

        Usage:
            with workflow_tracer.trace_workflow("wf_123", "daily_cycle", "trading") as span:
                # execute workflow steps
                span.set_attribute("total_signals", 10)
        """
        with self.tracer.span(
            name=f"workflow:{workflow_name}",
            kind="internal",
            attributes={
                "workflow.id": workflow_id,
                "workflow.name": workflow_name,
                "workflow.type": workflow_type,
                **(context or {}),
            }
        ) as span:
            self._active_workflows[workflow_id] = span

            span.add_event("workflow_start", {
                "id": workflow_id,
                "name": workflow_name,
            })

            yield span

            span.add_event("workflow_complete", {
                "id": workflow_id,
            })

            del self._active_workflows[workflow_id]

    @contextmanager
    def trace_step(
        self,
        workflow_id: str,
        step_name: str,
        step_number: int,
        total_steps: int,
        input_data: Optional[Dict[str, Any]] = None,
    ):
        """
        Trace a workflow step.
        """
        workflow_span = self._active_workflows.get(workflow_id)

        with self.tracer.span(
            name=f"step:{step_name}",
            parent=workflow_span,
            kind="internal",
            attributes={
                "step.name": step_name,
                "step.number": step_number,
                "step.total": total_steps,
                "step.workflow": workflow_id,
            }
        ) as span:
            span.add_event("step_start", {
                "name": step_name,
                "number": step_number,
            })

            yield span

            span.add_event("step_complete", {
                "name": step_name,
                "number": step_number,
            })

    @contextmanager
    def trace_pipeline_stage(
        self,
        pipeline_name: str,
        stage_name: str,
        stage_input: Optional[Dict[str, Any]] = None,
    ):
        """
        Trace a pipeline stage.
        """
        with self.tracer.span(
            name=f"pipeline:{pipeline_name}:{stage_name}",
            kind="internal",
            attributes={
                "pipeline.name": pipeline_name,
                "pipeline.stage": stage_name,
            }
        ) as span:
            span.add_event("stage_start", {
                "pipeline": pipeline_name,
                "stage": stage_name,
            })

            yield span

            span.add_event("stage_complete", {
                "pipeline": pipeline_name,
                "stage": stage_name,
            })

    def record_step_result(
        self,
        step_name: str,
        success: bool,
        output_summary: Optional[str] = None,
        duration_ms: Optional[float] = None,
    ):
        """Record the result of a step"""
        span = self.tracer.current_span
        if span:
            span.set_attribute("step.success", success)
            if output_summary:
                span.set_attribute("step.output_summary", output_summary[:200])
            if duration_ms:
                span.set_attribute("step.duration_ms", duration_ms)

            span.add_event("step_result", {
                "step": step_name,
                "success": success,
            })

    def record_workflow_metric(
        self,
        workflow_id: str,
        metric_name: str,
        value: float,
    ):
        """Record a workflow metric"""
        workflow_span = self._active_workflows.get(workflow_id)
        if workflow_span:
            workflow_span.set_attribute(f"metric.{metric_name}", value)
            workflow_span.add_event("metric", {
                "name": metric_name,
                "value": value,
            })

    def record_workflow_error(
        self,
        workflow_id: str,
        step_name: str,
        error: Exception,
    ):
        """Record a workflow error"""
        workflow_span = self._active_workflows.get(workflow_id)
        if workflow_span:
            workflow_span.add_event("workflow_error", {
                "step": step_name,
                "error_type": type(error).__name__,
                "error_message": str(error),
            })
            workflow_span.set_status(SpanStatus.ERROR, f"Error in {step_name}: {error}")

        # Also record on current span
        span = self.tracer.current_span
        if span:
            span.set_status(SpanStatus.ERROR, str(error))
            span.add_event("exception", {
                "type": type(error).__name__,
                "message": str(error),
            })

    def get_workflow_trace(self, workflow_id: str) -> Dict[str, Any]:
        """Get complete trace for a workflow"""
        # Find the workflow's trace
        workflow_span = self._active_workflows.get(workflow_id)
        if workflow_span:
            trace_id = workflow_span.context.trace_id
            return {
                "workflow_id": workflow_id,
                "trace_id": trace_id,
                "spans": self.tracer.get_trace(trace_id),
            }
        return {"error": "Workflow not found"}

    def get_workflow_performance(self, workflow_id: str) -> Dict[str, Any]:
        """Get performance metrics for a workflow"""
        workflow_span = self._active_workflows.get(workflow_id)

        if not workflow_span:
            return {"error": "Workflow not found"}

        trace_id = workflow_span.context.trace_id
        spans = self.tracer.get_trace(trace_id)

        # Calculate metrics
        total_duration = workflow_span.duration_ms
        step_durations = []
        step_statuses = []

        for span_dict in spans:
            if span_dict.get("name", "").startswith("step:"):
                duration = span_dict.get("duration_ms")
                if duration:
                    step_durations.append(duration)
                step_statuses.append(span_dict.get("status", "unknown"))

        return {
            "workflow_id": workflow_id,
            "total_duration_ms": total_duration,
            "total_steps": len(step_durations),
            "avg_step_duration_ms": sum(step_durations) / len(step_durations) if step_durations else 0,
            "max_step_duration_ms": max(step_durations) if step_durations else 0,
            "steps_ok": step_statuses.count("ok"),
            "steps_error": step_statuses.count("error"),
        }


# Global workflow tracer instance
_workflow_tracer: Optional[WorkflowTracer] = None


def get_workflow_tracer() -> WorkflowTracer:
    """Get the global workflow tracer"""
    global _workflow_tracer
    if _workflow_tracer is None:
        _workflow_tracer = WorkflowTracer()
    return _workflow_tracer
