# Getting Started with Fincept Terminal Development
## Your Complete Onboarding Guide

Welcome to Fincept Terminal! This guide will get you from zero to your first contribution.

---

## What is Fincept Terminal?

Fincept Terminal is an **open-source financial analysis platform** — a free, open-source alternative to Bloomberg Terminal. Version 4 is a native C++20 application using Dear ImGui for GPU-accelerated UI.

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

- **C++20 + ImGui** — Native performance, GPU-accelerated UI, single binary (~15MB)
- **GLFW + OpenGL** — Cross-platform rendering, mature and stable
- **Embedded Python** — Access to vast ecosystem of financial libraries (yfinance, pandas, etc.)
- **SQLite** — Local data caching for speed
- **vcpkg** — Reproducible dependency management

---

## Quick Setup

### Prerequisites

#### 1. Install a C++20 compiler

- **Windows**: Install [Visual Studio 2022](https://visualstudio.microsoft.com/) with "Desktop development with C++" workload
- **Linux**: `sudo apt install -y g++ cmake ninja-build`
- **macOS**: `xcode-select --install && brew install cmake ninja`

#### 2. Install CMake 3.20+ and Ninja

- **Windows**: `winget install Kitware.CMake Ninja-build.Ninja`
- **Linux/macOS**: included in step above

#### 3. Install vcpkg and set VCPKG_ROOT

```bash
# Linux / macOS
git clone https://github.com/microsoft/vcpkg.git ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh
echo 'export VCPKG_ROOT=~/vcpkg' >> ~/.bashrc  # or ~/.zshrc
source ~/.bashrc

# Windows (PowerShell)
git clone https://github.com/microsoft/vcpkg.git "$env:USERPROFILE\vcpkg"
& "$env:USERPROFILE\vcpkg\bootstrap-vcpkg.bat"
[System.Environment]::SetEnvironmentVariable("VCPKG_ROOT","$env:USERPROFILE\vcpkg","User")
# Restart your terminal after this
```

#### 4. Linux only — install system dependencies

```bash
sudo apt install -y \
  libxinerama-dev libxcursor-dev xorg-dev libglu1-mesa-dev \
  libxrandr-dev libxi-dev libxext-dev libxfixes-dev \
  libwayland-dev libxkbcommon-dev pkg-config
```

#### 5. Install Python 3.11+

- **Windows**: [python.org](https://www.python.org/downloads/) (check "Add to PATH")
- **Linux**: `sudo apt install python3`
- **macOS**: `brew install python`

### Setup in 4 Steps

```bash
# 1. Clone the repository
git clone https://github.com/Fincept-Corporation/FinceptTerminal.git
cd FinceptTerminal/fincept-cpp

# 2. Configure — vcpkg installs all C++ dependencies automatically
cmake --preset=default

# 3. Build
cmake --build build --config Release

# 4. Run
./build/FinceptTerminal              # Linux / macOS
.\build\Release\FinceptTerminal.exe  # Windows
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
├── fincept-cpp/                    ← Main application (you'll work here)
│   ├── src/                        ← C++ source code
│   │   ├── main.cpp                ← Entry point, GLFW/OpenGL setup
│   │   ├── app.cpp/h               ← App state machine, routing
│   │   ├── core/                   ← Shared infrastructure
│   │   │   ├── config.h            ← Constants
│   │   │   ├── event_bus.h         ← Pub/sub messaging
│   │   │   ├── logger.h            ← Logging
│   │   │   └── result.h            ← Error handling
│   │   ├── ui/                     ← Reusable ImGui widgets
│   │   ├── http/                   ← HTTP client (libcurl)
│   │   ├── storage/                ← SQLite database
│   │   ├── auth/                   ← Authentication
│   │   ├── python/                 ← Python runtime bridge
│   │   ├── trading/                ← Trading engine + brokers
│   │   └── screens/                ← 40+ terminal screens
│   │       ├── dashboard/
│   │       ├── markets/
│   │       ├── crypto_trading/
│   │       └── ...
│   │
│   ├── scripts/                    ← Python analytics scripts
│   │   ├── Analytics/              ← 34 analytics modules
│   │   ├── agents/                 ← AI agents
│   │   └── *.py                    ← 80+ data fetchers
│   │
│   ├── CMakeLists.txt              ← Build configuration
│   └── vcpkg.json                  ← Dependencies
│
└── docs/                           ← Documentation
```

### How Data Flows

```
User clicks "Get AAPL quote"
        │
        ▼
Screen (markets_screen.cpp)         ← UI rendering only
        │
        ▼
Data Service (markets_data.cpp)     ← Fetching + caching
        │
        ├─── HTTP API call (libcurl)
        │         │
        │         ▼
        │    Parse JSON (nlohmann/json)
        │
        └─── Python Script (optional)
                  │
                  ▼
             Python fetches from API
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
| Make HTTP calls | `src/http/http_client.cpp` |
| Store data locally | `src/storage/database.cpp` |
| Add a Python analytics module | `scripts/Analytics/` |
| Add a broker | `src/trading/brokers/` |

---

## Your First Contribution

### Path A: Easy (15 minutes)
**Improve Documentation** — Fix a typo or unclear explanation in any `.md` file.

### Path B: Easy (30 minutes)
**Add a Python Data Fetcher** — Follow the pattern in `scripts/yfinance_data.py` to create a wrapper for a new free API.

### Path C: Medium (1-2 hours)
**Add a New Screen** — Follow `fincept-cpp/CONTRIBUTING.md` to add a new ImGui screen.

### Path D: Advanced (2-4 hours)
**Add a Broker Integration** — Follow the `BrokerInterface` pattern in `src/trading/`.

---

## Common Development Workflows

### Workflow 1: Add a New Python Script

```bash
cd fincept-cpp/scripts

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
2. Create files: `your_screen.h/.cpp`, `your_data.h/.cpp` (if needed)
3. Add `.cpp` files to `CMakeLists.txt` `SOURCES` list
4. Add screen instance in `app.cpp`
5. Add tab entry in `render_tab_bar()`
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
- Run `clang-format -i` before committing

---

## Troubleshooting

### Build fails — missing vcpkg packages
```bash
# Ensure VCPKG_ROOT is set
echo $VCPKG_ROOT          # Linux / macOS (should print the vcpkg path)
echo %VCPKG_ROOT%         # Windows CMD

# If empty, set it (see Prerequisites step 3 above), then reconfigure
cmake --preset=default --fresh
cmake --build build --config Release
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
# Install OpenGL dev packages
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
- Read the C++ guide: [fincept-cpp/CONTRIBUTING.md](../fincept-cpp/CONTRIBUTING.md)
- Read the Python guide: [PYTHON_CONTRIBUTOR_GUIDE.md](./PYTHON_CONTRIBUTOR_GUIDE.md)
