# 金融终端

<div align="center">

[![License: AGPL-3.0](https://img.shields.io/badge/license-AGPL--3.0-C06524)](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/LICENSE)[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus)](https://isocpp.org/)[![Qt6](https://img.shields.io/badge/Qt-6-41CD52?logo=qt&logoColor=white)](https://www.qt.io/)[![Python](https://img.shields.io/badge/Python-3.11+-3776AB?logo=python&logoColor=white)](https://www.python.org/)[![Hits](https://hits.sh/github.com/Fincept-Corporation/FinceptTerminal.svg?label=Visits)](https://hits.sh/github.com/Fincept-Corporation/FinceptTerminal/)

[![Twitter](https://img.shields.io/badge/-Twitter-1DA1F2?style=flat-square&logo=twitter&logoColor=white)](https://twitter.com/intent/tweet?text=Check%20out%20FinceptTerminal&url=https%3A//github.com/Fincept-Corporation/FinceptTerminal/)[![LinkedIn](https://img.shields.io/badge/-LinkedIn-0077B5?style=flat-square&logo=linkedin&logoColor=white)](https://www.linkedin.com/sharing/share-offsite/?url=https%3A//github.com/Fincept-Corporation/FinceptTerminal/)[![Facebook](https://img.shields.io/badge/-Facebook-1877F2?style=flat-square&logo=facebook&logoColor=white)](https://www.facebook.com/sharer/sharer.php?u=https%3A//github.com/Fincept-Corporation/FinceptTerminal/)[![Reddit](https://img.shields.io/badge/-Reddit-FF4500?style=flat-square&logo=reddit&logoColor=white)](https://www.reddit.com/submit?url=https%3A//github.com/Fincept-Corporation/FinceptTerminal/&title=FinceptTerminal)[![WhatsApp](https://img.shields.io/badge/-WhatsApp-25D366?style=flat-square&logo=whatsapp&logoColor=white)](https://api.whatsapp.com/send?text=Check%20out%20FinceptTerminal%3A%20https%3A//github.com/Fincept-Corporation/FinceptTerminal/)

### **你的思维是唯一的限制。数据不是。**

最先进的金融情报平台，具有机构级金融分析、人工智能自动化和无限数据连接。

[📥 下载](https://github.com/Fincept-Corporation/FinceptTerminal/releases)·[📚 文档](https://github.com/Fincept-Corporation/FinceptTerminal/tree/main/docs)·[💬 讨论](https://github.com/Fincept-Corporation/FinceptTerminal/discussions)·[💬 不和谐](https://discord.gg/ae87a8ygbN)·[🤝 合作伙伴](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/docs/COMMERCIAL_LICENSE.md)

![Fincept Terminal](https://raw.githubusercontent.com/Fincept-Corporation/FinceptTerminal/main/images/Dashboard.png)

</div>

* * *

## 关于

**Fincept终端vch**是一个纯原生 C++20 桌面应用程序。它使用**Qt6**用于 UI 和渲染，嵌入式**Python**用于分析，并在单个本机二进制文件中提供彭博终端级的性能。

* * *

## 特征

| **特征**            | **描述**                                                                                                         |
| ----------------- | -------------------------------------------------------------------------------------------------------------- |
| 📊**多资产分析**      | DCF 模型、投资组合优化、风险指标（VaR、夏普）、跨股票、固定收益、衍生品、投资组合及另类投资的衍生品定价，通过嵌入式 Python 实现                                       |
| 🤖**人工智能代理**      | 20 多个投资者角色（巴菲特、戴利奥、格雷厄姆）、对冲基金策略、本地法学硕士支持、多提供商（OpenAI、Anthropic、Gemini、Groq、DeepSeek、MiniMax、OpenRouter、Ollama） |
| 🌐**100 多个数据连接器** | DBnomics、Polygon、Kraken、雅虎财经、FRED、IMF、世界银行、AkShare、政府 API，以及可选的替代数据叠加，例如用于股票研究的 Adanos 市场情绪                    |
| 📈**实时交易**        | 加密货币（Kraken/HyperLiquid WebSocket）、股权、算法交易、纸质交易引擎                                                              |
| 🔬**定量库套件**       | 18个定量分析模块——定价、风险、随机、波动性、固定收益                                                                                   |
| 🚢**全球情报**        | 海上跟踪、地缘政治分析、关系映射、卫星数据                                                                                          |
| 🎨**可视化工作流程**     | 用于自动化管道、MCP 工具集成的节点编辑器                                                                                         |
| 🧠**人工智能量化实验室**   | 机器学习模型、因子发现、高频交易、强化学习交易                                                                                        |

* * *

## 安装

### 选项 1 — 下载预构建的二进制文件（推荐）

预构建的二进制文件可在[发布页面](https://github.com/Fincept-Corporation/FinceptTerminal/releases)。不需要构建工具 - 只需提取并运行。

| 平台              | 下载                                       | 跑步                                                   |
| --------------- | ---------------------------------------- | ---------------------------------------------------- |
| **Windows x64** | `FinceptTerminal-Windows-x64.zip`        | 提取 →`FinceptTerminal.exe`                            |
| **视窗ARM64**     | `FinceptTerminal-Windows-arm64.zip`      | 提取 →`FinceptTerminal.exe`                            |
| **Linux x64**   | `FinceptTerminal-Linux-x86_64.AppImage`  | `chmod +x`→`./FinceptTerminal-Linux-x86_64.AppImage` |
| **macOS（苹果芯片）** | `FinceptTerminal-macOS-arm64.tar.gz`     | 提取 →`./FinceptTerminal`                              |
| **macOS（英特尔）**  | `FinceptTerminal-macOS-x64.tar.gz`       | 提取 →`./FinceptTerminal`                              |
| **macOS（通用）**   | `FinceptTerminal-macOS-universal.tar.gz` | 提取 →`./FinceptTerminal`                              |

* * *

### 选项 2 — 快速启动（一键构建）

克隆并运行安装脚本 - 它会安装所有依赖项并自动构建应用程序：

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

该脚本处理：编译器检查、CMake、Qt6、Python、构建和启动。

* * *

### 选项 3 — Docker

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

> **笔记：**Docker 主要适用于 Linux。 macOS 和 Windows 需要额外的 XServer 配置。

* * *

### 选项 4 — 从源代码构建（手动）

> **版本已锁定**（Qt 6.7.2、CMake 3.27.7、MSVC 19.38 / GCC 12.3 / Apple Clang 15.0、Python 3.11.9）。为避免翻译漂移，请遵循官方英文说明：
>
> 👉 **[README.md (English) — Build from Source](../../README.md#option-4--build-from-source-manual)**
>
> 使用 CMake 预设快速启动：
> ```bash
> ./setup.sh                                            # Linux / macOS — 自动化安装
> setup.bat                                             # Windows（VS 2022 Developer Cmd）
>
> # 或手动：
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
| **git**    | 最新的   | `winget install Git.Git`                                 | `apt install git`     | `brew install git`                 |
| **CMake**  | 3.20+ | `winget install Kitware.CMake`                           | `apt install cmake`   | `brew install cmake`               |
| **C++编译器** | C++20 | MSVC 2022 ([视觉工作室](https://visualstudio.microsoft.com/)) | `apt install g++`     | Xcode CLT：`xcode-select --install` |
| **Qt6**    | 6.5+  | 见下文                                                      | 见下文                   | 见下文                                |
| **Python** | 3.11+ | [python.org](https://www.python.org/downloads/)          | `apt install python3` | `brew install python`              |

#### 安装Qt6

**视窗：**

```powershell
# Via Qt online installer (recommended — includes windeployqt)
# Download from https://www.qt.io/download-qt-installer
# Select: Qt 6.x > MSVC 2022 64-bit

# Or via winget
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

#### 建造

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

#### 跑步

```bash
./build/FinceptTerminal              # Linux / macOS
.\build\Release\FinceptTerminal.exe  # Windows
```

</details>

* * *

## 是什么让我们与众不同

**金融终端**是一个为那些拒绝受传统软件限制的人打造的开源金融平台。我们竞争的是**分析深度**和**数据可访问性**- 不在内部信息或独家提要中。

最近的版本还支持可选**艾达诺市场情绪**连接性在**数据源 → 替代数据**。配置后，Equity Research 可以显示 Reddit、X、财经新闻和 Polymarket 上的跨源零售情绪快照。如果没有活动的 Adanos 连接，该功能将保持休眠状态，并且应用程序的其余部分的行为与以前完全相同。

-   **原生性能**— 使用 Qt6 的 C++20，无 Electron/web 开销
-   **单一二进制**— 没有 Node.js，没有浏览器运行时，没有 JavaScript 捆绑器
-   **完整的买方分析师工具包**— 股票、投资组合、衍生品、固定收益、企业金融、另类投资
-   **100 多个数据连接器**— 从雅虎财经到政府数据库
-   **免费和开源**(AGPL-3.0) 提供商业许可证

* * *

## 路线图

| 时间轴            | 里程碑                         |
| -------------- | --------------------------- |
| **2026 年第一季度** | 实时流媒体、高级回测、经纪商集成            |
| **呕吐 2026**    | 期权策略构建器、多投资组合管理、50 多个人工智能代理 |
| **喀什2026**     | 编程式 API、ML 培训 UI、机构功能       |
| **未来**         | 移动伴侣、云同步、社区市场               |

* * *

## 贡献

我们正在共同建设财务分析的未来。

**贡献：**新的数据连接器、AI 代理、分析模块、C++ 屏幕、文档

-   [贡献指南](docs/CONTRIBUTING.md)
-   [C++ 贡献指南](fincept-qt/CONTRIBUTING.md)
-   [Python 贡献者指南](docs/PYTHON_CONTRIBUTOR_GUIDE.md)
-   [报告错误](https://github.com/Fincept-Corporation/FinceptTerminal/issues)
-   [请求功能](https://github.com/Fincept-Corporation/FinceptTerminal/discussions)

* * *

## 对于大学和教育工作者

**将专业级财务分析带入您的课堂。**

-   **$799/月**20 个帐户
-   完全访问 Fincept 数据和 API
-   非常适合金融、经济学和数据科学课程
-   内置股票、投资组合、衍生品、固定收益和经济学分析

**感兴趣的？**电子邮件**[support@fincept.in](mailto:support@fincept.in)**与您的机构名称。

[大学许可详情](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/docs/COMMERCIAL_LICENSE.md)

* * *

## 执照

**双重许可：AGPL-3.0（开源）+商业**

### 开源 (AGPL-3.0)

-   免费供个人、教育和非商业用途
-   分发或用作网络服务时需要共享修改
-   源代码完全透明

### 商业许可

-   商业用途或商业访问 Fincept 数据/API 所需
-   接触：**[support@fincept.in](mailto:support@fincept.in)**
-   细节：[商业许可指南](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/docs/COMMERCIAL_LICENSE.md)

### 商标

“Fincept Terminal”和“Fincept”是Fincept Corporation 的商标。

© 2025-2026 Fincept 公司。版权所有。

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

⭐**星星**· 🔄**分享**· 🤝**贡献**

</div>
