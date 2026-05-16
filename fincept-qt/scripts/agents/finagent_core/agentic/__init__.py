"""
Agentic mode runtime — true-agentic extensions on top of ResumableTaskRunner.

Layered on top of the base agent runtime. With the user's "Agentic Mode"
toggle OFF (default), this package is loaded only when an `agentic_*` action
is dispatched from main.py, so the standard chat / plan flows are unaffected.

Phase 1 (this commit):
  - AgenticRunner: extends ResumableTaskRunner; emits per-step events; honors
    pause/cancel via DB status polling between steps.
  - events: JSONL stdout emitter consumed by AgentService_Execution.cpp.

Phase 2 (later commits):
  - reflector: post-step LLM critic (wraps CoreAgent.evaluate_response).
  - budget: token/$/wall-clock/step caps.
  - HITL: PlanStep.WAIT handling.
"""
