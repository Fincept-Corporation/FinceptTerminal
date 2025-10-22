# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**Fincept Terminal** is an open-source, Bloomberg-inspired financial analysis platform built with Tauri (Rust) + React + TypeScript. The goal is to create a data powerhouse for financial professionals by integrating 100+ data sources, AI agents, MCP connectors, and cutting-edge analysis tools - making institutional-grade financial analysis accessible to everyone.

**Vision**: One-stop solution for financial professionals integrating stocks, forex, crypto, commodities, economic data, trade data, supply chain analytics, research papers, and real-time analysis with AI-powered insights.

## Core Technology Stack

- **Frontend**: React 19 + TypeScript + Vite + TailwindCSS v4
- **Backend**: Tauri 2.0 (Rust) for native desktop wrapper
- **Python Integration**: Embedded Python runtime for API wrappers and data processing
- **UI Components**: Custom library based on Radix UI primitives (shadcn/ui inspired)
- **Data Storage**: SQLite (via tauri-plugin-sql), IndexedDB (Dexie), localStorage
- **State Management**: React Context API (AuthContext, ThemeContext)

## Development Commands

### Essential Commands
```bash
# Navigate to the main app directory first
cd fincept-terminal-desktop

# Install dependencies
npm install

# Development mode (Vite dev server + Tauri)
npm run tauri dev

# Build frontend only
npm run build

# Build production binary
npm run tauri build

# Run Vite dev server only (for frontend debugging)
npm run dev
```

### Project Structure Locations
```
fincept-terminal-desktop/
├── src/                          # React frontend
│   ├── components/
│   │   ├── auth/                 # Login, Register, Help, ForgotPassword
│   │   ├── dashboard/            # Main terminal dashboard
│   │   ├── payment/              # Payment processing screens
│   │   ├── tabs/                 # Financial terminal tabs
│   │   │   ├── MarketsTab.tsx    # Real-time market data
│   │   │   ├── DashboardTab.tsx  # Bloomberg-style overview
│   │   │   ├── ChatTab.tsx       # AI chat interface
│   │   │   ├── agents/           # AI agent management
│   │   │   ├── mcp/              # MCP server marketplace
│   │   │   └── data-sources/     # Data source connectors
│   │   ├── ui/                   # Reusable UI components
│   │   ├── common/               # Header, Footer, BackgroundPattern
│   │   └── info/                 # Legal pages
│   ├── contexts/                 # React contexts
│   ├── services/                 # API service layers
│   ├── hooks/                    # Custom React hooks
│   └── stockBrokers/             # Broker integrations
├── src-tauri/
│   ├── src/                      # Rust backend code
│   │   ├── main.rs               # Entry point
│   │   ├── lib.rs                # Library setup
│   │   ├── commands/             # Tauri commands
│   │   └── data_sources/         # Data source modules
│   ├── resources/
│   │   └── scripts/              # Python API wrappers
│   │       ├── polygon_data.py   # Polygon.io wrapper
│   │       ├── yfinance_data.py  # Yahoo Finance wrapper
│   │       ├── fred_data.py      # FRED economic data
│   │       └── econdb_data.py    # EconDB economic indicators
│   ├── Cargo.toml                # Rust dependencies
│   └── tauri.conf.json           # Tauri configuration
└── package.json                  # Node dependencies

legacy-python-depreciated/        # OLD DearPyGUI implementation (DO NOT USE)
```

## Architecture Deep Dive

### 1. Python Integration Architecture

**Embedded Python Runtime**: The project uses an embedded Python environment to run data fetching scripts that interface with various financial APIs.

**Location**: `fincept-terminal-desktop/src-tauri/resources/scripts/`

**Pattern**: All Python scripts follow a CLI-based JSON I/O pattern:
- Input: Command-line arguments
- Output: JSON to stdout
- Rust invokes via `Command::new("python").arg("script.py")`

