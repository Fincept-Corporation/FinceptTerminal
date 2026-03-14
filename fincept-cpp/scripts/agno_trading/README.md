# Agno Trading Agent System

A comprehensive AI trading agent framework powered by **Agno** for Fincept Terminal.

## Overview

This system provides intelligent AI trading agents that can:
- Analyze markets and generate insights
- Develop and execute trading strategies
- Manage portfolio risk
- Provide real-time decision support
- Learn from market patterns

## Architecture

```
agno_trading/
├── config/           # Configuration management
│   ├── settings.py   # Main configuration classes
│   └── models.py     # LLM model registry
├── core/             # Core agent framework
│   ├── agent_manager.py
│   └── team_orchestrator.py
├── agents/           # Specialized trading agents
│   ├── market_analyst.py
│   ├── trading_strategy.py
│   ├── risk_manager.py
│   └── portfolio_manager.py
├── tools/            # Agent tools
│   ├── market_data.py
│   ├── technical_indicators.py
│   └── order_execution.py
├── knowledge/        # Knowledge base
│   └── market_knowledge.py
└── utils/            # Utilities
    └── helpers.py
```

## Features

### Supported LLM Providers

- **OpenAI**: GPT-4, GPT-4 Turbo, GPT-4o, o1, o4
- **Anthropic**: Claude 3 (Opus, Sonnet, Haiku), Claude Sonnet 4
- **Google**: Gemini Pro, Gemini 1.5 Pro/Flash, Gemini 2.0 Flash
- **Groq**: Llama 3.1, Mixtral
- **DeepSeek**: DeepSeek Chat, DeepSeek Reasoner
- **xAI**: Grok, Grok-2
- **Ollama**: Local models (Llama, Mistral, Qwen, etc.)
- **Cohere, Mistral, Together, Fireworks**

### Agent Types

1. **Market Analyst Agent**
   - Analyzes market data and trends
   - Provides technical and fundamental insights
   - Monitors news and sentiment

2. **Trading Strategy Agent**
   - Develops trading strategies
   - Generates buy/sell signals
   - Optimizes entry/exit points

3. **Risk Manager Agent**
   - Monitors portfolio risk metrics
   - Calculates VaR and drawdowns
   - Provides risk mitigation recommendations

4. **Portfolio Manager Agent**
   - Manages asset allocation
   - Rebalances portfolio
   - Optimizes risk-adjusted returns

5. **Sentiment Analyst Agent**
   - Analyzes market sentiment
   - Monitors social media and news
   - Detects sentiment shifts

### Configuration Options

#### LLM Configuration
```python
{
  "provider": "openai",         # LLM provider
  "model": "gpt-4",             # Model name
  "temperature": 0.7,           # Creativity (0.0-1.0)
  "max_tokens": 4096,           # Max response length
  "top_p": 1.0,                 # Nucleus sampling
  "reasoning_effort": "medium", # For reasoning models
  "stream": false               # Enable streaming
}
```

#### Trading Configuration
```python
{
  "mode": "paper",              # paper, live, backtest
  "max_position_size": 0.1,     # Max 10% of portfolio
  "max_leverage": 1.0,          # No leverage
  "stop_loss_pct": 0.05,        # 5% stop loss
  "take_profit_pct": 0.15,      # 15% take profit
  "symbols": ["BTC/USD"],       # Trading symbols
  "exchange": "kraken"          # Exchange name
}
```

#### Knowledge Configuration
```python
{
  "vector_db": "lancedb",       # Vector database
  "embedder_provider": "openai",
  "embedder_model": "text-embedding-3-small",
  "search_type": "hybrid",      # vector, keyword, hybrid
  "search_limit": 5
}
```

#### Memory Configuration
```python
{
  "enable_memory": true,        # Persistent memory
  "enable_sessions": true,      # Session tracking
  "memory_db": "sqlite",        # Database backend
  "max_session_history": 100    # Max messages per session
}
```

## Installation

### 1. Install Python Dependencies

```bash
cd src-tauri/resources/scripts/agno_trading
pip install -r requirements.txt
```

### 2. Set API Keys

Create a `.env` file or set environment variables:

```bash
# OpenAI
export OPENAI_API_KEY="your-key"

# Anthropic
export ANTHROPIC_API_KEY="your-key"

# Google
export GOOGLE_API_KEY="your-key"

# Groq
export GROQ_API_KEY="your-key"

# DeepSeek
export DEEPSEEK_API_KEY="your-key"

# xAI (Grok)
export XAI_API_KEY="your-key"
```

## Usage from Rust/Tauri

### List Available Models

```rust
use tauri::command;

#[tauri::command]
async fn list_models(app: tauri::AppHandle) -> Result<String, String> {
    commands::agno_trading::agno_list_models(app, None).await
}
```

### Recommend Model for Task

```rust
commands::agno_trading::agno_recommend_model(
    app,
    "trading".to_string(),
    Some("medium".to_string()),
    Some(false)
).await
```

### Analyze Market

