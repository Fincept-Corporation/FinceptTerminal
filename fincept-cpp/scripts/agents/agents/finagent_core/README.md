# FinAgent Core - Pure Agno Implementation

Single configurable agent system for the entire terminal.

## Architecture

```
CoreAgent (single instance)
    ↓
AgentFactory (creates Agno agent)
    ↓
Agno Agent (adapts to config)
```

## Components

### 1. **CoreAgent** (`core_agent.py`)
Single agent instance that recreates itself when config changes.

```python
from finagent_core import CoreAgent

agent = CoreAgent(api_keys={"OPENAI_API_KEY": "sk-..."})

config = {
    "model": {
        "provider": "openai",
        "model_id": "gpt-4-turbo",
        "temperature": 0.7
    },
    "instructions": "You are a portfolio analyst",
    "tools": ["yfinance", "calculator"]
}

response = agent.run("Analyze AAPL", config)
```

### 2. **AgentFactory** (`agent_factory.py`)
Creates Agno agents from configuration.

**Handles:**
- Model creation (OpenAI, Anthropic, Google, Groq, Ollama, DeepSeek)
- Tool loading (100+ Agno tools)
- Knowledge base setup
- Memory/storage configuration

### 3. **ConfigLoader** (`config_loader.py`)
Loads and validates tab configurations (for future use).

## Configuration Schema

```json
{
  "model": {
    "provider": "openai|anthropic|google|groq|ollama|deepseek",
    "model_id": "gpt-4-turbo",
    "temperature": 0.7,
    "max_tokens": 4096
  },
  "name": "Agent Name",
  "instructions": "System prompt...",
  "tools": ["yfinance", "calculator", "duckduckgo"],
  "memory": true,
  "output_format": "markdown|text|json",
  "knowledge": {
    "path": "docs/",
    "vector_db": {"type": "pgvector", "url": "..."},
    "embedder": {"type": "openai"}
  },
  "storage": {
    "type": "sqlite|postgres",
    "db_file": "sessions.db"
  }
}
```

## Usage from Rust/Frontend

```python
# Via agent_manager.py
python agent_manager.py execute_single_agent \
  '{"query": "Analyze NVDA stock"}' \
  '{"model": {"provider": "anthropic", "model_id": "claude-sonnet-4-5"}, "instructions": "Portfolio analyst"}' \
  '{"ANTHROPIC_API_KEY": "sk-..."}'
```

## Supported Models

| Provider | Models |
|----------|--------|
| OpenAI | gpt-4o, gpt-4-turbo, gpt-4, gpt-3.5-turbo |
| Anthropic | claude-sonnet-4-5, claude-3-5-sonnet, claude-3-opus |
| Google | gemini-2.0-flash, gemini-1.5-pro |
| Groq | llama-3.3-70b, mixtral-8x7b |
| Ollama | llama3.3, mistral, mixtral (local) |
| DeepSeek | deepseek-chat, deepseek-coder |

## Supported Tools

**Finance:**
- yfinance, financial_datasets

**Search:**
- duckduckgo, tavily

**Development:**
- python, calculator, file, shell

**Knowledge:**
- knowledge (RAG)

**100+ more available** via `agno.tools.*`

## Benefits

✅ **Single Agent** - One instance for entire terminal
✅ **Dynamic Config** - Changes behavior per tab
✅ **Full Agno** - Leverages all framework features
✅ **No Static Code** - Pure framework, no custom executors
✅ **Extensible** - Easy to add tools, models, knowledge

## Next Steps

1. Frontend sends config + query
2. CoreAgent adapts and responds
3. Tab-specific configs stored in DB (future)
