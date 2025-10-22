# 🚀 Contributing to Fincept Terminal

<div align="center">

**Help us build the future of open-source financial technology**

[![PRs Welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg)](http://makeapullrequest.com)
[![First Timers Friendly](https://img.shields.io/badge/first--timers--friendly-blue.svg)](https://www.firsttimersonly.com/)

*Every contribution counts, no matter how small!*

</div>

---

## 📋 Table of Contents

- [🎯 Ways to Contribute](#-ways-to-contribute)
- [🏗️ Tech Stack](#️-tech-stack)
- [⚡ Quick Start](#-quick-start)
- [💻 Contribution Areas](#-contribution-areas)
- [🐍 Python Script Contributions](#-python-script-contributions)
- [📁 Project Structure](#-project-structure)
- [🔄 Development Workflow](#-development-workflow)
- [✅ Pull Request Process](#-pull-request-process)
- [🧪 Testing Guidelines](#-testing-guidelines)
- [📝 Code Style](#-code-style)

---

## 🎯 Ways to Contribute

<table>
<tr>
<td width="33%">

### 💻 **Code**
- Fix bugs
- Add features
- Write Python scripts
- Improve performance
- Refactor code

</td>
<td width="33%">

### 📚 **Documentation**
- Improve README
- Write tutorials
- API documentation
- Code examples
- Translations

</td>
<td width="33%">

### 🐛 **Testing & QA**
- Report bugs
- Test features
- Write tests
- Review PRs
- Improve CI/CD

</td>
</tr>
<tr>
<td width="33%">

### 🎨 **Design**
- UI/UX improvements
- Component design
- Icon creation
- Screenshots
- Demo videos

</td>
<td width="33%">

### 🌐 **Community**
- Answer questions
- Write blog posts
- Share on social media
- Organize events
- Mentor newcomers

</td>
<td width="33%">

### 💡 **Ideas**
- Suggest features
- Share use cases
- Provide feedback
- Report issues
- Discuss improvements

</td>
</tr>
</table>

---

## 🏗️ Tech Stack

<div align="center">

| Layer | Technologies |
|-------|-------------|
| **Frontend** | ![React](https://img.shields.io/badge/React-19-61DAFB?logo=react) ![TypeScript](https://img.shields.io/badge/TypeScript-5.6-3178C6?logo=typescript) ![TailwindCSS](https://img.shields.io/badge/TailwindCSS-4.0-38B2AC?logo=tailwind-css) |
| **Desktop** | ![Tauri](https://img.shields.io/badge/Tauri-2.0-FFC131?logo=tauri) ![Rust](https://img.shields.io/badge/Rust-1.70+-CE422B?logo=rust) |
| **Data Processing** | ![Python](https://img.shields.io/badge/Python-3.11+-3776AB?logo=python) ![Node.js](https://img.shields.io/badge/Node.js-22-339933?logo=node.js) |
| **UI Components** | ![Radix UI](https://img.shields.io/badge/Radix_UI-Latest-161618) ![shadcn/ui](https://img.shields.io/badge/shadcn/ui-Latest-000000) |

</div>

---

## ⚡ Quick Start

### 🚀 **Automated Setup (Recommended)**

**Windows:**
```bash
# Clone and setup
git clone https://github.com/YOUR_USERNAME/FinceptTerminal.git
cd FinceptTerminal

# Run automated setup (as Administrator)
setup.bat
```

**Linux/macOS:**
```bash
# Clone and setup
git clone https://github.com/YOUR_USERNAME/FinceptTerminal.git
cd FinceptTerminal

# Run automated setup
chmod +x setup.sh
./setup.sh
```

The automated script installs:
- ✅ Node.js LTS (v22.14.0)
- ✅ Rust (latest stable)
- ✅ All npm dependencies
- ✅ Everything you need to start!

### ⚙️ **Manual Setup**

<details>
<summary><b>Click to expand manual setup instructions</b></summary>

**Prerequisites:**
- Node.js 18+
- Rust 1.70+
- Python 3.11+ (for Python scripts)
- Git

**Installation:**
```bash
# Clone repository
git clone https://github.com/YOUR_USERNAME/FinceptTerminal.git
cd FinceptTerminal/fincept-terminal-desktop

# Install dependencies
npm install

# Run development server
npm run tauri dev

# Build for production
npm run tauri build
```

</details>

---

## 💻 Contribution Areas

### 1️⃣ **Frontend Development** ⚛️

**Focus:** React + TypeScript + TailwindCSS

```typescript
// Example: Creating a new component
import { Card, CardHeader, CardTitle, CardContent } from "@/components/ui/card"
import { Button } from "@/components/ui/button"

interface StockCardProps {
  symbol: string
  price: number
  change: number
}

export function StockCard({ symbol, price, change }: StockCardProps) {
  const isPositive = change >= 0

  return (
    <Card className="hover:shadow-lg transition-shadow">
      <CardHeader>
        <CardTitle className="flex justify-between">
          <span>{symbol}</span>
          <span className={isPositive ? "text-green-500" : "text-red-500"}>
            ${price.toFixed(2)}
          </span>
        </CardTitle>
      </CardHeader>
      <CardContent>
        <p className={isPositive ? "text-green-500" : "text-red-500"}>
          {isPositive ? "▲" : "▼"} {Math.abs(change).toFixed(2)}%
        </p>
      </CardContent>
    </Card>
  )
}
```

**Guidelines:**
- ✅ Use TypeScript for type safety
- ✅ Leverage shadcn/ui components
- ✅ Follow React best practices (hooks, functional components)
- ✅ Ensure responsive design (mobile, tablet, desktop)
- ✅ Add proper error boundaries and loading states

### 2️⃣ **Backend Development** 🦀

**Focus:** Rust + Tauri Commands

```rust
// Example: Adding a new Tauri command
use tauri::command;

#[command]
async fn calculate_portfolio_value(
    holdings: Vec<Holding>,
    prices: Vec<f64>
) -> Result<f64, String> {
    if holdings.len() != prices.len() {
        return Err("Mismatched data".to_string());
    }

    let total = holdings
        .iter()
        .zip(prices.iter())
        .map(|(h, p)| h.quantity * p)
        .sum();

    Ok(total)
}

// Register in main.rs
fn main() {
    tauri::Builder::default()
        .invoke_handler(tauri::generate_handler![calculate_portfolio_value])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
```

**Guidelines:**
- ✅ Use `#[tauri::command]` for frontend-callable functions
- ✅ Implement proper error handling
- ✅ Use async when necessary
- ✅ Keep commands focused and testable
- ✅ Document all public APIs

---

## 🐍 Python Script Contributions

**We have embedded Python for financial analysis!** You can contribute Python scripts for:

### 📊 **Financial Data Processing**

**Location:** `fincept-terminal-desktop/src-tauri/resources/scripts/`

**Example: Adding a new indicator**
```python
#!/usr/bin/env python3
"""
Technical Indicator Calculator
Usage: python indicators.py <symbol> <indicator> <period>
"""

import sys
import json
import yfinance as yf
import pandas as pd
from typing import Dict, Any

def calculate_rsi(prices: pd.Series, period: int = 14) -> pd.Series:
    """Calculate Relative Strength Index"""
    delta = prices.diff()
    gain = (delta.where(delta > 0, 0)).rolling(window=period).mean()
    loss = (-delta.where(delta < 0, 0)).rolling(window=period).mean()

    rs = gain / loss
    rsi = 100 - (100 / (1 + rs))
    return rsi

def main():
    if len(sys.argv) < 3:
        print(json.dumps({
            "success": False,
            "error": "Usage: indicators.py <symbol> <indicator>"
        }))
        sys.exit(1)

    symbol = sys.argv[1]
    indicator = sys.argv[2]

    try:
        # Fetch data
        ticker = yf.Ticker(symbol)
        hist = ticker.history(period="1y")

        # Calculate indicator
        result = None
        if indicator == "rsi":
            result = calculate_rsi(hist['Close']).iloc[-1]

        # Return JSON response
        print(json.dumps({
            "success": True,
            "data": {
                "symbol": symbol,
                "indicator": indicator,
                "value": float(result) if result is not None else None
            }
        }))

    except Exception as e:
        print(json.dumps({
            "success": False,
            "error": str(e)
        }))
        sys.exit(1)

if __name__ == "__main__":
    main()
```

### 🐍 **Python Script Guidelines**

<table>
<tr>
<td width="50%">

**✅ Best Practices**
- Use type hints (PEP 484)
- Include docstrings
- Handle errors gracefully
- Return JSON output
- Use argparse for CLI args
- Add logging for debugging
- Test with sample data
- Document dependencies

</td>
<td width="50%">

**📦 Dependencies**
- Use `requirements.txt`
- Pin versions (e.g., `yfinance==0.2.28`)
- Keep dependencies minimal
- Test compatibility
- Document installation
- Use virtual environments
- Handle missing imports

</td>
</tr>
</table>

### 🔌 **Calling Python from Rust**

```rust
// Example: Invoking Python script from Tauri
use std::process::Command;

#[tauri::command]
async fn calculate_indicator(
    symbol: String,
    indicator: String
) -> Result<serde_json::Value, String> {
    let output = Command::new("./resources/python/python.exe")
        .arg("./resources/scripts/indicators.py")
        .arg(&symbol)
        .arg(&indicator)
        .output()
        .map_err(|e| e.to_string())?;

    let stdout = String::from_utf8_lossy(&output.stdout);
    let result: serde_json::Value = serde_json::from_str(&stdout)
        .map_err(|e| format!("JSON parse error: {}", e))?;

    Ok(result)
}
```

### 📁 **Python Script Structure**

```
src-tauri/resources/
├── python/              # Embedded Python runtime
│   ├── python.exe
│   └── Lib/
└── scripts/             # Your Python scripts go here!
    ├── yfinance_data.py
    ├── indicators.py    # ← Add new scripts here
    ├── ml_predictions.py
    └── sentiment_analysis.py
```

---

## 📁 Project Structure

```
fincept-terminal-desktop/
├── src/                          # React frontend
│   ├── components/
│   │   ├── auth/                 # Authentication screens
│   │   ├── dashboard/            # Main terminal dashboard
│   │   ├── tabs/                 # Terminal tabs (Markets, News, etc.)
│   │   └── ui/                   # Reusable UI components (shadcn/ui)
│   ├── contexts/                 # React contexts (Auth, Theme)
│   ├── services/                 # API service layers
│   ├── hooks/                    # Custom React hooks
│   └── types/                    # TypeScript type definitions
│
├── src-tauri/                    # Rust backend (Tauri)
│   ├── src/
│   │   ├── main.rs               # Main Tauri entry point
│   │   ├── commands/             # Tauri commands
│   │   └── data_sources/         # Data source integrations
│   ├── resources/
│   │   ├── python/               # Embedded Python runtime
│   │   └── scripts/              # Python scripts ← Add yours here!
│   ├── Cargo.toml                # Rust dependencies
│   └── tauri.conf.json           # Tauri configuration
│
├── public/                       # Static assets
├── setup.bat                     # Windows automated setup
├── setup.sh                      # Linux/macOS automated setup
└── package.json                  # npm dependencies
```

---

## 🔄 Development Workflow

### 🌿 **Branch Strategy**

```bash
# Create feature branch
git checkout -b feature/amazing-feature

# Create bugfix branch
git checkout -b fix/bug-description

# Create Python script branch
git checkout -b python/new-indicator
```

### 💡 **Adding a New Feature**

**1. Frontend Feature (React Component)**
```bash
# Create component
touch src/components/new-feature/NewFeature.tsx

# Add to relevant page
# Edit src/components/dashboard/DashboardScreen.tsx
```

**2. Backend Feature (Rust Command)**
```bash
# Create command file
touch src-tauri/src/commands/new_command.rs

# Register in main.rs
```

**3. Python Script Feature**
```bash
# Create Python script
touch src-tauri/resources/scripts/my_analysis.py

# Add to requirements (if needed)
echo "new-library==1.0.0" >> requirements.txt
```

### 🔌 **Connecting Frontend to Backend**

```typescript
// React component calling Rust command
import { invoke } from '@tauri-apps/api/core'

async function fetchAnalysis(symbol: string) {
  try {
    const result = await invoke('calculate_indicator', {
      symbol,
      indicator: 'rsi'
    })
    console.log('Analysis result:', result)
  } catch (error) {
    console.error('Error:', error)
  }
}
```

---

## ✅ Pull Request Process

### 📝 **Before Submitting**

- [ ] Code follows project style guidelines
- [ ] All tests pass locally
- [ ] Added tests for new features
- [ ] Updated documentation
- [ ] Commits are clean and descriptive
- [ ] No console errors or warnings
- [ ] Python scripts tested independently

### 🎯 **PR Title Format**

```
feat: Add RSI indicator calculation
fix: Resolve chart rendering issue
docs: Update Python script guidelines
chore: Update dependencies
python: Add sentiment analysis script
```

### 📋 **PR Description Template**

```markdown
## Description
Brief description of changes

## Type of Change
- [ ] Bug fix
- [ ] New feature
- [ ] Python script
- [ ] Documentation
- [ ] Performance improvement

## Testing
How was this tested?

## Screenshots (if applicable)
Add screenshots here

## Checklist
- [ ] Code follows style guidelines
- [ ] Self-review completed
- [ ] Documentation updated
- [ ] Tests added/updated
```

---

## 🧪 Testing Guidelines

### ⚛️ **Frontend Tests**

```bash
npm test              # Run all tests
npm run test:watch    # Watch mode
npm run test:coverage # Coverage report
```

### 🦀 **Rust Tests**

```bash
cd src-tauri
cargo test           # Run all tests
cargo test --release # Release mode
```

### 🐍 **Python Script Tests**

```bash
# Test Python script directly
python src-tauri/resources/scripts/indicators.py AAPL rsi

# Expected output:
# {"success": true, "data": {"symbol": "AAPL", "indicator": "rsi", "value": 45.23}}
```

---

## 📝 Code Style

### 📐 **TypeScript/React**

```typescript
// ✅ Good
interface Props {
  symbol: string
  price: number
}

export function Component({ symbol, price }: Props) {
  const [data, setData] = useState<Data | null>(null)

  useEffect(() => {
    // Effect logic
  }, [symbol])

  return <div>{symbol}: ${price}</div>
}

// ❌ Bad
export function Component(props: any) {
  const [data, setData] = useState(null)
  return <div>{props.symbol}</div>
}
```

### 🦀 **Rust**

```rust
// ✅ Good
#[tauri::command]
async fn fetch_data(symbol: String) -> Result<Data, String> {
    match get_data(&symbol).await {
        Ok(data) => Ok(data),
        Err(e) => Err(e.to_string()),
    }
}

// ❌ Bad
#[tauri::command]
fn fetch_data(symbol: String) -> Data {
    get_data(&symbol).unwrap()
}
```

### 🐍 **Python**

```python
# ✅ Good
def calculate_metric(data: pd.Series, period: int = 14) -> float:
    """
    Calculate metric from price data.

    Args:
        data: Price series
        period: Calculation period

    Returns:
        Calculated metric value
    """
    result = data.rolling(window=period).mean().iloc[-1]
    return float(result)

# ❌ Bad
def calc(d, p=14):
    return d.rolling(p).mean().iloc[-1]
```

---

## 🎁 First-Time Contributors

**New to open source?** We got you! Here are some easy ways to start:

### 🌱 **Good First Issues**

Look for issues labeled:
- `good first issue` - Perfect for beginners
- `documentation` - Help improve docs
- `help wanted` - We need your help!
- `python-script` - Add Python analysis scripts

### 📚 **Learning Resources**

- [React Documentation](https://react.dev/)
- [TypeScript Handbook](https://www.typescriptlang.org/docs/)
- [Tauri Documentation](https://tauri.app/)
- [Python Best Practices](https://docs.python.org/3/tutorial/)

---

## 🏆 Recognition

**We celebrate our contributors!** 🎉

- 🌟 Listed in [Contributors](https://github.com/Fincept-Corporation/FinceptTerminal/graphs/contributors)
- 📰 Mentioned in release notes
- 🎖️ Special badges for consistent contributors
- 💬 Shoutouts in community discussions

---

## 📞 Need Help?

<div align="center">

| Channel | Link |
|---------|------|
| 💬 **Discussions** | [GitHub Discussions](https://github.com/Fincept-Corporation/FinceptTerminal/discussions) |
| 🐛 **Bug Reports** | [GitHub Issues](https://github.com/Fincept-Corporation/FinceptTerminal/issues) |
| 📧 **Email** | dev@fincept.in |
| 📖 **Documentation** | [Project Wiki](https://github.com/Fincept-Corporation/FinceptTerminal/wiki) |

</div>

---

<div align="center">

## 🎉 Thank You!

**Your contributions make Fincept Terminal better for everyone**

[![Contributors](https://img.shields.io/github/contributors/Fincept-Corporation/FinceptTerminal)](https://github.com/Fincept-Corporation/FinceptTerminal/graphs/contributors)
[![PRs Welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg)](http://makeapullrequest.com)

**Start small, think big! Every contribution counts.** 💙

*Built by the community, for the community*

</div>
