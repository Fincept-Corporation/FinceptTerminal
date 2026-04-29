# 金融终端

<div align="center">

[![License: AGPL-3.0](https://img.shields.io/badge/license-AGPL--3.0-C06524)](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/LICENSE)[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus)](https://isocpp.org/)[![Qt6](https://img.shields.io/badge/Qt-6-41CD52?logo=qt&logoColor=white)](https://www.qt.io/)[![Python](https://img.shields.io/badge/Python-3.11+-3776AB?logo=python&logoColor=white)](https://www.python.org/)[![Hits](https://hits.sh/github.com/Fincept-Corporation/FinceptTerminal.svg?label=Visits)](https://hits.sh/github.com/Fincept-Corporation/FinceptTerminal/)

[![Twitter](https://img.shields.io/badge/-Twitter-1DA1F2?style=flat-square&logo=twitter&logoColor=white)](https://twitter.com/intent/tweet?text=Check%20out%20FinceptTerminal&url=https%3A//github.com/Fincept-Corporation/FinceptTerminal/)[![LinkedIn](https://img.shields.io/badge/-LinkedIn-0077B5?style=flat-square&logo=linkedin&logoColor=white)](https://www.linkedin.com/sharing/share-offsite/?url=https%3A//github.com/Fincept-Corporation/FinceptTerminal/)[![Facebook](https://img.shields.io/badge/-Facebook-1877F2?style=flat-square&logo=facebook&logoColor=white)](https://www.facebook.com/sharer/sharer.php?u=https%3A//github.com/Fincept-Corporation/FinceptTerminal/)[![Reddit](https://img.shields.io/badge/-Reddit-FF4500?style=flat-square&logo=reddit&logoColor=white)](https://www.reddit.com/submit?url=https%3A//github.com/Fincept-Corporation/FinceptTerminal/&title=FinceptTerminal)[![WhatsApp](https://img.shields.io/badge/-WhatsApp-25D366?style=flat-square&logo=whatsapp&logoColor=white)](https://api.whatsapp.com/send?text=Check%20out%20FinceptTerminal%3A%20https%3A//github.com/Fincept-Corporation/FinceptTerminal/)

### **你的思维是唯一的限制。数据不是。**

最先进的金融情报平台，具有 CFA 级分析、人工智能自动化和无限数据连接。

[📥 下载](https://github.com/Fincept-Corporation/FinceptTerminal/releases)·[📚 文档](https://github.com/Fincept-Corporation/FinceptTerminal/tree/main/docs)·[💬 讨论](https://github.com/Fincept-Corporation/FinceptTerminal/discussions)·[💬 Discord](https://discord.gg/ae87a8ygbN)·[🤝 合作伙伴](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/docs/COMMERCIAL_LICENSE.md)

![Fincept Terminal](https://raw.githubusercontent.com/Fincept-Corporation/FinceptTerminal/main/images/Dashboard.png)

</div>

* * *

## 关于

**Fincept终端vch**是一个纯原生 C++20 桌面应用程序。它使用**Qt6**用于 UI 和渲染，嵌入式**Python**用于分析，并在单个本机二进制文件中提供彭博终端级的性能。

* * *

## 特征

| **特征** | **描述** |
| ----------------- | -------------------------------------------------------------------------------------------------------------- |
| 📊**CFA 级别分析** | DCF 模型、投资组合优化、风险指标（VaR、夏普比率）、通过嵌入式 Python 进行衍生品定价                                                                |
| 🤖**AI 智能体** | 20 多个投资者角色（巴菲特、达里奥、格雷厄姆）、对冲基金策略、**本地大语言模型 (LLM) 支持**、多供应商（OpenAI、Anthropic、Gemini、Groq、DeepSeek、MiniMax、OpenRouter、Ollama） |
| 🌐**100 多个数据连接器** | DBnomics、Polygon、Kraken、雅虎财经、FRED、IMF、世界银行、AkShare、政府 API，以及可选的替代数据叠加（例如用于股票研究的 Adanos 市场情绪数据）                    |
| 📈**实时交易** | 加密货币（Kraken/HyperLiquid WebSocket）、股票、算法交易、模拟交易引擎 (Paper Trading)                                                              |
| 🔬**量化库套件** | 18 个定量分析模块 —— 定价、风险、随机过程、波动率、固定收益                                                                                   |
| 🚢**全球情报** | 海上追踪、地缘政治分析、关系映射、卫星数据                                                                                          |
| 🎨**可视化工作流程** | 用于自动化管线、MCP 工具集成的节点编辑器                                                                                         |
| 🧠**人工智能量化实验室** | 机器学习模型、因子发现、高频交易、强化学习交易

* * *

## 安装

### 选项 1 — 下载预构建的二进制文件（推荐）

预构建的二进制文件可在[发布页面](https://github.com/Fincept-Corporation/FinceptTerminal/releases)下载。不需要构建工具 —— 只需解压并运行。

| 平台              | 下载文件                                       | 运行方式                                                   |
| --------------- | ---------------------------------------- | ---------------------------------------------------- |
| **Windows x64** | `FinceptTerminal-Windows-x64.zip`        | 解压 → 运行 `FinceptTerminal.exe`                            |
| **Windows ARM64** | `FinceptTerminal-Windows-arm64.zip`      | 解压 → 运行 `FinceptTerminal.exe`                            |
| **Linux x64** | `FinceptTerminal-Linux-x86_64.AppImage`  | `chmod +x` → `./FinceptTerminal-Linux-x86_64.AppImage` |
| **macOS (Apple 芯片)** | `FinceptTerminal-macOS-arm64.tar.gz`     | 解压 → 运行 `./FinceptTerminal`                              |
| **macOS (Intel)** | `FinceptTerminal-macOS-x64.tar.gz`       | 解压 → 运行 `./FinceptTerminal`                              |
| **macOS (通用)** | `FinceptTerminal-macOS-universal.tar.gz` | 解压 → 运行 `./FinceptTerminal`                              |

* * *

### 选项 2 — 快速启动（一键构建）

克隆并运行安装脚本 —— 它会安装所有依赖项并自动构建应用程序：

```bash
# Linux / macOS
git clone [https://github.com/Fincept-Corporation/FinceptTerminal.git](https://github.com/Fincept-Corporation/FinceptTerminal.git)
cd FinceptTerminal
chmod +x setup.sh && ./setup.sh
```

```bat
# Windows — 请从 Visual Studio 2022 的 Developer Command Prompt 运行
git clone [https://github.com/Fincept-Corporation/FinceptTerminal.git](https://github.com/Fincept-Corporation/FinceptTerminal.git)
cd FinceptTerminal
setup.bat
```

该脚本将自动处理：编译器检查、CMake、Qt6、Python 环境配置、构建和启动。

* * *

### 选项 3 — Docker

```bash
# 拉取并运行
docker pull ghcr.io/fincept-corporation/fincept-terminal:latest
docker run --rm -e DISPLAY=$DISPLAY -v /tmp/.X11-unix:/tmp/.X11-unix \
    ghcr.io/fincept-corporation/fincept-terminal:latest

# 或从源代码构建
git clone [https://github.com/Fincept-Corporation/FinceptTerminal.git](https://github.com/Fincept-Corporation/FinceptTerminal.git)
cd FinceptTerminal
docker build -t fincept-terminal .
docker run --rm -e DISPLAY=$DISPLAY -v /tmp/.X11-unix:/tmp/.X11-unix fincept-terminal
```

> **注意： Docker 主要适用于 Linux。macOS 和 Windows 需要额外的 XServer 配置。

* * *

### 选项 4 — 从源代码构建（手动）

> **版本已锁定**（Qt 6.7.2、CMake 3.27.7、MSVC 19.38 / GCC 12.3 / Apple Clang 15.0、Python 3.11.9）。为避免翻译出现偏差，请遵循官方英文说明：
>
> 👉 **[README.md (English) — Build from Source](../../README.md#option-4--build-from-source-manual)**
>
> 使用 CMake 预设快速启动：
> ```bash
> ./setup.sh                                       # Linux / macOS — 自动化安装
> setup.bat                                  # Windows（VS 2022 Developer Cmd）
>
> # 或采用手动方式：
> cd FinceptTerminal/fincept-qt
> cmake --preset linux-release   && cmake --build --preset linux-release
> cmake --preset macos-release   && cmake --build --preset macos-release
> cmake --preset win-release     && cmake --build --preset win-release
> ```

<details>
<summary>原始说明（已过时 — 保留供参考）</summary>

#### 先决条件

| 工具         | 版本    | 视窗                                                       | Linux                 | macOS                              |
| ---------- | ----- | -------------------------------------------------------- | --------------------- | ---------------------------------- |
| **git**    | 最新版   | `winget install Git.Git`                                 | `apt install git`     | `brew install git`                 |
| **CMake**  | 3.20+ | `winget install Kitware.CMake`                           | `apt install cmake`   | `brew install cmake`               |
| **C++编译器** | C++20 | MSVC 2022 ([Visual Studio](https://visualstudio.microsoft.com/)) | `apt install g++`     | Xcode CLT：`xcode-select --install` |
| **Qt6**    | 6.5+  | 见下文                                                      | 见下文                   | 见下文                                |
| **Python** | 3.11+ | [python.org](https://www.python.org/downloads/)          | `apt install python3` | `brew install python`              |

#### 安装Qt6

**视窗：**

```powershell
# 通过 Qt 在线安装程序 (推荐 — 包含 windeployqt)
# 从 [https://www.qt.io/download-qt-installer](https://www.qt.io/download-qt-installer) 下载
# 选择: Qt 6.x > MSVC 2022 64-bit

# 或通过 winget 安装
winget install Qt.QtCreator
```

**Linux（Ubuntu/Debian）：**

```bash
sudo apt install -y \
  qt6-base-dev qt6-charts-dev qt6-tools-dev \
  libqt6sql6-sqlite libqt6websockets6-dev \
  libgl1-mesa-dev libglu1-mesa-dev
```

**苹果系统：**

```bash
brew install qt
```

#### 构建

```bash
git clone [https://github.com/Fincept-Corporation/FinceptTerminal.git](https://github.com/Fincept-Corporation/FinceptTerminal.git)
cd FinceptTerminal/fincept-qt

# Linux / macOS
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# Windows (请从 VS 2022 Developer Command Prompt 运行)
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/msvc2022_64"
cmake --build build --config Release --parallel
```

#### 运行

```bash
./build/FinceptTerminal              # Linux / macOS
.\build\Release\FinceptTerminal.exe  # Windows
```

</details>

* * *

## 是什么让我们与众不同


**金融终端** 是一个为拒绝受限于传统软件框架的使用者所打造的开源金融平台。我们竞争的核心在于**分析的深度**与**数据的可及性** —— 而非依赖内幕消息或封闭的独家数据源。

最新版本还支持在 **数据源 → 替代数据** 中，连接可选的 **Adanos 市场情绪** 数据。配置完成后，股票研究界面将能显示 Reddit、X (Twitter)、财经新闻和 Polymarket 等跨数据源的散户情绪快照。若未启用 Adanos 连接，此功能将保持休眠状态，且应用程序的其他逻辑与以往完全相同。



-   **原生性能**— 采用 C++20 与 Qt6 开发，没有 Electron 或网页浏览器的性能开销
-   **单一二进制**— 没有 Node.js，没有浏览器运行时，没有 JavaScript 打包工具
-   **CFA级别分析**— 通过内嵌的 Python 模块，完整涵盖 CFA 课程级别的分析能力
-   **100 多个数据连接器**— 从雅虎财经到政府数据库
-   **免费和开源**(AGPL-3.0) 并提供商业许可选项

* * *

## 路线图

| 时间轴            | 里程碑                         |
| -------------- | --------------------------- |
| **2026 年第一季度** | 实时流媒体报价、高级回测系统、券商 API 集成            |
| **2026 年第二季度**    | 期权策略构建器、多重投资组合管理、50+ AI 智能体 |
| **2026 年第三季度**     | 编程式 API、机器学习训练 UI、机构级功能      |
| **未来**         | 移动伴侣、云同步、社区市场               |

* * *

## 贡献

我们正在共同建设财务分析的未来。

**欢迎贡献：** 新的数据连接器、AI 智能体、分析模块、C++ 界面设计、官方文档撰写。


-   [贡献指南](docs/CONTRIBUTING.md)
-   [C++ 贡献指南](fincept-qt/CONTRIBUTING.md)
-   [Python 贡献者指南](docs/PYTHON_CONTRIBUTOR_GUIDE.md)
-   [报告bug](https://github.com/Fincept-Corporation/FinceptTerminal/issues)
-   [请求新功能](https://github.com/Fincept-Corporation/FinceptTerminal/discussions)

* * *

## 面向大学与教育工作者

**将专业级的财务分析系统带入您的课堂中**

-   **$799/月**20 个帐户
-   完全访问 Fincept 数据和 API
-   非常适合金融、经济学和数据科学课程
-   内置 CFA 课程分析

**感兴趣吗？** 请附上您的机构名称，发送电子邮件至**[support@fincept.in]。
[大学许可详细信息](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/docs/COMMERCIAL_LICENSE.md)

* * *

## 许可证

**双重许可模式：AGPL-3.0（开源）+ 商业许可**

### 开源许可 (AGPL-3.0)

-   免费供个人、教育和非商业用途
-   分发或用作网络服务时需要共享修改
-   源代码完全透明

### 商业许可

-   若用于商业用途或需要商业级访问 Fincept 数据/API 时必备
-   联系方式：**[support@fincept.in](mailto:support@fincept.in)**
-   细节：[商业许可指南](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/docs/COMMERCIAL_LICENSE.md)

### 商标声明

“Fincept Terminal”和“Fincept”是Fincept Corporation 的商标。

© 2025-2026 Fincept Corporation. 保留所有权利。

* * *

<div align="center">

### **你的思维是唯一的限制。数据不是。**

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

⭐**支持**· 🔄**分享**· 🤝**贡献**

</div>
