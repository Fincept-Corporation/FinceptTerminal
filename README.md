# Fincept Terminal

<div align="center">

[![License: AGPL-3.0](https://img.shields.io/badge/license-AGPL--3.0-C06524)](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus)](https://isocpp.org/)
[![Qt6](https://img.shields.io/badge/Qt-6-41CD52?logo=qt&logoColor=white)](https://www.qt.io/)
[![Python](https://img.shields.io/badge/Python-3.11+-3776AB?logo=python&logoColor=white)](https://www.python.org/)
[![Hits](https://hits.sh/github.com/Fincept-Corporation/FinceptTerminal.svg?label=Visits)](https://hits.sh/github.com/Fincept-Corporation/FinceptTerminal/)

[![Twitter](https://img.shields.io/badge/-Twitter-1DA1F2?style=flat-square&logo=twitter&logoColor=white)](https://twitter.com/intent/tweet?text=Check%20out%20FinceptTerminal&url=https%3A//github.com/Fincept-Corporation/FinceptTerminal/) [![LinkedIn](https://img.shields.io/badge/-LinkedIn-0077B5?style=flat-square&logo=linkedin&logoColor=white)](https://www.linkedin.com/sharing/share-offsite/?url=https%3A//github.com/Fincept-Corporation/FinceptTerminal/) [![Facebook](https://img.shields.io/badge/-Facebook-1877F2?style=flat-square&logo=facebook&logoColor=white)](https://www.facebook.com/sharer/sharer.php?u=https%3A//github.com/Fincept-Corporation/FinceptTerminal/) [![Reddit](https://img.shields.io/badge/-Reddit-FF4500?style=flat-square&logo=reddit&logoColor=white)](https://www.reddit.com/submit?url=https%3A//github.com/Fincept-Corporation/FinceptTerminal/&title=FinceptTerminal) [![WhatsApp](https://img.shields.io/badge/-WhatsApp-25D366?style=flat-square&logo=whatsapp&logoColor=white)](https://api.whatsapp.com/send?text=Check%20out%20FinceptTerminal%3A%20https%3A//github.com/Fincept-Corporation/FinceptTerminal/)

### **Your Thinking is the Only Limit. The Data Isn't.**

State-of-the-art financial intelligence platform with CFA-level analytics, AI automation, and unlimited data connectivity.

[📥 Download](https://github.com/Fincept-Corporation/FinceptTerminal/releases) · [📚 Docs](https://github.com/Fincept-Corporation/FinceptTerminal/tree/main/docs) · [💬 Discussions](https://github.com/Fincept-Corporation/FinceptTerminal/discussions) · [💬 Discord](https://discord.gg/ae87a8ygbN) · [🤝 Partner](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/docs/COMMERCIAL_LICENSE.md)

![Fincept Terminal](https://raw.githubusercontent.com/Fincept-Corporation/FinceptTerminal/main/images/Equity.png)

</div>

---

## About

**Fincept Terminal v4** is a pure native C++20 desktop application — a complete rewrite from the previous Tauri/React/Rust stack. It uses **Qt6** for UI and rendering, embedded **Python** for analytics, and delivers Bloomberg-terminal-class performance in a single native binary.

---

## Technology Stack

| Layer | Technologies |
|-------|-------------|
| **Language** | C++20 (MSVC / GCC / Clang) |
| **UI Framework** | Qt6 Widgets |
| **Charts** | Qt6 Charts |
| **Networking** | Qt6 Network + Qt6 WebSockets |
| **Database** | Qt6 Sql (SQLite) |
| **Analytics** | Embedded Python 3.11+ (100+ scripts) |
| **Build** | CMake 3.20+ |

---

## Features

| **Feature** | **Description** |
|-------------|-----------------|
| 📊 **CFA-Level Analytics** | DCF models, portfolio optimization, risk metrics (VaR, Sharpe), derivatives pricing via embedded Python |
| 🤖 **AI Agents** | 20+ investor personas (Buffett, Dalio, Graham), hedge fund strategies, local LLM support |
| 🌐 **100+ Data Connectors** | DBnomics, Polygon, Kraken, Yahoo Finance, FRED, IMF, World Bank, AkShare, government APIs |
| 📈 **Real-Time Trading** | Crypto (Kraken/HyperLiquid WebSocket), equity, algo trading, paper trading engine |
| 🔬 **QuantLib Suite** | 18 quantitative analysis modules — pricing, risk, stochastic, volatility, fixed income |
| 🚢 **Global Intelligence** | Maritime tracking, geopolitical analysis, relationship mapping, satellite data |
| 🎨 **Visual Workflows** | Node editor for automation pipelines, MCP tool integration |
| 🧠 **AI Quant Lab** | ML models, factor discovery, HFT, reinforcement learning trading |

---

## 40+ Screens

| Category | Screens |
|----------|---------|
| **Core** | Dashboard, Markets, News, Watchlist |
| **Trading** | Crypto Trading, Equity Trading, Algo Trading, Backtesting, Trade Visualization |
| **Research** | Equity Research, Screener, Portfolio, Surface Analytics, M&A Analytics, Derivatives, Alt Investments |
| **QuantLib** | Core, Analysis, Curves, Economics, Instruments, ML, Models, Numerical, Physics, Portfolio, Pricing, Regulatory, Risk, Scheduling, Solver, Statistics, Stochastic, Volatility |
| **AI/ML** | AI Quant Lab, Agent Studio, AI Chat, Alpha Arena |
| **Economics** | Economics, DBnomics, AkShare, Asia Markets |
| **Geopolitics** | Geopolitics, Gov Data, Relationship Map, Maritime, Polymarket |
| **Tools** | Code Editor, Node Editor, Excel, Report Builder, Notes, Data Sources, Data Mapping, MCP Servers |
| **Community** | Forum, Profile, Settings, Support, Docs, About |

---

## Installation

### Option 1 — Download Pre-built Binary (Recommended)

Pre-built binaries are available on the [Releases page](https://github.com/Fincept-Corporation/FinceptTerminal/releases). No build tools required — just extract and run.

| Platform | Download | Run |
|----------|----------|-----|
| **Windows x64** | `FinceptTerminal-Windows-x64.zip` | Extract → `FinceptTerminal.exe` |
| **Linux x64** | `FinceptTerminal-Linux-x64.tar.gz` | Extract → `./FinceptTerminal` |
| **macOS (Apple Silicon)** | `FinceptTerminal-macOS-arm64.tar.gz` | Extract → `./FinceptTerminal` |
| **macOS (Intel)** | `FinceptTerminal-macOS-x64.tar.gz` | Extract → `./FinceptTerminal` |
| **macOS (Universal)** | `FinceptTerminal-macOS-universal.tar.gz` | Extract → `./FinceptTerminal` |

---

### Option 2 — Quick Start (One-Click Build)

Clone and run the setup script — it installs all dependencies and builds the app automatically:

```bash
# Linux / macOS
git clone https://github.com/Fincept-Corporation/FinceptTerminal.git
cd FinceptTerminal
chmod +x setup.sh && ./setup.sh
```

```bat
# Windows — run from Developer Command Prompt for VS 2022
git clone https://github.com/Fincept-Corporation/FinceptTerminal.git
cd FinceptTerminal
setup.bat
```

The script handles: compiler check, CMake, Qt6, Python, build, and launch.

---

### Option 3 — Docker

```bash
# Pull and run
docker pull ghcr.io/fincept-corporation/fincept-terminal:latest
docker run --rm -e DISPLAY=$DISPLAY -v /tmp/.X11-unix:/tmp/.X11-unix \
    ghcr.io/fincept-corporation/fincept-terminal:latest

# Or build from source
git clone https://github.com/Fincept-Corporation/FinceptTerminal.git
cd FinceptTerminal
docker build -t fincept-terminal .
docker run --rm -e DISPLAY=$DISPLAY -v /tmp/.X11-unix:/tmp/.X11-unix fincept-terminal
```

> **Note:** Docker is primarily intended for Linux. macOS and Windows require additional XServer configuration.

---

### Option 4 — Build from Source (Manual)

#### Prerequisites

| Tool | Version | Windows | Linux | macOS |
|------|---------|---------|-------|-------|
| **Git** | latest | `winget install Git.Git` | `apt install git` | `brew install git` |
| **CMake** | 3.20+ | `winget install Kitware.CMake` | `apt install cmake` | `brew install cmake` |
| **C++ compiler** | C++20 | MSVC 2022 ([Visual Studio](https://visualstudio.microsoft.com/)) | `apt install g++` | Xcode CLT: `xcode-select --install` |
| **Qt6** | 6.5+ | See below | See below | See below |
| **Python** | 3.11+ | [python.org](https://www.python.org/downloads/) | `apt install python3` | `brew install python` |

#### Install Qt6

**Windows:**
```powershell
# Via Qt online installer (recommended — includes windeployqt)
# Download from https://www.qt.io/download-qt-installer
# Select: Qt 6.x > MSVC 2022 64-bit

# Or via winget
winget install Qt.QtCreator
```

**Linux (Ubuntu/Debian):**
```bash
sudo apt install -y \
  qt6-base-dev qt6-charts-dev qt6-tools-dev \
  libqt6sql6-sqlite libqt6websockets6-dev \
  libgl1-mesa-dev libglu1-mesa-dev
```

**macOS:**
```bash
brew install qt
```

#### Build

```bash
git clone https://github.com/Fincept-Corporation/FinceptTerminal.git
cd FinceptTerminal/fincept-qt

# Linux / macOS
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# Windows (from Developer Command Prompt for VS 2022)
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/msvc2022_64"
cmake --build build --config Release --parallel
```

#### Run

```bash
./build/FinceptTerminal              # Linux / macOS
.\build\Release\FinceptTerminal.exe  # Windows
```

---

## What Sets Us Apart

**Fincept Terminal** is an open-source financial platform built for those who refuse to be limited by traditional software. We compete on **analytics depth** and **data accessibility** — not on insider info or exclusive feeds.

- **Native performance** — C++20 with Qt6, no Electron/web overhead
- **Single binary** — no Node.js, no browser runtime, no JavaScript bundler
- **CFA-level analytics** — complete curriculum coverage via Python modules
- **100+ data connectors** — from Yahoo Finance to government databases
- **Free & Open Source** (AGPL-3.0) with commercial licenses available

---

## Roadmap

**Q1 2026:** Qt6 migration complete, enhanced real-time streaming, advanced backtesting
**Q2 2026:** Options strategy builder, multi-portfolio management, 50+ AI agents
**Future:** Mobile companion, institutional features, programmatic API, ML training UI

---

## Contributing

We're building the future of financial analysis — together.

**Contribute:** New data connectors, AI agents, analytics modules, C++ screens, documentation

- [Contributing Guide](docs/CONTRIBUTING.md)
- [C++ Contributing Guide](fincept-qt/CONTRIBUTING.md)
- [Python Contributor Guide](docs/PYTHON_CONTRIBUTOR_GUIDE.md)
- [Report Bug](https://github.com/Fincept-Corporation/FinceptTerminal/issues)
- [Request Feature](https://github.com/Fincept-Corporation/FinceptTerminal/discussions)

---

## For Universities & Educators

**Bring professional-grade financial analytics to your classroom.**

- **$799/month** for 20 accounts
- Full access to Fincept Data & APIs
- Perfect for finance, economics, and data science courses
- CFA curriculum analytics built-in

**Interested?** Email **support@fincept.in** with your institution name.

[University Licensing Details](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/docs/COMMERCIAL_LICENSE.md)

---

## License

**Dual Licensed: AGPL-3.0 (Open Source) + Commercial**

### Open Source (AGPL-3.0)
- Free for personal, educational, and non-commercial use
- Requires sharing modifications when distributed or used as network service
- Full source code transparency

### Commercial License
- Required for business use or to access Fincept Data/APIs commercially
- Contact: **support@fincept.in**
- Details: [Commercial License Guide](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/docs/COMMERCIAL_LICENSE.md)

### Trademarks
"Fincept Terminal" and "Fincept" are trademarks of Fincept Corporation.

© 2025-2026 Fincept Corporation. All rights reserved.

---

<div align="center">

### **Your Thinking is the Only Limit. The Data Isn't.**

<div align="center">
<a href="https://star-history.com/#Fincept-Corporation/FinceptTerminal&Date">
 <picture>
   <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/svg?repos=Fincept-Corporation/FinceptTerminal&type=Date&theme=dark" />
   <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/svg?repos=Fincept-Corporation/FinceptTerminal&type=Date" />
   <img alt="Star History Chart" src="https://api.star-history.com/svg?repos=Fincept-Corporation/FinceptTerminal&type=Date" />
 </picture>
</a>
</div>

[![Email](https://img.shields.io/badge/Email-support@fincept.in-blue)](mailto:support@fincept.in)

⭐ **Star** · 🔄 **Share** · 🤝 **Contribute**

</div>
