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

![Fincept Terminal](https://raw.githubusercontent.com/Fincept-Corporation/FinceptTerminal/main/images/Dashboard.png)

</div>

---

## About

**Fincept Terminal v4** is a pure native C++20 desktop application. It uses **Qt6** for UI and rendering, embedded **Python** for analytics, and delivers Bloomberg-terminal-class performance in a single native binary.

---

## Features

| **Feature** | **Description** |
|-------------|-----------------|
| 📊 **CFA-Level Analytics** | DCF models, portfolio optimization, risk metrics (VaR, Sharpe), derivatives pricing via embedded Python |
| 🤖 **AI Agents** | 20+ investor personas (Buffett, Dalio, Graham), hedge fund strategies, local LLM support, multi-provider (OpenAI, Anthropic, Gemini, Groq, DeepSeek, MiniMax, OpenRouter, Ollama) |
| 🌐 **100+ Data Connectors** | DBnomics, Polygon, Kraken, Yahoo Finance, FRED, IMF, World Bank, AkShare, government APIs, plus optional alternative-data overlays such as Adanos market sentiment for equity research |
| 📈 **Real-Time Trading** | Crypto (Kraken/HyperLiquid WebSocket), equity, algo trading, paper trading engine |
| 🔬 **QuantLib Suite** | 18 quantitative analysis modules — pricing, risk, stochastic, volatility, fixed income |
| 🚢 **Global Intelligence** | Maritime tracking, geopolitical analysis, relationship mapping, satellite data |
| 🎨 **Visual Workflows** | Node editor for automation pipelines, MCP tool integration |
| 🧠 **AI Quant Lab** | ML models, factor discovery, HFT, reinforcement learning trading |

---

## Installation

<!-- DOWNLOAD-TABLE-START -->
### Option 1 — Download Installer (Recommended)

Latest release: **v4.0.1** — [View all releases](https://github.com/Fincept-Corporation/FinceptTerminal/releases/tag/v4.0.1)

| Platform | Download | Run |
|----------|----------|-----|
| **Windows x64** | [FinceptTerminal-Windows-x64-setup.exe](https://github.com/Fincept-Corporation/FinceptTerminal/releases/download/v4.0.1/FinceptTerminal-4.0.1-win64-setup.exe) | Run installer → launch `FinceptTerminal.exe` |
| **Linux x64** | [FinceptTerminal-Linux-x64.run](https://github.com/Fincept-Corporation/FinceptTerminal/releases/download/v4.0.1/FinceptTerminal-4.0.1-linux-x64-setup.run) | `chmod +x` → run installer |
| **macOS Apple Silicon** | [FinceptTerminal-macOS-arm64.dmg](https://github.com/Fincept-Corporation/FinceptTerminal/releases/download/v4.0.1/FinceptTerminal-4.0.1-macOS-setup.dmg) | Open DMG → drag to Applications |
<!-- DOWNLOAD-TABLE-END -->

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

> **Versions are pinned.** Use the exact versions below. Newer or older versions are unsupported and may fail to build or produce unstable binaries.

#### Prerequisites (exact versions)

| Tool | Pinned Version | Notes |
|------|----------------|-------|
| **Git** | latest | — |
| **CMake** | **3.27.7** | [Download](https://cmake.org/download/) |
| **Ninja** | **1.11.1** | [Download](https://github.com/ninja-build/ninja/releases) |
| **C++ compiler** | **MSVC 19.38** (VS 2022 17.8) / **GCC 12.3** / **Apple Clang 15.0** (Xcode 15.2) | C++20 required |
| **Qt** | **6.7.2** (LTS) | [Qt Online Installer](https://www.qt.io/download-qt-installer) |
| **Python** | **3.11.9** | [python.org](https://www.python.org/downloads/release/python-3119/) |
| **Platform SDK** | Win10 SDK 10.0.22621.0 / macOS SDK 14.0 (deploy 11.0+) / glibc 2.31+ | — |

#### Install Qt 6.7.2

**Windows:** Qt Online Installer → select `Qt 6.7.2 > MSVC 2022 64-bit` (install path: `C:/Qt/6.7.2/msvc2022_64`)

**Linux:** Qt Online Installer → `Qt 6.7.2 > Desktop gcc 64-bit` (install path: `~/Qt/6.7.2/gcc_64`). **Or** for system packages, install `qt6-base-dev qt6-charts-dev qt6-tools-dev qt6-base-private-dev libqt6websockets6-dev libgl1-mesa-dev` — note system packages may be a different 6.x minor.

**macOS:** Qt Online Installer → `Qt 6.7.2 > macOS` (install path: `~/Qt/6.7.2/macos`)

#### Build (using CMake presets — recommended)

```bash
git clone https://github.com/Fincept-Corporation/FinceptTerminal.git
cd FinceptTerminal/fincept-qt

# Configure + build (pick your platform)
cmake --preset win-release     && cmake --build --preset win-release      # Windows (Dev Cmd for VS 2022)
cmake --preset linux-release   && cmake --build --preset linux-release    # Linux
cmake --preset macos-release   && cmake --build --preset macos-release    # macOS
```

Debug variants: `win-debug`, `linux-debug`, `macos-debug`.

#### Build (manual — if presets can't resolve your Qt path)

```bash
# Windows (Developer Command Prompt for VS 2022)
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_PREFIX_PATH="C:/Qt/6.7.2/msvc2022_64"
cmake --build build

# Linux
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$HOME/Qt/6.7.2/gcc_64"
cmake --build build

# macOS
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
  -DCMAKE_PREFIX_PATH="$HOME/Qt/6.7.2/macos"
cmake --build build
```

#### Run

```bash
./build/<preset>/FinceptTerminal         # Linux / macOS (preset build)
.\build\<preset>\FinceptTerminal.exe     # Windows (preset build)
```

#### Troubleshooting

1. **"Could not find Qt6 6.7.2"** — verify `CMAKE_PREFIX_PATH` points to the Qt 6.7.2 install, not 6.5/6.6/6.8.
2. **MSVC version error** — use VS 2022 17.8+ (MSVC 19.38+). Check with `cl /?`.
3. **Need to unblock with a different Qt minor?** Pass `-DFINCEPT_ALLOW_QT_DRIFT=ON` (local testing only — never for releases or CI).
4. Clean rebuild: delete `build/<preset>/` and re-run configure.

---

## What Sets Us Apart

**Fincept Terminal** is an open-source financial platform built for those who refuse to be limited by traditional software. We compete on **analytics depth** and **data accessibility** — not on insider info or exclusive feeds.

Recent builds also support optional **Adanos Market Sentiment** connectivity in **Data Sources → Alternative Data**. When configured, Equity Research can surface cross-source retail sentiment snapshots across Reddit, X, finance news, and Polymarket. Without an active Adanos connection, the feature remains dormant and the rest of the app behaves exactly as before.

- **Native performance** — C++20 with Qt6, no Electron/web overhead
- **Single binary** — no Node.js, no browser runtime, no JavaScript bundler
- **CFA-level analytics** — complete curriculum coverage via Python modules
- **100+ data connectors** — from Yahoo Finance to government databases
- **Free & Open Source** (AGPL-3.0) with commercial licenses available

---

## Roadmap

| Timeline | Milestone |
|----------|-----------|
| **Q1 2026** | Real-time streaming, advanced backtesting, broker integrations |
| **Q2 2026** | Options strategy builder, multi-portfolio management, 50+ AI agents |
| **Q3 2026** | Programmatic API, ML training UI, institutional features |
| **Future** | Mobile companion, cloud sync, community marketplace |

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
