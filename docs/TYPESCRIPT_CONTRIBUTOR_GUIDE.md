# TypeScript/React Developer Intern Guide
## Fincept Terminal - Building Financial UI & Features

Welcome to the Fincept Terminal TypeScript/React development team! This guide will help you contribute to building user interfaces and features for the financial terminal, even if you're new to the project.

---

## 📋 Table of Contents

1. [What You'll Be Building](#what-youll-be-building)
2. [Getting Started](#getting-started)
3. [Understanding the Codebase](#understanding-the-codebase)
4. [Your First Contribution](#your-first-contribution)
5. [Component Development Guide](#component-development-guide)
6. [Tab Development](#tab-development)
7. [Working with APIs](#working-with-apis)
8. [Bloomberg-Style UI Standards](#bloomberg-style-ui-standards)
9. [State Management](#state-management)
10. [Testing & Debugging](#testing--debugging)
11. [Code Standards & Best Practices](#code-standards--best-practices)
12. [Submitting Your Work](#submitting-your-work)
13. [Resources & Learning](#resources--learning)

---

## 🎯 What You'll Be Building

As a TypeScript/React developer intern, you'll be creating:

- **Financial Terminal Tabs** - Dashboard, Markets, Analysis, Charts, News, etc.
- **UI Components** - Data tables, charts, forms, modals
- **Real-time Data Displays** - Stock quotes, market indices, economic indicators
- **Interactive Features** - Watchlists, alerts, portfolio tracking
- **Data Visualization** - Charts, graphs, heatmaps
- **Integration UIs** - API connectors, broker integrations, MCP servers

### The Vision

You're building a **Bloomberg Terminal alternative** - a professional-grade financial analysis platform with:
- Clean, terminal-style interface
- Real-time data updates
- Powerful analysis tools
- AI-powered insights
- All open source and free!

---

## 🚀 Getting Started

### Prerequisites

You need:
- **Node.js 18+** and **npm** installed
- **Rust** (for Tauri) - https://www.rust-lang.org/tools/install
- **Git** for version control
- A **code editor** (VS Code recommended)
- **Basic React/TypeScript knowledge**

### Setup Instructions

1. **Fork and Clone the Repository**
```bash
# Fork the repo on GitHub first, then:
git clone https://github.com/YOUR_USERNAME/FinceptTerminal.git
cd FinceptTerminal
```

2. **Navigate to the App Directory**
```bash
cd fincept-terminal-desktop
```

3. **Install Dependencies**
```bash
npm install
```

4. **Run Development Server**
```bash
# Option 1: Just frontend (hot reload, fast)
npm run dev

# Option 2: Full Tauri app (includes Rust backend)
npm run tauri dev
```

5. **Test the App**
- Frontend will open at http://localhost:1420
- Or Tauri window will launch
- Try navigating through different tabs
- Check browser console for errors

---

## 🏗️ Understanding the Codebase

### Project Structure

```
fincept-terminal-desktop/
├── src/                          # React frontend (YOUR WORK HERE)
│   ├── main.tsx                  # App entry point
│   ├── App.tsx                   # Root component
│   │
│   ├── components/
│   │   ├── auth/                 # Authentication screens
│   │   │   ├── LoginScreen.tsx
│   │   │   ├── RegisterScreen.tsx
│   │   │   └── PricingScreen.tsx
│   │   │
│   │   ├── dashboard/            # Main terminal interface
│   │   │   └── DashboardScreen.tsx   # Bloomberg-style dashboard
│   │   │
│   │   ├── tabs/                 # Financial terminal tabs
│   │   │   ├── DashboardTab.tsx      # Overview tab
│   │   │   ├── MarketsTab.tsx        # Live market data
│   │   │   ├── ChatTab.tsx           # AI chat
│   │   │   ├── NewsTab.tsx
│   │   │   ├── AnalyticsTab.tsx
│   │   │   ├── agents/               # AI agent management
│   │   │   ├── mcp/                  # MCP marketplace
│   │   │   └── data-sources/         # Data connectors
│   │   │
│   │   ├── ui/                   # Reusable components
│   │   │   ├── button.tsx
│   │   │   ├── input.tsx
│   │   │   ├── card.tsx
│   │   │   ├── table.tsx
│   │   │   └── ... (40+ components)
│   │   │
│   │   ├── common/               # Shared components
│   │   │   ├── Header.tsx
│   │   │   ├── Footer.tsx
│   │   │   └── BackgroundPattern.tsx
│   │   │
│   │   ├── payment/              # Payment screens
│   │   └── info/                 # Legal/info pages
│   │
│   ├── contexts/                 # React contexts
│   │   └── AuthContext.tsx       # Authentication state
│   │
│   ├── services/                 # API services
│   │   ├── authApi.tsx           # Auth API calls
│   │   ├── paymentApi.tsx        # Payment API
│   │   ├── forumApi.tsx          # Forum API
│   │   └── marketDataService.tsx # Market data
│   │
│   ├── hooks/                    # Custom React hooks
│   │   └── use-mobile.ts
│   │
│   ├── lib/                      # Utilities
│   │   └── utils.ts              # Helper functions
│   │
│   └── stockBrokers/             # Broker integrations
│
├── src-tauri/                    # Rust backend (less relevant)
├── package.json                  # Dependencies
├── tsconfig.json                 # TypeScript config
├── vite.config.ts                # Vite config
└── tailwind.config.js            # Tailwind config
```

### Tech Stack

- **React 19** - UI library
- **TypeScript** - Type-safe JavaScript
- **Vite** - Build tool (super fast!)
- **TailwindCSS v4** - Styling framework
- **Radix UI** - Accessible component primitives
- **Lucide React** - Icon library
- **Tauri** - Desktop app framework (Rust backend)

---

## 🎓 Your First Contribution

Let's build your first feature - a simple stock watchlist component!

### Step 1: Create a New Component

Create `src/components/tabs/WatchlistWidget.tsx`:

```tsx
import React, { useState } from 'react';
import { X, Plus } from 'lucide-react';

interface Stock {
  symbol: string;
  price: number;
  change: number;
  changePercent: number;
}

const WatchlistWidget: React.FC = () => {
  // State for watchlist
  const [stocks, setStocks] = useState<Stock[]>([
    { symbol: 'AAPL', price: 175.43, change: 2.34, changePercent: 1.35 },
    { symbol: 'MSFT', price: 420.67, change: 4.12, changePercent: 0.99 },
    { symbol: 'GOOGL', price: 140.23, change: -1.23, changePercent: -0.87 }
  ]);

  const [newSymbol, setNewSymbol] = useState('');

  // Bloomberg colors
  const BLOOMBERG_ORANGE = '#FFA500';
  const BLOOMBERG_WHITE = '#FFFFFF';
  const BLOOMBERG_RED = '#FF0000';
  const BLOOMBERG_GREEN = '#00C800';
  const BLOOMBERG_GRAY = '#787878';
  const BLOOMBERG_DARK_BG = '#1a1a1a';
  const BLOOMBERG_PANEL_BG = '#000000';

  // Add new stock
  const addStock = () => {
    if (!newSymbol) return;

    // In real app, fetch from API
    const newStock: Stock = {
      symbol: newSymbol.toUpperCase(),
      price: 100,
      change: 0,
      changePercent: 0
    };

    setStocks([...stocks, newStock]);
    setNewSymbol('');
  };

  // Remove stock
  const removeStock = (symbol: string) => {
    setStocks(stocks.filter(s => s.symbol !== symbol));
  };

  return (
    <div style={{
      backgroundColor: BLOOMBERG_PANEL_BG,
      border: `1px solid ${BLOOMBERG_GRAY}`,
      padding: '12px',
      borderRadius: '4px',
      maxWidth: '400px'
    }}>
      {/* Header */}
      <div style={{
        color: BLOOMBERG_ORANGE,
        fontSize: '14px',
        fontWeight: 'bold',
        marginBottom: '12px',
        fontFamily: 'Consolas, monospace'
      }}>
        MY WATCHLIST
      </div>

      {/* Add Stock Form */}
      <div style={{
        display: 'flex',
        gap: '8px',
        marginBottom: '12px'
      }}>
        <input
          type="text"
          value={newSymbol}
          onChange={(e) => setNewSymbol(e.target.value)}
          onKeyPress={(e) => e.key === 'Enter' && addStock()}
          placeholder="Enter symbol..."
          style={{
            backgroundColor: BLOOMBERG_DARK_BG,
            border: `1px solid ${BLOOMBERG_GRAY}`,
            color: BLOOMBERG_WHITE,
            padding: '6px 10px',
            fontSize: '12px',
            flex: 1,
            fontFamily: 'Consolas, monospace'
          }}
        />
        <button
          onClick={addStock}
          style={{
            backgroundColor: BLOOMBERG_ORANGE,
            border: 'none',
            color: 'black',
            padding: '6px 12px',
            fontSize: '12px',
            fontWeight: 'bold',
            cursor: 'pointer',
            display: 'flex',
            alignItems: 'center',
            gap: '4px'
          }}
        >
          <Plus size={14} />
          ADD
        </button>
      </div>

      {/* Stock List */}
      <div style={{ borderBottom: `1px solid ${BLOOMBERG_GRAY}`, marginBottom: '8px' }}></div>

      {stocks.length === 0 ? (
        <div style={{
          color: BLOOMBERG_GRAY,
          fontSize: '12px',
          textAlign: 'center',
          padding: '20px',
          fontFamily: 'Consolas, monospace'
        }}>
          No stocks in watchlist
        </div>
      ) : (
        stocks.map(stock => (
          <div
            key={stock.symbol}
            style={{
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'space-between',
              padding: '8px',
              borderBottom: `1px solid rgba(120,120,120,0.3)`,
              fontFamily: 'Consolas, monospace'
            }}
          >
            <div style={{ flex: 1 }}>
              <div style={{
                color: BLOOMBERG_WHITE,
                fontSize: '13px',
                fontWeight: 'bold'
              }}>
                {stock.symbol}
              </div>
              <div style={{
                color: BLOOMBERG_WHITE,
                fontSize: '11px'
              }}>
                ${stock.price.toFixed(2)}
              </div>
            </div>

            <div style={{
              color: stock.change >= 0 ? BLOOMBERG_GREEN : BLOOMBERG_RED,
              fontSize: '11px',
              textAlign: 'right',
              marginRight: '8px'
            }}>
              <div>
                {stock.change >= 0 ? '+' : ''}{stock.change.toFixed(2)}
              </div>
              <div>
                {stock.changePercent >= 0 ? '+' : ''}{stock.changePercent.toFixed(2)}%
              </div>
            </div>

            <button
              onClick={() => removeStock(stock.symbol)}
              style={{
                background: 'none',
                border: 'none',
                color: BLOOMBERG_GRAY,
                cursor: 'pointer',
                padding: '4px'
              }}
            >
              <X size={16} />
            </button>
          </div>
        ))
      )}
    </div>
  );
};

export default WatchlistWidget;
```

### Step 2: Test Your Component

Create a test page in `src/App.tsx`:

```tsx
import WatchlistWidget from './components/tabs/WatchlistWidget';

function App() {
  return (
    <div style={{
      backgroundColor: '#1a1a1a',
      minHeight: '100vh',
      padding: '20px'
    }}>
      <WatchlistWidget />
    </div>
  );
}

export default App;
```

Run `npm run dev` and see your component!

### Step 3: Add Real Data

Replace dummy data with actual API calls:

```tsx
import { useEffect } from 'react';
import { marketDataService } from '../../services/marketDataService';

const WatchlistWidget: React.FC = () => {
  // ... existing code ...

  // Fetch real data on mount
  useEffect(() => {
    const fetchData = async () => {
      const symbols = stocks.map(s => s.symbol);
      const quotes = await marketDataService.getQuotes(symbols);

      setStocks(quotes.map(q => ({
        symbol: q.symbol,
        price: q.price,
        change: q.change,
        changePercent: q.change_percent
      })));
    };

    fetchData();
  }, []);

  // ... rest of component ...
};
```

---

## 📚 Component Development Guide

### Component Structure Template

```tsx
import React, { useState, useEffect } from 'react';
import { SomeIcon } from 'lucide-react';

// Type definitions
interface ComponentProps {
  title: string;
  data: DataType[];
  onAction?: (id: string) => void;
}

interface DataType {
  id: string;
  name: string;
  value: number;
}

// Bloomberg color constants
const COLORS = {
  ORANGE: '#FFA500',
  WHITE: '#FFFFFF',
  RED: '#FF0000',
  GREEN: '#00C800',
  GRAY: '#787878',
  DARK_BG: '#1a1a1a',
  PANEL_BG: '#000000'
};

const MyComponent: React.FC<ComponentProps> = ({ title, data, onAction }) => {
  // State
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  // Effects
  useEffect(() => {
    // Fetch data, setup listeners, etc.
  }, []);

  // Event handlers
  const handleClick = (id: string) => {
    onAction?.(id);
  };

  // Render
  return (
    <div style={{
      backgroundColor: COLORS.PANEL_BG,
      border: `1px solid ${COLORS.GRAY}`,
      padding: '12px'
    }}>
      {/* Header */}
      <div style={{
        color: COLORS.ORANGE,
        fontSize: '14px',
        fontWeight: 'bold',
        marginBottom: '12px'
      }}>
        {title.toUpperCase()}
      </div>

      {/* Content */}
      {isLoading ? (
        <div>Loading...</div>
      ) : error ? (
        <div style={{ color: COLORS.RED }}>{error}</div>
      ) : (
        <div>
          {data.map(item => (
            <div key={item.id} onClick={() => handleClick(item.id)}>
              {item.name}: {item.value}
            </div>
          ))}
        </div>
      )}
    </div>
  );
};

export default MyComponent;
```

### Using Existing UI Components

Fincept Terminal has 40+ pre-built components in `src/components/ui/`:

```tsx
import { Button } from '@/components/ui/button';
import { Input } from '@/components/ui/input';
import { Card, CardHeader, CardTitle, CardContent } from '@/components/ui/card';
import { Table, TableBody, TableCell, TableHead, TableHeader, TableRow } from '@/components/ui/table';

function MyTab() {
  return (
    <Card>
      <CardHeader>
        <CardTitle>Market Data</CardTitle>
      </CardHeader>
      <CardContent>
        <Table>
          <TableHeader>
            <TableRow>
              <TableHead>Symbol</TableHead>
              <TableHead>Price</TableHead>
            </TableRow>
          </TableHeader>
          <TableBody>
            <TableRow>
              <TableCell>AAPL</TableCell>
              <TableCell>$175.43</TableCell>
            </TableRow>
          </TableBody>
        </Table>

        <Input placeholder="Enter symbol..." />
        <Button>Search</Button>
      </CardContent>
    </Card>
  );
}
```

---

## 🔥 Tab Development

### Creating a New Tab

Tabs are the main features of Fincept Terminal. Here's how to create one:

**Step 1: Create Tab File** (`src/components/tabs/MyNewTab.tsx`)

```tsx
import React, { useState, useEffect } from 'react';

const MyNewTab: React.FC = () => {
  const [currentTime, setCurrentTime] = useState(new Date());

  useEffect(() => {
    const timer = setInterval(() => {
      setCurrentTime(new Date());
    }, 1000);
    return () => clearInterval(timer);
  }, []);

  // Bloomberg colors
  const BLOOMBERG_ORANGE = '#FFA500';
  const BLOOMBERG_WHITE = '#FFFFFF';
  const BLOOMBERG_DARK_BG = '#1a1a1a';
  const BLOOMBERG_PANEL_BG = '#000000';
  const BLOOMBERG_GRAY = '#787878';

  return (
    <div style={{
      height: '100%',
      backgroundColor: BLOOMBERG_DARK_BG,
      color: BLOOMBERG_WHITE,
      fontFamily: 'Consolas, monospace',
      overflow: 'hidden',
      display: 'flex',
      flexDirection: 'column'
    }}>
      {/* Header Bar */}
      <div style={{
        padding: '8px 12px',
        backgroundColor: BLOOMBERG_PANEL_BG,
        borderBottom: `1px solid ${BLOOMBERG_GRAY}`,
        fontSize: '13px'
      }}>
        <span style={{ color: BLOOMBERG_ORANGE, fontWeight: 'bold' }}>
          MY NEW TAB
        </span>
        <span style={{ marginLeft: '16px', color: BLOOMBERG_WHITE }}>
          {currentTime.toLocaleTimeString()}
        </span>
      </div>

      {/* Main Content */}
      <div style={{
        flex: 1,
        overflow: 'auto',
        padding: '16px'
      }}>
        <div style={{
          color: BLOOMBERG_ORANGE,
          fontSize: '16px',
          fontWeight: 'bold',
          marginBottom: '16px'
        }}>
          TAB CONTENT
        </div>

        {/* Your content here */}
      </div>

      {/* Status Bar */}
      <div style={{
        borderTop: `1px solid ${BLOOMBERG_GRAY}`,
        backgroundColor: BLOOMBERG_PANEL_BG,
        padding: '6px 12px',
        fontSize: '10px',
        color: BLOOMBERG_GRAY
      }}>
        Status: Ready
      </div>
    </div>
  );
};

export default MyNewTab;
```

**Step 2: Register Tab** in `DashboardScreen.tsx`:

```tsx
// Import your tab
import MyNewTab from '../tabs/MyNewTab';

// Add to tabs array
const tabs = [
  { id: 'dashboard', label: 'Dashboard', component: DashboardTab },
  { id: 'markets', label: 'Markets', component: MarketsTab },
  { id: 'mynew', label: 'My New Tab', component: MyNewTab },  // ADD THIS
  // ... other tabs
];
```

**Step 3: Assign Function Key** (optional):

```tsx
// In DashboardScreen.tsx, add keyboard shortcut
useEffect(() => {
  const handleKeyPress = (e: KeyboardEvent) => {
    if (e.key === 'F9') {
      setActiveTab('mynew');
    }
  };

  window.addEventListener('keydown', handleKeyPress);
  return () => window.removeEventListener('keydown', handleKeyPress);
}, []);
```

---

## 🌐 Working with APIs

### API Service Pattern

All API calls go through service files in `src/services/`:

**Example: Market Data Service** (`marketDataService.tsx`)

```typescript
interface QuoteData {
  symbol: string;
  price: number;
  change: number;
  change_percent: number;
  volume?: number;
  high?: number;
  low?: number;
}

export const marketDataService = {
  async getQuote(symbol: string): Promise<QuoteData> {
    try {
      const response = await fetch(`/api/quote/${symbol}`);
      if (!response.ok) {
        throw new Error(`Failed to fetch ${symbol}`);
      }
      return await response.json();
    } catch (error) {
      console.error('Error fetching quote:', error);
      throw error;
    }
  },

  async getQuotes(symbols: string[]): Promise<QuoteData[]> {
    const promises = symbols.map(s => this.getQuote(s));
    return await Promise.all(promises);
  }
};
```

### Using Tauri Commands

For native functionality, invoke Rust commands:

```typescript
import { invoke } from '@tauri-apps/api/core';

// Call Rust backend
const result = await invoke<string>('fetch_market_data', {
  symbol: 'AAPL'
});

const data = JSON.parse(result);
```

### Real-time Data Updates

```tsx
const [quotes, setQuotes] = useState<QuoteData[]>([]);
const [lastUpdate, setLastUpdate] = useState(new Date());

// Auto-update every 10 seconds
useEffect(() => {
  const fetchData = async () => {
    const data = await marketDataService.getQuotes(['AAPL', 'MSFT', 'GOOGL']);
    setQuotes(data);
    setLastUpdate(new Date());
  };

  fetchData(); // Initial fetch

  const interval = setInterval(fetchData, 10000); // Every 10s
  return () => clearInterval(interval);
}, []);
```

---

## 🎨 Bloomberg-Style UI Standards

### Color Palette

Always use these exact colors:

```typescript
const BLOOMBERG_COLORS = {
  ORANGE: '#FFA500',      // Headers, highlights
  WHITE: '#FFFFFF',       // Primary text
  RED: '#FF0000',         // Negative values
  GREEN: '#00C800',       // Positive values
  YELLOW: '#FFFF00',      // Warnings, secondary headers
  GRAY: '#787878',        // Borders, disabled text
  DARK_BG: '#1a1a1a',     // Background
  PANEL_BG: '#000000'     // Panel backgrounds
};
```

### Typography

```css
/* Use Consolas monospace font */
fontFamily: 'Consolas, monospace'

/* Size hierarchy */
fontSize: '16px'  // Main headers
fontSize: '14px'  // Section headers
fontSize: '12px'  // Body text
fontSize: '11px'  // Small text
fontSize: '10px'  // Status bar
fontSize: '9px'   // Table data
```

### Layout Patterns

**Terminal Header:**
```tsx
<div style={{
  padding: '8px 12px',
  backgroundColor: '#000000',
  borderBottom: '1px solid #787878',
  fontSize: '13px'
}}>
  <span style={{ color: '#FFA500', fontWeight: 'bold' }}>
    SECTION NAME
  </span>
  <span style={{ marginLeft: '16px', color: '#FFFFFF' }}>
    Additional Info
  </span>
</div>
```

**Data Panel:**
```tsx
<div style={{
  backgroundColor: '#000000',
  border: '1px solid #787878',
  padding: '12px'
}}>
  {/* Content */}
</div>
```

**Positive/Negative Values:**
```tsx
<div style={{
  color: value >= 0 ? '#00C800' : '#FF0000'
}}>
  {value >= 0 ? '+' : ''}{value.toFixed(2)}%
</div>
```

---

## 🔄 State Management

### Using React Context

**Access Authentication:**

```tsx
import { useAuth } from '../../contexts/AuthContext';

function MyComponent() {
  const { user, isAuthenticated, login, logout } = useAuth();

  if (!isAuthenticated) {
    return <div>Please log in</div>;
  }

  return (
    <div>
      Welcome, {user?.email}!
      Subscription: {user?.subscription_plan}
    </div>
  );
}
```

### Local State

```tsx
// Simple state
const [count, setCount] = useState(0);

// Object state
const [formData, setFormData] = useState({
  symbol: '',
  quantity: 0
});

// Update object state
setFormData(prev => ({
  ...prev,
  symbol: 'AAPL'
}));
```

### Derived State

```tsx
const stocks = [...]; // array of stocks
const totalValue = stocks.reduce((sum, s) => sum + s.price, 0);
const avgChange = stocks.reduce((sum, s) => sum + s.changePercent, 0) / stocks.length;
```

---

## 🧪 Testing & Debugging

### Manual Testing Checklist

Before submitting:
- [ ] Component renders without errors
- [ ] All buttons/inputs work correctly
- [ ] Data loads properly
- [ ] Error states display correctly
- [ ] Loading states show
- [ ] Responsive on different screen sizes
- [ ] Bloomberg colors used correctly
- [ ] No console errors or warnings

### Debugging Tools

**React DevTools:**
- Install React DevTools browser extension
- Inspect component tree
- View props and state
- Track re-renders

**Console Logging:**
```tsx
console.log('Data loaded:', data);
console.error('Error occurred:', error);
console.table(arrayOfObjects); // Nice table view
```

**TypeScript Errors:**
```bash
# Check TypeScript errors
npm run build
```

### Common Issues

**Issue: "Cannot find module '@/components/ui/button'"**
**Fix:** Check import path, ensure `@` alias is configured in `vite.config.ts`

**Issue: "Hooks called out of order"**
**Fix:** Ensure hooks are always called in the same order, not inside conditions

**Issue: "State not updating"**
**Fix:** Use functional update: `setState(prev => ...)`

---

## ✅ Code Standards & Best Practices

### TypeScript Best Practices

**1. Always Define Types:**
```tsx
// ❌ BAD
const handleClick = (data: any) => { ... }

// ✅ GOOD
interface ClickData {
  id: string;
  value: number;
}
const handleClick = (data: ClickData) => { ... }
```

**2. Use Interfaces for Props:**
```tsx
interface ComponentProps {
  title: string;
  count: number;
  onUpdate?: (value: number) => void;
}

const Component: React.FC<ComponentProps> = ({ title, count, onUpdate }) => {
  // ...
};
```

**3. Use Enums for Constants:**
```tsx
enum TabType {
  Dashboard = 'dashboard',
  Markets = 'markets',
  News = 'news'
}
```

### React Best Practices

**1. Cleanup Effects:**
```tsx
useEffect(() => {
  const timer = setInterval(() => { ... }, 1000);

  // CLEANUP
  return () => clearInterval(timer);
}, []);
```

**2. Memoize Expensive Calculations:**
```tsx
import { useMemo } from 'react';

const expensiveValue = useMemo(() => {
  return complexCalculation(data);
}, [data]);
```

**3. Use Callbacks for Event Handlers:**
```tsx
import { useCallback } from 'react';

const handleClick = useCallback(() => {
  console.log('Clicked');
}, []);
```

**4. Conditional Rendering:**
```tsx
// ✅ GOOD
{isLoading && <LoadingSpinner />}
{error && <ErrorMessage error={error} />}
{data && <DataDisplay data={data} />}
```

### File Organization

**Component Structure:**
```
MyFeatureTab/
├── MyFeatureTab.tsx         # Main component
├── components/              # Sub-components
│   ├── DataTable.tsx
│   ├── FilterPanel.tsx
│   └── ChartView.tsx
├── hooks/                   # Custom hooks
│   └── useFeatureData.ts
└── types.ts                 # Type definitions
```

---

## 🚀 Submitting Your Work

### Pre-Submission Checklist

- [ ] Code builds without errors (`npm run build`)
- [ ] No TypeScript errors
- [ ] No console errors/warnings
- [ ] Follows Bloomberg color scheme
- [ ] Uses Consolas font
- [ ] Responsive design
- [ ] Tested in dev mode (`npm run tauri dev`)
- [ ] Code is commented
- [ ] Similar patterns to existing code

### Creating a Pull Request

**1. Create Branch:**
```bash
git checkout -b feature/watchlist-widget
```

**2. Commit Changes:**
```bash
git add src/components/tabs/WatchlistWidget.tsx
git commit -m "Add watchlist widget component

- Real-time price updates
- Add/remove stocks
- Bloomberg-style UI
- Responsive design"
```

**3. Push and Create PR:**
```bash
git push origin feature/watchlist-widget
```

**4. Fill PR Template:**
```markdown
## Description
Add watchlist widget for tracking favorite stocks

## Type of Change
- [x] New feature
- [ ] Bug fix
- [ ] UI improvement

## Screenshots
[Paste screenshot of the widget]

## Testing
- [x] Tested add/remove functionality
- [x] Tested real-time updates
- [x] Tested responsive design
- [x] No console errors

## Checklist
- [x] Code follows project style
- [x] Bloomberg colors used
- [x] TypeScript types defined
- [x] Tested in dev mode
```

---

## 📖 Resources & Learning

### React & TypeScript

**Basics:**
- React Docs: https://react.dev/
- TypeScript Handbook: https://www.typescriptlang.org/docs/
- React + TypeScript Cheatsheet: https://react-typescript-cheatsheet.netlify.app/

**Advanced:**
- React Hooks: https://react.dev/reference/react
- TypeScript Advanced Types: https://www.typescriptlang.org/docs/handbook/2/types-from-types.html

### UI/UX

**Design:**
- Bloomberg Terminal UI (Google images for reference)
- Radix UI Docs: https://www.radix-ui.com/
- TailwindCSS: https://tailwindcss.com/

**Charts & Visualization:**
- Lightweight Charts: https://tradingview.github.io/lightweight-charts/
- Recharts: https://recharts.org/

### Financial Domain

**Learn Finance:**
- Stock market basics
- Technical indicators (RSI, MACD, Bollinger Bands)
- Candlestick patterns
- Portfolio management

**Resources:**
- Investopedia: https://www.investopedia.com/
- Trading View ideas: https://www.tradingview.com/

### Project-Specific

**Study Existing Code:**
- `MarketsTab.tsx` - Real-time data, auto-refresh
- `DashboardTab.tsx` - Bloomberg layout, 3-panel design
- `ChatTab.tsx` - AI integration
- All components in `src/components/ui/`

---

## 💡 Project Ideas

### Beginner Tasks (Week 1-2)
- Fix UI bugs
- Add small features to existing tabs
- Create reusable UI components
- Improve mobile responsiveness

### Intermediate Tasks (Week 3-6)
- Build new tab (News, Screener, etc.)
- Integrate charting library
- Add real-time WebSocket data
- Create data visualization widgets

### Advanced Tasks (Week 7+)
- Build portfolio management UI
- Create backtesting interface
- Implement AI chat features
- Design custom indicators
- Build broker integration UIs

### High-Impact Features

1. **News Tab**
   - RSS feed integration
   - Sentiment analysis display
   - Real-time news ticker

2. **Screener Tab**
   - Stock screening filters
   - Custom criteria builder
   - Results table with sorting

3. **Chart Tab**
   - Interactive charts (TradingView-style)
   - Multiple timeframes
   - Technical indicators overlay

4. **Portfolio Tab**
   - Holdings display
   - P&L tracking
   - Asset allocation pie chart

5. **Alerts Tab**
   - Price alerts management
   - Technical indicator alerts
   - Notification system

---

## 🤝 Getting Help

**When Stuck:**
1. Check existing components for similar patterns
2. Read React error messages carefully
3. Search GitHub issues
4. Ask in GitHub Discussions
5. Reach out on community channels

**Where to Ask:**
- GitHub Issues: Bugs or technical questions
- GitHub Discussions: General help
- Project Discord/Slack (if available)

---

## 🎉 Final Words

You're now ready to build amazing financial UI features for Fincept Terminal!

**Remember:**
- Start with small components
- Study existing code patterns
- Test thoroughly
- Follow Bloomberg styling
- Ask questions when stuck
- Have fun coding!

Your work will help thousands of financial professionals access powerful analysis tools for free!

Welcome aboard! 🚀

---

**Repository**: https://github.com/Fincept-Corporation/FinceptTerminal/
**Contact**: dev@fincept.in
