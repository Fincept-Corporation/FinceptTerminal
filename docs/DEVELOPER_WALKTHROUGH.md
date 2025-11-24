# Developer Walkthrough - Fincept Terminal

**Version:** 3.0.11
**Last Updated:** 2025-01-04

## Quick Navigation

- [Project Structure](#project-structure)
- [Getting Started](#getting-started)
- [Frontend Architecture](#frontend-architecture)
- [Backend Architecture](#backend-architecture)
- [Key Systems](#key-systems)
- [Adding Features](#adding-features)
- [Common Tasks](#common-tasks)

---

## Project Structure

```
fincept-terminal-desktop/
├── src/                           # Frontend React application
│   ├── main.tsx                  # Entry point
│   ├── App.tsx                   # Root component
│   ├── components/               # UI components
│   ├── contexts/                 # React contexts (global state)
│   ├── services/                 # API & business logic
│   ├── hooks/                    # Custom React hooks
│   └── lib/                      # Utility libraries
│
├── src-tauri/                    # Backend Rust application
│   ├── src/
│   │   ├── lib.rs               # Main Tauri library
│   │   ├── main.rs              # Entry point
│   │   ├── commands/            # Tauri commands (exposed to frontend)
│   │   ├── data_sources/        # Data source utilities
│   │   └── utils/               # Backend utilities
│   ├── resources/
│   │   ├── python/              # Bundled Python runtime
│   │   └── scripts/             # Python analytics & agents
│   ├── Cargo.toml               # Rust dependencies
│   └── tauri.conf.json          # Tauri configuration
│
├── docs/                         # Documentation
├── public/                       # Static assets
└── package.json                  # Node.js dependencies
```

---

## Getting Started

### Prerequisites
- Node.js 18+ (LTS recommended)
- Rust 1.70+
- Python 3.11+ (bundled with app, needed for development)

### Installation
```bash
cd fincept-terminal-desktop
npm install
```

### Development
```bash
npm run dev           # Frontend only (port 1420)
npm run tauri dev     # Full app with Tauri
```

### Building
```bash
npm run build         # Frontend build
npm run tauri build   # Production build (creates installer)
```

---

## Frontend Architecture

### 📁 Directory Map

#### **Components** (`src/components/`)

**Authentication** (`auth/`)
- `LoginScreen.tsx` - User login
- `RegisterScreen.tsx` - New user registration
- `PricingScreen.tsx` - Subscription plans
- `ForgotPasswordScreen.tsx` - Password recovery

**Dashboard** (`dashboard/`)
- `DashboardScreen.tsx` - Main terminal layout & tab management

**Terminal Tabs** (`tabs/`)
All major features are in separate tab components:
- `DashboardTab.tsx` - Overview & metrics
- `MarketsTab.tsx` - Real-time market data
- `PortfolioTab.tsx` - Portfolio management → [`portfolio/`](../src/components/tabs/portfolio/)
- `AnalyticsTab.tsx` - Financial analytics
- `ChatTab.tsx` - AI chat (Ollama integration)
- `NodeEditorTab.tsx` - Visual workflow builder → [`node-editor/`](../src/components/tabs/node-editor/)
- `SettingsTab.tsx` - App configuration
- [See full list in CLAUDE.md](../CLAUDE.md#available-terminal-tabs)

**Specialized Modules**
- [`node-editor/`](../src/components/tabs/node-editor/) - Workflow system (ReactFlow-based)
  - `DataSourceNode.tsx` - Data source nodes
  - `MCPToolNode.tsx` - MCP tool nodes
  - `PythonAgentNode.tsx` - Python agent nodes
  - `WorkflowExecutor.ts` - Execution engine

- [`data-sources/`](../src/components/tabs/data-sources/) - Data connectivity
  - [`adapters/`](../src/components/tabs/data-sources/adapters/) - 100+ data connectors
    - `PostgreSQLAdapter.ts`, `MongoDBAdapter.ts`, `KafkaAdapter.ts`, etc.
  - `dataSourceConfigs.ts` - Adapter registry

- [`mcp/`](../src/components/tabs/mcp/) - Model Context Protocol integration
  - `MCPServerCard.tsx` - MCP server management UI
  - [`marketplace/`](../src/components/tabs/mcp/marketplace/) - MCP server marketplace
  - [`docs/`](../src/components/tabs/mcp/docs/) - MCP integration guides

**UI Components** (`ui/`)
shadcn/ui based components: button, card, dialog, dropdown, etc.

#### **Contexts** (`src/contexts/`)
Global state management via React Context:
- `AuthContext.tsx` - Authentication, user session
- `NavigationContext.tsx` - Tab navigation state
- `ThemeContext.tsx` - Theme preferences
- `DataSourceContext.tsx` - Data source connections

#### **Services** (`src/services/`)
API communication & business logic:

**Authentication & User**
- `authApi.tsx` - Login, register, session management
- `paymentApi.tsx` - Subscription payments
- `userApi.tsx` - User profile operations

**Market Data**
- `marketDataService.ts` - Unified market data interface
- `polygonService.ts` - Polygon.io integration
- `alphaVantageService.ts` - Alpha Vantage API
- `yfinanceService.ts` - Yahoo Finance wrapper

**Analytics & Portfolio**
- `portfolioService.ts` - Portfolio analytics
- `workflowService.ts` - Workflow execution engine
- `nodeExecutionManager.ts` - Node execution logic

**AI & Agents**
- `ollamaService.ts` - Local LLM (Ollama)
- `pythonAgentService.ts` - Python agent execution
- `agentLLMService.ts` - Agent-LLM bridge
- `mcpManager.ts` - MCP server lifecycle
- `mcpClient.ts` - MCP client implementation

**Data Management**
- `dataSourceRegistry.ts` - Register all data adapters
- `storageService.ts` - Local storage utilities
- `chatDatabase.ts` - Chat history (Dexie/IndexedDB)
- `duckdbService.ts` - DuckDB integration

#### **Hooks** (`src/hooks/`)
- `useDataSource.ts` - Data source connection hook
- `useWebSocket.ts` - WebSocket connection management
- `useAutoUpdater.ts` - Auto-update logic
- `use-mobile.ts` - Mobile detection

---

## Backend Architecture

### 📁 Rust Backend (`src-tauri/src/`)

#### **Main Files**
- **`lib.rs`** - Tauri application entry, command registration
  - MCP server management (spawn, kill, communicate)
  - Python script execution
  - Command handler registration (~900 commands!)

- **`main.rs`** - Application bootstrap

#### **Commands** (`commands/`)
Tauri commands expose backend functionality to frontend via `invoke()`:

**Market Data Commands**
- `market_data.rs` - Generic market data operations
- `polygon.rs` - Polygon.io API wrapper
- `yfinance.rs` - Yahoo Finance wrapper
- `alphavantage.rs` - Alpha Vantage API

**Economic Data Commands**
- `fred.rs` - Federal Reserve Economic Data
- `bis.rs` - Bank for International Settlements
- `bls.rs` - Bureau of Labor Statistics
- `eia.rs` - Energy Information Administration
- `ecb.rs` - European Central Bank
- `bea.rs` - Bureau of Economic Analysis
- `worldbank.rs` - World Bank data
- `oecd.rs` - OECD statistics
- `imf.rs` - International Monetary Fund
- `unesco.rs` - UNESCO statistics

**Government & Regulatory**
- `sec.rs` - SEC filings & data
- `cftc.rs` - CFTC Commitments of Traders
- `congress_gov.rs` - US Congress bills & legislation
- `government_us.rs` - US Treasury data
- `cboe.rs` - CBOE market data
- `nasdaq.rs` - NASDAQ data & screeners

**Financial Analytics**
- `fmp.rs` - Financial Modeling Prep API
- `multpl.rs` - Market multiples & valuations
- `trading_economics.rs` - Trading Economics data
- `econdb.rs` - EconDB integration

**International Data**
- `wto.rs` - World Trade Organization
- `canada_gov.rs`, `datagovuk.rs`, `datagov_au.rs` - Government data
- `estat_japan.rs`, `govdata_de.rs`, `datagovsg.rs` - Regional data
- `french_gov.rs`, `swiss_gov.rs`, `spain_data.rs` - European data

**Specialized**
- `portfolio.rs` - Portfolio analytics commands
- `agents.rs` - Python agent execution
- `jupyter.rs` - Jupyter notebook operations
- `coingecko.rs` - Cryptocurrency data
- `oscar.rs` - Satellite & instrument data
- `sentinelhub.rs` - Satellite imagery

**Command Registration**
All commands are registered in `lib.rs` (~line 390-945):
```rust
.invoke_handler(tauri::generate_handler![
    // ... 900+ commands
])
```

#### **Utilities** (`utils/`)
- Python path resolution
- Bundled runtime management

### 📁 Python Backend (`src-tauri/resources/scripts/`)

#### **Analytics Modules** ([`Analytics/`](https://github.com/Fincept-Corporation/FinceptTerminal/tree/main/fincept-terminal-desktop/src-tauri/resources/scripts/Analytics))
Complete CFA-level financial analytics:

- **`equityInvestment/`** - Stock valuation
  - DCF models (FCFF, FCFE)
  - Dividend discount models
  - Multiples analysis
  - Fundamental analysis

- **`portfolioManagement/`** - Portfolio optimization
  - Mean-variance optimization
  - Risk parity
  - Black-Litterman model
  - Factor models

- **`derivatives/`** - Options pricing
  - Black-Scholes model
  - Greeks calculation
  - Options strategies
  - Hedging analysis

- **`economics/`** - Economic indicators
  - GDP analysis
  - Inflation models
  - Policy analysis

- **`quant/`** - Quantitative finance
  - Statistical models
  - Backtesting frameworks
  - Risk metrics

- **`ml4Trading/`** - Machine learning
  - Prediction models
  - Feature engineering
  - Model evaluation

- **`technical_indicators.py`** - Technical analysis functions

#### **AI Agents** ([`agents/`](https://github.com/Fincept-Corporation/FinceptTerminal/tree/main/fincept-terminal-desktop/src-tauri/resources/scripts/agents))

**Investor Personas** (`TraderInvestorsAgent/`)
- Warren Buffett strategy
- Benjamin Graham value investing
- Peter Lynch growth investing
- Ray Dalio principles
- George Soros reflexivity
- 20+ legendary investors

**Hedge Fund Strategies** (`hedgeFundAgents/`)
- Bridgewater All-Weather portfolio
- Citadel multi-strategy quant
- Renaissance Technologies statistical arbitrage
- Two Sigma ML-driven strategies

**Geopolitical Analysis** (`GeopoliticsAgents/`)
- Grand Chessboard framework
- Prisoners of Geography analysis
- World Order models
- Central bank policy tracking

**Economic Agents** (`EconomicAgents/`)
- Macro analysis
- Policy impact assessment
- Economic forecasting

#### **Data Scripts**
Individual Python scripts for data APIs:
- `yfinance_data.py`, `polygon_data.py`, `alphavantage_data.py`
- `bis_data.py`, `fred_data.py`, `worldbank_data.py`
- `sec_data.py`, `eia_data.py`, `bls_data.py`
- 50+ data source scripts

---

## Key Systems

### 🔐 Authentication Flow

**Guest Users** (Temporary access)
1. User clicks "Continue as Guest" → `LoginScreen.tsx`
2. `authApi.guestLogin()` → Backend creates 24hr session
3. `AuthContext` stores session → localStorage
4. Navigate to dashboard

**Registered Users**
1. Login/Register → `LoginScreen.tsx` / `RegisterScreen.tsx`
2. `authApi.login()` / `authApi.register()` → JWT token
3. Check subscription status → redirect to pricing if needed
4. `AuthContext` manages session

**Files:**
- Frontend: `src/components/auth/*.tsx`, `src/contexts/AuthContext.tsx`, `src/services/authApi.tsx`
- Backend: Authentication handled by external API (not in Tauri)

### 💳 Payment System

1. User selects plan → `PricingScreen.tsx`
2. `paymentApi.createOrder()` → Order ID
3. Payment window opens (Razorpay/Stripe)
4. Callback → verify payment → update subscription
5. Navigate to dashboard

**Files:**
- `src/components/auth/PricingScreen.tsx`
- `src/components/payment/*.tsx`
- `src/services/paymentApi.tsx`

### 📊 Market Data Pipeline

**Unified Interface:**
```typescript
// Frontend
import { marketDataService } from '@/services/marketDataService';
const quote = await marketDataService.getQuote('AAPL');
```

**Data Flow:**
1. Frontend calls `marketDataService.ts`
2. Service selects provider (Polygon/Alpha Vantage/yfinance)
3. Invokes Tauri command: `invoke('get_market_quote', { symbol })`
4. Rust command executes Python script
5. Python fetches from API → returns JSON
6. Rust parses → returns to frontend
7. Frontend updates UI

**Files:**
- Frontend: `src/services/marketDataService.ts`, `src/services/polygonService.ts`
- Backend: `src-tauri/src/commands/market_data.rs`, `src-tauri/resources/scripts/polygon_data.py`

### 🤖 AI Chat (Ollama)

**Local LLM Integration:**
1. User sends message → `ChatTab.tsx`
2. `ollamaService.chat()` → Ollama API (local)
3. Stream response → update UI
4. Save to `chatDatabase.ts` (Dexie)

**Optional MCP Tools:**
- MCP servers provide tools to LLM
- `mcpManager.ts` manages server lifecycle
- LLM can invoke tools during chat

**Files:**
- `src/components/tabs/ChatTab.tsx`
- `src/services/ollamaService.ts`
- `src/services/mcpManager.ts`
- `src/services/chatDatabase.ts`

### 🔄 Workflow System (Node Editor)

**Visual Programming:**
1. User creates workflow → `NodeEditorTab.tsx` (ReactFlow)
2. Drag nodes: Data Sources, Transformations, MCP Tools, Python Agents
3. Connect nodes → define data flow
4. Execute → `WorkflowExecutor.ts`
5. Results displayed in `ResultsDisplayNode.tsx`

**Node Types:**
- **Data Source Node** (`DataSourceNode.tsx`) - Fetch data from 100+ sources
- **MCP Tool Node** (`MCPToolNode.tsx`) - Execute MCP tools
- **Python Agent Node** (`PythonAgentNode.tsx`) - Run AI agents
- **Results Node** (`ResultsDisplayNode.tsx`) - Display output

**Execution Engine:**
- `WorkflowExecutor.ts` - Topological sort, dependency resolution
- `nodeExecutionManager.ts` - Execute individual nodes
- `workflowService.ts` - Save/load workflows

**Files:**
- `src/components/tabs/NodeEditorTab.tsx`
- `src/components/tabs/node-editor/*`
- `src/services/workflowService.ts`

### 🔌 Data Sources (100+ Connectors)

**Architecture:**
```
BaseAdapter (abstract)
  ├── PostgreSQLAdapter
  ├── MongoDBAdapter
  ├── KafkaAdapter
  ├── ...
  └── RESTAdapter
```

**Adding a New Data Source:**
1. Create adapter: `src/components/tabs/data-sources/adapters/MyAdapter.ts`
   - Extend `BaseAdapter`
   - Implement `connect()`, `execute()`, `disconnect()`
2. Register: `src/components/tabs/data-sources/adapters/index.ts`
3. Add config: `src/components/tabs/data-sources/dataSourceConfigs.ts`
4. Now available in Node Editor + Data Sources tab

**Files:**
- All adapters: `src/components/tabs/data-sources/adapters/`
- See [ADD_NEW_DATA_SOURCE.md](ADD_NEW_DATA_SOURCE.md)

### 🛠️ MCP Integration

**Model Context Protocol** - Extend AI capabilities

**MCP Server Lifecycle:**
1. User adds server → `MCPMarketplace.tsx` or `MCPAddServerModal.tsx`
2. Frontend invokes: `invoke('spawn_mcp_server', { command, args })`
3. Rust spawns process → manages stdin/stdout
4. Frontend sends JSON-RPC → `mcpClient.ts`
5. Server returns tools/resources
6. Tools available in Node Editor & Chat

**Available in:**
- **Node Editor** - `MCPToolNode.tsx` executes MCP tools
- **Chat** - LLM can invoke MCP tools
- **MCP Tab** - Manage servers, view logs

**Adding MCP Server:**
- See `src/components/tabs/mcp/docs/ADDING_NEW_MCP.md`
- Define in `src/components/tabs/mcp/marketplace/serverDefinitions.ts`

**Files:**
- `src/components/tabs/mcp/*`
- `src/services/mcpManager.ts`, `src/services/mcpClient.ts`
- `src-tauri/src/lib.rs` (MCP commands: spawn, send, kill)

### 📈 Portfolio Analytics

**CFA-Level Analytics:**
1. User inputs portfolio → `PortfolioTab.tsx`
2. Frontend calls: `invoke('calculate_portfolio_metrics', { holdings })`
3. Rust executes: `src-tauri/src/commands/portfolio.rs`
4. Python script: `src-tauri/resources/scripts/Analytics/portfolioManagement/`
5. Returns: Sharpe ratio, VaR, beta, correlations, etc.

**Available Analytics:**
- Portfolio optimization (efficient frontier)
- Risk metrics (VaR, CVaR, max drawdown)
- Factor analysis
- Scenario analysis
- Retirement planning
- Behavioral bias detection

**Files:**
- Frontend: `src/components/tabs/PortfolioTab.tsx`, `src/components/tabs/portfolio/*`
- Backend: `src-tauri/src/commands/portfolio.rs`
- Python: `src-tauri/resources/scripts/Analytics/portfolioManagement/*`

### 🧠 Python Agents

**Execution Flow:**
1. User selects agent → Node Editor or Analytics tab
2. Frontend: `invoke('execute_python_agent', { agentPath, params })`
3. Rust: `src-tauri/src/commands/agents.rs`
4. Python: `src-tauri/resources/scripts/agents/[AgentCategory]/agent.py`
5. Agent executes → returns analysis
6. Frontend displays results

**Agent Categories:**
- Investor personas (Buffett, Dalio, etc.)
- Hedge fund strategies
- Geopolitical analysis
- Economic forecasting

**Files:**
- Frontend: `src/components/tabs/node-editor/PythonAgentNode.tsx`
- Backend: `src-tauri/src/commands/agents.rs`
- Python: `src-tauri/resources/scripts/agents/*`

---

## Adding Features

### ✨ Add a New Terminal Tab

**1. Create Component**
```tsx
// src/components/tabs/MyNewTab.tsx
import React from 'react';

const MyNewTab: React.FC = () => {
  return (
    <div className="p-4">
      <h1>My New Feature</h1>
    </div>
  );
};

export default MyNewTab;
```

**2. Register Tab**
Edit `src/components/dashboard/DashboardScreen.tsx`:
```tsx
import MyNewTab from '@/components/tabs/MyNewTab';

// Add to tabs array
const tabs = [
  // ...
  { id: 'mynew', label: 'My Feature', icon: Sparkles, component: MyNewTab },
];
```

**3. Test**
```bash
npm run tauri dev
```

### 🔌 Add a Data Source Adapter

**1. Create Adapter**
```typescript
// src/components/tabs/data-sources/adapters/MyAPIAdapter.ts
import { BaseAdapter } from './BaseAdapter';

export class MyAPIAdapter extends BaseAdapter {
  async connect(config: any): Promise<void> {
    // Initialize connection
  }

  async execute(query: string): Promise<any> {
    // Fetch data
  }

  async disconnect(): Promise<void> {
    // Cleanup
  }
}
```

**2. Register**
```typescript
// src/components/tabs/data-sources/adapters/index.ts
export { MyAPIAdapter } from './MyAPIAdapter';
```

**3. Add Config**
```typescript
// src/components/tabs/data-sources/dataSourceConfigs.ts
export const dataSourceConfigs = {
  // ...
  myapi: {
    name: 'My API',
    fields: [
      { name: 'apiKey', label: 'API Key', type: 'password' },
    ],
  },
};
```

### 🤖 Add a Python Analytics Module

**1. Create Python Module**
```python
# src-tauri/resources/scripts/Analytics/myAnalysis/calculator.py
import json
import sys

def calculate(data):
    # Your analysis logic
    result = { "metric": 42 }
    return result

if __name__ == "__main__":
    input_data = json.loads(sys.argv[1])
    output = calculate(input_data)
    print(json.dumps(output))
```

**2. Create Rust Command**
```rust
// src-tauri/src/commands/my_analysis.rs
use tauri::command;

#[command]
pub fn run_my_analysis(
    app: tauri::AppHandle,
    data: String,
) -> Result<String, String> {
    crate::execute_python_script(
        app,
        "Analytics/myAnalysis/calculator.py".to_string(),
        vec![data],
        None,
    )
}
```

**3. Register Command**
```rust
// src-tauri/src/commands/mod.rs
pub mod my_analysis;

// src-tauri/src/lib.rs
.invoke_handler(tauri::generate_handler![
    // ...
    commands::my_analysis::run_my_analysis,
])
```

**4. Call from Frontend**
```typescript
import { invoke } from '@tauri-apps/api/core';

const result = await invoke('run_my_analysis', {
  data: JSON.stringify({ values: [1, 2, 3] })
});
```

### 🛠️ Add an MCP Server Definition

**1. Define Server**
```typescript
// src/components/tabs/mcp/marketplace/serverDefinitions.ts
export const serverDefinitions = {
  // ...
  my_tool: {
    id: 'my_tool',
    name: 'My Custom Tool',
    description: 'Does something useful',
    command: 'npx',
    args: ['-y', '@my/mcp-server'],
    env: {},
    category: 'utilities',
  },
};
```

**2. (Optional) Create Custom Form**
```tsx
// src/components/tabs/mcp/marketplace/forms/MyToolForm.tsx
export const MyToolForm: React.FC<Props> = ({ onSubmit }) => {
  // Custom configuration UI
};
```

**3. User Can Now:**
- Install from MCP Marketplace tab
- Use in Node Editor (`MCPToolNode`)
- Available to AI chat

### 🎨 Add a Custom Node Type

**1. Create Node Component**
```tsx
// src/components/tabs/node-editor/MyCustomNode.tsx
import { Handle, Position } from 'reactflow';

export const MyCustomNode = ({ data }: NodeProps) => {
  return (
    <div className="custom-node">
      <Handle type="target" position={Position.Top} />
      <div>My Custom Logic</div>
      <Handle type="source" position={Position.Bottom} />
    </div>
  );
};
```

**2. Register Node Type**
```typescript
// src/components/tabs/NodeEditorTab.tsx
const nodeTypes = {
  // ...
  myCustom: MyCustomNode,
};
```

**3. Add Execution Logic**
```typescript
// src/components/tabs/node-editor/WorkflowExecutor.ts
case 'myCustom':
  result = await executeMyCustomLogic(node.data);
  break;
```

---

## Common Tasks

### 🔍 Debugging

**Frontend**
```bash
npm run dev  # Open http://localhost:1420
# Use browser DevTools (F12)
# React DevTools extension recommended
```

**Backend (Rust)**
```bash
npm run tauri dev
# Check terminal output for Rust logs
# println!() statements appear in terminal
```

**Python Scripts**
- Errors appear in Tauri terminal
- Add `print()` statements for debugging
- Output captured in Rust command

### 🧪 Testing

**Manual Testing**
```bash
npm run tauri dev  # Test in development
npm run tauri build  # Test production build
```

**Test Checklist:**
- Authentication flows (guest & registered)
- Tab navigation
- Data source connections
- Workflow execution
- Python analytics
- MCP server spawning

### 📦 Building Release

**Full Build**
```bash
npm run build  # Frontend
npm run tauri build  # Creates installer
```

**Output:**
- Windows: `.msi` in `src-tauri/target/release/bundle/msi/`
- macOS: `.dmg` in `src-tauri/target/release/bundle/dmg/`
- Linux: `.AppImage`, `.deb` in `src-tauri/target/release/bundle/`

**Version Bump**
```bash
npm run bump-version  # Auto-increment version
git tag v3.0.12
git push --tags
```

### 🔧 Common Issues

**Build fails with Rust errors:**
```bash
cd src-tauri
cargo clean
cargo build
```

**Python script not found:**
- Check path in Rust command
- Verify script exists in `src-tauri/resources/scripts/`
- Ensure Python runtime is bundled

**MCP server not spawning:**
- Check server command/args in definition
- Verify npm packages installed globally
- Check Tauri terminal for spawn errors

**Data source connection fails:**
- Verify adapter config
- Check API keys/credentials
- Review adapter implementation

### 📝 Code Style

**TypeScript:**
- Use functional components
- Prefer `const` over `let`
- Type all function parameters
- Use interfaces for complex types

**Rust:**
- Follow Rust naming conventions
- Use `Result<T, String>` for error handling
- Document public functions

**Python:**
- PEP 8 style guide
- Type hints recommended
- JSON input/output for Tauri integration

---

## File Quick Reference

### Essential Files

| File | Purpose |
|------|---------|
| `src/main.tsx` | Frontend entry point |
| `src/App.tsx` | Root component, routing |
| `src/components/dashboard/DashboardScreen.tsx` | Main terminal layout |
| `src/contexts/AuthContext.tsx` | Authentication state |
| `src/services/authApi.tsx` | Auth API calls |
| `src-tauri/src/lib.rs` | Tauri commands registration |
| `src-tauri/src/commands/mod.rs` | Command module registry |
| `src-tauri/tauri.conf.json` | Tauri configuration |
| `package.json` | Node.js dependencies |
| `src-tauri/Cargo.toml` | Rust dependencies |

### Key Directories

| Directory | Contents |
|-----------|----------|
| `src/components/tabs/` | All terminal tabs |
| `src/components/tabs/node-editor/` | Workflow system |
| `src/components/tabs/data-sources/adapters/` | Data connectors |
| `src/components/tabs/mcp/` | MCP integration |
| `src/services/` | Business logic & APIs |
| `src-tauri/src/commands/` | Backend commands |
| `src-tauri/resources/scripts/Analytics/` | Python analytics |
| `src-tauri/resources/scripts/agents/` | AI agents |

---

## Additional Resources

- [README.md](../README.md) - Project overview
- [CLAUDE.md](../CLAUDE.md) - Detailed architecture guide
- [ADD_NEW_DATA_SOURCE.md](ADD_NEW_DATA_SOURCE.md) - Data source tutorial
- [src/components/tabs/mcp/docs/ADDING_NEW_MCP.md](../src/components/tabs/mcp/docs/ADDING_NEW_MCP.md) - MCP integration

**External Docs:**
- [Tauri Documentation](https://tauri.app/v2/)
- [React 19 Docs](https://react.dev/)
- [ReactFlow Docs](https://reactflow.dev/)

---

## Get Help

- **Issues:** [GitHub Issues](https://github.com/Fincept-Corporation/FinceptTerminal/issues)
- **Discussions:** [GitHub Discussions](https://github.com/Fincept-Corporation/FinceptTerminal/discussions)
- **Email:** support@fincept.in

---

**Happy Building! 🚀**
