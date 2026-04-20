# Getting Started with Fincept Terminal Development
## Your Complete Onboarding Guide

Welcome to Fincept Terminal! This guide will get you from zero to your first contribution.

---

## What is Fincept Terminal?

Fincept Terminal is an **open-source financial analysis platform** — a free, open-source alternative to Bloomberg Terminal. Version 4 is a native C++20 application built with Qt6.

### The Big Picture

**Current Reality:**
- Bloomberg Terminal: $24,000/year per user
- Refinitiv Eikon: $20,000+/year
- Professional tools locked behind paywalls

**Our Vision:**
- 100% free and open source
- Integrate 100+ data sources (stocks, crypto, forex, economic data, news, etc.)
- AI-powered analysis and insights
- Native C++ performance — no browser/JS overhead
- Built by the community, for the community

### Technology Decisions (and Why)

- **C++20 + Qt6** — Native performance, polished UI, single binary
- **Qt6 Network + WebSockets** — Cross-platform networking, TLS built-in
- **Qt6 Charts** — Financial charting without extra dependencies
- **Qt6 Sql (SQLite)** — Local data caching for speed
- **Embedded Python** — Access to vast ecosystem of financial libraries (yfinance, pandas, etc.)

---

## Quick Setup

### Prerequisites (pinned versions — required)

> **Use exactly these versions.** Fincept Terminal's CMake build enforces them with `FATAL_ERROR` checks. Newer/older versions will refuse to configure.

| Tool | Pinned Version |
|------|----------------|
| **C++ compiler** | MSVC 19.38 (VS 2022 17.8) / GCC 12.3 / Apple Clang 15.0 (Xcode 15.2) |
| **CMake** | 3.27.7 |
| **Ninja** | 1.11.1 |
| **Qt** | 6.8.3 |
| **Python** | 3.11.9 |
| **Platform SDK** | Win10 SDK 10.0.22621.0 / macOS SDK 14.0 (deploy target 11.0) / glibc 2.31+ |

#### 1. Install a C++20 compiler