**Example Python Wrapper Structure**:
```python
# Standard structure for all API wrappers
import sys
import json

def get_data(symbol, **kwargs):
    """Fetch data from API"""
    try:
        # API call logic
        return {"success": True, "data": result}
    except Exception as e:
        return {"error": str(e)}

def main():
    if len(sys.argv) < 2:
        print(json.dumps({"error": "Usage info"}))
        sys.exit(1)

    command = sys.argv[1]
    # Route to appropriate function
    result = get_data(sys.argv[2])
    print(json.dumps(result, indent=2))

if __name__ == "__main__":
    main()
```

### 2. Data Source Integration Pattern

**Current Data Sources** (in `resources/scripts/`):
- `polygon_data.py` - Polygon.io API (stocks, crypto, forex)
- `yfinance_data.py` - Yahoo Finance (quotes, historical, financials)
- `fred_data.py` - Federal Reserve Economic Data
- `econdb_data.py` - EconDB global economic indicators

**Adding New Data Sources**:
1. Create Python wrapper in `resources/scripts/`
2. Follow the CLI JSON I/O pattern
3. Support commands: `quote`, `historical`, `info`, etc.
4. Add Rust command in `src-tauri/src/commands/`
5. Expose to frontend via Tauri invoke

### 3. Frontend Architecture

**Tab-Based Bloomberg-Style Interface**:
- Dashboard: Bloomberg-inspired 3-panel overview
- Markets: Real-time market data with customizable watchlists
- Chat: AI-powered financial assistant (Ollama integration)
- Agents: AI agent orchestration and management
- MCP: Model Context Protocol server marketplace
- Data Sources: Connector management UI
- Docs: In-app documentation browser

**State Management**:
- `AuthContext` (`src/contexts/AuthContext.tsx`): User authentication, subscription, session
- Guest mode (24-hour device-based sessions) + Registered users (email/password)
- Payment integration with Razorpay
- Session persistence via localStorage

**UI Consistency**: All tabs use Bloomberg color scheme:
```javascript
const BLOOMBERG_ORANGE = '#FFA500';
const BLOOMBERG_WHITE = '#FFFFFF';
const BLOOMBERG_RED = '#FF0000';
const BLOOMBERG_GREEN = '#00C800';
const BLOOMBERG_GRAY = '#787878';
const BLOOMBERG_DARK_BG = '#1a1a1a';
const BLOOMBERG_PANEL_BG = '#000000';
```

### 4. API Service Pattern

All API services (`src/services/`) follow a consistent pattern:

```typescript
interface ApiResponse<T> {
  success: boolean;
  data?: T;
  error?: string;
  status_code?: number;
}

const makeApiRequest = async <T>(
  endpoint: string,
  options: RequestInit
): Promise<ApiResponse<T>> => {
  // Comprehensive error handling
  // CORS support
  // API key authentication
  // Structured logging
};
```

**Key Services**:
- `authApi.tsx` - Authentication, registration, session management
- `paymentApi.tsx` - Subscription plans, payment processing
- `forumApi.tsx` - Community forum features
- `marketDataService.tsx` - Market data fetching and caching

## Key Features & Implementation Status

### ✅ Completed Features
- Tauri 2.0 desktop application framework
- Authentication system (guest + registered users)
- Payment integration (Razorpay)
- Bloomberg-style UI with tab navigation
- Function key shortcuts (F1-F12)
- Forum tab with real functionality
- Python wrapper framework
- Basic market data integration (yfinance)
- SQLite caching layer

### 🚧 In Progress
- Real-time WebSocket market data streaming
- AI agent framework with Ollama
- MCP server marketplace
- Advanced charting (lightweight-charts)
- Data source connector UI
- Economic data integration (FRED, EconDB)

### 📋 Planned Features
- Broker integrations (Zerodha, Fyers, etc.)
- Multi-language support
- Plugin system
- Mobile companion app
- Custom indicator development
- Backtesting engine

## Development Guidelines

### Adding New Financial Data Sources

**Step 1: Create Python Wrapper** (`resources/scripts/new_source_data.py`)
```python
import sys
import json
import requests

class NewSourceAPI:
    def __init__(self, api_key: str):
        self.api_key = api_key
        self.base_url = "https://api.newsource.com"

    def get_data(self, symbol: str):
        # Implementation
        pass

def main():
    # CLI interface
    pass

if __name__ == "__main__":
    main()
```

