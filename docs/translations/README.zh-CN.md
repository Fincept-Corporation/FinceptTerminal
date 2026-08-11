# Fincept Terminal

<div align="center">

[![License: AGPL-3.0](https://img.shields.io/badge/license-AGPL--3.0-C06524)](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus)](https://isocpp.org/)
[![Qt6](https://img.shields.io/badge/Qt-6-41CD52?logo=qt&logoColor=white)](https://www.qt.io/)
[![Python](https://img.shields.io/badge/Python-3.11+-3776AB?logo=python&logoColor=white)](https://www.python.org/)

### **唯一的上限是你的思考，而不是数据。**

面向机构级金融分析、AI 自动化与无限数据接入的前沿金融智能平台。

[📥 下载](https://github.com/Fincept-Corporation/FinceptTerminal/releases) · [🏢 Enterprise](https://fincept.in/enterprise) · [💳 价格](https://fincept.in/pricing) · [📖 手册](https://fincept.in/manual) · [💬 Discord](https://discord.gg/ae87a8ygbN)

![Fincept Terminal](https://raw.githubusercontent.com/Fincept-Corporation/FinceptTerminal/main/images/FinceptBanner.png)

</div>

> [!IMPORTANT]
> **Fincept Terminal 提供两个版本。**
>
> 🏢 **[Fincept Terminal Enterprise](https://fincept.in/enterprise)** — 面向基金、家族办公室和研究部门的私有闭源版本。41 个模块、专有数据集、多智能体研究、实时券商路由、私有数据室、SSO 与 SLA，**每用户每月 99 美元**起。团队的日常开发都在这一版本上；如果你靠终端赚钱，就该用它。
>
> 📖 **本仓库** — 基于 AGPL-3.0 的免费开源版本，面向学习、个人使用与学术研究。仓库将持续公开、不会删除，并保持**每月一次发布**。
>
> [**你需要哪一个？ →**](#应该选择哪个版本) · [对比](https://fincept.in/comparison) · [价格](https://fincept.in/pricing)

---

## 关于

**Fincept Terminal** 是一款用于金融研究的原生 C++20 桌面终端 —— Qt6 界面、内嵌 Python 3.11 分析引擎、单一二进制文件，不依赖 Electron。

它在**同一数据内核上提供两个版本**：

| | **开源版** — 本仓库 | **Enterprise** — [fincept.in/enterprise](https://fincept.in/enterprise) |
|---|---|---|
| **适用对象** | 学习、个人使用、学术研究 | 基金、家族办公室、研究部门 |
| **价格** | 免费 · AGPL-3.0 | **每用户每月 99 美元**起 |
| **发布节奏** | 每月一次更新 | 持续更新 |

---

## 应该选择哪个版本

| 如果你是…… | 选择 | 原因 |
|---|---|---|
| 学生、爱好者或自学者 | **开源版** | 真正免费，并且会一直免费 |
| 学术研究人员 | **开源版** | 学术用途免费 —— 需要统一管理席位的高校请看下方学术套餐 |
| 基金、家族办公室、自营交易台、银行或金融科技公司 | **Enterprise** | 不适用 AGPL 传染性条款，并获得专有数据、实时路由、SSO 与 SLA |
| 以此为职业、靠它赚钱的人 | **Enterprise** | 实际成本更低，而且是唯一在每日开发中的版本 |

> **实话实说。** 开源版是真材实料，不是阉割版演示 —— 但它跑在免费公开数据源上，用的是**你的** API 密钥和**你的** LLM 密钥，每一次 AI 调用都按 token 计入你的账单，没有上限。社区 issue 尽力处理，不承诺响应时间。如果终端是你的谋生工具，Enterprise 才是更便宜也更稳妥的席位。

[**了解 Enterprise →**](https://fincept.in/enterprise) · [完整对比](https://fincept.in/comparison) · [价格](https://fincept.in/pricing) · [常见问题](https://fincept.in/faq)

---

## 开源版与 Enterprise 对比

| | 开源版 | **Enterprise** |
|---|---|---|
| **许可证** | AGPL-3.0 —— 强传染性。一旦分发或托管，就必须公开你的修改 | 专有许可 —— 无需处理任何 copyleft 义务 |
| **成本** | 免费 —— 数据与 LLM 账单自付 | 每用户每月 99 / 199 / 299 美元，全部包含 |
| **模块** | 核心终端 | **6 个业务台、41 个模块** |
| **数据** | 免费公开数据源，有速率限制，需自备密钥 | 私有与专有数据集 —— 历史更长、刷新更快 |
| **价格历史** | 取决于免费额度 | 1 年 / 5 年 / 无限 · 支持**时点数据**，回测更可信 |
| **AI 预算** | 自备 LLM 密钥，按 token 自付 | 每月含 400 / 2,000 / 5,000 积分 |
| **AI 研究** | 基础行情助手 | 会**规划并分派任务**的多智能体研究 · Agent Studio · 最多 53 个智能体 · 6 种团队模式 |
| **私有数据室** | — | 你的文件与模型仅由你自己的智能体读取 —— 不与他人混用，也不用于训练 |
| **交易** | 模拟交易 + 自备密钥的券商接入 | **实时券商路由 + 实时算法部署** |
| **量化** | 社区回测 | Quant Lab、Alpha Arena、波动率分析、时点回测 |
| **安全与合规** | — | SSO/SAML、审计日志、基于角色的访问控制、数据隔离 |
| **支持** | GitHub issue，尽力而为，不承诺响应 | 有 SLA 保障的优先处理 |
| **文档** | 本仓库 | [**700 页手册**](https://fincept.in/manual) —— 41 篇指南、472 个章节 |
| **平台** | Windows、macOS、Linux + 托管网页终端 | Windows 10/11、macOS 13+（Apple 芯片）、Linux |

Enterprise 使用**独立账号** —— 免费 Fincept 账号无法登录，且在绑定订阅之前应用会保持锁定。[创建 Enterprise 账号 →](https://fincept.in/enterprise/signup)

---

## Enterprise —— 六大业务台，41 个模块

| 业务台 | 功能 |
|---|---|
| **Agentic Research** · 6 | 智能体规划工作、分派给专职智能体，读取实时数据与你的数据室，返回带来源的研究笔记 |
| **Quant Lab & Backtesting** · 4 | 信号研究、时点回测、波动率曲面 |
| **Deep Fundamental Research** · 8 | 股票分析、估值、衍生品、并购分析 |
| **Markets & Execution** · 7 | 股票、加密货币与预测市场实时交易，券商路由，算法部署 |
| **Macro & Global Intelligence** · 7 | 统计数据、政府数据、地缘政治、航运航线 |
| **你的专属工作区** · 9 | 仪表盘、电子表格、笔记、文件、代码、报告生成器 |

[查看产品 →](https://fincept.in/products)

### 价格

| | **Exclusive** | **Exclusive+** ★ 最受欢迎 | **Exclusive Pro** |
|---|---|---|---|
| | **99 美元**/用户/月 | **199 美元**/用户/月 | **299 美元**/用户/月 |
| AI 积分 / 月 | 400 | 2,000 | 5,000 |
| 投资组合 · 自选列表 | 1 · 3 | 10 · 25 | 无限 |
| 价格历史 | 1 年 | 5 年 | 无限 |
| 包含席位 | 1 | 1 | 2 |
| 深度研究 + 智能体团队 | — | ✓ | ✓ |
| 券商账户绑定 | — | ✓ | ✓ |
| 实时券商交易 + 算法部署 | — | — | ✓ |

按月付费 · 无年度绑定 · 无最低席位数 · **按季付费享 8.5 折**。折合**每用户每年 1,188–3,588 美元**，而一个彭博终端席位约为 **27,000 美元** —— [查看完整对比](https://fincept.in/comparison)。

**高校与学术机构：** 统一套餐 —— **5 个 Exclusive Pro 席位每月 699 美元**（标价 1,495 美元）。请联系 [support@fincept.in](mailto:support@fincept.in)。

这三档方案加上学术套餐就是全部价格表。没有定制或议价的企业报价，也不再单独出售商业许可证。

[**创建 Enterprise 账号**](https://fincept.in/enterprise/signup) · [**预约演示**](https://calendly.com/nikultilak/fincept-terminal-demo) · [阅读手册](https://fincept.in/manual)

---

## 安装开源版

**Windows x64**、**Linux x64**（`.run` / `.deb` / `.rpm`）与 **macOS（Apple 芯片）**的安装包见 [Releases 页面](https://github.com/Fincept-Corporation/FinceptTerminal/releases/latest)。

**从源码构建** —— Linux/macOS：`git clone … && ./setup.sh`。Windows、手动构建、锁定的工具链（**CMake 3.27.7 · Ninja 1.11.1 · Qt 6.8.3 · Python 3.11.9**）与故障排查见 **[docs/GETTING_STARTED.md](../GETTING_STARTED.md)**。版本已锁定，更新或更旧的版本均不受支持。

> 在找 Enterprise 版本？它有面向 Windows、macOS 与 Linux 的独立签名安装包，需通过 Enterprise 账号登录获取 —— [在此获取](https://fincept.in/enterprise)。

---

## 开源版包含什么

| | |
|---|---|
| **多资产分析** | DCF、组合优化、VaR/夏普比率、衍生品定价、固定收益、另类资产 —— 通过内嵌 Python 实现 |
| **QuantLib 套件** | 18 个量化模块 —— 定价、风险、随机过程、波动率、固定收益 |
| **AI 智能体** | 覆盖交易员/投资人、经济与地缘政治框架的 37 个智能体；需自备密钥（OpenAI、Anthropic、Gemini、Groq、DeepSeek、OpenRouter、Ollama） |
| **100+ 数据连接器** | DBnomics、FRED、IMF、世界银行、AkShare、Polygon、Kraken、Yahoo Finance、政府 API |
| **交易** | 加密货币与股票行情、模拟交易引擎、16 家券商接入 |
| **自动化** | 可视化节点编辑器、MCP 工具集成、AI Quant Lab（机器学习、因子挖掘、强化学习） |
| **全球情报** | 海运追踪、地缘政治分析、关系图谱 |

原生 C++20 · Qt6 · 内嵌 Python 3.11 · 单一二进制 · 无 Node.js、无浏览器运行时。

---

## 本仓库的维护方式

本仓库**将持续公开，不会被删除**。已经发布的内容会一直保留。

现在改为**每月发布一次**，而非持续开发，因为团队的日常工作在 Enterprise 上。Issue 与 Pull Request 仍会审阅，修复按月度周期发布。安全问题请报告至 [support@fincept.in](mailto:support@fincept.in)。

---

## 参与贡献

欢迎提交新的数据连接器、AI 智能体、分析模块、C++ 界面与文档。

[贡献指南](../CONTRIBUTING.md) · [C++ 指南](../CPP_CONTRIBUTOR_GUIDE.md) · [Python 指南](../PYTHON_CONTRIBUTOR_GUIDE.md) · [架构说明](../ARCHITECTURE.md) · [报告缺陷](https://github.com/Fincept-Corporation/FinceptTerminal/issues) · [提出需求](https://github.com/Fincept-Corporation/FinceptTerminal/discussions)

---

## Fincept 的其他产品

- **[Fincept Data API](https://docs.fincept.in)** —— 500 多个 REST 接口、423,000 多个标的、2,000 多个数据源。任何账号均含免费额度。
- **[Quantcept](https://quantcept.io)** —— 开源的 AI 驱动命令行金融终端（Apache-2.0）。

---

## 许可证

**AGPL-3.0-or-later** —— 全文见 [LICENSE](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/LICENSE)。

个人使用、学习与学术研究免费。AGPL-3.0 是**强传染性许可证，而非宽松许可证**：如果你分发修改过的版本，或将其作为他人可访问的服务运行，就必须以相同许可证公开你的修改。对大多数法务团队而言，讨论到这一条就结束了 —— 这也是企业选择 **[Enterprise](https://fincept.in/enterprise)** 的原因：它是专有软件，没有任何 copyleft 义务需要处理。不涉及分发的个人使用则没有任何义务。

对于本仓库，Fincept **不再出售单独的商业或学术许可证**。商业、机构与高校需求由 **[Fincept Terminal Enterprise](https://fincept.in/enterprise)** 按上述公开价格提供。

**商标。** “Fincept”、“Fincept Terminal” 及 Fincept 标识均为 Fincept Corporation 的商标。在任何分叉、衍生、改名或商业产品中使用，均须事先获得书面许可。

咨询：[support@fincept.in](mailto:support@fincept.in) · [服务条款](https://fincept.in/terms) · [隐私政策](https://fincept.in/privacy)

© 2025–2026 Fincept Corporation. 保留所有权利。

---

<div align="center">

### **唯一的上限是你的思考，而不是数据。**

⭐ **点星** · 🔄 **分享** · 🤝 **贡献**

<sub>英文原版：<a href="../../README.md">README.md</a></sub>

</div>
