"""
A failed model run must never be reported as a successful answer.

agno returns a failed run (missing key, provider down) as a normal response
whose status is ERROR and whose content is the error text. These tests drive
the planner, the agentic runner, the workflows, plan execution, structured
runs and the streaming run with such a response — no network, no LLM.
"""
import sqlite3
from types import SimpleNamespace
from unittest.mock import patch

import pytest

from finagent_core.run_errors import NO_MODEL_MESSAGE, explain_llm_error, run_error_text

KEY_ERROR = "OPENAI_API_KEY not set. Please set the OPENAI_API_KEY environment variable."


def _failed(text=KEY_ERROR):
    return SimpleNamespace(status=SimpleNamespace(value="ERROR"), content=text)


def _ok(text):
    return SimpleNamespace(status=SimpleNamespace(value="COMPLETED"), content=text)


@pytest.fixture(autouse=True)
def _isolated_data(monkeypatch, tmp_path):
    monkeypatch.setenv("FINAGENT_DATA_DIR", str(tmp_path / "finagent"))
    monkeypatch.setenv("FINCEPT_AGENT_DATA_DIR", str(tmp_path / "agents"))


# ── helpers ──────────────────────────────────────────────────────────────────

def test_run_error_text_explains_missing_model():
    err = run_error_text(_failed())
    assert err.startswith(NO_MODEL_MESSAGE) and KEY_ERROR in err
    assert run_error_text(_ok("fine")) is None
    # Errors that are not about the model are passed through unchanged.
    assert explain_llm_error("Rate limit reached for tokens") == "Rate limit reached for tokens"


# ── planner (Planner tab: generate_dynamic_plan) ─────────────────────────────

def test_plan_fails_cleanly_without_model():
    from finagent_core.execution_planner import generate_dynamic_plan
    with patch("finagent_core.core_agent.CoreAgent.run", return_value=_failed()):
        out = generate_dynamic_plan("What is 2+2?")
    assert out["success"] is False
    assert NO_MODEL_MESSAGE in out["error"]
    assert "plan" not in out  # no canned "Stock Analysis: AAPL" plan


def test_unparseable_plan_runs_the_request_itself():
    from finagent_core.execution_planner import generate_dynamic_plan
    with patch("finagent_core.core_agent.CoreAgent.run", return_value=_ok("Sure! Here is my plan: step one...")):
        out = generate_dynamic_plan("What is GDP growth in the USA?")
    assert out["success"] is True and out["generated_by"] == "single_step_fallback"
    assert out["warning"]
    steps = out["plan"]["steps"]
    assert len(steps) == 1 and steps[0]["config"]["query"] == "What is GDP growth in the USA?"
    assert "AAPL" not in str(out) and "Stock Analysis" not in str(out)


def test_planner_exception_is_a_failure_not_a_plan():
    from finagent_core.execution_planner import generate_dynamic_plan
    with patch("finagent_core.core_agent.CoreAgent.run", side_effect=RuntimeError("Connection refused")):
        out = generate_dynamic_plan("anything")
    assert out["success"] is False and NO_MODEL_MESSAGE in out["error"]


# ── agentic runner ───────────────────────────────────────────────────────────

def _runner(tmp_path, events):
    from finagent_core.agentic.runner import AgenticRunner
    return AgenticRunner(api_keys={}, db_path=str(tmp_path / "tasks.db"),
                         emit=lambda kind, payload: events.append((kind, payload)))


def test_agentic_task_without_model_fails(tmp_path):
    events = []
    runner = _runner(tmp_path, events)
    with patch("finagent_core.core_agent.CoreAgent.run", return_value=_failed()):
        result = runner.start_task("What is 2+2?", {})
    kinds = [k for k, _ in events]
    assert result["success"] is False and NO_MODEL_MESSAGE in result["error"]
    assert kinds[-1] == "error" and "done" not in kinds and "plan_ready" not in kinds
    with sqlite3.connect(tmp_path / "tasks.db") as c:
        status = c.execute("SELECT status FROM agent_tasks WHERE id=?", (result["task_id"],)).fetchone()[0]
    assert status == "failed"


def test_agentic_step_failure_stops_the_task(tmp_path):
    plan = {"id": "p", "name": "Two steps", "description": "", "steps": [
        {"id": "step_1", "name": "Look up", "step_type": "agent", "config": {"query": "look up"}},
        {"id": "step_2", "name": "Write up", "step_type": "agent", "config": {"query": "write up"}},
    ]}
    events = []
    runner = _runner(tmp_path, events)
    with patch("finagent_core.execution_planner.generate_dynamic_plan",
               return_value={"success": True, "plan": plan}), \
         patch("finagent_core.core_agent.CoreAgent.run", return_value=_failed()):
        result = runner.start_task("Two-step task", {"agentic": {"reflector": False}})
    kinds = [k for k, _ in events]
    assert "plan_ready" in kinds and "done" not in kinds and "step_end" not in kinds
    err = dict(events)["error"]
    assert err["step_index"] == 0 and NO_MODEL_MESSAGE in err["error"]
    assert result["success"] is False