```rust
commands::agno_trading::agno_analyze_market(
    app,
    "BTC/USD".to_string(),
    Some("anthropic:claude-sonnet-4".to_string()),
    Some("comprehensive".to_string())
).await
```

### Generate Trade Signal

```rust
commands::agno_trading::agno_generate_trade_signal(
    app,
    "ETH/USD".to_string(),
    Some("momentum".to_string()),
    Some("openai:gpt-4o".to_string()),
    None
).await
```

### Risk Management

```rust
let portfolio_data = json!({
    "positions": [...],
    "total_value": 100000.0
}).to_string();

commands::agno_trading::agno_manage_risk(
    app,
    portfolio_data,
    Some("openai:gpt-4".to_string()),
    Some("moderate".to_string())
).await
```

## Usage from Frontend (TypeScript)

```typescript
import { invoke } from '@tauri-apps/api/core';

// List available models
const models = await invoke('agno_list_models', {
  provider: 'openai'
});

// Analyze market
const analysis = await invoke('agno_analyze_market', {
  symbol: 'BTC/USD',
  agentModel: 'anthropic:claude-sonnet-4',
  analysisType: 'comprehensive'
});

// Generate trade signal
const signal = await invoke('agno_generate_trade_signal', {
  symbol: 'ETH/USD',
  strategy: 'momentum',
  agentModel: 'openai:gpt-4o-mini'
});

// Risk management
const risk = await invoke('agno_manage_risk', {
  portfolioData: JSON.stringify(portfolio),
  agentModel: 'openai:gpt-4',
  riskTolerance: 'moderate'
});
```

## CLI Usage (Direct Python)

```bash
# List models
python agno_trading_service.py list_models

# Recommend model for task
python agno_trading_service.py recommend_model trading medium false

# Get configuration template
python agno_trading_service.py get_config_template default

# Analyze market
python agno_trading_service.py analyze_market BTC/USD openai:gpt-4o-mini comprehensive
```

## Response Format

All commands return JSON in this format:

```json
{
  "success": true,
  "data": {
    ...
  },
  "error": null
}
```

Error response:

```json
{
  "success": false,
  "data": null,
  "error": "Error message here"
}
```

## Advanced Features

### Multi-Agent Teams

Create teams of specialized agents that collaborate:

```python
team = Team(
    name="TradingTeam",
    members=[market_analyst, strategy_agent, risk_manager],
    lead_agent=strategy_agent,
    delegation_strategy="sequential"
)
```

### Workflows

Build deterministic trading workflows:

```python
workflow = Workflow(
    name="TradingWorkflow",
    steps=[
        fetch_market_data,
        analyze_market,
        generate_signal,
        check_risk,
        execute_trade
    ]
)
```

### Knowledge Integration

Agents can access:
- Historical market data
- Economic indicators
- News and sentiment
- Research reports
- Company filings

### Memory System

Agents remember:
- Past decisions and outcomes
- User preferences
- Market patterns
- Strategy performance

## Model Recommendations

### For Trading (Fast decisions)
- **High Budget**: `anthropic:claude-sonnet-4`
- **Medium Budget**: `openai:gpt-4o-mini`
- **Low Budget**: `groq:llama-3.1-70b`

### For Analysis (Deep insights)
- **High Budget**: `google:gemini-1.5-pro`
- **Medium Budget**: `anthropic:claude-3-sonnet`
- **Low Budget**: `groq:mixtral-8x7b`

### For Risk Management (Conservative)
- **High Budget**: `openai:gpt-4`
- **Medium Budget**: `anthropic:claude-3-sonnet`
- **Low Budget**: `deepseek:deepseek-chat`

### For Reasoning (Complex analysis)
- **High Budget**: `openai:o4`
- **Medium Budget**: `openai:o1-mini`
- **Low Budget**: `deepseek:deepseek-reasoner`

## Troubleshooting

### Missing API Keys
Error: `API key not found`
Solution: Set environment variables or configure in settings

### Model Not Found
Error: `Model 'xyz' not found for provider 'abc'`
Solution: Use `list_models` command to see available models

### Rate Limits
Error: `Rate limit exceeded`
Solution: Reduce request frequency or upgrade API plan

### Python Dependencies
Error: `Module not found`
Solution: Run `pip install -r requirements.txt`

## Performance Optimization

1. **Use appropriate models**: Don't use GPT-4 for simple tasks
2. **Enable caching**: Reduce redundant API calls
3. **Batch requests**: Process multiple items together
4. **Local models**: Use Ollama for development/testing
5. **Streaming**: Enable streaming for long responses

## Security

- API keys stored in environment variables
- Never log sensitive data
- Validate all inputs
- Rate limiting enabled
- Secure database connections

## Future Enhancements

- [ ] Real-time market data integration
- [ ] Backtesting framework
- [ ] Strategy optimization
- [ ] Multi-exchange support
- [ ] Advanced risk models
- [ ] Portfolio optimization
- [ ] Social trading features
- [ ] Performance analytics

## Support

For issues or questions:
- Check the documentation
- Review example code
- Contact support@fincept.in

## License

MIT License - Fincept Terminal

## Version

v1.0.0 - Initial Release