**Step 2: Add Rust Command** (`src-tauri/src/commands/new_source.rs`)
```rust
use tauri::command;
use std::process::Command;

#[command]
pub async fn fetch_new_source_data(symbol: String) -> Result<String, String> {
    let output = Command::new("python")
        .arg("resources/scripts/new_source_data.py")
        .arg("quote")
        .arg(symbol)
        .output()
        .map_err(|e| e.to_string())?;

    let result = String::from_utf8_lossy(&output.stdout).to_string();
    Ok(result)
}
```

**Step 3: Register Command** (in `src-tauri/src/lib.rs`)
```rust
mod commands;
use commands::new_source::fetch_new_source_data;

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .invoke_handler(tauri::generate_handler![
            fetch_new_source_data,
            // ... other commands
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
```

**Step 4: Create Frontend Service** (`src/services/newSourceService.ts`)
```typescript
import { invoke } from '@tauri-apps/api/core';

export const newSourceService = {
  async getQuote(symbol: string) {
    const result = await invoke<string>('fetch_new_source_data', { symbol });
    return JSON.parse(result);
  }
};
```

### Adding New UI Tabs

1. Create component in `src/components/tabs/NewTab.tsx`
2. Follow Bloomberg color scheme
3. Use existing UI components from `src/components/ui/`
4. Add to tab list in `DashboardScreen.tsx`
5. Assign function key shortcut (F1-F12)

### Working with Authentication

**Guest Users**: Automatically created on first launch, 24-hour sessions
**Registered Users**: Email/password + API key authentication

```typescript
const { user, isAuthenticated, login, logout } = useAuth();

// Check subscription status
if (user?.subscription_plan === 'free') {
  // Limited features
}
```

## Important Notes

### DO NOT Modify
- `legacy-python-depreciated/` - Old DearPyGUI implementation (archived)
- Existing CLAUDE.md - Already contains legacy project context

### Python Environment
- Python scripts run via embedded interpreter
- Dependencies managed separately from main project
- Must be cross-platform compatible

### Rust Backend
- Keep lightweight - heavy processing in Python or frontend
- Use for: File I/O, native integrations, performance-critical paths
- Tauri commands are the bridge to frontend

### Data Caching Strategy
- **Short-term**: localStorage for UI state
- **Medium-term**: SQLite for market data (10-15 min cache)
- **Long-term**: IndexedDB for historical data

## Testing & Debugging

### Frontend Debugging
```bash
npm run dev  # Vite dev server with hot reload
```

### Full App Debugging
```bash
npm run tauri dev  # Opens app with DevTools
```

### Python Script Testing
```bash
cd fincept-terminal-desktop/src-tauri
python resources/scripts/yfinance_data.py quote AAPL
```

### Rust Backend Debugging
- Enable console in `tauri.conf.json` (development mode)
- Check Rust output in terminal running `tauri dev`

## GitHub Repository & Contribution

**Repository**: https://github.com/Fincept-Corporation/FinceptTerminal/
**Stars**: 376+ (as of 2025)
**License**: MIT

**Contribution Areas**:
1. **Python API Wrappers** - Add new financial data sources
2. **React Components** - UI improvements and new features
3. **Rust Performance** - High-computation modules
4. **Documentation** - Guides and tutorials
5. **Analysis Models** - Statistical models and indicators

## Intern Onboarding

This project welcomes interns in three tracks:

1. **Python Developers** - Build API wrappers for financial data providers
2. **TypeScript/React Developers** - Enhance UI and create new features
3. **Rust Developers** - Performance optimization and native integrations

See dedicated intern guides:
- `PYTHON_INTERN_GUIDE.md`
- `TYPESCRIPT_INTERN_GUIDE.md`
- `RUST_INTERN_GUIDE.md`

## Resources & Documentation

- **Tauri Docs**: https://tauri.app/
- **React 19 Docs**: https://react.dev/
- **Radix UI**: https://www.radix-ui.com/
- **TailwindCSS v4**: https://tailwindcss.com/
- **Yahoo Finance (yfinance)**: Python library for market data
- **Polygon.io API**: Financial data API
- **FRED API**: Federal Reserve economic data
