> [!IMPORTANT]
> **两个版本。** **[Enterprise](https://fincept.in/enterprise)** 是面向基金和研究部门的私有闭源版本 —— 41 个模块、专有数据、实时券商路由、SSO，**每用户每月 99 美元**起。**本仓库**是面向学习与学术用途的免费 AGPL-3.0 版本，每月发布一次。
> [对比](https://fincept.in/comparison) · [价格](https://fincept.in/pricing)

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

---

## 关于

**Fincept Terminal** 是一款用于金融研究的原生 C++20 桌面终端 —— Qt6 界面、内嵌 Python 3.11 分析引擎、单一二进制文件，不依赖 Electron。

两个版本运行在同一数据内核之上。**[Enterprise](https://fincept.in/enterprise)** 是团队日常开发的私有闭源版本，面向基金、家族办公室和研究部门。**本仓库**是免费的 AGPL-3.0 版本 —— 学习、个人使用、学术研究 —— 每月发布一次。

如果你是学生、爱好者或学术研究者，用开源版。如果你是机构，或者靠终端赚钱，用 Enterprise：AGPL 的传染性条款不适用，而开源版的真实成本是你自己的数据与 LLM 账单，按 token 计费且没有上限。

| | 开源版 | **Enterprise** |
|---|---|---|
| **许可证** | AGPL-3.0 —— 强传染性 | 专有 —— 无 copyleft 义务 |
| **成本** | 免费，另加自付的数据与 LLM 账单 | 每用户每月 99 / 199 / 299 美元 |
| **数据** | 免费公开数据源，需自备密钥 | 专有数据集、更长历史、时点数据 |
| **AI** | 自备 LLM 密钥 | 含 400–5,000 积分 · 多智能体研究 · 私有数据室 |
| **交易** | 模拟交易 + 券商接入 | 实时券商路由 + 实时算法部署 |
| **管控** | — | SSO/SAML、审计日志、RBAC、SLA 保障支持 |

[**了解 Enterprise →**](https://fincept.in/enterprise) · [完整对比](https://fincept.in/comparison) · [价格](https://fincept.in/pricing) · [常见问题](https://fincept.in/faq)

---

## Enterprise

六大业务台、41 个模块 —— 智能体研究、量化实验室与回测、深度基本面研究、市场与执行、宏观与全球情报，以及你的专属工作区。全部收录于一本 [700 页手册](https://fincept.in/manual)。

| | **Exclusive** | **Exclusive+** ★ | **Exclusive Pro** |
|---|---|---|---|
| | **99 美元**/用户/月 | **199 美元**/用户/月 | **299 美元**/用户/月 |
| AI 积分 / 月 | 400 | 2,000 | 5,000 |
| 深度研究 + 智能体团队 | — | ✓ | ✓ |
| 实时交易 + 算法 | — | — | ✓ |

按月付费、无绑定、无最低席位数、按季付费享 8.5 折 —— 折合**每用户每年 1,188–3,588 美元**，而一个彭博终端席位约为 27,000 美元。**高校：** 5 个 Exclusive Pro 席位每月 **699 美元**。这就是全部价格表：没有议价报价，也没有单独出售的商业许可证。

Enterprise 需要独立账号 —— 免费 Fincept 账号无法登录。

[**创建账号**](https://fincept.in/enterprise/signup) · [**预约演示**](https://calendly.com/nikultilak/fincept-terminal-demo)

---

## 安装

**Windows x64**、**Linux x64**（`.run` / `.deb` / `.rpm`）与 **macOS（Apple 芯片）**的安装包见 [Releases 页面](https://github.com/Fincept-Corporation/FinceptTerminal/releases/latest)。

**从源码构建** —— Linux/macOS：`git clone … && ./setup.sh`。Windows、手动构建、锁定的工具链（**CMake 3.27.7 · Ninja 1.11.1 · Qt 6.8.3 · Python 3.11.9**）与故障排查见 **[docs/GETTING_STARTED.md](../GETTING_STARTED.md)**。版本已锁定，更新或更旧的版本均不受支持。

> 在找 Enterprise 版本？它有面向 Windows、macOS 与 Linux 的独立签名安装包，需通过 Enterprise 账号登录获取 —— [在此获取](https://fincept.in/enterprise)。

---

## 开源版包含什么

- **分析** —— DCF、组合优化、VaR/夏普比率、衍生品定价、固定收益、另类资产，外加 18 个模块的 QuantLib 套件
- **AI** —— 覆盖交易员/投资人、经济与地缘政治的 37 个智能体；需自备密钥（OpenAI、Anthropic、Gemini、Groq、DeepSeek、OpenRouter、Ollama）
- **数据** —— 100 多个连接器：FRED、IMF、世界银行、DBnomics、AkShare、Polygon、Kraken、Yahoo Finance、政府 API
- **交易** —— 加密货币与股票行情、模拟交易引擎、16 家券商接入
- **自动化** —— 可视化节点编辑器、MCP 工具、AI Quant Lab（机器学习、因子挖掘、强化学习）
- **全球情报** —— 海运追踪、地缘政治分析、关系图谱

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

对于本仓库，Fincept 不再出售单独的商业或学术许可证。商业、企业与高校需求由 **[Fincept Terminal Enterprise](https://fincept.in/enterprise)** 按上述公开价格提供。

**商标。** “Fincept”、“Fincept Terminal” 及 Fincept 标识均为 Fincept Corporation 的商标。在任何分叉、衍生、改名或商业产品中使用，均须事先获得书面许可。

咨询：[support@fincept.in](mailto:support@fincept.in) · [服务条款](https://fincept.in/terms) · [隐私政策](https://fincept.in/privacy)

© 2025–2026 Fincept Corporation. 保留所有权利。

---

<div align="center">

### **唯一的上限是你的思考，而不是数据。**

⭐ **点星** · 🔄 **分享** · 🤝 **贡献**

<sub>英文原版：<a href="../../README.md">README.md</a></sub>

</div>