- **Windows**: [Visual Studio 2022 17.8+](https://visualstudio.microsoft.com/) with "Desktop development with C++"
- **Linux (Ubuntu 22.04+)**: `sudo apt install -y g++-12`
- **macOS**: Xcode 15.2 — `xcode-select --install`

#### 2. Install CMake 3.27.7 + Ninja 1.11.1

- **Windows**: `winget install Kitware.CMake Ninja-build.Ninja`
- **Linux**: `sudo apt install -y cmake ninja-build` (verify `cmake --version` ≥ 3.27.7; if not, download from [cmake.org](https://cmake.org/download/))
- **macOS**: `brew install cmake ninja`

#### 3. Install Qt 6.8.3

**Windows / Linux / macOS** — Qt online installer (recommended):
- Download from https://www.qt.io/download-qt-installer
- Select exactly **Qt 6.8.3** → platform-appropriate kit (MSVC 2022 64-bit / Desktop gcc 64-bit / macOS)
- Default install paths: `C:/Qt/6.8.3/msvc2022_64`, `~/Qt/6.8.3/gcc_64`, `~/Qt/6.8.3/macos`

**Linux system packages (alternative — may not be exactly 6.8.3):**
```bash
sudo apt install -y \
  qt6-base-dev qt6-charts-dev qt6-tools-dev qt6-base-private-dev \
  libqt6sql6-sqlite libqt6websockets6-dev \
  libgl1-mesa-dev libglu1-mesa-dev
```

#### 4. Install Python 3.11.9

- **Windows**: [python.org 3.11.9](https://www.python.org/downloads/release/python-3119/) (check "Add to PATH")
- **Linux**: `sudo apt install python3.11` (or build from source if your distro ships 3.10)
- **macOS**: `brew install python@3.11`

### Setup in 3 Steps (using CMake presets)

```bash
# 1. Clone
git clone https://github.com/Fincept-Corporation/FinceptTerminal.git
cd FinceptTerminal/fincept-qt

# 2. Configure + Build (pick your platform)
cmake --preset win-release     && cmake --build --preset win-release     # Windows (Dev Cmd for VS 2022)
cmake --preset linux-release   && cmake --build --preset linux-release   # Linux
cmake --preset macos-release   && cmake --build --preset macos-release   # macOS

# 3. Run
./build/linux-release/FinceptTerminal              # Linux
./build/macos-release/FinceptTerminal.app/Contents/MacOS/FinceptTerminal   # macOS
.\build\win-release\FinceptTerminal.exe            # Windows
```

**If CMake presets can't resolve your Qt path**, use the manual configure:
```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="<path/to/Qt/6.8.3/<kit>>"
cmake --build build
```

**Expected Result:** Fincept Terminal window should open, showing the login screen.

### Verify Everything Works

Once the app opens:

1. **Click "Continue as Guest"** — No registration needed for development
2. **Navigate to Markets tab** — Should see market data
3. **Try switching tabs** — Dashboard, Markets, News, etc.

---

## Understanding the Codebase

### Project Structure

```
FinceptTerminal/
│
├── fincept-qt/                     ← Main application (you'll work here)
│   ├── src/                        ← C++ source code
│   │   ├── app/                    ← Entry point, MainWindow, ScreenRouter
│   │   ├── core/                   ← Shared infrastructure
│   │   │   ├── config/             ← App-wide constants
│   │   │   ├── events/             ← Pub/sub messaging (EventBus)
│   │   │   ├── logging/            ← Structured logging (Logger)
│   │   │   ├── result/             ← Result<T> error handling
│   │   │   └── session/            ← Session management
│   │   ├── ui/                     ← Reusable Qt widgets
│   │   │   ├── theme/              ← Obsidian design system (StyleSheets)
│   │   │   ├── widgets/            ← Card, SearchBar, StatusBadge, etc.
│   │   │   ├── charts/             ← ChartFactory (Qt6 Charts)
│   │   │   ├── tables/             ← DataTable
│   │   │   └── navigation/         ← NavigationBar, StatusBar, FKeyBar
│   │   ├── network/                ← HTTP client, WebSocket client
│   │   ├── storage/                ← SQLite databases + repositories
│   │   ├── auth/                   ← Authentication (JWT, guest mode)
│   │   ├── python/                 ← Python runtime bridge
│   │   ├── trading/                ← Trading engine + 20+ brokers
│   │   ├── services/               ← Market data, news services
│   │   └── screens/                ← Terminal screens
│   │       ├── dashboard/
│   │       ├── markets/
│   │       ├── crypto_trading/
│   │       ├── news/
│   │       ├── watchlist/
│   │       ├── report_builder/
│   │       └── ...
│   │
│   ├── scripts/                    ← Python analytics scripts
│   │   ├── Analytics/              ← CFA-level analytics modules
│   │   ├── agents/                 ← AI agent frameworks
│   │   └── *.py                    ← 100+ data fetchers
│   │
│   ├── CMakeLists.txt              ← Build configuration
│   └── DESIGN_SYSTEM.md           ← Obsidian UI/UX spec
│
└── docs/                           ← Documentation
```

### How Data Flows

```
User clicks "Get AAPL quote"
        │
        ▼
Screen (MarketsScreen.cpp)          ← UI rendering only
        │
        ▼
Data Service (MarketDataService)    ← Fetching + caching
        │
        ├─── HTTP API call (QNetworkAccessManager)
        │         │
        │         ▼
        │    Parse JSON (QJsonDocument)
        │
        └─── Python Script (optional)
                  │
                  ▼
             PythonRunner executes script
                  │
                  ▼
             Returns JSON to C++
        │
        ▼
UI updates with data
```

### Key Patterns

| If you want to... | Look at this |
|--------------------|-------------|
| Add a new screen | `src/screens/dashboard/` (as template) |
| Add a data fetcher | `scripts/yfinance_data.py` (as template) |
| Use UI widgets | `src/ui/widgets/` |
| Make HTTP calls | `src/network/http/HttpClient.cpp` |
| Store data locally | `src/storage/sqlite/Database.cpp` |
| Add a Python analytics module | `scripts/Analytics/` |
| Add a broker | `src/trading/brokers/` |
| Style a screen | `src/ui/theme/StyleSheets.cpp` + `DESIGN_SYSTEM.md` |

---

## Your First Contribution

### Path A: Easy (15 minutes)
**Improve Documentation** — Fix a typo or unclear explanation in any `.md` file.

### Path B: Easy (30 minutes)
**Add a Python Data Fetcher** — Follow the pattern in `scripts/yfinance_data.py` to create a wrapper for a new free API.

### Path C: Medium (1-2 hours)
**Add a New Screen** — Follow the patterns in `src/screens/` to add a new Qt6 screen.

### Path D: Advanced (2-4 hours)
**Add a Broker Integration** — Follow the `BrokerInterface` pattern in `src/trading/brokers/`.

---

## Common Development Workflows

### Workflow 1: Add a New Python Script

```bash
cd fincept-qt/scripts

# Create the script
# Follow yfinance_data.py pattern: CLI args → JSON stdout

# Test standalone
python my_script.py command arg1 arg2

# Rebuild to include in resources
cd ..
cmake --build build --config Release
```

### Workflow 2: Add a New Screen

1. Create folder: `src/screens/your_feature/`
2. Create files: `YourScreen.h/.cpp` (subclass `QWidget`)
3. Add `.cpp` file to `CMakeLists.txt` `SCREEN_SOURCES` list
4. Register screen in `src/app/MainWindow.cpp` via `ScreenRouter`
5. Add navigation entry in `NavigationBar`
6. Build and test

### Workflow 3: Fix a Bug

```bash
git checkout -b fix/issue-123-description
# Fix the issue
cmake --build build --config Release
# Test the fix
git add <files>
git commit -m "fix: resolve market data loading crash"
git push origin fix/issue-123-description
```

---

## Code Style

- 4-space indentation, 120 column limit
- `snake_case` for functions/variables, `PascalCase` for types/classes
- Trailing underscore for member variables: `data_`, `loading_`
- No `using namespace std;` — use explicit `std::` prefix
- Use `Q_OBJECT` macro in all QObject subclasses
- Connect signals/slots with the new pointer-to-member syntax

---

## Troubleshooting

### CMake can't find Qt6
```bash
# Set CMAKE_PREFIX_PATH to your Qt install
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="/path/to/Qt/6.x.x/platform"

# Linux: verify Qt6 dev packages are installed
dpkg -l | grep qt6
```

### Python script not found
```bash
# Verify Python is in PATH
python --version

# Test script directly
python scripts/yfinance_data.py quote AAPL
```

### OpenGL errors on Linux
```bash
# Install OpenGL dev packages (required by Qt6 rendering)
sudo apt install libgl1-mesa-dev libglu1-mesa-dev
```

---

## Getting Help

| Channel | Link |
|---------|------|
| GitHub Issues | https://github.com/Fincept-Corporation/FinceptTerminal/issues |
| GitHub Discussions | https://github.com/Fincept-Corporation/FinceptTerminal/discussions |
| Discord | https://discord.gg/ae87a8ygbN |
| Email | support@fincept.in |

---

**Ready to contribute?**
- Pick an issue: https://github.com/Fincept-Corporation/FinceptTerminal/issues
- Read the C++ guide: [fincept-qt/CONTRIBUTING.md](../fincept-qt/CONTRIBUTING.md)
- Read the Python guide: [PYTHON_CONTRIBUTOR_GUIDE.md](./PYTHON_CONTRIBUTOR_GUIDE.md)
