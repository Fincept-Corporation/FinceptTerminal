"""Team and workflow members must resolve through PersonaRegistry so
   isolation applies even inside multi-agent flows."""

from unittest.mock import MagicMock, patch

import pytest

from finagent_core.core_agent import CoreAgent


@pytest.fixture
def core_agent(tmp_path, monkeypatch):
    monkeypatch.setenv("FINAGENT_DATA_DIR", str(tmp_path))
    return CoreAgent(api_keys={"openai": "sk-test"}, user_id="test_user")


def test_run_team_members_resolve_via_registry(core_agent):
    """Each team member should be a registry-cached runtime."""
    fake_runtime = MagicMock()
    fake_runtime.agno_agent = MagicMock()

    with patch.object(core_agent._registry, "get_or_create", return_value=fake_runtime) as spy, \
         patch("finagent_core.modules.TeamModule.from_config") as team_cls:
        fake_team = MagicMock()
        team_cls.return_value = fake_team
        fake_team.run.return_value = {"response": "team-ok"}

        team_config = {
            "name": "Value Investors",
            "agents": [
                {"id": "warren_buffett_agent", "name": "Buffett"},
                {"id": "charlie_munger_agent", "name": "Munger"},
            ],
        }
        core_agent.run_team("Pick stocks", team_config)

    # Registry consulted once per team member
    assert spy.call_count == 2
    seen_agent_ids = {call.kwargs["agent_id"] for call in spy.call_args_list}
    assert seen_agent_ids == {"warren_buffett_agent", "charlie_munger_agent"}
    # user_id propagates from CoreAgent
    assert all(call.kwargs["user_id"] == "test_user" for call in spy.call_args_list)


def test_run_team_member_reuses_solo_runtime(core_agent):
    """Team member Buffett should reuse the same PersonaRuntime as solo Buffett."""
    fake_runtime = MagicMock()
    fake_runtime.agno_agent = MagicMock()
    fake_runtime.run.return_value = {"response": "solo-ok"}

    with patch.object(core_agent._registry, "get_or_create", return_value=fake_runtime) as spy, \
         patch("finagent_core.modules.TeamModule.from_config") as team_cls:
        fake_team = MagicMock()
        team_cls.return_value = fake_team
        fake_team.run.return_value = {"response": "team-ok"}

        # Solo run
        core_agent.run("What do you think of AAPL?",
                       config={"id": "warren_buffett_agent"})
        # Team run containing Buffett
        core_agent.run_team("Pick stocks", {
            "agents": [{"id": "warren_buffett_agent"}]
        })

    buffett_calls = [c for c in spy.call_args_list
                     if c.kwargs["agent_id"] == "warren_buffett_agent"]
    assert len(buffett_calls) == 2
    # Same key -> registry cache hit -> same runtime (we mocked so same object)
    assert all(c.kwargs["user_id"] == "test_user" for c in buffett_calls)


def test_run_workflow_steps_resolve_via_registry(core_agent):
    """Each workflow step with agent_config should resolve via registry."""
    fake_runtime = MagicMock()
    fake_runtime.agno_agent = MagicMock()

    with patch.object(core_agent._registry, "get_or_create", return_value=fake_runtime) as spy, \
         patch("finagent_core.modules.WorkflowModule.from_config", create=True) as wf_cls:
        fake_wf = MagicMock()
        wf_cls.return_value = fake_wf
        fake_wf.run.return_value = {"response": "wf-ok"}

        workflow_config = {
            "name": "Research Pipeline",
            "steps": [
                {"name": "research", "agent_config": {"id": "warren_buffett_agent"}},
                {"name": "critique", "agent_config": {"id": "charlie_munger_agent"}},
                {"name": "no-agent-step"},  # steps without agent_config skipped
            ],
        }
        core_agent.run_workflow(workflow_config)

    assert spy.call_count == 2
    seen = {c.kwargs["agent_id"] for c in spy.call_args_list}
    assert seen == {"warren_buffett_agent", "charlie_munger_agent"}


def test_run_workflow_fallback_agent_id(core_agent):
    """Workflow step with no id/agent_id falls back to 'unnamed'."""
    fake_runtime = MagicMock()
    fake_runtime.agno_agent = MagicMock()

    with patch.object(core_agent._registry, "get_or_create", return_value=fake_runtime) as spy, \
         patch("finagent_core.modules.WorkflowModule.from_config", create=True) as wf_cls:
        fake_wf = MagicMock()
        wf_cls.return_value = fake_wf
        fake_wf.run.return_value = {}

        core_agent.run_workflow({
            "steps": [{"name": "anon", "agent_config": {"instructions": "hi"}}]
        })

    assert spy.call_count == 1
    assert spy.call_args_list[0].kwargs["agent_id"] == "unnamed"
