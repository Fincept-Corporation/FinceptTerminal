# Contributing to Fincept Terminal

Welcome! Fincept Terminal is an open-source financial intelligence platform with 80+ terminal tabs, 99 Rust commands, 119 Python scripts, and 42 UI components.

---

## Table of Contents

- [Ways to Contribute](#ways-to-contribute)
- [Tech Stack](#tech-stack)
- [Getting Started](#getting-started)
- [Project Architecture](#project-architecture)
- [Development Workflow](#development-workflow)
- [Pull Request Process](#pull-request-process)
- [Language-Specific Guides](#language-specific-guides)
- [Getting Help](#getting-help)

---

## Ways to Contribute

| Area | Examples |
|------|----------|
| **Code** | Bug fixes, new features, new tabs, new analytics |
| **Documentation** | Improve guides, add examples, translations |
| **Testing** | Report bugs, write tests, review PRs |
| **Design** | UI/UX improvements, icons, themes |
| **Data** | New data sources, API integrations |

---

## Tech Stack

| Layer | Technologies |
|-------|-------------|
| **Frontend** | React 19, TypeScript 5.8, TailwindCSS v4, Radix UI |
| **Desktop** | Tauri 2.x, Rust |
| **Analytics** | Python 3.11+ (embedded runtime) |
| **Package Manager** | Bun |
| **Charts** | Lightweight Charts, Recharts, Plotly, D3.js |
| **AI/LLM** | Langchain, Ollama, OpenAI, Anthropic, Google GenAI |
| **Database** | SQLite (via Tauri plugin) |
| **State** | React Context, Redux |

**Language-Specific Guides:**
- [TypeScript/React Guide](./TYPESCRIPT_CONTRIBUTOR_GUIDE.md) - 80 tabs, 42 UI components, 12 contexts
- [Rust Guide](./RUST_CONTRIBUTOR_GUIDE.md) - 99 commands, WebSocket, database
- [Python Guide](./PYTHON_CONTRIBUTOR_GUIDE.md) - 119 scripts, 34 Analytics modules

---

## Getting Started

### Prerequisites

- **Bun** 1.0+ - [bun.sh](https://bun.sh)
- **Rust** 1.70+ - [rust-lang.org](https://www.rust-lang.org/tools/install)
- **Python** 3.11+ (for analytics development)
- **Git** - [git-scm.com](https://git-scm.com)

### Setup

```bash
# Clone repository
git clone https://github.com/Fincept-Corporation/FinceptTerminal.git
cd FinceptTerminal/fincept-terminal-desktop

# Install dependencies
bun install

# Run development server
bun run tauri:dev
```

### Verify Installation

```bash
bun --version      # 1.0+
rustc --version    # 1.70+
python --version   # 3.11+
```

---

## Project Architecture

### Frontend (`src/`)

```
src/
├── components/
│   ├── tabs/              # 80 terminal tabs
│   │   ├── dashboard/     # Main dashboard
│   │   ├── markets/       # Market data
│   │   ├── portfolio-tab/ # Portfolio management
│   │   ├── algo-trading/  # Algorithmic trading
│   │   ├── ai-quant-lab/  # AI/ML analytics
│   │   ├── quantlib-*/    # 15+ QuantLib modules
│   │   ├── geopolitics/   # Geopolitical analysis
│   │   ├── crypto-trading/# Cryptocurrency
│   │   └── ...            # 60+ more tabs
│   │
│   ├── ui/                # 42 UI components (shadcn/ui)
│   ├── auth/              # Authentication
│   ├── dashboard/         # Terminal shell
│   ├── charts/            # Chart components
│   ├── chat-mode/         # AI chat interface
│   ├── command-bar/       # Command palette
│   └── visualization/     # 3D visualizations
│
├── services/              # 35+ API services
│   ├── analytics/         # Analytics APIs
│   ├── trading/           # Trading APIs
│   ├── markets/           # Market data
│   ├── portfolio/         # Portfolio APIs
│   ├── mcp/               # MCP integration
│   └── ...
│
├── contexts/              # 12 React contexts
│   ├── AuthContext.tsx
│   ├── BrokerContext.tsx
│   ├── DataSourceContext.tsx
│   ├── NavigationContext.tsx
│   ├── ThemeContext.tsx
│   ├── WorkspaceContext.tsx
│   └── ...
│
├── brokers/               # Broker integrations
│   ├── crypto/            # Crypto exchanges
│   ├── india/             # Indian brokers
│   └── stocks/            # Stock brokers
│
├── hooks/                 # 8 custom hooks
├── store/                 # Redux store
├── i18n/                  # Internationalization
└── paper-trading/         # Paper trading system
```

### Backend (`src-tauri/`)

```
src-tauri/
├── src/
│   ├── lib.rs             # Main library (130K+ lines)
│   ├── python.rs          # Python runtime
│   ├── setup.rs           # App initialization
│   ├── paper_trading.rs   # Paper trading
│   │
│   ├── commands/          # 99 Tauri commands
│   │   ├── analytics.rs   # Financial analytics
│   │   ├── agents.rs      # AI agents
│   │   ├── algo_trading.rs# Algo trading
│   │   ├── databento.rs   # Market data
│   │   ├── portfolio.rs   # Portfolio
│   │   ├── brokers/       # Broker commands
│   │   └── ...            # 90+ more
│   │
│   ├── database/          # SQLite operations
│   ├── services/          # Backend services
│   └── websocket/         # WebSocket handlers
│
└── resources/scripts/     # 119 Python scripts
    ├── Analytics/         # 34 analytics modules
    ├── agents/            # AI agents
    ├── strategies/        # Trading strategies
    ├── technicals/        # Technical analysis
    └── *.py               # 80+ data fetchers
```

---

## Development Workflow

### Branch Naming

```
feature/add-options-tab
fix/chart-rendering-bug
docs/update-python-guide
```

### Making Changes

1. Create branch from `main`
2. Follow the appropriate language guide
3. Test locally: `bun run build && bun run tauri:dev`
4. Commit with clear messages

### Commit Format

```
type: short description

Types: feat, fix, docs, refactor, test, chore
```

---

## Pull Request Process

### Before Submitting

- [ ] Code builds without errors
- [ ] No console warnings
- [ ] Tested in development mode
- [ ] Follows existing patterns
- [ ] Documentation updated

### PR Template

```markdown
## Description
Brief description

## Type
- [ ] Bug fix
- [ ] New feature
- [ ] Documentation

## Testing
How was this tested?

## Screenshots (if UI)
```

---

## Language-Specific Guides

| Guide | Coverage |
|-------|----------|
| [TypeScript Guide](./TYPESCRIPT_CONTRIBUTOR_GUIDE.md) | 80 tabs, 42 UI components, services, contexts, hooks |
| [Rust Guide](./RUST_CONTRIBUTOR_GUIDE.md) | 99 commands, WebSocket, database, Python execution |
| [Python Guide](./PYTHON_CONTRIBUTOR_GUIDE.md) | 119 scripts, 34 Analytics modules, data fetchers |

---

## Getting Help

| Channel | Link |
|---------|------|
| Issues | [GitHub Issues](https://github.com/Fincept-Corporation/FinceptTerminal/issues) |
| Discussions | [GitHub Discussions](https://github.com/Fincept-Corporation/FinceptTerminal/discussions) |
| Discord | [Join Discord](https://discord.gg/ae87a8ygbN) |
| Email | dev@fincept.in |

### Good First Issues

Look for labels: `good first issue`, `help wanted`, `documentation`

---

**Repository**: https://github.com/Fincept-Corporation/FinceptTerminal
**Version**: 3.3.1
**License**: AGPL-3.0
