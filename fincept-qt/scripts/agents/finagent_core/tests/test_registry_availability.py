"""
Registries report what can load, and a run says which requested tools are
missing. Synthetic modules in tmp_path make the availability checks
independent of what this interpreter has installed.
"""
import sys
import textwrap
from types import SimpleNamespace
from unittest.mock import patch

import pytest


@pytest.fixture(autouse=True)
def _isolated(monkeypatch, tmp_path):
    monkeypatch.setenv("FINAGENT_DATA_DIR", str(tmp_path / "finagent"))
    monkeypatch.setenv("FINCEPT_AGENT_DATA_DIR", str(tmp_path / "agents"))


@pytest.fixture
def fake_pkg(tmp_path, monkeypatch):
    """zzfake_integrations.{present,missing,optional} on sys.path."""
    pkg = tmp_path / "zzfake_integrations"
    pkg.mkdir()
    (pkg / "__init__.py").write_text("")
    (pkg / "present.py").write_text("import json\n\nclass GoodTools:\n    pass\n")
    (pkg / "missing.py").write_text(textwrap.dedent("""
        try:
            import zz_not_installed_pkg
        except ImportError:
            raise ImportError("`zz-not-installed` not installed. Please install using `pip install zz-not-installed`")

        class NeedsPkgTools:
            pass
    """))
    (pkg / "optional.py").write_text(textwrap.dedent("""
        try:
            import zz_not_installed_pkg
        except ImportError:
            zz_not_installed_pkg = None

        class OptionalTools:
            pass
    """))
    monkeypatch.syspath_prepend(str(tmp_path))
    from finagent_core.registries import availability
    monkeypatch.setattr(availability, "_cache", {})
    monkeypatch.setattr(availability, "_static_cache", {})
    yield "zzfake_integrations"
    for name in [m for m in sys.modules if m.startswith("zzfake_integrations")]:
        del sys.modules[name]


def test_static_and_real_probe_agree(fake_pkg):
    from finagent_core.registries.availability import class_static_reason, class_unavailable_reason
    cases = {
        f"{fake_pkg}.present.GoodTools": None,
        f"{fake_pkg}.optional.OptionalTools": None,
    }
    for path, expected in cases.items():
        assert class_static_reason(path) is expected
        assert class_unavailable_reason(path) is expected
    missing = f"{fake_pkg}.missing.NeedsPkgTools"
    assert "pip install zz-not-installed" in class_static_reason(missing)
    assert "pip install zz-not-installed" in class_unavailable_reason(missing)
    renamed = f"{fake_pkg}.present.OldNameTools"
    assert "does not exist" in class_static_reason(renamed)
    assert "does not exist" in class_unavailable_reason(renamed)


def test_no_catalogue_entry_names_a_class_agno_does_not_have():
    """Guards against agno renames (JinaTools -> JinaReaderTools, LanceDB ->
    LanceDb, ...): a missing package is fine, a wrong class name is a bug."""
    from finagent_core.registries import (EmbedderRegistry, ModelsRegistry, ToolsRegistry,
                                          VectorDBRegistry)
    from finagent_core.registries.availability import class_static_reason
    paths = [p for tools in ToolsRegistry.TOOL_CATALOG.values() for p in tools.values()]
    paths += [c["class"] for c in ModelsRegistry.MODEL_CATALOG.values()]
    paths += [c["class"] for c in EmbedderRegistry.EMBEDDER_CATALOG.values()]
    paths += [c["class"] for c in VectorDBRegistry.VECTORDB_CATALOG.values()]
    drifted = {p: r for p in paths if (r := class_static_reason(p)) and
               ("does not exist in" in r or r.startswith("module "))}
    assert not drifted, drifted


def _with_fake_tools(fake_pkg):
    catalog = {"zz_fake": {"good_tool": f"{fake_pkg}.present.GoodTools",
                           "needs_pkg": f"{fake_pkg}.missing.NeedsPkgTools"}}
    from finagent_core.registries import ToolsRegistry
    return patch.multiple(ToolsRegistry, TOOL_CATALOG=catalog, _tool_map={}, _loaded_tools={})


def test_list_tools_keeps_shape_and_reports_unavailable(fake_pkg):
    from finagent_core.main import dispatch_action
    with _with_fake_tools(fake_pkg):
        out = dispatch_action("list_tools", {}, {}, {})
    # Shape the C++ reads is unchanged: tools{category: [names]}, categories, total_count.
    assert out["tools"] == {"zz_fake": ["good_tool", "needs_pkg"]}
    assert out["categories"] == ["zz_fake"] and out["total_count"] == 2
    assert set(out["unavailable"]) == {"needs_pkg"} and out["available_count"] == 1
    assert "zz-not-installed" in out["unavailable"]["needs_pkg"]


def test_list_models_reports_unavailable_providers():
    from finagent_core.main import dispatch_action
    from finagent_core.registries import ModelsRegistry
    catalog = {"good": {"class": "json.JSONDecoder", "models": [], "api_key_env": None},
               "gone": {"class": "json.NoSuchModel", "models": [], "api_key_env": None}}
    with patch.object(ModelsRegistry, "MODEL_CATALOG", catalog):
        out = dispatch_action("list_models", {}, {}, {})
    assert out["providers"] == ["good", "gone"] and out["count"] == 2
    assert list(out["unavailable"]) == ["gone"] and out["available_count"] == 1


def test_resolve_tools_names_each_missing_tool(fake_pkg):
    from finagent_core.registries import ToolsRegistry
    with _with_fake_tools(fake_pkg):
        tools, problems = ToolsRegistry.resolve_tools(["good_tool", "needs_pkg", "no_such_tool"])
    assert [type(t).__name__ for t in tools] == ["GoodTools"]
    assert set(problems) == {"needs_pkg", "no_such_tool"}
    assert "zz-not-installed" in problems["needs_pkg"]


def test_run_result_says_which_tools_are_unavailable(fake_pkg):
    """A requested tool that cannot load is reported in the run's `warnings`
    (it used to be dropped at DEBUG level). Terminal-bridge tool names are
    not reported, they are loaded by TerminalToolkit."""
    from agno.agent import Agent
    from finagent_core.main import dispatch_action
    ok = SimpleNamespace(status=SimpleNamespace(value="COMPLETED"), content="answer")
    config = {"model": {"provider": "openai", "model_id": "gpt-4o-mini", "api_key": "test"},
              "tools": ["good_tool", "needs_pkg", "polymarket_get_markets"],
              "terminal_tools": [{"name": "polymarket_get_markets"}]}
    with _with_fake_tools(fake_pkg), patch.object(Agent, "run", return_value=ok):
        out = dispatch_action("run", {}, {"query": "hi"}, config)
    assert out["success"] is True and out["response"] == "answer"
    assert len(out["warnings"]) == 1 and "needs_pkg" in out["warnings"][0]


def test_system_info_reports_availability():
    from finagent_core.main import dispatch_action
    out = dispatch_action("system_info", {}, {}, {})
    caps = out["capabilities"]
    assert isinstance(caps["vectordbs"], list) and "local" in caps["vectordbs"]
    assert "local" not in caps["unavailable"]["vectordbs"]
    assert set(caps["available_counts"]) == {"tools", "model_providers", "embedders", "vectordbs"}
