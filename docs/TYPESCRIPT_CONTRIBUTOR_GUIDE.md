# TypeScript/React Contributor Guide

This guide covers frontend development for Fincept Terminal - 80 terminal tabs, 42 UI components, 35+ services, 12 contexts, and 8 hooks.

> **Prerequisites**: Read the [Contributing Guide](./CONTRIBUTING.md) first for setup and workflow.

---

## Table of Contents

- [Overview](#overview)
- [Project Structure](#project-structure)
- [Terminal Tabs](#terminal-tabs)
- [Services](#services)
- [Contexts & State](#contexts--state)
- [Component Development](#component-development)
- [Calling Rust Commands](#calling-rust-commands)
- [Styling](#styling)
- [Code Standards](#code-standards)
- [Resources](#resources)

---

## Overview

The frontend provides:
- **80 Terminal tabs** - Financial tools, analytics, trading
- **42 UI components** - Reusable shadcn/ui components
- **35+ Services** - API integrations, data fetching
- **12 Contexts** - Global state management
- **8 Custom hooks** - Reusable logic

**Related Guides:**
- [Rust Guide](./RUST_CONTRIBUTOR_GUIDE.md) - Backend commands
- [Python Guide](./PYTHON_CONTRIBUTOR_GUIDE.md) - Analytics scripts

---

## Project Structure

```
src/
├── main.tsx                       # Entry point
├── App.tsx                        # Root component, routing
│
├── components/
│   ├── tabs/                      # 80 terminal tabs
│   │   ├── dashboard/             # Main dashboard
│   │   ├── markets/               # Market data
│   │   ├── portfolio-tab/         # Portfolio management
│   │   ├── algo-trading/          # Algorithmic trading
│   │   ├── ai-quant-lab/          # AI/ML analytics
│   │   ├── crypto-trading/        # Cryptocurrency
│   │   ├── derivatives/           # Options & futures
│   │   ├── equity-research/       # Stock research
│   │   ├── screener/              # Stock screener
│   │   ├── geopolitics/           # Geopolitical analysis
│   │   ├── economics/             # Economic data
│   │   ├── chat/                  # AI chat
│   │   ├── node-editor/           # Visual workflows
│   │   ├── strategies/            # Trading strategies
│   │   ├── backtesting/           # Strategy testing
│   │   ├── quantlib-*/            # 15+ QuantLib modules
│   │   └── ...                    # 50+ more tabs
│   │
│   ├── ui/                        # 42 UI components
│   │   ├── button.tsx
│   │   ├── card.tsx
│   │   ├── table.tsx
│   │   ├── dialog.tsx
│   │   ├── dropdown-menu.tsx
│   │   ├── tabs.tsx
│   │   └── ...
│   │
│   ├── auth/                      # Authentication
│   ├── dashboard/                 # Terminal shell
│   ├── charts/                    # Chart components
│   ├── chat-mode/                 # AI chat interface
│   ├── command-bar/               # Command palette (cmdk)
│   ├── settings/                  # Settings screens
│   ├── payment/                   # Payment flow
│   └── visualization/             # 3D visualizations
│
├── services/                      # 35+ API services
│   ├── analytics/                 # Analytics APIs
│   ├── trading/                   # Trading APIs
│   ├── markets/                   # Market data
│   ├── portfolio/                 # Portfolio APIs
│   ├── auth/                      # Authentication
│   ├── mcp/                       # MCP integration
│   ├── news/                      # News feeds
│   ├── geopolitics/               # Geopolitical data
│   └── ...
│
├── contexts/                      # 12 React contexts
│   ├── AuthContext.tsx            # User authentication
│   ├── BrokerContext.tsx          # Broker connections
│   ├── DataSourceContext.tsx      # Data sources
│   ├── NavigationContext.tsx      # Tab navigation
│   ├── ThemeContext.tsx           # Theme settings
│   ├── WorkspaceContext.tsx       # Workspace state
│   ├── LanguageContext.tsx        # i18n
│   ├── TimezoneContext.tsx        # Timezone
│   ├── InterfaceModeContext.tsx   # Interface mode
│   ├── ProviderContext.tsx        # AI providers
│   └── StockBrokerContext.tsx     # Stock broker state
│
├── hooks/                         # 8 custom hooks
│   ├── useAutoUpdater.ts          # Auto-update
│   ├── useCache.ts                # Caching
│   ├── useDataSource.ts           # Data sources
│   ├── use-mobile.ts              # Mobile detection
│   ├── useRustWebSocket.ts        # WebSocket
│   ├── useTabSession.ts           # Tab sessions
│   ├── useWatchlist.ts            # Watchlist
│   └── useWorkspaceTabState.ts    # Workspace tabs
│
├── brokers/                       # Broker integrations
│   ├── crypto/                    # Crypto exchanges
│   ├── india/                     # Indian brokers
│   └── stocks/                    # Stock brokers
│
├── store/                         # Redux store
├── i18n/                          # Internationalization
├── paper-trading/                 # Paper trading
├── types/                         # TypeScript types
├── utils/                         # Utility functions
├── constants/                     # Constants
├── lib/                           # Shared libraries
└── polyfills/                     # Browser polyfills
```

---

## Terminal Tabs

### Tab Categories

| Category | Tabs | Description |
|----------|------|-------------|
| Markets | 5+ | Market data, charts, watchlists |
| Trading | 8+ | Algo trading, crypto, equity |
| Analytics | 10+ | Portfolio, risk, performance |
| Research | 5+ | Equity research, screener |
| QuantLib | 15+ | Quantitative analysis modules |
| AI/ML | 3+ | AI chat, agents, quant lab |
| Data | 5+ | Data sources, mapping, DBnomics |
| Other | 30+ | Geopolitics, economics, forum |

### Creating a New Tab

```tsx
// src/components/tabs/my-tab/MyTab.tsx
import React, { useState, useEffect } from 'react';
import { invoke } from '@tauri-apps/api/core';

const MyTab: React.FC = () => {
  const [data, setData] = useState<any>(null);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    const fetchData = async () => {
      try {
        const result = await invoke('my_command');
        setData(result);
      } catch (err) {
        console.error(err);
      } finally {
        setLoading(false);
      }
    };
    fetchData();
  }, []);

  if (loading) return <div>Loading...</div>;

  return (
    <div className="h-full bg-[#1a1a1a] text-white font-mono p-4">
      <h1 className="text-orange-500 font-bold">MY TAB</h1>
      {/* Content */}
    </div>
  );
};

export default MyTab;
```

---

## Services

### Service Categories

| Service | Purpose |
|---------|---------|
| `analytics/` | Financial calculations |
| `trading/` | Order management |
| `markets/` | Market data |
| `portfolio/` | Portfolio operations |
| `auth/` | Authentication |
| `news/` | News aggregation |
| `mcp/` | MCP integration |
| `geopolitics/` | Geopolitical data |
| `maritime/` | Maritime tracking |
| `screener/` | Stock screening |

### Service Pattern

```tsx
// services/marketService.ts
import { invoke } from '@tauri-apps/api/core';

export interface Quote {
  symbol: string;
  price: number;
  change: number;
  changePercent: number;
}

export const marketService = {
  async getQuote(symbol: string): Promise<Quote> {
    return invoke('get_quote', { symbol });
  },

  async getQuotes(symbols: string[]): Promise<Quote[]> {
    return invoke('get_quotes', { symbols });
  },

  async getHistoricalData(symbol: string, period: string) {
    return invoke('get_historical', { symbol, period });
  }
};
```

---

## Contexts & State

### Available Contexts

| Context | Purpose | Key Values |
|---------|---------|------------|
| `AuthContext` | User auth | user, isAuthenticated, login, logout |
| `BrokerContext` | Brokers | activeBroker, connect, disconnect |
| `DataSourceContext` | Data sources | sources, addSource, removeSource |
| `NavigationContext` | Navigation | activeTab, setActiveTab |
| `ThemeContext` | Theming | theme, setTheme |
| `WorkspaceContext` | Workspace | workspaces, activeWorkspace |

### Using Context

```tsx
import { useAuth } from '@/contexts/AuthContext';
import { useNavigation } from '@/contexts/NavigationContext';

function MyComponent() {
  const { user, isAuthenticated } = useAuth();
  const { activeTab, setActiveTab } = useNavigation();

  if (!isAuthenticated) {
    return <div>Please log in</div>;
  }

  return <div>Welcome, {user?.email}</div>;
}
```

---

## Component Development

### UI Components

Pre-built shadcn/ui components in `src/components/ui/`:

```tsx
import { Button } from '@/components/ui/button';
import { Card, CardHeader, CardContent } from '@/components/ui/card';
import { Table, TableBody, TableRow, TableCell } from '@/components/ui/table';
import { Dialog, DialogTrigger, DialogContent } from '@/components/ui/dialog';
import { Tabs, TabsList, TabsTrigger, TabsContent } from '@/components/ui/tabs';
```

### Component Template

```tsx
import React, { useState } from 'react';

interface Props {
  symbol: string;
  onSelect?: (symbol: string) => void;
}

const StockCard: React.FC<Props> = ({ symbol, onSelect }) => {
  const [price, setPrice] = useState<number>(0);

  return (
    <div
      className="bg-black border border-gray-600 p-3 cursor-pointer"
      onClick={() => onSelect?.(symbol)}
    >
      <span className="text-orange-500 font-bold">{symbol}</span>
      <span className="text-white ml-4">${price.toFixed(2)}</span>
    </div>
  );
};

export default StockCard;
```

---

## Calling Rust Commands

### Basic Invocation

```tsx
import { invoke } from '@tauri-apps/api/core';

// Simple call
const result = await invoke<string>('greet', { name: 'World' });

// With types
interface PortfolioResult {
  weights: number[];
  sharpe: number;
}

const portfolio = await invoke<PortfolioResult>('optimize_portfolio', {
  symbols: ['AAPL', 'MSFT', 'GOOGL'],
  optimize: true
});
```

### Error Handling

```tsx
try {
  const data = await invoke('fetch_data', { symbol: 'AAPL' });
  setData(data);
} catch (error) {
  console.error('Error:', error);
  setError(error as string);
}
```

---

## Styling

### Bloomberg Theme Colors

```tsx
const COLORS = {
  ORANGE: '#FFA500',    // Headers
  WHITE: '#FFFFFF',     // Text
  GREEN: '#00C800',     // Positive
  RED: '#FF0000',       // Negative
  GRAY: '#787878',      // Borders
  DARK_BG: '#1a1a1a',   // Background
  BLACK: '#000000'      // Panels
};
```

### Common Patterns

```tsx
// Header
<div className="text-orange-500 font-bold text-sm font-mono">
  SECTION HEADER
</div>

// Panel
<div className="bg-black border border-gray-600 p-3">
  Content
</div>

// Positive/Negative
<span className={value >= 0 ? 'text-green-500' : 'text-red-500'}>
  {value >= 0 ? '+' : ''}{value.toFixed(2)}%
</span>

// Table row
<div className="flex justify-between py-1 border-b border-gray-800">
  <span className="text-white">{symbol}</span>
  <span className="text-gray-400">${price}</span>
</div>
```

---

## Code Standards

### TypeScript

```tsx
// ✅ Good - Define types
interface Props {
  symbol: string;
  price: number;
  onUpdate?: (value: number) => void;
}

const Component: React.FC<Props> = ({ symbol, price, onUpdate }) => {
  // ...
};

// ❌ Bad - Using any
const Component = (props: any) => { ... };
```

### React Best Practices

```tsx
// Cleanup effects
useEffect(() => {
  const timer = setInterval(() => {}, 1000);
  return () => clearInterval(timer);
}, []);

// Memoize expensive calculations
const expensive = useMemo(() => calculate(data), [data]);

// Stable callbacks
const handleClick = useCallback(() => {
  console.log('Clicked');
}, []);
```

### Pre-Submission Checklist

- [ ] `bun run build` passes
- [ ] No TypeScript errors
- [ ] No console errors
- [ ] Bloomberg colors used
- [ ] Monospace font
- [ ] Tested in dev mode

---

## Resources

### React & TypeScript

- [React Docs](https://react.dev/)
- [TypeScript Handbook](https://www.typescriptlang.org/docs/)

### UI Libraries

- [Radix UI](https://www.radix-ui.com/)
- [TailwindCSS](https://tailwindcss.com/)
- [Lucide Icons](https://lucide.dev/)
- [shadcn/ui](https://ui.shadcn.com/)

### Charting

- [Lightweight Charts](https://tradingview.github.io/lightweight-charts/)
- [Recharts](https://recharts.org/)
- [Plotly](https://plotly.com/javascript/)
- [D3.js](https://d3js.org/)

### Tauri

- [Tauri v2 Docs](https://tauri.app/v2/)
- [invoke API](https://tauri.app/v2/reference/js/core/#invoke)

### Related Guides

- [Contributing Guide](./CONTRIBUTING.md) - General workflow
- [Rust Guide](./RUST_CONTRIBUTOR_GUIDE.md) - Backend commands
- [Python Guide](./PYTHON_CONTRIBUTOR_GUIDE.md) - Analytics scripts

---

**Questions?** Open an issue on [GitHub](https://github.com/Fincept-Corporation/FinceptTerminal).
