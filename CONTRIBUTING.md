# Contributing to Fincept Terminal

Welcome to the Fincept Terminal community! Your contributions help build the future of open-source financial technology.

## 🏗️ Tech Stack

- **Frontend**: React + TypeScript + shadcn/ui
- **Desktop**: Tauri (Rust)
- **Backend**: Python/Node.js sidecar processes
- **Heavy Processing**: Rust (when needed)

## 📁 Project Structure

```
/src-tauri          # Rust backend (Tauri core)
├── /src
│   └── main.rs     # Main Tauri application
├── /sidecar        # Python/Node.js processes
│   ├── /python     # Financial calculations & ML
│   └── /nodejs     # API services & data processing
└── Cargo.toml

/src                # React frontend
├── /components     # Reusable UI components (shadcn/ui)
├── /pages          # Main application pages
├── /hooks          # Custom React hooks
├── /lib            # Utilities and API calls
├── /types          # TypeScript type definitions
└── /assets         # Static assets

/public             # Static files
```

## 🎯 Contribution Areas

### Frontend (React + TypeScript)
- **Components**: Build reusable UI with shadcn/ui
- **Pages**: Create new financial analysis modules
- **Charts**: Implement trading charts and visualizations
- **Hooks**: Add data fetching and state management

### Backend Services (Python/Node.js Sidecars)
- **Data Fetching**: Add new market data sources
- **Financial Analysis**: Implement trading algorithms
- **ML Models**: Add predictive analytics
- **API Integration**: Connect external financial APIs

### Rust (Heavy Processing Only)
- **Performance-Critical**: Only when Python/Node.js isn't fast enough
- **System Integration**: File system, OS-level operations
- **Security**: Sensitive operations requiring Rust safety

## 🚀 Getting Started

### Prerequisites
```bash
# Install Rust
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh

# Install Node.js & pnpm
npm install -g pnpm

# Install Python dependencies
pip install -r requirements.txt
```

### Development Setup
```bash
# Clone repository
git clone https://github.com/Fincept-Corporation/FinceptTerminal
cd FinceptTerminal

# Install frontend dependencies  
pnpm install

# Run development server
pnpm tauri dev
```

## 📋 Contribution Guidelines

### 1. Frontend Contributions
**Focus**: React components, TypeScript, shadcn/ui

```typescript
// Example: New component
import { Card, CardHeader, CardTitle } from "@/components/ui/card"

export function StockCard({ symbol, price }: StockCardProps) {
  return (
    <Card>
      <CardHeader>
        <CardTitle>{symbol}: ${price}</CardTitle>
      </CardHeader>
    </Card>
  )
}
```

**Guidelines**:
- Use shadcn/ui components
- Add TypeScript types for all props
- Follow React best practices
- Write responsive designs

### 2. Backend Contributions  
**Focus**: Python/Node.js sidecar processes

```python
# Example: Python sidecar for financial data
def fetch_stock_data(symbols: list[str]) -> dict:
    # Fetch data from APIs
    # Process and return
    return processed_data
```

**Guidelines**:
- Use Python for ML/financial calculations
- Use Node.js for API services
- Handle errors gracefully
- Add proper logging

### 3. Rust Contributions
**Focus**: Only performance-critical operations

```rust
// Example: High-performance calculation
#[tauri::command]
async fn calculate_portfolio_risk(data: Vec<f64>) -> f64 {
    // Heavy mathematical operations
    data.iter().sum::<f64>() / data.len() as f64
}
```

**Guidelines**:
- Only for heavy processing
- Expose functions via `#[tauri::command]`
- Use async when possible

## 🔄 Development Workflow

### Adding New Features

1. **Frontend Feature**:
   ```bash
   # Create component
   /src/components/new-feature/
   
   # Add to page  
   /src/pages/analytics/
   ```

2. **Backend Service**:
   ```bash
   # Python sidecar
   /src-tauri/sidecar/python/new_service.py
   
   # Node.js sidecar  
   /src-tauri/sidecar/nodejs/api-service.js
   ```

3. **Connect Frontend to Backend**:
   ```typescript
   // API call from React
   import { invoke } from '@tauri-apps/api/tauri'
   
   const data = await invoke('fetch_market_data', { symbols })
   ```

### File Naming Conventions
- **Components**: `PascalCase` (e.g., `StockChart.tsx`)
- **Pages**: `kebab-case` (e.g., `market-analysis.tsx`)  
- **Hooks**: `camelCase` starting with `use` (e.g., `useMarketData.ts`)
- **Types**: `PascalCase` with `Props` suffix (e.g., `StockCardProps`)

## 🎨 UI/UX Guidelines

### Using shadcn/ui
```typescript
import { Button } from "@/components/ui/button"
import { Input } from "@/components/ui/input"
import { Card } from "@/components/ui/card"

// Always use shadcn components when available
<Button variant="default">Analyze</Button>
```

### Responsive Design
```typescript
// Use Tailwind classes for responsiveness
<div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-4">
  {/* Components */}
</div>
```

## 🧪 Testing

### Frontend Tests
```bash
pnpm test          # Run React tests
pnpm test:e2e      # End-to-end tests
```

### Backend Tests  
```bash
pytest             # Python sidecar tests
npm test           # Node.js sidecar tests
```

## 📝 Code Quality

### TypeScript
- Use strict type checking
- Add interfaces for all data structures
- Avoid `any` types

### React Best Practices
- Use functional components with hooks
- Implement proper error boundaries  
- Add loading states

### Python/Node.js
- Follow PEP 8 (Python) / ESLint (Node.js)
- Add type hints (Python) / JSDoc (Node.js)
- Handle async operations properly

## 🚀 Deployment

### Build Commands
```bash
pnpm tauri build   # Build for production
pnpm tauri build --debug  # Debug build
```

## 📞 Getting Help

- **GitHub Issues**: Report bugs and request features
- **Discussions**: Ask questions and share ideas  
- **Email**: support@fincept.in

## 🎯 Easy First Contributions

- Add new shadcn/ui components
- Create new chart visualizations
- Add financial data sources
- Improve TypeScript types
- Write documentation
- Add unit tests

---

**Start small, think big!** Every contribution helps build better financial technology for everyone.