def test_runner_owns_replan_and_step_execution():
    # These two were indented under a module-level function after its return,
    # so they were dead code: _replan did not exist and budget accounting in
    # _execute_step never ran.
    from finagent_core.agentic.runner import AgenticRunner
    from finagent_core.task_state import ResumableTaskRunner
    assert callable(getattr(AgenticRunner, "_replan", None))
    assert AgenticRunner._execute_step is not ResumableTaskRunner._execute_step


# ── workflows (stock_analysis / portfolio_rebal / risk_assessment) ───────────

class _FakeWorkflowAgent:
    def __init__(self, response):
        self.response = response
        self.calls = 0

    def run(self, prompt, **kwargs):
        self.calls += 1
        return self.response


@pytest.mark.parametrize("action,params", [
    ("stock_analysis", {"symbol": "AAPL"}),
    ("portfolio_rebal", {}),
    ("risk_assessment", {}),
])
def test_workflows_report_failure(action, params):
    from finagent_core.main import dispatch_action
    fake = _FakeWorkflowAgent(_failed())
    with patch("finagent_core.modules.workflow_module._make_workflow_agent", return_value=fake), \
         patch("finagent_core.modules.workflow_module._load_workflow_tools", return_value=[]):
        out = dispatch_action(action, {}, dict(params), {})
    assert out["success"] is False and NO_MODEL_MESSAGE in out["error"]
    assert "response" not in out
    assert fake.calls == 1  # the first failure stops the workflow


def test_workflow_success_returns_final_report():
    from finagent_core.main import dispatch_action
    fake = _FakeWorkflowAgent(_ok("REPORT"))
    with patch("finagent_core.modules.workflow_module._make_workflow_agent", return_value=fake), \
         patch("finagent_core.modules.workflow_module._load_workflow_tools", return_value=[]):
        out = dispatch_action("stock_analysis", {}, {"symbol": "MSFT"}, {})
    assert out["success"] is True and out["response"] == "REPORT" and out["symbol"] == "MSFT"


# ── agent cards ──────────────────────────────────────────────────────────────

def test_agent_id_only_config_gets_its_card_in_a_fresh_process():
    """The loader registry is filled by discovery only; the card lookup used to
    miss in every fresh process, so the run silently used a generic assistant."""
    from finagent_core import agent_loader
    from finagent_core.main import _apply_agent_card
    registry = agent_loader.AgentRegistry()  # process-wide singleton
    saved = dict(registry._agents)
    registry._agents.clear()  # as in a freshly started process
    try:
        with patch.object(agent_loader, "_loader", None):
            cfg, name = _apply_agent_card({"agent_id": "warren_buffett_agent", "user_id": "u1"})
    finally:
        registry._agents.clear()
        registry._agents.update(saved)
    assert name and "Buffett" in name
    assert cfg.get("instructions") and cfg.get("tools")
    assert cfg["user_id"] == "u1"  # run wiring survives


# ── plan execution, structured run, streaming run ────────────────────────────

def test_execute_plan_marks_failed_agent_step():
    from finagent_core.execution_planner import execute_plan
    plan = {"id": "p", "name": "n", "steps": [
        {"id": "s1", "name": "Ask", "step_type": "agent", "config": {"query": "q"}}]}
    with patch("finagent_core.core_agent.CoreAgent.run", return_value=_failed()):
        out = execute_plan(plan, {})
    step = out["steps"][0]
    assert out["has_failed"] is True and step["status"] == "failed"
    assert NO_MODEL_MESSAGE in step["error"] and step["result"] is None


def test_run_structured_reports_the_model_error():
    from finagent_core.core_agent import CoreAgent
    with patch.object(CoreAgent, "run", return_value=_failed()):
        out = CoreAgent(api_keys={}).run_with_output_model("q", {}, "trade_signal")
    assert out["success"] is False and NO_MODEL_MESSAGE in out["error"]


def test_streaming_run_error_is_not_success(capsys):
    from finagent_core import main as main_mod

    def fake_stream(self, query, config, session_id=None):
        self.emit("error", "boom: API key required")
        yield {"type": "error", "content": "boom: API key required"}

    with patch("finagent_core.core_agent_stream.StreamingCoreAgent.run_streaming", fake_stream):
        out = main_mod.dispatch_action_streaming("run", {}, {"query": "hello", "stream": True}, {})
    printed = capsys.readouterr().out
    assert out["success"] is False and "API key required" in out["error"]
    assert "ERROR:" in printed and "DONE:" not in printed
