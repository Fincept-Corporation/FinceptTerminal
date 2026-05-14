"""
JSONL stdout emitter for agentic-mode tasks.

Each event is a single line `AGENTIC_EVENT: <json>` followed by a newline,
flushed immediately so the parent QProcess can stream it. The C++ host
(AgentService_Execution.cpp readyReadStandardOutput handler) parses lines
prefixed with `AGENTIC_EVENT:` and republishes on DataHub topic
`task:event:<task_id>`.

Event kinds:
  task_started    { task_id, query }
  task_resumed    { task_id, from_step }
  plan_ready      { task_id, plan }
  step_start      { task_id, step_index, step }
  step_end        { task_id, step_index, result }
  paused          { task_id, at_step }
  cancelled       { task_id, at_step }
  done            { task_id, final, steps_completed }
  error           { task_id, error }
"""
import json
from typing import Any, Dict


EVENT_PREFIX = "AGENTIC_EVENT: "


def emit(kind: str, payload: Dict[str, Any]) -> None:
    """Write one JSONL event line to stdout and flush."""
    line = EVENT_PREFIX + json.dumps({"kind": kind, **payload}, ensure_ascii=False, default=str)
    # Single-shot print/flush keeps line atomicity across worker output.
    print(line, flush=True)


def make_emitter():
    """Return the emitter callable matching AgenticRunner's EventEmitter signature."""
    return emit
