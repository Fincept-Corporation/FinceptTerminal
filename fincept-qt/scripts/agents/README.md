# AI agents

The agent frameworks behind Agent Config, the Agent Workbench, AI Quant Lab's
Deep Agent and RD-Agent panels. The C++ side (`src/services/agents/AgentService*`,
`src/services/ai_quant_lab/AIQuantLabService`) starts them through
`PythonRunner`; every entry point takes its parameters as JSON on **stdin**
(they carry LLM API keys) and prints JSON on stdout.

| Entry point | Framework | Used for |
|---|---|---|
| `finagent_core/main.py --stdin [--stream]` | agno | Single agents, teams, the SuperAgent router, execution plans, workflows, agentic mode. One JSON payload `{"action", "api_keys", "params", "config"}`; the actions are listed in `main()`'s docstring. |
| `deepagents/cli.py <command> --stdin` | LangChain deepagents | AI Quant Lab › Deep Agent (planning + tool-calling research tasks). |
| `rdagents/cli.py <command> --stdin` | RD-Agent | AI Quant Lab factor mining / model optimisation loops. |

## Personas are configuration, not code

Every persona is a JSON entry; there is no Python class per agent.

| File | Personas |
|---|---|
| `EconomicAgents/configs/agent_definitions.json` | 6 economic-school analysts (capitalist, Keynesian, neoliberal, socialist, mixed economy, mercantilist) |
| `GeopoliticsAgents/configs/agent_definitions.json` | 20 geopolitical analysts: Prisoners of Geography regions, World Order views, Grand Chessboard concepts |
| `TraderInvestorsAgent/configs/agent_definitions.json` | 11 investor personas (Buffett, Graham, Lynch, Munger, Klarman, Marks, Greenblatt, Einhorn, Miller, Eveillard, Whitman) |
| `finagent_core/configs/*.json` | 15 built-in specialists (analysis, portfolio, trading, macro cycle, central bank, currency, sentiment, supply chain, Polymarket, …) |

Each entry carries `id`, `name`, `description`, `category`, `version`,
`capabilities` and a `config` block (`model`, `instructions`, `tools`, …).
`finagent_core/agent_loader.py` discovers them from these directories at run
time. To add a persona, add an entry to the matching file; the model and keys
come from the user's LLM settings at run time, so never put a key in a config.

Agents reach the terminal itself — quotes, screens, paper trading — through the
MCP bridge (`finagent_core/tools/terminal_toolkit.py` → `TerminalMcpBridge`).

## Tests

`finagent_core/tests/` and `tests/agentic/` are pytest suites for the
framework. CI does not run them and they are not shipped; run them locally with
the app's `venv-numpy2` interpreter (`python -m pytest agents/finagent_core/tests`
from `fincept-qt/scripts/`).
