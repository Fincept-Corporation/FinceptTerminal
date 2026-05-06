# 金融終端機 (Fincept Terminal)

<div align="center">

[![License: AGPL-3.0](https://img.shields.io/badge/license-AGPL--3.0-C06524)](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/LICENSE)[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus)](https://isocpp.org/)[![Qt6](https://img.shields.io/badge/Qt-6-41CD52?logo=qt&logoColor=white)](https://www.qt.io/)[![Python](https://img.shields.io/badge/Python-3.11+-3776AB?logo=python&logoColor=white)](https://www.python.org/)[![Hits](https://hits.sh/github.com/Fincept-Corporation/FinceptTerminal.svg?label=Visits)](https://hits.sh/github.com/Fincept-Corporation/FinceptTerminal/)

[![Twitter](https://img.shields.io/badge/-Twitter-1DA1F2?style=flat-square&logo=twitter&logoColor=white)](https://twitter.com/intent/tweet?text=Check%20out%20FinceptTerminal&url=https%3A//github.com/Fincept-Corporation/FinceptTerminal/)[![LinkedIn](https://img.shields.io/badge/-LinkedIn-0077B5?style=flat-square&logo=linkedin&logoColor=white)](https://www.linkedin.com/sharing/share-offsite/?url=https%3A//github.com/Fincept-Corporation/FinceptTerminal/)[![Facebook](https://img.shields.io/badge/-Facebook-1877F2?style=flat-square&logo=facebook&logoColor=white)](https://www.facebook.com/sharer/sharer.php?u=https%3A//github.com/Fincept-Corporation/FinceptTerminal/)[![Reddit](https://img.shields.io/badge/-Reddit-FF4500?style=flat-square&logo=reddit&logoColor=white)](https://www.reddit.com/submit?url=https%3A//github.com/Fincept-Corporation/FinceptTerminal/&title=FinceptTerminal)[![WhatsApp](https://img.shields.io/badge/-WhatsApp-25D366?style=flat-square&logo=whatsapp&logoColor=white)](https://api.whatsapp.com/send?text=Check%20out%20FinceptTerminal%3A%20https%3A//github.com/Fincept-Corporation/FinceptTerminal/)

### **你的思維是唯一的限制。數據不是。**

最先進的金融情報平台，具備 CFA 級別分析、人工智慧自動化，以及無限的數據連接能力。

[📥 下載](https://github.com/Fincept-Corporation/FinceptTerminal/releases)·[📚 官方文件](https://github.com/Fincept-Corporation/FinceptTerminal/tree/main/docs)·[💬 討論區](https://github.com/Fincept-Corporation/FinceptTerminal/discussions)·[💬 Discord](https://discord.gg/ae87a8ygbN)·[🤝 合作夥伴](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/docs/COMMERCIAL_LICENSE.md)

![Fincept Terminal](https://raw.githubusercontent.com/Fincept-Corporation/FinceptTerminal/main/images/Dashboard.png)

</div>

* * *

## 關於

**Fincept Terminal v4** 是一個純原生的 C++20 桌面應用程式。它採用 **Qt6** 負責使用者介面與圖表渲染，並嵌入 **Python** 作為分析引擎，在單一原生執行檔中，提供媲美專業金融終端機級別的強大效能。

* * *

## 核心特色

| **功能** | **描述** |
| ----------------- | -------------------------------------------------------------------------------------------------------------- |
| 📊**CFA 級別分析** | DCF 模型、投資組合最佳化 (Portfolio Optimization)、風險指標（VaR、夏普比率）、透過嵌入式 Python 進行衍生性金融商品定價                                                                |
| 🤖**AI 代理** | 20 多種投資者角色（巴菲特、達里歐、葛拉漢）、避險基金策略、**本地大型語言模型 (LLM) 支援**、多模型供應商（OpenAI、Anthropic、Gemini、Groq、DeepSeek、MiniMax、OpenRouter、Ollama） |
| 🌐**100+ 數據連接器** | DBnomics、Polygon、Kraken、Yahoo 財經、FRED、IMF、世界銀行、AkShare、政府 API，以及可選的替代數據疊加（例如用於股票研究的 Adanos 市場情緒數據）                    |
| 📈**實盤交易** | 加密貨幣（Kraken/HyperLiquid WebSocket）、股票、演算法交易、模擬交易引擎 (Paper Trading)                                                              |
| 🔬**量化函式庫套件** | 18 個量化分析模組 —— 定價、風險、隨機微積分 (Stochastic)、波動率、固定收益                                                                                   |
| 🚢**全球情報** | 海上船舶追蹤、地緣政治分析、關聯性映射、衛星數據                                                                                          |
| 🎨**視覺化工作流程** | 支援自動化管線 (Pipeline) 與 MCP 工具整合的節點編輯器 (Node Editor)                                                                                         |
| 🧠**AI 量化實驗室** | 機器學習模型、因子發掘 (Factor Discovery)、高頻交易 (HFT)、強化學習交易                                                                                        |

* * *

* * *

## 安裝

### 選項 1 — 下載預先建置的二進位檔（推薦）

您可以在 [發佈頁面 (Releases)](https://github.com/Fincept-Corporation/FinceptTerminal/releases) 下載預先建置的二進位檔。不需要安裝任何建置工具 — 只需解壓縮並執行即可。

| 平台              | 下載檔案                                     | 執行方式                                               |
| --------------- | ---------------------------------------- | ---------------------------------------------------- |
| **Windows x64** | `FinceptTerminal-Windows-x64.zip`        | 解壓縮 → 執行 `FinceptTerminal.exe`                            |
| **Windows ARM64** | `FinceptTerminal-Windows-arm64.zip`      | 解壓縮 → 執行 `FinceptTerminal.exe`                            |
| **Linux x64** | `FinceptTerminal-Linux-x86_64.AppImage`  | `chmod +x` → `./FinceptTerminal-Linux-x86_64.AppImage` |
| **macOS (Apple 晶片)** | `FinceptTerminal-macOS-arm64.tar.gz`     | 解壓縮 → 執行 `./FinceptTerminal`                              |
| **macOS (Intel)** | `FinceptTerminal-macOS-x64.tar.gz`       | 解壓縮 → 執行 `./FinceptTerminal`                              |
| **macOS (通用)** | `FinceptTerminal-macOS-universal.tar.gz` | 解壓縮 → 執行 `./FinceptTerminal`                              |

* * *

### 選項 2 — 快速啟動（一鍵建置）

複製 (Clone) 專案並執行安裝腳本 — 它會自動安裝所有相依套件 (Dependencies) 並建置應用程式：

```bash
# Linux / macOS
git clone [https://github.com/Fincept-Corporation/FinceptTerminal.git](https://github.com/Fincept-Corporation/FinceptTerminal.git)
cd FinceptTerminal
chmod +x setup.sh && ./setup.sh


```bat
# Windows — 請從 VS 2022 的開發人員命令提示字元 (Developer Command Prompt) 執行
git clone [https://github.com/Fincept-Corporation/FinceptTerminal.git](https://github.com/Fincept-Corporation/FinceptTerminal.git)
cd FinceptTerminal
setup.bat
```

該腳本會自動處理：編譯器檢查、CMake、Qt6、Python 環境設定、建置與啟動。

* * *

### 選項 3 — Docker

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

> **注意：** Docker 映像檔主要適用於 Linux 環境。macOS 和 Windows 若要使用 Docker 執行 GUI，需要進行額外的 XServer 設定。。

* * *

### 選項 4 — 從原始碼建置 (手動)

> **版本已鎖定**（Qt 6.7.2、CMake 3.27.7、MSVC 19.38 / GCC 12.3 / Apple Clang 15.0、Python 3.11.9）。為避免翻譯文件與實際程式碼產生落差，請優先遵循官方英文說明：
>
> 👉 **[README.md (English) — Build from Source](../../README.md#option-4--build-from-source-manual)**
>
> 使用 CMake 預設配置 (Presets) 快速啟動：
> ```bash
> ./setup.sh                                            # Linux / macOS — 自動化安裝
> setup.bat                                             # Windows（於 VS 2022 開發人員命令提示字元執行）
>
> # 或採用手動方式：
> cd FinceptTerminal/fincept-qt
> cmake --preset linux-release   && cmake --build --preset linux-release
> cmake --preset macos-release   && cmake --build --preset macos-release
> cmake --preset win-release     && cmake --build --preset win-release
> ```

<details>
<summary>原始說明（已過時 — 保留供參考）</summary>

#### 先決條件 (Prerequisites)

| 工具         | 版本    | Windows                                                       | Linux                 | macOS                              |
| ---------- | ----- | -------------------------------------------------------- | --------------------- | ---------------------------------- |
| **git** | 最新版   | `winget install Git.Git`                                 | `apt install git`     | `brew install git`                 |
| **CMake** | 3.20+ | `winget install Kitware.CMake`                           | `apt install cmake`   | `brew install cmake`               |
| **C++ 編譯器** | C++20 | MSVC 2022 ([Visual Studio](https://visualstudio.microsoft.com/)) | `apt install g++`     | Xcode CLT：`xcode-select --install` |
| **Qt6** | 6.5+  | 見下文                                                      | 見下文                   | 見下文                                |
| **Python** | 3.11+ | [python.org](https://www.python.org/downloads/)          | `apt install python3` | `brew install python`              |

#### 安裝 Qt6

**Windows：**

```powershell
# 透過 Qt 線上安裝程式 (推薦 — 包含 windeployqt)
# 從 [https://www.qt.io/download-qt-installer](https://www.qt.io/download-qt-installer) 下載
# 選擇: Qt 6.x > MSVC 2022 64-bit

# 或透過 winget 安裝
winget install Qt.QtCreator
```

**Linux（Ubuntu/Debian）：**

```bash
sudo apt install -y \
  qt6-base-dev qt6-charts-dev qt6-tools-dev \
  libqt6sql6-sqlite libqt6websockets6-dev \
  libgl1-mesa-dev libglu1-mesa-dev
```

**macOS：**

```bash
brew install qt
```

#### 建置 (Build)

```bash
git clone [https://github.com/Fincept-Corporation/FinceptTerminal.git](https://github.com/Fincept-Corporation/FinceptTerminal.git)
cd FinceptTerminal/fincept-qt

# Linux / macOS
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# Windows (請從 VS 2022 開發人員命令提示字元執行)
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/msvc2022_64"
cmake --build build --config Release --parallel
```

#### 執行 (Run)

```bash
./build/FinceptTerminal              # Linux / macOS
.\build\Release\FinceptTerminal.exe  # Windows
```

</details>

* * *

## 是什麼讓我們與眾不同

**金融終端機 (Fincept Terminal)**是一個為拒絕受限於傳統軟體框架的使用者所打造的開源金融平台。我們競爭的優勢在於**分析的深度**與**數據的可及性** —— 而非依賴內線消息或封閉的獨家數據源。

最新版本更支援在**資料源 → 替代數據**中，連接可選的**Adanos 市場情緒**數據。設定完成後，股票研究介面將能顯示 Reddit、X (Twitter)、財經新聞和 Polymarket 等多個來源的散戶情緒快照。若未啟用 Adanos 連接，此功能將保持休眠狀態，且應用程式的其他運作邏輯與以往完全相同


-  **原生效能** — 採用 C++20 與 Qt6 開發，沒有 Electron 或網頁瀏覽器的效能負擔
-  **單一執行檔** — 不需要 Node.js、瀏覽器執行環境或 JavaScript 打包工具
-  **CFA 級別分析** — 透過內建的 Python 模組，完整涵蓋 CFA 課程級別的分析能力
-  **100+ 數據連接器** — 從 Yahoo 財經到各國政府資料庫
-  **免費且開源** (AGPL-3.0)，並提供商業授權選項

* * *

## 產品開發路線圖

| 時間軸            | 里程碑                         |
| -------------- | --------------------------- |
| **2026 年第一季** | 即時串流報價、進階回測系統、券商 API 整合            |
| **2026 年第二季**    | 選擇權策略建構器、多重投資組合管理、50+ AI 智能體 |
| **2026 年第三季**     | 程式化 API、機器學習訓練介面、機構級功能       |
| **未來展望**         | 移动端 App、雲端同步、社群市場               |

* * *

## 貢獻

我們正在共同建構財務分析的未來。

**歡迎貢獻**： 新的數據連接器、AI 智能體、分析模組、C++ 介面設計、官方文件撰寫。

-   [貢獻指南](docs/CONTRIBUTING.md)
-   [C++ 貢獻指南](fincept-qt/CONTRIBUTING.md)
-   [Python 貢獻者指南](docs/PYTHON_CONTRIBUTOR_GUIDE.md)
-   [回報bug](https://github.com/Fincept-Corporation/FinceptTerminal/issues)
-   [許願新功能](https://github.com/Fincept-Corporation/FinceptTerminal/discussions)

* * *

## 針對大學與教育工作者

**將專業級的財務分析系統帶入您的課堂中。**

-   $799/月 涵蓋 20 個使用者帳號
-   完全存取 Fincept 數據庫與 API
-   非常適合財務、經濟學與資料科學等課程
-   內建 CFA 課程分析

 
**有興趣嗎？**  請附上您的機構名稱，發送電子郵件至 support@fincept.in。
[大學授權詳細資訊](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/docs/COMMERCIAL_LICENSE.md)

* * *

## 授權條款

**雙重授權模式：AGPL-3.0（開源）+ 商業授權**

### 開源授權 (AGPL-3.0)

-   供個人、教育及非商業用途免費使用
-   若進行散佈或作為網路服務 (Web Service) 提供時，必須開源您的修改內容
-   原始碼完全透明

### 商業授權

-   若用於商業用途或需要商業級存取 Fincept 數據/API 時必備
-   聯絡方式：**[support@fincept.in](mailto:support@fincept.in)**
-   詳細資訊：[商業授權指南](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/docs/COMMERCIAL_LICENSE.md)

### 商標聲明

「Fincept Terminal」與「Fincept」為 Fincept Corporation 之註冊商標。

© 2025-2026 Fincept Corporation. 保留所有權利。

* * *

<div align="center">

### **你的思維是唯一的限制。數據不是。**

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

⭐ Star 支持 · 🔄 Fork 分享 · 🤝 參與貢獻

</div>