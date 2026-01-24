# AI Agents

> Multi-agent systems for trading, geopolitical analysis, and investment strategies

## Overview

Collection of 30+ AI agents powered by local LLMs (Ollama) and Langchain. Agents simulate investment strategies from legendary investors, hedge funds, and provide geopolitical intelligence analysis.

**Framework**: FinAgent Core - custom agent framework with LLM execution, tool registry, and database management.

## Agent Categories

| Category | Agents | Description |
|----------|--------|-------------|
| 🌍 **Geopolitics** | 19 agents | Grand Chessboard, Prisoners of Geography, World Order |
| 💰 **Hedge Funds** | 8 agents | Top hedge fund strategies (Bridgewater, Citadel, Renaissance, etc.) |
| 📊 **Legendary Investors** | 2 agents | Warren Buffett, Benjamin Graham value investing |
| 🤖 **Economic Analysis** | 1 agent | Economic policy and indicators |

## FinAgent Core Framework (`/finagent_core/`)

| Component | File | Purpose |
|-----------|------|---------|
| **Base Agent** | `base_agent.py` | Abstract agent class, lifecycle management |
| **LLM Executor** | `llm_executor.py` | Ollama integration, prompt execution |
| **Tool Registry** | `tools/tool_registry.py` | Tool management and discovery |
| **Database Manager** | `database/db_manager.py` | SQLite persistence, agent memory |
| **LLM Providers** | `config/llm_providers.py` | Model configurations (Llama, Mistral, etc.) |
| **Logger** | `utils/logger.py` | Structured logging |
| **Path Resolver** | `utils/path_resolver.py` | File path management |

## Geopolitical Agents (`/GeopoliticsAgents/`)

### Grand Chessboard Framework (5 agents)

Based on Zbigniew Brzezinski's geopolitical strategy:

| Agent | File | Focus |
|-------|------|-------|
| **American Primacy** | `american_primacy_agent.py` | US global leadership strategy |
| **Eurasian Balkans** | `eurasian_balkans_agent.py` | Central Asian geopolitics |
| **Heartland Theory** | `heartland_agent.py` | Mackinder's heartland control |
| **Pivots** | `pivots_agent.py` | Critical geopolitical pivot states |
| **Players** | `players_agent.py` | Major global power players |

### Prisoners of Geography Framework (10 agents)

Based on Tim Marshall's geographic constraints:

| Agent | File | Region Coverage |
|-------|------|-----------------|
| **Russia** | `russia_geography_agent.py` | Russian geographic constraints |
| **China** | `china_geography_agent.py` | Chinese territorial strategy |
| **USA** | `usa_geography_agent.py` | American geographic advantages |
| **Europe** | `europe_geography_agent.py` | European geographic challenges |
| **Middle East** | `middle_east_geography_agent.py` | Middle Eastern geography |
| **Africa** | `africa_geography_agent.py` | African development constraints |
| **India & Pakistan** | `india_pakistan_geography_agent.py` | South Asian geography |
| **Japan & Korea** | `japan_korea_geography_agent.py` | East Asian island nations |
| **Latin America** | `latin_america_geography_agent.py` | Latin American geography |
| **Arctic** | `arctic_geography_agent.py` | Arctic strategic importance |

### World Order Framework (4 agents)

Based on Henry Kissinger's world order analysis:

| Agent | File | Order Type |
|-------|------|------------|
| **American Order** | `american_order_agent.py` | Liberal international order |
| **Chinese Order** | `chinese_order_agent.py` | Confucian harmony concept |
| **European Order** | `european_order_agent.py` | Balance of power system |
| **Islamic Order** | `islamic_order_agent.py` | Islamic governance principles |
| **Multipolar Order** | `multipolar_order_agent.py` | Multiple power centers |

## Hedge Fund Agents (`/hedgeFundAgents/`)

Simulate strategies from world's top hedge funds:

| Hedge Fund | File/Directory | Strategy Style | AUM |
|------------|----------------|----------------|-----|
| **Bridgewater Associates** | `bridgewater_associates_hedge_fund_agent/` | Global macro, risk parity | $124B |
| **Citadel** | `citadel_hedge_fund_agent/` | Multi-strategy, quant | $62B |
| **Renaissance Technologies** | `renaissance_technologies_hedge_fund_agent/` | Quantitative, mathematical models | $55B |
| **Two Sigma** | `two_sigma_hedge_fund_agent/` | AI/ML, systematic trading | $60B |
| **D.E. Shaw** | | `de_shaw_hedge_fund_agent/` | Computational finance | $60B |
| **Elliott Management** | `elliott_management_hedge_fund_agent/` | Activist, distressed debt | $56B |
| **Pershing Square** | `pershing_square_hedge_fund_agent/` | Activist value investing | $16B |
| **AQR Capital** | `arq_capital_hedge_fund_agent/` | Factor investing, quant | $90B |

