# Contributing to Fincept Terminal

Welcome! Fincept Terminal is an open-source native C++20 financial intelligence platform with 40+ screens, embedded Python analytics, and 100+ data connectors.

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
| **Code** | Bug fixes, new features, new screens, new analytics |
| **Documentation** | Improve guides, add examples |
| **Testing** | Report bugs, write tests, review PRs |
| **Design** | UI/UX improvements, themes |
| **Data** | New data sources, API integrations, Python scripts |

---

## Tech Stack

| Layer | Technologies |
|-------|-------------|
| **Language** | C++20 (MSVC / GCC / Clang) |
| **UI** | Dear ImGui (docking) + ImPlot |
| **Layout** | Yoga (Flexbox engine) |
| **Rendering** | GLFW 3 + OpenGL 3.3+ |
| **Networking** | libcurl + OpenSSL |
| **Database** | SQLite 3 |
| **JSON** | nlohmann/json |
| **Analytics** | Embedded Python 3.11+ (100+ scripts) |
| **Build** | CMake 3.20+ / vcpkg |

**Language-Specific Guides:**
- [C++ Guide](../fincept-cpp/CONTRIBUTING.md) — screens, services, core infrastructure
- [Python Guide](./PYTHON_CONTRIBUTOR_GUIDE.md) — analytics modules, data fetchers

---

## Getting Started

### Prerequisites

- **CMake** 3.20+ — [cmake.org](https://cmake.org/download/)
- **vcpkg** — [vcpkg.io](https://vcpkg.io/)
- **C++20 compiler** — MSVC 2022, GCC 12+, or Clang 15+
- **Python** 3.11+ (for analytics development)
- **Git** — [git-scm.com](https://git-scm.com)

### Setup

```bash
# Clone repository
git clone https://github.com/Fincept-Corporation/FinceptTerminal.git
cd FinceptTerminal/fincept-cpp

# Build
cmake --preset=default
cmake --build build --config Release

# Run
./build/FinceptTerminal
```

### Verify Installation

```bash
cmake --version     # 3.20+
python --version    # 3.11+
```

---

## Project Architecture

### Source Layout (`fincept-cpp/src/`)

```
src/
├── main.cpp                # Entry point, GLFW/OpenGL setup
├── app.cpp/h               # App state machine, routing
│
├── core/                   # Shared infrastructure
│   ├── config.h            # Constants (URLs, timeouts, versions)
│   ├── event_bus.h         # Pub/sub messaging
│   ├── logger.h            # Structured logging
│   └── result.h            # Result<T> error handling
│
├── ui/                     # Reusable ImGui widgets
├── http/                   # HTTP client (libcurl)
├── storage/                # SQLite database
├── auth/                   # Authentication
├── python/                 # Python runtime bridge
├── mcp/                    # MCP integration
├── trading/                # Trading engine + brokers
├── portfolio/              # Portfolio management
│
└── screens/                # 40+ terminal screens
    ├── dashboard/
    ├── markets/
    ├── crypto_trading/
    ├── research/
    ├── quantlib/
    ├── ai_chat/
    ├── economics/
    ├── geopolitics/
    └── ...
```

### Python Scripts (`fincept-cpp/scripts/`)

```
scripts/
├── Analytics/              # 34 analytics modules
│   ├── equityInvestment/   # Stock valuation, DCF
│   ├── portfolioManagement/# Portfolio optimization
│   ├── derivatives/        # Options pricing, Greeks
│   ├── fixedIncome/        # Bond analytics
│   └── ...
│
├── agents/                 # AI agents
├── strategies/             # Trading strategies
├── technicals/             # Technical analysis
└── *.py                    # 80+ data fetchers
```

---

## Development Workflow

### Branch Naming

```
feature/add-options-screen
fix/chart-rendering-crash
docs/update-architecture
```

### Making Changes

1. Create branch from `main`
2. Follow the appropriate language guide
3. Build and test locally
4. Commit with clear messages

### Commit Format

```
type: short description

Types: feat, fix, docs, refactor, test, chore, perf
```

---

## Pull Request Process

### Before Submitting

- [ ] Code compiles without warnings (`-Wall -Wextra`)
- [ ] No duplicated code — use `core/` and `ui/widgets/`
- [ ] Screens don't call HTTP directly (use data services)
- [ ] New `.cpp` files added to `CMakeLists.txt`
- [ ] `.clang-format` applied
- [ ] Tested in development mode

### PR Template

```markdown
## Description
Brief description

## Type
- [ ] Bug fix
- [ ] New feature / screen
- [ ] Documentation

## Testing
How was this tested?

## Screenshots (if UI)
```

---

## Language-Specific Guides

| Guide | Coverage |
|-------|----------|
| [C++ Guide](../fincept-cpp/CONTRIBUTING.md) | Screens, services, core infrastructure, widgets |
| [Python Guide](./PYTHON_CONTRIBUTOR_GUIDE.md) | Analytics modules, data fetchers, AI agents |

---

## Getting Help

| Channel | Link |
|---------|------|
| Issues | [GitHub Issues](https://github.com/Fincept-Corporation/FinceptTerminal/issues) |
| Discussions | [GitHub Discussions](https://github.com/Fincept-Corporation/FinceptTerminal/discussions) |
| Discord | [Join Discord](https://discord.gg/ae87a8ygbN) |
| Email | support@fincept.in |

### Good First Issues

Look for labels: `good first issue`, `help wanted`, `documentation`

---

**Repository**: https://github.com/Fincept-Corporation/FinceptTerminal
**Version**: 4.0.0
**License**: AGPL-3.0
