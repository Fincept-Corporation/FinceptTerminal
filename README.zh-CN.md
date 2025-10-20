# Fincept终端✨

<div align="center">

[![License: MIT](https://img.shields.io/badge/license-MIT-C06524)](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/LICENSE.txt)[![Tauri](https://img.shields.io/badge/Tauri-2.0-FFC131?logo=tauri)](https://tauri.app/)[![React](https://img.shields.io/badge/React-19-61DAFB?logo=react)](https://react.dev/)[![TypeScript](https://img.shields.io/badge/TypeScript-5.6-3178C6?logo=typescript)](https://www.typescriptlang.org/)[![Hits](https://hits.sh/github.com/Fincept-Corporation/FinceptTerminal.svg?label=Visits)](https://hits.sh/github.com/Fincept-Corporation/FinceptTerminal/)

[![Twitter](https://img.shields.io/badge/-Twitter-1DA1F2?style=flat-square&logo=twitter&logoColor=white)](https://twitter.com/intent/tweet?text=Check%20out%20FinceptTerminal&url=https%3A//github.com/Fincept-Corporation/FinceptTerminal/)[![LinkedIn](https://img.shields.io/badge/-LinkedIn-0077B5?style=flat-square&logo=linkedin&logoColor=white)](https://www.linkedin.com/sharing/share-offsite/?url=https%3A//github.com/Fincept-Corporation/FinceptTerminal/)[![Facebook](https://img.shields.io/badge/-Facebook-1877F2?style=flat-square&logo=facebook&logoColor=white)](https://www.facebook.com/sharer/sharer.php?u=https%3A//github.com/Fincept-Corporation/FinceptTerminal/)[![Reddit](https://img.shields.io/badge/-Reddit-FF4500?style=flat-square&logo=reddit&logoColor=white)](https://www.reddit.com/submit?url=https%3A//github.com/Fincept-Corporation/FinceptTerminal/&title=FinceptTerminal)[![WhatsApp](https://img.shields.io/badge/-WhatsApp-25D366?style=flat-square&logo=whatsapp&logoColor=white)](https://api.whatsapp.com/send?text=Check%20out%20FinceptTerminal%3A%20https%3A//github.com/Fincept-Corporation/FinceptTerminal/)

[英语](README.md)\|[西班牙语](README.es.md)\|[中文](README.zh-CN.md)\|[日本人](README.ja.md)\|[法语](README.fr.md)\|[德语](README.de.md)\|[韩国人](README.ko.md)\|[印地语](README.hi.md)

### _专业财务分析平台_

**为每个人提供彭博级的见解。开源。跨平台。**

[📥 下载](#-getting-started)•[✨ 特点](#-features)•[📸 截图](#-platform-preview)•[🤝 贡献](#-contributing)

![Fincept Terminal](https://raw.githubusercontent.com/Fincept-Corporation/FinceptTerminal/main/images/Geopolitics.png)

</div>

* * *

## 🎯Fincept终端是什么？

**金融终端**是一个现代化的跨平台金融终端**困难**,**反应**， 和**打字稿**。它为散户投资者和交易者带来了机构级的金融分析工具——完全免费且开源。

受 Bloomberg 和 Refinitiv 的启发，Fincept Terminal 提供实时市场数据、高级分析、人工智能驱动的见解和专业界面 - 所有这些都无需企业价格标签。

### 🌟 为什么选择Fincept终端？

| 传统平台          | 金融终端           |
| ------------- | -------------- |
| 💸 每年$20,000+ | 🆓**免费和开源**    |
| 🏢 仅限企业       | 👤**所有人都可以使用** |
| 🔒 供应商锁定      | 💻**跨平台桌面**    |
| ⚙️ 有限定制       | 🎨**完全可定制**    |

**技术堆栈：**Tauri (Rust) • React 19 • TypeScript • TailwindCSS

* * *

## 🚀 开始使用

### **选项 1：从 Microsoft Store 下载**🎉

<div align="center">

[![Get it from Microsoft Store](https://get.microsoft.com/images/en-us%20dark.svg)](https://apps.microsoft.com/detail/XPDDZR13CXS466?hl=en-US&gl=IN&ocid=pdpshare)

**最简单的安装 • 自动更新 • Windows 10/11**

</div>

### **选项 2：下载安装程序**

**视窗：**

-   📦[下载 MSI 安装程序](http://product.fincept.in/FinceptTerminalV2Alpha.msi)（Windows 10/11）

**macOS 和 Linux：**

-   预构建安装程序即将推出。

### **选项 3：从源代码构建**

#### 🚀**快速设置（自动）**

**对于 Windows：**

```bash
# 1. Clone the repository
git clone https://github.com/Fincept-Corporation/FinceptTerminal.git
cd FinceptTerminal

# 2. Run setup script (as Administrator)
setup.bat
```

**对于 Linux/macOS：**

```bash
# 1. Clone the repository
git clone https://github.com/Fincept-Corporation/FinceptTerminal.git
cd FinceptTerminal

# 2. Make script executable and run
chmod +x setup.sh
./setup.sh
```

自动设置脚本将：

-   ✅ 安装 Node.js LTS (v22.14.0)
-   ✅ 安装 Rust（最新稳定版）
-   ✅ 安装项目依赖项
-   ✅ 自动设置一切

#### ⚙️**手动设置**

**先决条件：**Node.js 18+、Rust 1.70+、Git

```bash
# Clone the repository
git clone https://github.com/Fincept-Corporation/FinceptTerminal.git
cd FinceptTerminal/fincept-terminal-desktop

# Install dependencies
npm install

# Run in development
npm run tauri dev

# Build for production
npm run tauri build
```

* * *

## ✨ 特点

<table>
<tr>
<td width="50%">

### 📊 市场情报

🌍 全球覆盖（股票、外汇、加密货币、大宗商品）<br>📈 实时数据和流式更新<br>📰 多源新闻整合<br>📉 自定义监视列表

### 🧠 AI 支持的分析

🤖 GenAI 聊天界面<br>📊 实时情绪分析<br>💡 AI 驱动的见解和建议<br>🎯 自动模式识别

</td>
<td width="50%">

### 📈 专业工具

📊 高级图表（50 多个指标）<br>💼 公司财务和研究<br>📋 多账户投资组合跟踪<br>⚡ 回测策略<br>🔔 定制价格和技术警报

### 🌐 全球洞察

🏛️经济数据（利率、GDP、通货膨胀）<br>🗺️ 贸易路线和海上物流<br>🌍 地缘政治风险评估

</td>
</tr>
</table>

**用户体验：**Bloomberg 风格的 UI • 功能键快捷键 (F1-F12) • 深色模式 • 基于选项卡的工作流程

* * *

## 🎬 平台预览

<div align="center">

|                                                 聊天模块                                                |                                                      仪表板                                                      |
| :-------------------------------------------------------------------------------------------------: | :-----------------------------------------------------------------------------------------------------------: |
| ![Chat](https://raw.githubusercontent.com/Fincept-Corporation/FinceptTerminal/main/images/Chat.png) | ![Dashboard](https://raw.githubusercontent.com/Fincept-Corporation/FinceptTerminal/main/images/Dashboard.png) |
|                                              AI驱动的财务助理                                              |                                                     实时市场概览                                                    |

|                                                     经济                                                    |                                                   股票研究                                                  |
| :-------------------------------------------------------------------------------------------------------: | :-----------------------------------------------------------------------------------------------------: |
| ![Economy](https://raw.githubusercontent.com/Fincept-Corporation/FinceptTerminal/main/images/Economy.png) | ![Equity](https://raw.githubusercontent.com/Fincept-Corporation/FinceptTerminal/main/images/Equity.png) |
|                                                   全球经济指标                                                  |                                                 全面的股票分析                                                 |

|                                                   论坛                                                  |                                                        地缘政治                                                       |
| :---------------------------------------------------------------------------------------------------: | :---------------------------------------------------------------------------------------------------------------: |
| ![Forum](https://raw.githubusercontent.com/Fincept-Corporation/FinceptTerminal/main/images/Forum.png) | ![Geopolitics](https://raw.githubusercontent.com/Fincept-Corporation/FinceptTerminal/main/images/Geopolitics.png) |
|                                                  社区讨论                                                 |                                                       全球风险监控                                                      |

|                                                        全球贸易                                                       |                                                     市场                                                    |
| :---------------------------------------------------------------------------------------------------------------: | :-------------------------------------------------------------------------------------------------------: |
| ![GlobalTrade](https://raw.githubusercontent.com/Fincept-Corporation/FinceptTerminal/main/images/GlobalTrade.png) | ![Markets](https://raw.githubusercontent.com/Fincept-Corporation/FinceptTerminal/main/images/Markets.png) |
|                                                       国际贸易流量                                                      |                                                  实时多资产市场                                                  |

|                                                          贸易分析                                                         |
| :-------------------------------------------------------------------------------------------------------------------: |
| ![TradeAnalysis](https://raw.githubusercontent.com/Fincept-Corporation/FinceptTerminal/main/images/TradeAnalysis.png) |
|                                                        高级分析和回测                                                        |

</div>

* * *

## 🛣️路线图

### **目前状态**

-   ✅ Tauri 应用框架
-   ✅ 认证系统（访客+注册）
-   ✅ 彭博风格的用户界面
-   ✅ 支付整合
-   ✅ 论坛功能
-   🚧 实时市场数据
-   🚧 高级图表
-   🚧人工智能助手

### **即将推出（2025 年第 2 季度）**

-   📊 完整的市场数据流
-   📈 具有 50 多个指标的交互式图表
-   🤖 生产人工智能功能
-   💼 投资组合管理
-   🔔 警报系统

### **未来**

-   🌍 多语言支持
-   🏢 经纪商整合
-   📱 移动伴侣应用程序
-   🔌插件系统
-   🎨 主题市场

* * *

## 🤝 贡献

我们欢迎开发商、贸易商和金融专业人士的贡献！

**贡献方式：**

-   🐛 报告错误和问题
-   💡 建议新功能
-   🔧 提交代码（React、Rust、TypeScript）
-   📚 改进文档
-   🎨 设计贡献

**快速链接：**

-   [贡献指南](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/CONTRIBUTING.md)
-   [报告错误](https://github.com/Fincept-Corporation/FinceptTerminal/issues/new?template=bug_report.md)
-   [请求功能](https://github.com/Fincept-Corporation/FinceptTerminal/issues/new?template=feature_request.md)
-   [讨论](https://github.com/Fincept-Corporation/FinceptTerminal/discussions)

**开发设置：**

```bash
# Fork and clone
git clone https://github.com/YOUR_USERNAME/FinceptTerminal.git
cd FinceptTerminal

# Automated setup (recommended for first-time contributors)
# Windows: run setup.bat as Administrator
# Linux/macOS: chmod +x setup.sh && ./setup.sh

# Or manual setup
cd fincept-terminal-desktop
npm install
npm run dev          # Start Vite dev server
npm run tauri dev    # Start Tauri app
```

* * *

## 🏗️项目结构

    fincept-terminal-desktop/
    ├── src/
    │   ├── components/      # React components (auth, dashboard, tabs, ui)
    │   ├── contexts/        # React Context (Auth, Theme)
    │   ├── services/        # API service layers
    │   └── hooks/           # Custom React hooks
    ├── src-tauri/
    │   ├── src/             # Rust backend
    │   ├── Cargo.toml       # Rust dependencies
    │   └── tauri.conf.json  # Tauri config
    └── package.json         # Node dependencies

* * *

## 📊 技术细节

**表现：**

-   二进制大小：~15MB
-   内存：~150MB（Electron 为 500MB+）
-   启动：&lt;2秒

**平台支持：**

-   ✅ Windows 10/11（x64、ARM64）
-   ✅ macOS 11+（英特尔、Apple Silicon）
-   ✅ Linux（Ubuntu、Debian、Fedora）

**安全：**

-   Tauri 沙盒环境
-   没有 Node.js 运行时
-   加密凭证存储
-   仅 HTTPS API 调用

* * *

## 📈 明星历史

**⭐ 给回购加注星标并分享项目❤️**

<div align="center">
<a href="https://star-history.com/#Fincept-Corporation/FinceptTerminal&Date">
 <picture>
   <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/svg?repos=Fincept-Corporation/FinceptTerminal&type=Date&theme=dark" />
   <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/svg?repos=Fincept-Corporation/FinceptTerminal&type=Date" />
   <img alt="Star History Chart" src="https://api.star-history.com/svg?repos=Fincept-Corporation/FinceptTerminal&type=Date" />
 </picture>
</a>
</div>

* * *

## 🌐 与我们联系

<div align="center">

[![GitHub Discussions](https://img.shields.io/badge/GitHub-Discussions-181717?style=for-the-badge&logo=github)](https://github.com/Fincept-Corporation/FinceptTerminal/discussions)[![Email Support](https://img.shields.io/badge/Email-dev@fincept.in-D14836?style=for-the-badge&logo=gmail&logoColor=white)](mailto:dev@fincept.in)[![Contact Form](https://img.shields.io/badge/Contact-Form-4285F4?style=for-the-badge&logo=google&logoColor=white)](https://forms.gle/DUsDHwxBNRVstYMi6)

**由社区建造，为社区服务**_让每个人都能获得专业的财务分析_

⭐**给我们加星标**• 🔄**分享**• 🤝**贡献**

</div>

* * *

## 📜 许可证

麻省理工学院许可证 - 参见[许可证.txt](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/LICENSE.txt)

* * *

## 🙏致谢

内置：[困难](https://tauri.app/)•[反应](https://react.dev/)•[锈](https://www.rust-lang.org/)•[顺风CSS](https://tailwindcss.com/)•[基数用户界面](https://www.radix-ui.com/)

* * *

**笔记：**早期版本是使用 Python/DearPyGUI 构建的，并存档于`legacy-python-depreciated/`。目前基于 Tauri 的应用程序是用现代技术完全重写的。

* * *

<div align="center">

**© 2024-2025 Fincept Corporation • 开源 • MIT 许可证**

</div>
