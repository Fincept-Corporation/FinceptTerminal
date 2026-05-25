# Getting Started with Fincept Terminal Development
## Your Complete Onboarding Guide

Welcome to Fincept Terminal! This guide will get you from zero to your first contribution.

---

## What is Fincept Terminal?

Fincept Terminal is an **open-source financial analysis platform** — a free, open-source alternative to legacy professional financial terminals. Version 4 is a native C++20 application built with Qt6.

### The Big Picture

**Current Reality:**
- Legacy professional terminals: $20,000+/year per user
- Industry-standard market data subscriptions: $20,000+/year
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

### Prerequisites

| Tool | Required Version | Notes |
|------|-----------------|-------|
| **C++ compiler** | MSVC 19.40+ / GCC 12.3+ / Apple Clang 15.0+ | See platform-specific instructions below |
| **CMake** | 3.27+ | Build system |
| **Qt** | 6.8.x (6.8.3 recommended) | UI framework |
| **Python** | 3.11.x | Embedded analytics runtime |

Optional (faster builds):
- **Ninja** — parallel build system (auto-detected if installed)
- **ccache** — compiler cache (auto-detected if installed)

### Step 1 — Install a C++20 Compiler

#### Windows
Install [Visual Studio 2022 17.10+](https://visualstudio.microsoft.com/) (Community edition is free).
Select the **"Desktop development with C++"** workload during installation.

#### Linux (Ubuntu 22.04+ / Debian 12+)
```bash
sudo apt install -y build-essential g++-12
```

#### macOS
```bash
xcode-select --install
```
Requires Xcode 15.2+ (Apple Clang 15.0).

### Step 2 — Install CMake

- **Windows**: `winget install Kitware.CMake`
- **Linux**: `sudo apt install -y cmake` (verify `cmake --version` >= 3.27; if older, download from [cmake.org](https://cmake.org/download/))
- **macOS**: `brew install cmake`

### Step 3 — Install Qt 6.8.3

Download the **Qt Online Installer** from https://www.qt.io/download-qt-installer

Select **Qt 6.8.3** and your platform kit:
- **Windows**: MSVC 2022 64-bit (default install: `C:\Qt\6.8.3\msvc2022_64`)
- **Linux**: Desktop gcc 64-bit (default install: `~/Qt/6.8.3/gcc_64`)
- **macOS**: macOS (default install: `~/Qt/6.8.3/macos`)

**Linux system packages (alternative):**
```bash
sudo apt install -y \
  qt6-base-dev qt6-charts-dev qt6-tools-dev qt6-base-private-dev \
  libqt6sql6-sqlite libqt6websockets6-dev \
  libgl1-mesa-dev libglu1-mesa-dev
```

### Step 4 — Install Python 3.11

- **Windows**: [python.org 3.11.9](https://www.python.org/downloads/release/python-3119/) (check "Add to PATH")
- **Linux**: `sudo apt install python3.11`
- **macOS**: `brew install python@3.11`

### Step 5 — Clone and Configure

```bash
git clone https://github.com/Fincept-Corporation/FinceptTerminal.git
cd FinceptTerminal/fincept-qt
```

**Tell CMake where Qt is** (pick one method):

**Option A — Environment variable (recommended, set once):**
```bash
# Windows (PowerShell)
$env:QT_DIR = "C:\Qt\6.8.3\msvc2022_64"

# Linux
export QT_DIR=~/Qt/6.8.3/gcc_64

# macOS
export QT_DIR=~/Qt/6.8.3/macos
```

**Option B — CMakeUserPresets.json (persistent per-clone):**
```bash
cp CMakeUserPresets.json.example CMakeUserPresets.json
# Edit CMakeUserPresets.json — set CMAKE_PREFIX_PATH to your Qt install path
```

**Option C — Command line (one-off):**
```bash
cmake -B build/win-release -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="/path/to/Qt/6.8.3/kit"
```

> **Note:** If you installed Qt to the default location, CMake will auto-detect it — no configuration needed.

### Step 6 — Build and Run

**Windows** (run from **Developer Command Prompt for VS 2022** or **Developer PowerShell**):
```powershell
cmake --preset win-release
cmake --build --preset win-release
.\build\win-release\FinceptTerminal.exe
```

**Linux:**
```bash
cmake --preset linux-release
cmake --build --preset linux-release
./build/linux-release/FinceptTerminal
```

**macOS:**
```bash
cmake --preset macos-release
cmake --build --preset macos-release
./build/macos-release/FinceptTerminal.app/Contents/MacOS/FinceptTerminal
```

> **Windows users:** You must run from a Developer Command Prompt (or run `vcvars64.bat` first) so that MSVC, Windows SDK headers, and Ninja are on PATH. A regular PowerShell/CMD will fail with "cannot find stddef.h" errors.

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
│   │   ├── trading/                ← Trading engine + broker integrations
│   │   ├── services/               ← Market data, news services
│   │   └── screens/                ← Terminal screens (50+)
│   │
│   ├── scripts/                    ← Python analytics scripts
│   ├── CMakeLists.txt              ← Build configuration
│   ├── CMakePresets.json           ← Build presets (portable, no local paths)
│   └── CMakeUserPresets.json.example  ← Template for local Qt path config
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

Set the `QT_DIR` environment variable or pass `-DCMAKE_PREFIX_PATH`:
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="/path/to/Qt/6.8.3/platform"
```

### Windows: "Cannot open include file: 'stddef.h'"

You're not running from a VS Developer Command Prompt. Either:
- Open **"Developer Command Prompt for VS 2022"** from the Start menu
- Or run `"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"` in your current shell

### Python script not found
```bash
python --version         # Verify Python is in PATH
python scripts/yfinance_data.py quote AAPL   # Test directly
```

### OpenGL errors on Linux
```bash
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
