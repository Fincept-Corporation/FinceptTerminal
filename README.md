# Fincept Terminal

<div align="center">

[![License: AGPL-3.0](https://img.shields.io/badge/license-AGPL--3.0-C06524)](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus)](https://isocpp.org/)
[![ImGui](https://img.shields.io/badge/ImGui-Docking-blue)](https://github.com/ocornut/imgui)
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

**Fincept Terminal v4** is a pure native C++20 desktop application — a complete rewrite from the previous Tauri/React/Rust stack. It uses **Dear ImGui** for GPU-accelerated UI, **GLFW + OpenGL** for rendering, embedded **Python** for analytics, and delivers Bloomberg-terminal-class performance in a single native binary.

---

## Technology Stack

| Layer | Technologies |
|-------|-------------|
| **Language** | C++20 (MSVC / GCC / Clang) |
| **UI** | Dear ImGui (docking branch) + ImPlot |
| **Layout** | Yoga (Flexbox engine) |
| **Rendering** | GLFW 3 + OpenGL 3.3+ |
| **Networking** | libcurl + OpenSSL |
| **Database** | SQLite 3 |
| **Serialization** | nlohmann/json |
| **Logging** | spdlog |
| **Audio** | miniaudio |
| **Video** | libmpv (optional) |
| **Analytics** | Embedded Python 3.11+ (100+ scripts) |
| **Build** | CMake 3.20+ / vcpkg |

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

## Build from Source

### Prerequisites

- **CMake** 3.20+
- **vcpkg** (for dependency management)
- **C++20 compiler** (MSVC 2022, GCC 12+, or Clang 15+)
- **Python** 3.11+ (for analytics scripts)

### Build

```bash
git clone https://github.com/Fincept-Corporation/FinceptTerminal.git
cd FinceptTerminal/fincept-cpp

# Windows (MSVC)
cmake --preset=default
cmake --build build --config Release

# Linux / macOS
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Run

```bash
./build/FinceptTerminal          # Linux / macOS
.\build\Release\FinceptTerminal  # Windows
```

### vcpkg Dependencies

glfw3, curl, nlohmann-json, sqlite3, openssl, imgui (docking + freetype), yoga, stb, implot, spdlog, miniaudio

---

## What Sets Us Apart

**Fincept Terminal** is an open-source financial platform built for those who refuse to be limited by traditional software. We compete on **analytics depth** and **data accessibility** — not on insider info or exclusive feeds.

- **Native performance** — C++ with GPU-accelerated ImGui, no Electron/web overhead
- **Single binary** — no Node.js, no browser runtime, no JavaScript bundler
- **CFA-level analytics** — complete curriculum coverage via Python modules
- **100+ data connectors** — from Yahoo Finance to government databases
- **Free & Open Source** (AGPL-3.0) with commercial licenses available

---

## Roadmap

**Q1 2026:** C++ migration complete, enhanced real-time streaming, advanced backtesting
**Q2 2026:** Options strategy builder, multi-portfolio management, 50+ AI agents
**Future:** Mobile companion, institutional features, programmatic API, ML training UI

---

## Contributing

We're building the future of financial analysis — together.

**Contribute:** New data connectors, AI agents, analytics modules, C++ screens, documentation

- [Contributing Guide](docs/CONTRIBUTING.md)
- [C++ Contributing Guide](fincept-cpp/CONTRIBUTING.md)
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