**Fincept Hedge Fund**: Custom multi-agent hedge fund system (`fincept_hedge_fund/main.py`)

## Legendary Investor Agents (`/TraderInvestorsAgent/`)

| Investor | File | Investment Philosophy |
|----------|------|----------------------|
| **Warren Buffett** | `warren_buffett_agent.py` | Value investing, moats, long-term holding |
| **Benjamin Graham** | `benjamin_graham_agent.py` | Deep value, margin of safety, Mr. Market |

## Economic Agents (`/EconomicAgents/`)

| Agent | Purpose |
|-------|---------|
| **Economic Analysis Agent** | Macroeconomic analysis, policy interpretation, indicator forecasting |

## Agent Capabilities

### All Agents Provide:
- **Analysis**: Market/geopolitical situation assessment
- **Recommendations**: Actionable investment or strategic advice
- **Risk Assessment**: Potential risks and mitigation strategies
- **Reasoning**: Transparent decision-making process
- **Memory**: Persistent state across conversations

### Geopolitical Agents:
- Regional conflict analysis
- Trade route vulnerabilities
- Resource competition assessment
- Strategic alliance evaluation
- Risk mapping for investments

### Hedge Fund Agents:
- Strategy-specific recommendations
- Portfolio construction
- Risk management approaches
- Market regime analysis
- Factor exposure analysis

### Investor Agents:
- Stock screening criteria
- Valuation analysis
- Quality assessment
- Entry/exit timing
- Position sizing

## Usage Examples

```python
# Geopolitical agent
from agents.GeopoliticsAgents.PrisonersOfGeographyAgents.region_agents.china_geography_agent import ChinaGeographyAgent
agent = ChinaGeographyAgent()
analysis = agent.analyze("Impact of Taiwan situation on semiconductor supply chain")

# Hedge fund agent
from agents.hedgeFundAgents.bridgewater_associates_hedge_fund_agent.bridgewater_associates_agent import BridgewaterAgent
agent = BridgewaterAgent()
recommendation = agent.analyze_market("US bond market outlook")

# Investor agent
from agents.TraderInvestorsAgent.warren_buffett_agent import WarrenBuffettAgent
agent = WarrenBuffettAgent()
evaluation = agent.evaluate_stock("AAPL")
```

## Agent Manager

**Central orchestration**: `agent_manager.py` - Manages agent lifecycle, routing, and coordination

```python
from agents.agent_manager import AgentManager

manager = AgentManager()
manager.register_agent('buffett', WarrenBuffettAgent())
response = manager.query('buffett', 'Should I invest in Apple?')
```

## LLM Models Supported

Via Ollama (local inference):

| Model | Size | Best For |
|-------|------|----------|
| **Llama 3.1** | 8B-70B | General reasoning, analysis |
| **Mistral** | 7B-22B | Financial analysis |
| **Mixtral** | 8x7B | Complex multi-step reasoning |
| **Phi-3** | 3.8B | Fast responses |
| **CodeLlama** | 7B-34B | Code generation, quant strategies |

## Configuration

Each agent category has `/configs/` directory with:
- LLM model selection
- Temperature/sampling parameters
- System prompts
- Tool configurations
- Memory settings

## Technical Details

- **Framework**: Custom FinAgent Core
- **LLM**: Ollama (local), Langchain integration
- **Database**: SQLite (agent memory, state)
- **Tools**: Extensible tool system
- **Language**: Python 3.11+
- **Dependencies**: langchain, ollama, sqlite3

## Agent Debate System

Agents can participate in multi-agent debates:
- Consensus building
- Contrarian analysis
- Risk devil's advocate
- Crowd wisdom aggregation

See `agno_trading/core/debate_orchestrator.py` for debate framework.

---

**Total Agents**: 30+ agents | **Categories**: 4 (Geopolitics, Hedge Funds, Investors, Economic) | **Framework**: FinAgent Core | **LLM**: Ollama (local) | **Last Updated**: 2026-01-23
